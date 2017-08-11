//
// Copyright 2011-2012,2015 Ettus Research LLC
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

#ifndef INCLUDED_SHD_UTILS_PATHS_HPP
#define INCLUDED_SHD_UTILS_PATHS_HPP

#include <shd/config.hpp>

#include <boost/filesystem.hpp>

#include <string>
#include <vector>

namespace fs = boost::filesystem;

namespace shd {

    //! Get a string representing the system's temporary directory
    SHD_API std::string get_tmp_path(void);

    //! Get a string representing the system's appdata directory
    SHD_API std::string get_app_path(void);

    //! Get a string representing the system's pkg directory
    SHD_API std::string get_pkg_path(void);

    //! Get SHD library paths
    SHD_API std::vector<fs::path> get_module_paths(void);

    /*! Return the SHD images directory path.
    *
    * This function returns the SHD images installation path on this system. The
    * returned directory path is guaranteed to exist (assuming a valid path is
    * found). This function will look for a directory that exists using this
    * order of precedence:
    *
    *   1) `SHD_IMAGES_DIR` environment variable
    *   2) Any paths passed to this function via `search_paths'
    *   3) SHD package path / share / shd / images
    *
    * The `search_paths` parameter may contain Windows registry keys.  If no
    * directory is found, an empty string is returned.
    *
    * \param search_paths A comma-separated list of hints for paths to include.
    * \returns A path string if one is found, or an empty string on failure.
    */
    SHD_API std::string get_images_dir(const std::string &search_paths);

    /*! Return the full path to particular SHD binary image.
     *
     * This function searches for the passed image name, and returns an absolute
     * path to it. The returned path is guaranteed to exist. The caller can also
     * provide a full path to the image in the argument, and this function will
     * validate it and convert it to an absolute system path.
     *
     * \param image_name The name of the file to search for, or the full path.
     * \param search_paths Hints / paths to use when calling `get_images_dir`
     * \return the full system path to the file
     * \throw exception shd::io_error if the file was not found.
     */
    SHD_API std::string find_image_path(const std::string &image_name, const std::string &search_paths = "");

    /*!
     * Search for the location of a particular SHD utility.
     * The utility must be installed in the `shd/utils` directory.
     * \param name the name of the utility to search for
     * \return the full system path to the given utility
     */
    SHD_API std::string find_utility(const std::string &name);

    /*!
     * Return an error string recommending the user run the utility.
     * The error string will include the full path to the utility to run.
     * \return the message suggesting the use of the named utility.
     */
    SHD_API std::string print_utility_error(const std::string &name, const std::string &args = "");
} //namespace shd

#endif /* INCLUDED_SHD_UTILS_PATHS_HPP */
