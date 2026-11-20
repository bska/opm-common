// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*
  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.

  Consult the COPYING file in the top-level source directory of this
  module for the precise wording of the license and the list of
  copyright holders.
*/

#include <config.h>

#include <opm/material/fluidmatrixinteractions/EclEpsScalingPoints.hpp>

#include <opm/input/eclipse/EclipseState/EclipseState.hpp>
#include <opm/input/eclipse/EclipseState/Grid/SatfuncPropertyInitializers.hpp>
#include <opm/input/eclipse/EclipseState/Tables/TableManager.hpp>

#include <opm/input/eclipse/Units/Units.hpp>

#include <opm/material/common/Means.hpp>

#include <opm/material/fluidmatrixinteractions/EclEpsConfig.hpp>
#include <opm/material/fluidmatrixinteractions/EclEpsGridProperties.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
    template <typename Scalar>
    void updateIfNonNull(Scalar& targetValue, const double* value_ptr)
    {
        if (value_ptr != nullptr) {
            targetValue = static_cast<Scalar>(*value_ptr);
        }
    }

    double jfuncPermeability(const Opm::JFunc::Direction      direction,
                             const Opm::EclEpsGridProperties& epsProperties,
                             const std::size_t                activeIndex)
    {
        if (direction == Opm::JFunc::Direction::XY) {
            return Opm::arithmeticMean(epsProperties.permx(activeIndex),
                                       epsProperties.permy(activeIndex));
        }

        if (direction == Opm::JFunc::Direction::X) {
            return epsProperties.permx(activeIndex);
        }

        if (direction == Opm::JFunc::Direction::Y) {
            return epsProperties.permy(activeIndex);
        }

        if (direction == Opm::JFunc::Direction::Z) {
            return epsProperties.permz(activeIndex);
        }

        throw std::runtime_error {
            fmt::format("Illegal direction indicator '{}' for the JFUNC keyword.",
                        static_cast<std::underlying_type_t<Opm::JFunc::Direction>>(direction))
        };
    }
} // Anonymous namespace

template<class Scalar>
void Opm::EclEpsScalingPointsInfo<Scalar>::print() const
{
    std::cout << "    Swl: " << Swl << '\n'
              << "    Sgl: " << Sgl << '\n'
              << "    Swcr: " << Swcr << '\n'
              << "    Sgcr: " << Sgcr << '\n'
              << "    Sowcr: " << Sowcr << '\n'
              << "    Sogcr: " << Sogcr << '\n'
              << "    Swu: " << Swu << '\n'
              << "    Sgu: " << Sgu << '\n'
              << "    maxPcow: " << maxPcow << '\n'
              << "    maxPcgo: " << maxPcgo << '\n'
              << "    pcowLeverettFactor: " << pcowLeverettFactor << '\n'
              << "    pcgoLeverettFactor: " << pcgoLeverettFactor << '\n'
              << "    Krwr: " << Krwr << '\n'
              << "    Krgr: " << Krgr << '\n'
              << "    Krorw: " << Krorw << '\n'
              << "    Krorg: " << Krorg << '\n'
              << "    maxKrw: " << maxKrw << '\n'
              << "    maxKrg: " << maxKrg << '\n'
              << "    maxKrow: " << maxKrow << '\n'
              << "    maxKrog: " << maxKrog << '\n';
}

template<class Scalar>
void Opm::EclEpsScalingPointsInfo<Scalar>::
extractUnscaled(const satfunc::RawTableEndPoints&    rtep,
                const satfunc::RawFunctionValues&    rfunc,
                const std::vector<double>::size_type satRegionIdx)
{
    this->Swl = rtep.connate.water[satRegionIdx];
    this->Sgl = rtep.connate.gas  [satRegionIdx];

    this->Swcr  = rtep.critical.water       [satRegionIdx];
    this->Sgcr  = rtep.critical.gas         [satRegionIdx];
    this->Sowcr = rtep.critical.oil_in_water[satRegionIdx];
    this->Sogcr = rtep.critical.oil_in_gas  [satRegionIdx];

    this->Swu = rtep.maximum.water[satRegionIdx];
    this->Sgu = rtep.maximum.gas  [satRegionIdx];

    this->maxPcgo = rfunc.pc.g[satRegionIdx];
    this->maxPcow = rfunc.pc.w[satRegionIdx];

    // there are no "unscaled" Leverett factors, so we just set them to 1.0
    this->pcowLeverettFactor = 1.0;
    this->pcgoLeverettFactor = 1.0;

    this->Krwr    = rfunc.krw.r [satRegionIdx];
    this->Krgr    = rfunc.krg.r [satRegionIdx];
    this->Krorw   = rfunc.kro.rw[satRegionIdx];
    this->Krorg   = rfunc.kro.rg[satRegionIdx];

    this->maxKrw  = rfunc.krw.max[satRegionIdx];
    this->maxKrow = rfunc.kro.max[satRegionIdx];
    this->maxKrog = rfunc.kro.max[satRegionIdx];
    this->maxKrg  = rfunc.krg.max[satRegionIdx];
}

template<class Scalar>
void Opm::EclEpsScalingPointsInfo<Scalar>::
extractScaled(const EclipseState&         eclState,
              const EclEpsGridProperties& epsProperties,
              unsigned                    activeIndex)
{
    // overwrite the unscaled values with the values for the cell if it is
    // explicitly specified by the corresponding keyword.
    updateIfNonNull(this->Swl,     epsProperties.swl(activeIndex));
    updateIfNonNull(this->Sgl,     epsProperties.sgl(activeIndex));
    updateIfNonNull(this->Swcr,    epsProperties.swcr(activeIndex));
    updateIfNonNull(this->Sgcr,    epsProperties.sgcr(activeIndex));

    updateIfNonNull(this->Sowcr,   epsProperties.sowcr(activeIndex));
    updateIfNonNull(this->Sogcr,   epsProperties.sogcr(activeIndex));
    updateIfNonNull(this->Swu,     epsProperties.swu(activeIndex));
    updateIfNonNull(this->Sgu,     epsProperties.sgu(activeIndex));
    updateIfNonNull(this->maxPcow, epsProperties.pcw(activeIndex));
    updateIfNonNull(this->maxPcgo, epsProperties.pcg(activeIndex));

    updateIfNonNull(this->Krwr,  epsProperties.krwr(activeIndex));
    updateIfNonNull(this->Krgr,  epsProperties.krgr(activeIndex));
    updateIfNonNull(this->Krorw, epsProperties.krorw(activeIndex));
    updateIfNonNull(this->Krorg, epsProperties.krorg(activeIndex));

    updateIfNonNull(this->maxKrw,  epsProperties.krw(activeIndex));
    updateIfNonNull(this->maxKrg,  epsProperties.krg(activeIndex));
    updateIfNonNull(this->maxKrow, epsProperties.kro(activeIndex));
    updateIfNonNull(this->maxKrog, epsProperties.kro(activeIndex));

    this->pcowLeverettFactor = 1.0;
    this->pcgoLeverettFactor = 1.0;

    if (eclState.getTableManager().useJFunc()) {
        this->calculateLeverettFactors(eclState, epsProperties, activeIndex);
    }
}

template <typename Scalar>
void Opm::EclEpsScalingPointsInfo<Scalar>::
calculateLeverettFactors(const EclipseState&         eclState,
                         const EclEpsGridProperties& epsProperties,
                         const unsigned              activeIndex)
{
    // Compute the Leverett capillary pressure scaling factors.
    const auto& jfunc = eclState.getTableManager().getJFunc();

    const auto alpha = static_cast<Scalar>(jfunc.porosityExponent());
    const auto beta  = static_cast<Scalar>(jfunc.permeabilityExponent());
    const auto poro  = static_cast<Scalar>(epsProperties.poro(activeIndex));
    const auto perm  = static_cast<Scalar>
        (jfuncPermeability(jfunc.direction(), epsProperties, activeIndex));

    // Just to guard against division by zero in zero-permeability cells.
    constexpr auto minPerm = static_cast<Scalar>
        (1.0e-10 * prefix::milli*unit::darcy);

    // The part of the Leverett capillary pressure scaling factor which does
    // not depend on the phase-systen specific surface tension.
    const auto commonFactor = std::pow(poro, alpha)
        / std::pow(std::max(perm, minPerm), beta);

    const auto jfuncFlag = jfunc.flag();

    if ((jfuncFlag == JFunc::Flag::WATER) || (jfuncFlag == JFunc::Flag::BOTH)) {
        // Oil-water Leverett factor.
        this->pcowLeverettFactor = commonFactor *
            static_cast<Scalar>(jfunc.owSurfaceTension());
    }

    if ((jfuncFlag == JFunc::Flag::GAS) || (jfuncFlag == JFunc::Flag::BOTH)) {
        // Gas-oil Leverett factor.
        this->pcgoLeverettFactor = commonFactor *
            static_cast<Scalar>(jfunc.goSurfaceTension());
    }
}

// ---------------------------------------------------------------------------

template<class Scalar>
void Opm::EclEpsScalingPoints<Scalar>::
init(const EclEpsScalingPointsInfo<Scalar>& epsInfo,
     const EclEpsConfig&                    config,
     const EclTwoPhaseSystemType            epsSystemType)
{
    if (epsSystemType == EclTwoPhaseSystemType::OilWater) {
        // saturation scaling for capillary pressure
        saturationPcPoints_[0] = epsInfo.Swl;
        saturationPcPoints_[2] = saturationPcPoints_[1] = epsInfo.Swu;

        // krw saturation scaling endpoints
        saturationKrwPoints_[0] = epsInfo.Swcr;
        saturationKrwPoints_[1] = 1.0 - epsInfo.Sowcr - epsInfo.Sgl;
        saturationKrwPoints_[2] = epsInfo.Swu;

        // krn saturation scaling endpoints (with the non-wetting phase being oil).
        // because opm-material specifies non-wetting phase relperms in terms of the
        // wetting phase saturations, the code here uses 1 minus the values specified
        // by the Eclipse TD and the order of the scaling points is reversed
        saturationKrnPoints_[2] = 1.0 - epsInfo.Sowcr;
        saturationKrnPoints_[1] = epsInfo.Swcr + epsInfo.Sgl;
        saturationKrnPoints_[0] = epsInfo.Swl + epsInfo.Sgl;

        if (config.enableLeverettScaling())
            maxPcnwOrLeverettFactor_ = epsInfo.pcowLeverettFactor;
        else
            maxPcnwOrLeverettFactor_ = epsInfo.maxPcow;

        Krwr_   = epsInfo.Krwr;
        Krnr_   = epsInfo.Krorw;

        maxKrw_ = epsInfo.maxKrw;
        maxKrn_ = epsInfo.maxKrow;
    }
    else if (epsSystemType == EclTwoPhaseSystemType::GasOil) {
        // saturation scaling for capillary pressure
        saturationPcPoints_[0] = 1.0 - epsInfo.Swl - epsInfo.Sgu;
        saturationPcPoints_[2] = saturationPcPoints_[1] = 1.0 - epsInfo.Swl - epsInfo.Sgl;

        // krw saturation scaling endpoints
        saturationKrwPoints_[0] = epsInfo.Sogcr;
        saturationKrwPoints_[1] = 1.0 - epsInfo.Sgcr - epsInfo.Swl;
        saturationKrwPoints_[2] = 1.0 - epsInfo.Swl - epsInfo.Sgl;

        // krn saturation scaling endpoints (with the non-wetting phase being gas).
        //
        // As opm-material specifies non-wetting phase relative
        // permeabilities in terms of the wetting phase saturations, the
        // code here uses (1-SWL) minus the values specified by the
        // ECLIPSE TD and the order of the scaling points is reversed.
        saturationKrnPoints_[2] = 1.0 - epsInfo.Swl - epsInfo.Sgcr;
        saturationKrnPoints_[1] = epsInfo.Sogcr;
        saturationKrnPoints_[0] = 1.0 - epsInfo.Swl - epsInfo.Sgu;

        if (config.enableLeverettScaling())
            maxPcnwOrLeverettFactor_ = epsInfo.pcgoLeverettFactor;
        else
            maxPcnwOrLeverettFactor_ = epsInfo.maxPcgo;

        Krwr_   = epsInfo.Krorg;
        Krnr_   = epsInfo.Krgr;

        maxKrw_ = epsInfo.maxKrog;
        maxKrn_ = epsInfo.maxKrg;
    }
    else {
        assert(epsSystemType == EclTwoPhaseSystemType::GasWater);

        // saturation scaling for capillary pressure
        saturationPcPoints_[0] = epsInfo.Swl;
        saturationPcPoints_[2] = saturationPcPoints_[1] = epsInfo.Swu;

        // krw saturation scaling endpoints
        saturationKrwPoints_[0] = epsInfo.Swcr;
        saturationKrwPoints_[1] = 1.0 - epsInfo.Sgcr;
        saturationKrwPoints_[2] = epsInfo.Swu;

        // krn saturation scaling endpoints (with the non-wetting phase being gas)
        saturationKrnPoints_[2] = 1.0 - epsInfo.Sgcr;
        saturationKrnPoints_[1] = epsInfo.Swcr;
        saturationKrnPoints_[0] = 1.0 - epsInfo.Sgu;

        // Pcgo is used for Pcgw for gas-water systems
        if (config.enableLeverettScaling())
            maxPcnwOrLeverettFactor_ = epsInfo.pcgoLeverettFactor;
        else
            maxPcnwOrLeverettFactor_ = epsInfo.maxPcgo;

        Krwr_   = epsInfo.Krwr;
        Krnr_   = epsInfo.Krgr;

        maxKrw_ = epsInfo.maxKrw;
        maxKrn_ = epsInfo.maxKrg;
    }
}

template<class Scalar>
void Opm::EclEpsScalingPoints<Scalar>::print() const
{
    std::cout << "    saturationKrnPoints_[0]: " << saturationKrnPoints_[0] << "\n"
              << "    saturationKrnPoints_[1]: " << saturationKrnPoints_[1] << "\n"
              << "    saturationKrnPoints_[2]: " << saturationKrnPoints_[2] << "\n";
}

// ===========================================================================

template struct Opm::EclEpsScalingPointsInfo<float>;
template struct Opm::EclEpsScalingPointsInfo<double>;

// ---------------------------------------------------------------------------

template class Opm::EclEpsScalingPoints<float>;
template class Opm::EclEpsScalingPoints<double>;
