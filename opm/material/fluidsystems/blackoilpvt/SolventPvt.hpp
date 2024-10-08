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
 * \copydoc Opm::SolventPvt
 */
#ifndef OPM_SOLVENT_PVT_HPP
#define OPM_SOLVENT_PVT_HPP

#include <opm/material/common/Tabulated1DFunction.hpp>

#include <cstddef>
#include <vector>

namespace Opm {

#if HAVE_ECL_INPUT
class EclipseState;
class Schedule;
#endif

/*!
 * \brief This class represents the Pressure-Volume-Temperature relations of the "second"
 *        gas phase in the of ECL simulations with solvents.
 */
template <class Scalar>
class SolventPvt
{
    using SamplingPoints = std::vector<std::pair<Scalar, Scalar>>;

public:
    using TabulatedOneDFunction = Tabulated1DFunction<Scalar>;

#if HAVE_ECL_INPUT
    /*!
     * \brief Initialize the parameters for "solvent gas" using an ECL deck.
     *
     * This method assumes that the deck features valid SDENSITY and PVDS keywords.
     */
    void initFromState(const EclipseState& eclState, const Schedule&);
#endif

    void setNumRegions(std::size_t numRegions);

    void setVapPars(const Scalar, const Scalar)
    {
    }

    /*!
     * \brief Initialize the reference density of the solvent gas for a given PVT region
     */
    void setReferenceDensity(unsigned regionIdx, Scalar rhoRefSolvent)
    { solventReferenceDensity_[regionIdx] = rhoRefSolvent; }

    /*!
     * \brief Initialize the viscosity of the solvent gas phase.
     *
     * This is a function of \f$(p_g)\f$...
     */
    void setSolventViscosity(unsigned regionIdx, const TabulatedOneDFunction& mug)
    { solventMu_[regionIdx] = mug; }

    /*!
     * \brief Initialize the function for the formation volume factor of solvent gas
     *
     * \param samplePoints A container of \f$(p_g, B_s)\f$ values
     */
    void setSolventFormationVolumeFactor(unsigned regionIdx,
                                         const SamplingPoints& samplePoints);

    /*!
     * \brief Finish initializing the oil phase PVT properties.
     */
    void initEnd();

    /*!
     * \brief Return the number of PVT regions which are considered by this PVT-object.
     */
    unsigned numRegions() const
    { return solventReferenceDensity_.size(); }

    /*!
     * \brief Returns the dynamic viscosity [Pa s] of the fluid phase given a set of parameters.
     */
    template <class Evaluation>
    Evaluation viscosity(unsigned regionIdx,
                                  const Evaluation&,
                                  const Evaluation& pressure) const
    {
        const Evaluation& invBg = inverseSolventB_[regionIdx].eval(pressure, /*extrapolate=*/true);
        const Evaluation& invMugBg = inverseSolventBMu_[regionIdx].eval(pressure, /*extrapolate=*/true);

        return invBg / invMugBg;
    }

    template <class Evaluation>
    Evaluation diffusionCoefficient(const Evaluation& /*temperature*/,
                                    const Evaluation& /*pressure*/,
                                    unsigned /*compIdx*/) const
    {
        throw std::runtime_error("Not implemented: The PVT model does not provide "
                                 "a diffusionCoefficient()");
    }

    /*!
     * \brief Return the reference density the solvent phase for a given PVT region
     */
    Scalar referenceDensity(unsigned regionIdx) const
    { return solventReferenceDensity_[regionIdx]; }

    /*!
     * \brief Returns the formation volume factor [-] of the fluid phase.
     */
    template <class Evaluation>
    Evaluation inverseFormationVolumeFactor(unsigned regionIdx,
                                            const Evaluation&,
                                            const Evaluation& pressure) const
    { return inverseSolventB_[regionIdx].eval(pressure, /*extrapolate=*/true); }

    const std::vector<Scalar>& solventReferenceDensity() const
    { return solventReferenceDensity_; }

    const std::vector<TabulatedOneDFunction>& inverseSolventB() const
    { return inverseSolventB_; }

    const std::vector<TabulatedOneDFunction>& solventMu() const
    { return solventMu_; }

    const std::vector<TabulatedOneDFunction>& inverseSolventBMu() const
    { return inverseSolventBMu_; }

private:
    std::vector<Scalar> solventReferenceDensity_{};
    std::vector<TabulatedOneDFunction> inverseSolventB_{};
    std::vector<TabulatedOneDFunction> solventMu_{};
    std::vector<TabulatedOneDFunction> inverseSolventBMu_{};
};

} // namespace Opm

#endif
