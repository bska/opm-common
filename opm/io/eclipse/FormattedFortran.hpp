/*
  Copyright (c) 2020 Equinor ASA

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OPM_FORMATTEDFORTRAN_HEADER_HPP
#define OPM_FORMATTEDFORTRAN_HEADER_HPP

#include <string>

namespace Opm { namespace EclIO {

std::string formatE100(const float x);
std::string formatE100(const double x);

std::string formatIX(const float x);
std::string formatIX(const double x);

}} // Opm::EclIO
#endif // OPM_FORMATTEDFORTRAN_HEADER_HPP
