//
// Copyright 2011 Ettus Research LLC
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

#include <shd/utils/static.hpp>
#include <stdexcept>
#include <iostream>
_shd_static_fixture::_shd_static_fixture(void (*fcn)(void), const char *name){
    try{
        fcn();
    }
    catch(const std::exception &e){
        std::cerr << "Exception in static block " << name << std::endl;
        std::cerr << "  " << e.what() << std::endl;
    }
    catch(...){
        std::cerr << "Exception in static block " << name << std::endl;
    }
}
