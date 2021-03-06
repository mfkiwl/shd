//
// Copyright 2010,2012,2014 Ettus Research LLC
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "smini_cal_utils.hpp"
#include <shd/utils/thread_priority.hpp>
#include <shd/utils/safe_main.hpp>
#include <shd/utils/paths.hpp>
#include <shd/utils/algorithm.hpp>
#include <shd/utils/msg.hpp>
#include <shd/smini/multi_smini.hpp>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/thread/thread.hpp>
#include <boost/math/special_functions/round.hpp>
#include <iostream>
#include <complex>
#include <cmath>
#include <ctime>
#include <cstdlib>

namespace po = boost::program_options;

/***********************************************************************
 * Transmit thread
 **********************************************************************/
static void tx_thread(shd::smini::multi_smini::sptr smini, const double tx_wave_ampl)
{
    shd::set_thread_priority_safe();

    //create a transmit streamer
    shd::stream_args_t stream_args("fc32"); //complex floats
    shd::tx_streamer::sptr tx_stream = smini->get_tx_stream(stream_args);

    //setup variables and allocate buffer
    shd::tx_metadata_t md;
    md.has_time_spec = false;
    std::vector<samp_type> buff(tx_stream->get_max_num_samps()*10);

    //fill buff and send until interrupted
    while (not boost::this_thread::interruption_requested())
    {
        for (size_t i = 0; i < buff.size(); i++)
            buff[i] = float(tx_wave_ampl);
        tx_stream->send(&buff.front(), buff.size(), md);
    }

    //send a mini EOB packet
    md.end_of_burst = true;
    tx_stream->send("", 0, md);
}

/***********************************************************************
 * Tune RX and TX routine
 **********************************************************************/
static double tune_rx_and_tx(shd::smini::multi_smini::sptr smini, const double rx_lo_freq, const double tx_offset)
{
    //tune the receiver with no cordic
    shd::tune_request_t rx_tune_req(rx_lo_freq);
    rx_tune_req.dsp_freq_policy = shd::tune_request_t::POLICY_MANUAL;
    rx_tune_req.dsp_freq = 0;
    smini->set_rx_freq(rx_tune_req);

    //tune the transmitter
    double tx_freq = smini->get_rx_freq() + tx_offset;
    double min_fe_tx_freq = smini->get_fe_tx_freq_range().start();
    double max_fe_tx_freq = smini->get_fe_tx_freq_range().stop();
    shd::tune_request_t tx_tune_req(tx_freq);
    tx_tune_req.dsp_freq_policy = shd::tune_request_t::POLICY_MANUAL;
    tx_tune_req.dsp_freq = 0;
    if (tx_freq < min_fe_tx_freq)
        tx_tune_req.dsp_freq = tx_freq - min_fe_tx_freq;
    else if (tx_freq > max_fe_tx_freq)
        tx_tune_req.dsp_freq = tx_freq - max_fe_tx_freq;
    smini->set_tx_freq(tx_tune_req);

    //wait for the LOs to become locked
    boost::this_thread::sleep(boost::posix_time::milliseconds(50));
    boost::system_time start = boost::get_system_time();
    while (not smini->get_tx_sensor("lo_locked").to_bool() or not smini->get_rx_sensor("lo_locked").to_bool())
    {
        if (boost::get_system_time() > start + boost::posix_time::milliseconds(100))
            throw std::runtime_error("timed out waiting for TX and/or RX LO to lock");
    }

    return smini->get_rx_freq();
}

/***********************************************************************
 * Main
 **********************************************************************/
int SHD_SAFE_MAIN(int argc, char *argv[])
{
    std::string args, subdev, serial;
    double tx_wave_ampl, tx_offset;
    double freq_start, freq_stop, freq_step;
    size_t nsamps;
    double precision;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "help message")
        ("verbose", "enable some verbose")
        ("args", po::value<std::string>(&args)->default_value(""), "Device address args [default = \"\"]")
        ("subdev", po::value<std::string>(&subdev), "Subdevice specification (default: first subdevice, often 'A')")
        ("tx_wave_ampl", po::value<double>(&tx_wave_ampl)->default_value(0.7), "Transmit wave amplitude")
        ("tx_offset", po::value<double>(&tx_offset)->default_value(.9344e6), "TX LO offset from the RX LO in Hz")
        ("freq_start", po::value<double>(&freq_start), "Frequency start in Hz (do not specify for default)")
        ("freq_stop", po::value<double>(&freq_stop), "Frequency stop in Hz (do not specify for default)")
        ("freq_step", po::value<double>(&freq_step)->default_value(default_freq_step), "Step size for LO sweep in Hz")
        ("nsamps", po::value<size_t>(&nsamps), "Samples per data capture")
        ("precision", po::value<double>(&precision)->default_value(default_precision), "Correction precision (default=0.0001)")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    //print the help message
    if (vm.count("help")){
        std::cout << boost::format("SMINI Generate RX IQ Balance Calibration Table %s") % desc << std::endl;
        std::cout <<
            "This application measures leakage between RX and TX on a transceiver daughterboard to self-calibrate.\n"
            "Note: Not all daughterboards support this feature. Refer to the SHD manual for details.\n"
            << std::endl;
        return EXIT_FAILURE;
    }

    // Create a SMINI device
    shd::smini::multi_smini::sptr smini = setup_smini_for_cal(args, subdev, serial);

    if (not vm.count("nsamps"))
        nsamps = size_t(smini->get_rx_rate() / default_fft_bin_size);

    //create a receive streamer
    shd::stream_args_t stream_args("fc32"); //complex floats
    shd::rx_streamer::sptr rx_stream = smini->get_rx_stream(stream_args);

    //create a transmitter thread
    boost::thread_group threads;
    threads.create_thread(boost::bind(&tx_thread, smini, tx_wave_ampl));

    //re-usable buffer for samples
    std::vector<samp_type> buff;

    //store the results here
    std::vector<result_t> results;

    if (not vm.count("freq_start")) freq_start = smini->get_fe_rx_freq_range().start();
    if (not vm.count("freq_stop")) freq_stop = smini->get_fe_tx_freq_range().stop();

    //check start and stop frequencies
    if (freq_start < smini->get_fe_rx_freq_range().start())
    {
        std::cerr << "freq_start must be " << smini->get_fe_rx_freq_range().start() << " or greater for this daughter board" << std::endl;
        return EXIT_FAILURE;
    }
    if (freq_stop > smini->get_fe_rx_freq_range().stop())
    {
        std::cerr << "freq_stop must be " << smini->get_fe_rx_freq_range().stop() << " or less for this daughter board" << std::endl;
        return EXIT_FAILURE;
    }

    //check tx_offset
    double min_tx_offset = smini->get_tx_freq_range().start() - smini->get_fe_rx_freq_range().start();
    double max_tx_offset = smini->get_tx_freq_range().stop() - smini->get_fe_rx_freq_range().stop();
    if (tx_offset < min_tx_offset or tx_offset > max_tx_offset)
    {
        std::cerr << "tx_offset must be between " << min_tx_offset << " and "
            << max_tx_offset << " for this daughter board" << std::endl;
        return EXIT_FAILURE;
    }

    SHD_MSG(status) << boost::format("Calibration frequency range: %d MHz -> %d MHz") % (freq_start/1e6) % (freq_stop/1e6) << std::endl;

    for (double rx_lo_i = freq_start; rx_lo_i <= freq_stop; rx_lo_i += freq_step)
    {
        const double rx_lo = tune_rx_and_tx(smini, rx_lo_i, tx_offset);

        //frequency constants for this tune event
        const double actual_rx_rate = smini->get_rx_rate();
        const double actual_tx_freq = smini->get_tx_freq();
        const double actual_rx_freq = smini->get_rx_freq();
        const double bb_tone_freq = actual_tx_freq - actual_rx_freq;
        const double bb_imag_freq = -bb_tone_freq;

        //reset RX IQ balance
        smini->set_rx_iq_balance(0.0);

        //set optimal RX gain setting for this frequency
        set_optimal_rx_gain(smini, rx_stream);

        //capture initial uncorrected value
        capture_samples(smini, rx_stream, buff, nsamps);
        const double initial_suppression = compute_tone_dbrms(buff, bb_tone_freq/actual_rx_rate) - compute_tone_dbrms(buff, bb_imag_freq/actual_rx_rate);

        //bounds and results from searching
        double phase_corr_start = -1.0;
        double phase_corr_stop = 1.0;
        double phase_corr_step = (phase_corr_stop - phase_corr_start)/(num_search_steps+1);
        double ampl_corr_start = -1.0;
        double ampl_corr_stop = 1.0;
        double ampl_corr_step = (ampl_corr_stop - ampl_corr_start)/(num_search_steps+1);
        double best_suppression = 0;
        double best_phase_corr = 0;
        double best_ampl_corr = 0;
        while (phase_corr_step >= precision or ampl_corr_step >= precision)
        {
            for (double phase_corr = phase_corr_start + phase_corr_step; phase_corr <= phase_corr_stop - phase_corr_step; phase_corr += phase_corr_step)
            {
                for (double ampl_corr = ampl_corr_start + ampl_corr_step; ampl_corr <= ampl_corr_stop - ampl_corr_step; ampl_corr += ampl_corr_step)
                {
                    const std::complex<double> correction(ampl_corr, phase_corr);
                    smini->set_rx_iq_balance(correction);

                    //receive some samples
                    capture_samples(smini, rx_stream, buff, nsamps);
                    const double tone_dbrms = compute_tone_dbrms(buff, bb_tone_freq/actual_rx_rate);
                    const double imag_dbrms = compute_tone_dbrms(buff, bb_imag_freq/actual_rx_rate);
                    const double suppression = tone_dbrms - imag_dbrms;

                    if (suppression > best_suppression)
                    {
                        best_suppression = suppression;
                        best_phase_corr = phase_corr;
                        best_ampl_corr = ampl_corr;
                    }
                }
            }

            phase_corr_start = best_phase_corr - phase_corr_step;
            phase_corr_stop = best_phase_corr + phase_corr_step;
            phase_corr_step = (phase_corr_stop - phase_corr_start)/(num_search_steps+1);
            ampl_corr_start = best_ampl_corr - ampl_corr_step;
            ampl_corr_stop = best_ampl_corr + ampl_corr_step;
            ampl_corr_step = (ampl_corr_stop - ampl_corr_start)/(num_search_steps+1);
        }

        if (best_suppression > initial_suppression) //keep result
        {
            result_t result;
            result.freq = rx_lo;
            result.real_corr = best_ampl_corr;
            result.imag_corr = best_phase_corr;
            result.best = best_suppression;
            result.delta = best_suppression - initial_suppression;
            results.push_back(result);
            if (vm.count("verbose"))
                std::cout << boost::format("RX IQ: %f MHz: best suppression %f dB, corrected %f dB") % (rx_lo/1e6) % result.best % result.delta << std::endl;
            else
                std::cout << "." << std::flush;
        }
    }
    std::cout << std::endl;

    //stop the transmitter
    threads.interrupt_all();
    boost::this_thread::sleep(boost::posix_time::milliseconds(500));    //wait for threads to finish
    threads.join_all();

    store_results(results, "RX", "rx", "iq", serial);

    return EXIT_SUCCESS;
}
