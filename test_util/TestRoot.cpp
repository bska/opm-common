/*
  Copyright 2020 Equinor ASA

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

#include <config.h>

#include "TestRoot.hpp"

#define X_MAKE_STRING(s) #s
#define MAKE_STRING(s) X_MAKE_STRING(s)

#define X_EXPAND(x) x ## 1
#define EXPAND(x) X_EXPAND(x)

#if !defined(OPM_TESTS_ROOT) || (EXPAND(OPM_TESTS_ROOT) == 1)
// OPM_TESTS_ROOT is undefined or has an empty definition.  Make it an empty
// string (literal) to ensure that MAKE_STRING does the right thing.
#undef OPM_TESTS_ROOT
#define OPM_TESTS_ROOT ""
#endif

Opm::filesystem::path Opm::TestUtility::testRoot()
{
    return { MAKE_STRING(OPM_TESTS_ROOT) };
}
