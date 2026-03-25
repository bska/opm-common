/*
  Copyright (c) 2026 Equinor ASA

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

#ifndef RESTART_OUTPUT_MANAGER_HPP
#define RESTART_OUTPUT_MANAGER_HPP

#include <opm/output/data/Cells.hpp>

#include <opm/output/eclipse/AggregateAquiferData.hpp>
#include <opm/output/eclipse/RestartValue.hpp>

#include <opm/input/eclipse/Units/UnitSystem.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <string>
#include <vector>

namespace Opm {
    class EclipseGrid;
    class EclipseState;
    class Schedule;
    class SummaryState;
    class UDQState;
    class WellTestState;
} // namespace Opm

namespace Opm::Action {
    class State;
} // namespace Opm::Action

namespace Opm::EclIO::OutputStream {
    class RestartBase;
}

namespace Opm::RestartIO {

class RestartOutputManager
{
public:
    using WriteSolutionArray = std::function
        <void(const std::string&                arrayName,
              const data::CellData&             value,
              EclIO::OutputStream::RestartBase& rstFile)>;

    struct DynamicStateValues
    {
        std::span<const RestartValue> solution;
        std::reference_wrapper<const Action::State> actionState;
        std::reference_wrapper<const WellTestState> wellTestState;
        std::reference_wrapper<const SummaryState> summaryState;
        std::reference_wrapper<const UDQState> udqState;
        std::optional<Helpers::AggregateAquiferData>& aquiferData;
    };

    explicit RestartOutputManager(const EclipseState& es,
                                  const EclipseGrid&  grid,
                                  const Schedule&     schedule);

    RestartOutputManager& solutionArrayBackend(const WriteSolutionArray& backend);
    RestartOutputManager& permitExtendedArrays(bool permit);

    void writeRestartStep(const int                         reportStep,
                          const double                      elapsedTime,
                          DynamicStateValues&               state,
                          EclIO::OutputStream::RestartBase& rstFile) const;

private:
    class IsFluidInPlace;

    struct WriteContext
    {
        int reportStep{};
        double elapsedTime{};
        std::reference_wrapper<DynamicStateValues> state;
        std::reference_wrapper<EclIO::OutputStream::RestartBase> rstFile;

        std::size_t printIndex{};
        std::size_t gridIndex{};
        std::string_view gridName{};
        std::vector<int> inteHead{};
    };

    std::reference_wrapper<const EclipseState> es_;
    std::reference_wrapper<const EclipseGrid> grid_;
    std::reference_wrapper<const Schedule> schedule_;

    bool permitExtendedArrays_{false};

    std::vector<std::size_t> gridIndices_{};
    std::vector<std::string> gridNames_{};

    std::optional<WriteSolutionArray> solutionArrayBackend_{};

    mutable std::optional<WriteContext> writeContext_{};

    void pushContext(const int                         reportStep,
                     const double                      elapsedTime,
                     DynamicStateValues&               state,
                     EclIO::OutputStream::RestartBase& rstFile) const;

    void popContext() const;

    template <typename T>
    void writeArray(std::string_view      arrayName,
                    const std::vector<T>& arrayValue) const;

    void message(std::string_view msg) const;

    void prologue() const;
    void epilogue() const;

    void headers() const;
    void groups() const;
    void networks() const;
    void wells() const;

    // Note: Member function aquifers() will mutate
    //
    //    writeContext_->state.get().aquiferData
    //
    // if it is engaged.
    void aquifers() const;

    void actions() const;

    void solutionCommon(const IsFluidInPlace& isFip) const;
    void solutionSpecial() const;
    void solutionNNC() const;
    void solutionLocalGlobal() const;
    void solutionLocalLocal() const;

    void logRestartOutputCompleted() const;

    bool hasValidWells() const;
    void writeMultisegWellArrays() const;
    void writeWellArrays() const;
    void writeWellListArrays() const;
    void writeWellConnectionArrays() const;

    void analyticalAquiferData() const;
    void numericalAquiferData() const;

    void regularSolutionArrays(const IsFluidInPlace& isFip) const;
    void fluidInPlaceArrays(const IsFluidInPlace& isFip) const;
    void tracerArrays() const;

    void udqArrays() const;
    void thresholdPressureArray() const;
    void extendedSolutionArrays() const;
    void lgrNames() const;

    void defaultSolutionArrayBackend(const std::string&    arrayName,
                                     const data::CellData& value) const;
};

} // namespace Opm::RestartIO

#endif // RESTART_OUTPUT_MANAGER_HPP
