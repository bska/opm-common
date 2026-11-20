/*
 Copyright 2016  Statoil ASA.

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

#ifndef OPM_JFUNC_HPP_
#define OPM_JFUNC_HPP_

#include <cstddef>

namespace Opm {

class Deck;
class DeckKeyword;
class DeckRecord;
class KeywordLocation;

} // namespace Opm

namespace Opm {

class JFunc
{
public:
    enum class Flag { BOTH, WATER, GAS, };
    enum class Direction { XY, X, Y, Z, };

    JFunc() = default;
    explicit JFunc(const Deck& deck);

    double porosityExponent() const { return this->m_alphaFactor; }
    double permeabilityExponent() const { return this->m_betaFactor; }

    double alphaFactor() const { return this->porosityExponent(); }
    double betaFactor() const { return this->permeabilityExponent(); }

    double goSurfaceTension() const { return this->m_goSurfaceTension; }
    double owSurfaceTension() const { return this->m_owSurfaceTension; }

    Flag flag() const { return this->m_flag; }
    Direction direction() const { return this->m_direction; }

    static JFunc serializationTestObject();

    bool operator==(const JFunc& data) const;

    template<class Serializer>
    void serializeOp(Serializer& serializer)
    {
        serializer(this->m_flag);
        serializer(this->m_owSurfaceTension);
        serializer(this->m_goSurfaceTension);
        serializer(this->m_alphaFactor);
        serializer(this->m_betaFactor);
        serializer(this->m_direction);
    }

private:
    explicit JFunc(const DeckKeyword& jfuncKeyword, const std::size_t recordIndex);
    explicit JFunc(const DeckRecord& jfuncRecord, const KeywordLocation& location);

    /// J-function phase/system flag.
    Flag m_flag { Flag::BOTH };

    /// Oil-water surface tension.
    ///
    /// Required if m_flag is WATER or BOTH.
    double m_owSurfaceTension { 0.0 };

    /// Gas-oil surface tension.
    ///
    /// Required if m_flag is GAS or BOTH.
    double m_goSurfaceTension { 0.0 };

    /// Porosity exponent ("alpha").
    double m_alphaFactor { 0.5 };

    /// Permeability exponent ("beta").
    double m_betaFactor { 0.5 };

    /// Permeability selection direction.
    Direction m_direction { Direction::XY };
};

} // namespace Opm

#endif // OPM_JFUNC_HPP_
