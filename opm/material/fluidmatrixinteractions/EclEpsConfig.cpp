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
/*!
 * \file
 * \copydoc Opm::EclEpsConfig
 */

#include <config.h>

#include <opm/material/fluidmatrixinteractions/EclEpsConfig.hpp>

#include <opm/common/OpmLog/OpmLog.hpp>

#include <opm/input/eclipse/EclipseState/EclipseState.hpp>
#include <opm/input/eclipse/EclipseState/Tables/JFunc.hpp>
#include <opm/input/eclipse/EclipseState/Tables/TableManager.hpp>

#include <stdexcept>
#include <cassert>

#include <fmt/format.h>

namespace {

    void logIncompatibleVerticalCapPressScaling(const Opm::EclTwoPhaseSystemType twoPhaseSystemType,
                                                const std::string&               vertScalingCapPressArray)
    {
        auto system = std::string{};

        switch (twoPhaseSystemType) {
        case Opm::EclTwoPhaseSystemType::OilWater:
            system = "Oil/Water";
            break;

        case Opm::EclTwoPhaseSystemType::GasOil:
            system = "Gas/Oil";
            break;

        case Opm::EclTwoPhaseSystemType::GasWater:
            system = "Gas/Water";
            break;
        }

        Opm::OpmLog::warning(fmt::format(R"({} system capillary pressure vertical scaling array {}
is not applicable and will be ignored in a run with active Leverett J-Function scaling)",
                                         system, vertScalingCapPressArray));
    }
}

namespace Opm {

bool EclEpsConfig::leverettScalingActive(const EclipseState&         eclState,
                                         const EclTwoPhaseSystemType twoPhaseSystemType)
{
    const auto& tmgr = eclState.getTableManager();

    if (! tmgr.useJFunc()) {
        return false;
    }

    if (twoPhaseSystemType == EclTwoPhaseSystemType::GasWater) {
        // Two-phase gas/water runs have a single capillary pressure curve,
        // so Leverett scaling applies irrespective of the JFUNC flag.
        return true;
    }

    switch (tmgr.getJFunc().flag()) {
    case JFunc::Flag::BOTH:
        return true;

    case JFunc::Flag::WATER:
        return twoPhaseSystemType == EclTwoPhaseSystemType::OilWater;

    case JFunc::Flag::GAS:
        return twoPhaseSystemType == EclTwoPhaseSystemType::GasOil;
    }

    return false;
}

void EclEpsConfig::initFromState(const EclipseState& eclState,
                                 EclTwoPhaseSystemType twoPhaseSystemType,
                                 const std::string& prefix,
                                 const std::string& suffix)
{
    const auto& endscale = eclState.runspec().endpointScaling();

    // find out if endpoint scaling is used in the first place
    if (!endscale) {
        // it is not used, i.e., just set all enable$Foo attributes to 0 and be done
        // with it.
        enableSatScaling_ = false;
        enableThreePointKrSatScaling_ = false;
        enablePcScaling_ = false;
        enableLeverettScaling_ = false;
        enableKrwScaling_ = false;
        enableKrnScaling_ = false;
        return;
    }

    // endpoint scaling is used, i.e., at least saturation scaling needs to be enabled
    enableSatScaling_ = true;
    enableThreePointKrSatScaling_ = endscale.threepoint();
    this->enableLeverettScaling_ = leverettScalingActive(eclState, twoPhaseSystemType);

    const auto& fp = eclState.fieldProps();

    // Prefix 'I' selects properties for the imbibition curves.  The
    // drainage curves, without a prefix, inspects the plain keywords.  As
    // an example, a run's imbibition-only IKRWR property must not activate
    // three-point vertical scaling for the drainage relative permeability
    // curve for water.
    auto hasKR = [&fp, &prefix, &suffix](const std::string& scaling)
    {
        return fp.has_double(prefix + "KR" + scaling + suffix);
    };

    auto hasPC = [&fp, &prefix](const std::string& scaling)
    {
        return fp.has_double(prefix + "PC" + scaling);
    };

    auto vertScalingCapPressArray = std::string{};

    // Check if run requests vertical saturation function scaling for this
    // twoPhaseSystemType.
    if (twoPhaseSystemType == EclTwoPhaseSystemType::OilWater) {
        this->setEnableThreePointKrwScaling(hasKR("WR"));
        this->setEnableThreePointKrnScaling(hasKR("ORW"));

        if (hasPC("W")) {
            vertScalingCapPressArray = prefix + "PCW";
        }

        this->enableKrnScaling_ = hasKR("O") || this->enableThreePointKrnScaling();
        this->enableKrwScaling_ = hasKR("W") || this->enableThreePointKrwScaling();
        this->enablePcScaling_  = fp.has_double("SWATINIT")
            || !vertScalingCapPressArray.empty();
    }
    else if (twoPhaseSystemType == EclTwoPhaseSystemType::GasOil) {
        this->setEnableThreePointKrwScaling(hasKR("ORG"));
        this->setEnableThreePointKrnScaling(hasKR("GR"));

        if (hasPC("G")) {
            vertScalingCapPressArray = prefix + "PCG";
        }

        this->enableKrnScaling_ = hasKR("G") || this->enableThreePointKrnScaling();
        this->enableKrwScaling_ = hasKR("O") || this->enableThreePointKrwScaling();
        this->enablePcScaling_  = ! vertScalingCapPressArray.empty();
    }
    else {
        assert(twoPhaseSystemType == EclTwoPhaseSystemType::GasWater);

        this->setEnableThreePointKrwScaling(hasKR("WR"));
        this->setEnableThreePointKrnScaling(hasKR("GR"));

        this->enableKrnScaling_ = hasKR("G") || this->enableThreePointKrnScaling();
        this->enableKrwScaling_ = hasKR("W") || this->enableThreePointKrwScaling();

        if (hasPC("G")) {
            vertScalingCapPressArray = prefix + "PCG";
        }

        // PCG is reused as the gas-water capillary pressure (Pcgw) for
        // two-phase gas-water systems (see EclEpsScalingPoints::init()).
        // SWATINIT scales this curve too
        // (EclMaterialLawManager::applySwatinit).
        this->enablePcScaling_ = fp.has_double("SWATINIT") ||
            ! vertScalingCapPressArray.empty();
    }

    if (this->enableLeverettScaling_) {
        // Leverett J-function scaling is implemented as vertical scaling of
        // the capillary pressure curve.  The maximum capillary pressure is
        // the Leverett factor times the curve's unscaled maximum value (see
        // EclEpsScalingPointsInfo::extractScaled()), and the PCW/PCG
        // keywords are ignored.
        this->enablePcScaling_ = true;

        if (! vertScalingCapPressArray.empty()) {
            logIncompatibleVerticalCapPressScaling(twoPhaseSystemType,
                                                   vertScalingCapPressArray);
        }
    }
}

} // namespace Opm
