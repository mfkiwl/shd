//
// Copyright 2017 Ettus Research
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

#include <shd/transport/zero_copy_flow_ctrl.hpp>
#include <shd/transport/bounded_buffer.hpp>
#include <shd/transport/buffer_pool.hpp>
#include <shd/utils/msg.hpp>
#include <shd/utils/log.hpp>
#include <shd/utils/safe_call.hpp>
#include <boost/format.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>

using namespace shd;
using namespace shd::transport;

typedef bounded_buffer<managed_send_buffer::sptr> bounded_buffer_t;

class zero_copy_flow_ctrl_msb : public managed_send_buffer
{
public:
    zero_copy_flow_ctrl_msb(
        flow_ctrl_func flow_ctrl
    ) :
        _mb(NULL),
        _flow_ctrl(flow_ctrl)
    {
        /* NOP */
    }

    ~zero_copy_flow_ctrl_msb()
    {
        /* NOP */
    }

    void release()
    {
        if (_mb)
        {
            _mb->commit(size());
            while (_flow_ctrl and not _flow_ctrl(_mb)) {}
            _mb.reset();
        }
    }

    SHD_INLINE sptr get(sptr &mb)
    {
        _mb = mb;
        return make(this, _mb->cast<void *>(), _mb->size());
    }

private:
    sptr _mb;
    flow_ctrl_func _flow_ctrl;
};

class zero_copy_flow_ctrl_mrb : public managed_recv_buffer
{
public:
    zero_copy_flow_ctrl_mrb(
        flow_ctrl_func flow_ctrl
    ) :
        _mb(NULL),
        _flow_ctrl(flow_ctrl)
    {
        /* NOP */
    }

    ~zero_copy_flow_ctrl_mrb()
    {
        /* NOP */
    }

    void release()
    {
        if (_mb)
        {
            _mb->commit(size());
            while (_flow_ctrl and not _flow_ctrl(_mb)) {}
            _mb.reset();
        }
    }

    SHD_INLINE sptr get(sptr &mb)
    {
        _mb = mb;
        return make(this, _mb->cast<void *>(), _mb->size());
    }

private:
    sptr _mb;
    flow_ctrl_func _flow_ctrl;
};

/***********************************************************************
 * Zero copy offload transport:
 * An intermediate transport that utilizes threading to free
 * the main thread from any receive work.
 **********************************************************************/
class zero_copy_flow_ctrl_impl : public zero_copy_flow_ctrl {
public:
    typedef boost::shared_ptr<zero_copy_flow_ctrl_impl> sptr;

    zero_copy_flow_ctrl_impl(zero_copy_if::sptr transport,
        flow_ctrl_func send_flow_ctrl,
        flow_ctrl_func recv_flow_ctrl) :
        _transport(transport),
        _send_buffers(transport->get_num_send_frames()),
        _recv_buffers(transport->get_num_recv_frames()),
        _send_buff_index(0),
        _recv_buff_index(0),
        _send_flow_ctrl(send_flow_ctrl),
        _recv_flow_ctrl(recv_flow_ctrl)
    {
        SHD_LOG << "Created zero_copy_flow_ctrl" << std::endl;

        for (size_t i = 0; i < transport->get_num_send_frames(); i++)
        {
            _send_buffers[i] = boost::make_shared<zero_copy_flow_ctrl_msb>(_send_flow_ctrl);
        }
        for (size_t i = 0; i < transport->get_num_recv_frames(); i++)
        {
            _recv_buffers[i] = boost::make_shared<zero_copy_flow_ctrl_mrb>(_recv_flow_ctrl);
        }
    }

    ~zero_copy_flow_ctrl_impl()
    {
    }

    /*******************************************************************
     * Receive implementation:
     * Pop the receive buffer pointer from the underlying transport
     ******************************************************************/
    SHD_INLINE managed_recv_buffer::sptr get_recv_buff(double timeout)
    {
        managed_recv_buffer::sptr ptr;
        managed_recv_buffer::sptr buff = _transport->get_recv_buff(timeout);
        if (buff)
        {
            boost::shared_ptr<zero_copy_flow_ctrl_mrb> mb = _recv_buffers[_recv_buff_index++];
            _recv_buff_index %= _recv_buffers.size();
            ptr = mb->get(buff);
        }
        return ptr;
    }

    SHD_INLINE size_t get_num_recv_frames() const
    {
        return _transport->get_num_recv_frames();
    }

    SHD_INLINE size_t get_recv_frame_size() const
    {
        return _transport->get_recv_frame_size();
    }

    /*******************************************************************
     * Send implementation:
     * Pass the send buffer pointer from the underlying transport
     ******************************************************************/
    managed_send_buffer::sptr get_send_buff(double timeout)
    {
        managed_send_buffer::sptr ptr;
        managed_send_buffer::sptr buff = _transport->get_send_buff(timeout);
        if (buff)
        {
            boost::shared_ptr<zero_copy_flow_ctrl_msb> mb = _send_buffers[_send_buff_index++];
            _send_buff_index %= _send_buffers.size();
            ptr = mb->get(buff);
        }
        return ptr;
    }

    SHD_INLINE size_t get_num_send_frames() const
    {
        return _transport->get_num_send_frames();
    }

    SHD_INLINE size_t get_send_frame_size() const
    {
        return _transport->get_send_frame_size();
    }

private:
    // The underlying transport
    zero_copy_if::sptr _transport;

    // buffers
    std::vector< boost::shared_ptr<zero_copy_flow_ctrl_msb> > _send_buffers;
    std::vector< boost::shared_ptr<zero_copy_flow_ctrl_mrb> > _recv_buffers;
    size_t _send_buff_index;
    size_t _recv_buff_index;

    // Flow control functions
    flow_ctrl_func _send_flow_ctrl;
    flow_ctrl_func _recv_flow_ctrl;
};

zero_copy_flow_ctrl::sptr zero_copy_flow_ctrl::make(
        zero_copy_if::sptr transport,
        flow_ctrl_func send_flow_ctrl,
        flow_ctrl_func recv_flow_ctrl
)
{
    zero_copy_flow_ctrl_impl::sptr zero_copy_flow_ctrl(
        new zero_copy_flow_ctrl_impl(transport, send_flow_ctrl, recv_flow_ctrl)
    );

    return zero_copy_flow_ctrl;
}
