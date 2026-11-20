/*
  Copyright 2016 Statoil ASA.

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

#include <opm/input/eclipse/EclipseState/Tables/JFunc.hpp>

#include <opm/common/OpmLog/KeywordLocation.hpp>
#include <opm/common/utility/OpmInputError.hpp>

#include <opm/input/eclipse/Deck/Deck.hpp>
#include <opm/input/eclipse/Deck/DeckItem.hpp>
#include <opm/input/eclipse/Deck/DeckRecord.hpp>
#include <opm/input/eclipse/Deck/DeckView.hpp>

#include <opm/input/eclipse/Parser/ParserKeywords/J.hpp>

#include <fmt/format.h>

namespace {

    const Opm::DeckKeyword& getJFuncKeyword(const Opm::Deck& deck)
    {
        return deck.get<Opm::ParserKeywords::JFUNC>().front();
    }

    Opm::JFunc::Flag parseJFuncSystemFlag(const Opm::DeckRecord&      jfuncRecord,
                                          const Opm::KeywordLocation& location)
    {
        using Kw = Opm::ParserKeywords::JFUNC;

        const auto flagStr = jfuncRecord.getItem<Kw::FLAG>().getTrimmedString(0);

        if (flagStr == "BOTH") {
            return Opm::JFunc::Flag::BOTH;
        }

        if (flagStr == "WATER") {
            return Opm::JFunc::Flag::WATER;
        }

        if (flagStr == "GAS") {
            return Opm::JFunc::Flag::GAS;
        }

        throw Opm::OpmInputError {
            fmt::format(R"(Two-phase system flag '{}' is not recognised.
Must be one of 'BOTH', 'WATER', or 'GAS'.)", flagStr),
            location
        };
    }

    double extractOilWaterSurfaceTension(const Opm::JFunc::Flag twoPhaseSystemFlag,
                                         const Opm::DeckRecord& jfuncRecord)
    {
        using Kw = Opm::ParserKeywords::JFUNC;

        switch (twoPhaseSystemFlag) {
        case Opm::JFunc::Flag::BOTH:
        case Opm::JFunc::Flag::WATER:
            return jfuncRecord.getItem<Kw::OW_SURFACE_TENSION>().getSIDouble(0);
            break;

        default:
            return 0.0;
        }
    }

    double extractGasOilSurfaceTension(const Opm::JFunc::Flag twoPhaseSystemFlag,
                                       const Opm::DeckRecord& jfuncRecord)
    {
        using Kw = Opm::ParserKeywords::JFUNC;

        switch (twoPhaseSystemFlag) {
        case Opm::JFunc::Flag::BOTH:
        case Opm::JFunc::Flag::GAS:
            return jfuncRecord.getItem<Kw::GO_SURFACE_TENSION>().getSIDouble(0);
            break;

        default:
            return 0.0;
        }
    }

    double extractPorosityExponent(const Opm::DeckRecord& jfuncRecord)
    {
        using Kw = Opm::ParserKeywords::JFUNC;

        return jfuncRecord.getItem<Kw::ALPHA_FACTOR>().getSIDouble(0);
    }

    double extractPermeabilityExponent(const Opm::DeckRecord& jfuncRecord)
    {
        using Kw = Opm::ParserKeywords::JFUNC;

        return jfuncRecord.getItem<Kw::BETA_FACTOR>().getSIDouble(0);
    }

    Opm::JFunc::Direction
    parseJFuncPermeabilityDirection(const Opm::DeckRecord&      jfuncRecord,
                                    const Opm::KeywordLocation& location)
    {
        const auto permDirection = jfuncRecord
            .getItem<Opm::ParserKeywords::JFUNC::DIRECTION>()
            .getTrimmedString(0);

        if (permDirection == "XY") {
            return Opm::JFunc::Direction::XY;
        }

        if (permDirection == "X") {
            return Opm::JFunc::Direction::X;
        }

        if (permDirection == "Y") {
            return Opm::JFunc::Direction::Y;
        }

        if (permDirection == "Z") {
            return Opm::JFunc::Direction::Z;
        }

        throw Opm::OpmInputError {
            fmt::format(R"(Permeability direction '{}' is not recognised.
Must be one of 'XY', 'X', 'Y', or 'Z'.)", permDirection),
            location
        };
    }
}

namespace Opm {

    JFunc::JFunc(const Deck& deck)
        : JFunc { getJFuncKeyword(deck), std::size_t{0} }
    {}

    JFunc::JFunc(const DeckKeyword& jfuncKeyword, const std::size_t recordIndex)
        : JFunc { jfuncKeyword.getRecord(recordIndex), jfuncKeyword.location() }
    {}

    JFunc::JFunc(const DeckRecord& jfuncRecord, const KeywordLocation& location)
        : m_flag             { parseJFuncSystemFlag(jfuncRecord, location) }
        , m_owSurfaceTension { extractOilWaterSurfaceTension(m_flag, jfuncRecord) }
        , m_goSurfaceTension { extractGasOilSurfaceTension(m_flag, jfuncRecord) }
        , m_alphaFactor      { extractPorosityExponent(jfuncRecord) }
        , m_betaFactor       { extractPermeabilityExponent(jfuncRecord) }
        , m_direction        { parseJFuncPermeabilityDirection(jfuncRecord, location) }
    {}

    JFunc JFunc::serializationTestObject()
    {
        JFunc result;
        result.m_flag = Flag::BOTH;
        result.m_owSurfaceTension = 1.0;
        result.m_goSurfaceTension = 2.0;
        result.m_alphaFactor = 3.0;
        result.m_betaFactor = 4.0;
        result.m_direction = Direction::X;

        return result;
    }

    bool JFunc::operator==(const JFunc& data) const {
        return this->flag() == data.flag() &&
               this->owSurfaceTension() == data.owSurfaceTension() &&
               this->goSurfaceTension() == data.goSurfaceTension() &&
               this->alphaFactor() == data.alphaFactor() &&
               this->betaFactor() == data.betaFactor() &&
               this->direction() == data.direction();
    }

} // Opm::
