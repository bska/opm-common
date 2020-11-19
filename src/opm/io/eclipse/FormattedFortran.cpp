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

#include <opm/io/eclipse/FormattedFortran.hpp>

#include <cmath>
#include <string>

#include <fmt/format.h>

namespace {
    template <typename Real>
    struct Frexp {
        Real significand;
        int  exponent;
    };

    struct ZeroStringE100
    {
        std::string operator()(const float) const
        {
            return "0.00000000E+00";
        }

        std::string operator()(const double) const
        {
            return "0.00000000000000D+00";
        }
    };

    struct ZeroStringIX
    {
        std::string operator()(const float) const
        {
            return " 0.0000000E+00";
        }

        std::string operator()(const double) const
        {
            return " 0.0000000000000E+00";
        }
    };

    template <typename Real>
    Frexp<Real> decompose(Real x)
    {
        auto d = Frexp<Real>{};

        const auto ten = Real(10);

        const auto ax = std::abs(x);
        d.exponent    = static_cast<int>(std::ceil(std::log10(ax)));
        d.significand = ax / std::pow(ten, d.exponent);

        if (! (d.significand < Real(1))) {
            // Input is exact power of 10, e.g., -1.0E+20
            d.exponent    += 1;
            d.significand /= ten;
        }

        d.significand = std::copysign(d.significand, x);

        return d;
    }

    template <typename Real>
    bool is_special(const Real x)
    {
        const auto fpclass = std::fpclassify(x);

        return (fpclass == FP_INFINITE)
            || (fpclass == FP_NAN)
            || (fpclass == FP_ZERO);
    }

    template <typename Real, class HandleZero>
    std::string special_formatting(const Real x, HandleZero&& zero)
    {
        const auto fpclass = std::fpclassify(x);

        if (fpclass == FP_ZERO) {
            return zero(x);
        }

        if (fpclass == FP_NAN) {
            return "NAN";
        }

        if (std::signbit(x)) {
            return "-INF";
        }

        return "INF";
    }
}

std::string Opm::EclIO::formatE100(const float x)
{
    using namespace fmt::literals;

    if (is_special(x)) {
        return special_formatting(x, ZeroStringE100{});
    }

    const auto d = decompose(x);
    return fmt::format("{significand:12.8f}E{exponent:+03d}",
                       "significand"_a = d.significand,
                       "exponent"_a = d.exponent);
}

std::string Opm::EclIO::formatE100(const double x)
{
    using namespace fmt::literals;

    if (is_special(x)) {
        return special_formatting(x, ZeroStringE100{});
    }

    const auto d = decompose(x);
    return fmt::format("{significand:18.14f}{D:s}{exponent:+03d}",
                       "significand"_a = d.significand,
                       "exponent"_a = d.exponent,
                       "D"_a = (std::abs(d.exponent) < 100) ? "D" : "");
}

std::string Opm::EclIO::formatIX(const float x)
{
    using namespace fmt::literals;

    if (is_special(x)) {
        return special_formatting(x, ZeroStringIX{});
    }

    return fmt::format("{:%10.7E}", x);
}

std::string Opm::EclIO::formatIX(const double x)
{
    using namespace fmt::literals;

    if (is_special(x)) {
        return special_formatting(x, ZeroStringIX{});
    }

    return fmt::format("{:%19.13E}", x);
}
