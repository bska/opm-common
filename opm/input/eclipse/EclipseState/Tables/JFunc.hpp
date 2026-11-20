/*
  Copyright 2026 Equinor ASA.
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

#ifndef OPM_JFUNC_HPP_
#define OPM_JFUNC_HPP_

#include <cstddef>
#include <optional>

namespace Opm {

class Deck;
class DeckKeyword;
class DeckRecord;
class KeywordLocation;

} // namespace Opm

namespace Opm {

/// Container for JFUNC keyword information
class JFunc
{
public:
    /// System identification flag.
    enum class Flag {
        /// This container provides Leverett J-function input data for both
        /// the gas/oil and the oil/water two-phase systems.
        BOTH,

        /// This container provides Leverett J-function input data for the
        /// oil/water two-phase system.
        WATER,

        /// This container provides Leverett J-function input data for the
        /// gas/oil two-phase system.
        GAS,
    };

    /// Permeability selection algorithm.
    enum class Direction {
        /// J-function permeability is arithmetic average of the cell
        /// permeability in the X and Y directions ((PERMX + PERMY) / 2).
        XY,

        /// J-function permeability is the cell's X-direction permeability
        /// (PERMX).
        X,

        /// J-function permeability is the cell's Y-direction permeability
        /// (PERMY).
        Y,

        /// J-function permeability is the cell's Z-direction permeability
        /// (PERMZ).
        Z,
    };

    /// Default constructor.
    JFunc() = default;

    /// Constructor.
    ///
    /// \param[in] deck Raw input values from a model description.  The
    /// resulting JFunc object will be formed based on the first instance of
    /// the JFUNC keyword in \p deck.  If \p deck does not have a JFUNC
    /// keyword, the constructor throws an exception of type
    /// std::logic_error.
    explicit JFunc(const Deck& deck);

    /// Retrieve exponent of J-function expression porosity term.
    double porosityExponent() const { return this->m_alphaFactor; }

    /// Retrieve exponent of J-function expression permeability term.
    double permeabilityExponent() const { return this->m_betaFactor; }

    /// Retrieve exponent of J-function expression porosity term.
    ///
    /// Older name for porosityExponent().
    double alphaFactor() const { return this->porosityExponent(); }

    /// Retrieve exponent of J-function expression permeability term.
    ///
    /// Older name for permeabilityExponent().
    double betaFactor() const { return this->permeabilityExponent(); }

    /// Retrieve surface tension factor for gas/oil two-phase system.
    ///
    /// Throws an exception of type std::invalid_argument if this container
    /// does not apply to the gas/oil system.  Check flag() before calling
    /// this function.
    double goSurfaceTension() const;

    /// Retrieve surface tension factor for oil/water two-phase system.
    ///
    /// Throws an exception of type std::invalid_argument if this container
    /// does not apply to the oil/water system.  Check flag() before calling
    /// this function.
    double owSurfaceTension() const;

    /// Retrieve this container's two-phase system identification.
    Flag flag() const { return this->m_flag; }

    /// Retrieve this container's permeability selection algorithm.
    Direction direction() const { return this->m_direction; }

    /// Create a serialisation test object.
    static JFunc serializationTestObject();

    /// Equality predicate.
    ///
    /// \param[in] data Object against which \code *this \endcode will be
    /// tested for equality.
    ///
    /// \return Whether or not \code *this \endcode is the same as \p data.
    bool operator==(const JFunc& data) const;

    /// Convert between byte array and object representation.
    ///
    /// \tparam Serializer Byte array conversion protocol.
    ///
    /// \param[in,out] serializer Byte array conversion object.
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
    /// Constructor.
    ///
    /// Forms an information container from a particular record of a JFUNC
    /// keyword.
    ///
    /// \param[in] jfuncKeyword Specific JFUNC keyword data container from
    /// an input deck.
    ///
    /// \param[in] recordIndex Specific index, zero-based, into \p
    /// jfuncKeyword, from which to retrieve the Leverett J-function
    /// information.
    explicit JFunc(const DeckKeyword& jfuncKeyword,
                   const std::size_t  recordIndex);

    /// Constructor.
    ///
    /// Forms an information container from a particular record of a JFUNC
    /// keyword.
    ///
    /// \param[in] jfuncRecord Specific record from a particular JFUNC
    /// keyword in an input deck.
    ///
    /// \param[in] location Input reference for the particular JFUNC
    /// keyword.  Used only for emitting diagnostic messages.
    explicit JFunc(const DeckRecord&      jfuncRecord,
                   const KeywordLocation& location);

    /// J-function phase/system flag.
    Flag m_flag { Flag::BOTH };

    /// Oil-water surface tension.
    ///
    /// Nullopt if m_flag is GAS.
    std::optional<double> m_owSurfaceTension{};

    /// Gas-oil surface tension.
    ///
    /// Nullopt if m_flag is WATER.
    std::optional<double> m_goSurfaceTension{};

    /// Porosity exponent ("alpha").
    double m_alphaFactor { 0.5 };

    /// Permeability exponent ("beta").
    double m_betaFactor { 0.5 };

    /// Permeability selection direction.
    Direction m_direction { Direction::XY };
};

} // namespace Opm

#endif // OPM_JFUNC_HPP_
