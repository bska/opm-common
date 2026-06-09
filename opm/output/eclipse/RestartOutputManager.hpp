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

#include <algorithm>
#include <concepts>
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

/// Manage creation and writing of simulation restart files.
class RestartOutputManager
{
public:
    /// Optional client callback for writing solution arrays.
    ///
    /// \param[in] arrayName Name of the solution array to write
    ///   (e.g., "PRESSURE", "SGAS", "FLROILI+").
    /// \param[in] value Cell data to write for the solution array.
    /// \param[in,out] rstFile Restart file output stream to write to.
    using WriteSolutionArray = std::function
        <void(const std::string&                arrayName,
              const data::CellData&             value,
              EclIO::OutputStream::RestartBase& rstFile)>;

    /// Descriptor for the dynamic simulation state at a particular report step.
    struct DynamicStateValues
    {
        /// Per-cell solution arrays.
        ///
        /// One object for each grid in the simulation, with .front()
        /// corresponding to the main grid.  Ordered according to grid index.
        std::span<const RestartValue> solution;

        /// Run's current action system state.
        ///
        /// Expected to hold current values for the number of times each
        /// action has run and the time of each action's last run.
        std::reference_wrapper<const Action::State> actionState;

        /// Run's current WTEST information.
        ///
        /// Expected to  hold information about those wells that have been
        /// closed due to various runtime conditions.
        std::reference_wrapper<const WellTestState> wellTestState;

        /// Summary variables at current time.
        ///
        /// Summary values from most recent call to Summary::eval().  Source
        /// object from which to retrieve the values that go into the output
        /// buffer.
        std::reference_wrapper<const SummaryState> summaryState;

        /// User-defined quantities at current time.
        std::reference_wrapper<const UDQState> udqState;

        /// Information pertaining to run's aquifers (analytic and numerical).
        ///
        /// Will be updated by RestartOutputManager::aquifers() if the optional is engaged.
        std::optional<Helpers::AggregateAquiferData>& aquiferData;
    };

    /// Constructor.
    ///
    /// \param[in] es Run's static parameters such as region definitions.
    ///
    /// \param[in] grid Run's active cells.
    ///
    /// \param[in] schedule Run's dynamic objects (wells, groups, &c).
    explicit RestartOutputManager(const EclipseState& es,
                                  const EclipseGrid&  grid,
                                  const Schedule&     schedule);

    /// Assign client callback for writing solution arrays.
    ///
    /// \param[in] backend Client callback for writing solution arrays.
    ///
    /// \return Reference to this object for method chaining.
    RestartOutputManager& solutionArrayBackend(const WriteSolutionArray& backend);

    /// Permit or prohibit extended solution arrays.
    ///
    /// \param[in] permit Whether or not to permit extended solution arrays.
    ///
    /// \return Reference to this object for method chaining.
    RestartOutputManager& permitExtendedArrays(bool permit);

    /// Internalise integer 'NORST' parameter for limiting restart output.
    ///
    /// \param[in] norst The value of the 'NORST' mnemonic in the RPTRST keyword.
    /// Value interpreted as follows:
    ///   - If zero or negative, no limit is applied and all restart and solution
    ///     arrays are emitted.
    ///
    ///   - If one, omit most restart arrays and emit only solution arrays.
    ///     This is for visualization only and does not support simulation
    ///     restart.
    ///
    ///   - If two (or larger), omit all restart arrays and emit only solution
    ///     arrays that are not related to wells.  This is for visualization of
    ///     reservoir properties and generates the smallest output files.
    ///
    /// \return Reference to this object for method chaining.
    RestartOutputManager& limitRestartOutput(const int norst);

    /// Write a restart step to the restart file.
    ///
    /// \param[in] reportStep One-based report step index for which to
    /// create output.
    ///
    /// \param[in] elapsedTime Elapsed physical (i.e., simulated) time
    /// in seconds since start of simulation.
    ///
    /// \param[in,out] state Current dynamic state values.
    ///
    /// \param[in,out] rstFile The restart file output stream.  Assumed
    /// to be positioned at the correct location for writing the restart step.
    void writeRestartStep(const int                         reportStep,
                          const double                      elapsedTime,
                          DynamicStateValues&               state,
                          EclIO::OutputStream::RestartBase& rstFile) const;

private:
    /// Predicate for whether or not a particular solution array represents
    /// a fluid-in-place quantity.
    ///
    /// Implementation intentionally opaque to avoid coupling with string
    /// matching logic.
    class IsFluidInPlace;

    enum class FileCategory
    {
        /// Emit a full set of solution and restart arrays.
        ///
        /// This is the default and supports simulation restart.
        FullRestart,

        /// Emit primarily solution arrays.
        ///
        /// This file type is for visualization only.
        GraphicsOnly,

        /// Emit only solution arrays that are not related to wells.
        ///
        /// This file type is for visualization of reservoir properties
        /// and generates the smallest output files.
        NoWell,
    };

    /// Context for writing a restart information for a single grid.
    struct WriteContext
    {
        /// One-based report step index for which to create output.
        int reportStep{};

        /// Elapsed physical (i.e., simulated) time in seconds since start of simulation.
        double elapsedTime{};

        /// Current dynamic state values.
        std::reference_wrapper<DynamicStateValues> state;

        /// The restart file output stream.  Assumed to be positioned at
        /// the correct location for writing restart information for the grid.
        std::reference_wrapper<EclIO::OutputStream::RestartBase> rstFile;

        /// Grid counter in runs with multiple grids (main + local).
        ///
        /// Written to restart file as part of the ENDLGR keyword information,
        /// but is otherwise a convenient lookup index for the current grid.
        ///
        /// Zero for the main grid.  Positive for local grids.
        std::size_t printIndex{};

        /// Grid index for the current grid.
        ///
        /// Zero for the main grid.  Positive for local grids.
        ///
        /// This is mainly a lookup index for local grid information.
        std::size_t gridIndex{};

        /// Name of the current grid.
        ///
        /// The name "GLOBAL" signifies the main grid.  LGR labels
        /// signify local grids.
        std::string_view gridName{};

        /// Intehead information for the current grid.
        ///
        /// Will be populated in the headers() member function.
        std::vector<int> inteHead{};
    };

    /// Run's static parameters such as region definitions.
    std::reference_wrapper<const EclipseState> es_;

    /// Run's active cells.
    std::reference_wrapper<const EclipseGrid> grid_;

    /// Run's dynamic objects (wells, groups, &c).
    std::reference_wrapper<const Schedule> schedule_;

    /// Whether or not to permit extended solution arrays (OPMEXTRA &c).
    bool permitExtendedArrays_{false};

    FileCategory fileCategory_{FileCategory::FullRestart};

    /// Numeric grid indices for all grids.
    ///
    /// Populated in the constructor according to local grid print order.
    ///
    /// First element (.front()) is zero which corresponds to the main grid.
    /// Subsequent elements are positive integers corresponding to local grids.
    std::vector<std::size_t> gridIndices_{};

    /// Names for all grids.
    ///
    /// Populated in the constructor according to local grid print order.
    ///
    /// First element (.front()) is "GLOBAL" which corresponds to the main grid.
    /// Subsequent elements are LGR labels corresponding to local grids.
    std::vector<std::string> gridNames_{};

    /// Optional client callback for writing solution arrays.
    ///
    /// If not engaged, RestartOutputManager will write solution arrays
    /// using its default implementation.
    std::optional<WriteSolutionArray> solutionArrayBackend_{};

    /// Context for writing a restart step for a single grid.
    ///
    /// Will be engaged during the execution of writeRestartStep() and will
    /// hold information about the current report step, elapsed time, dynamic
    /// state values, restart file output stream, and current grid.
    ///
    /// The context is necessary to avoid having to pass this information as
    /// arguments to the various helper functions that write different parts of
    /// the restart information for a grid.  By having a shared context, these
    /// helper functions can instead access this information as needed without
    /// requiring it to be passed through multiple layers of function calls.
    ///
    /// The context is mutable to allow engagement and modification within
    /// const member functions such as writeRestartStep() and its helpers.
    mutable std::optional<WriteContext> writeContext_{};

    /// Push a new write context.
    ///
    /// Engages the write context with the provided information.  Should be called at
    /// the beginning of writeRestartStep(), before any helper functions that write
    /// restart information for a grid are called.
    ///
    /// \param[in] reportStep One-based report step index for which to create output.
    ///
    /// \param[in] elapsedTime Elapsed physical (i.e., simulated) time in seconds
    /// since start of simulation.
    ///
    /// \param[in] state The dynamic state values.
    ///
    /// \param[in] rstFile The restart file output stream.
    void pushContext(const int                         reportStep,
                     const double                      elapsedTime,
                     DynamicStateValues&               state,
                     EclIO::OutputStream::RestartBase& rstFile) const;

    /// Pop the current write context.
    ///
    /// Disengages the write context.  Should be called at the end of
    /// writeRestartStep(), after restart information has been emitted
    /// for all grids.
    void popContext() const;

    /// Write an array to the restart file.
    ///
    /// Does not go through the client callback for writing solution arrays
    /// since this is intended for writing non-solution arrays.  For solution
    /// arrays, the client callback should be used if engaged, and the default
    /// implementation should be used otherwise.
    ///
    /// \tparam T Array element type.
    ///
    /// \param[in] arrayName The name of the array.
    ///
    /// \param[in] arrayValue The values of the array.
    template <typename T>
    void writeArray(std::string_view      arrayName,
                    const std::vector<T>& arrayValue) const;

    /// Write a message to the restart output.
    ///
    /// Emits a message string (keyword type 'MESS') to the restart file.  This
    /// is typically used for emitting messages that signify the beginning of
    /// different sections of the restart output (e.g., STARTSOL or ENDSOL).
    ///
    /// \param[in] msg The message to write.
    void message(std::string_view msg) const;

    /// Write a grid specific prologue.
    ///
    /// For the main grid, there is no prologue and this function is a no-op.
    /// For local grids, this function writes the LGR-specific headers that
    /// come before the main restart information for the grid.
    void prologue() const;

    /// Write a grid specific epilogue.
    ///
    /// For the main grid, there is no epilogue and this function is a no-op.
    /// For local grids, this function writes the LGR-specific information that
    /// comes after the main restart information for the grid.
    void epilogue() const;

    /// Write the header section of the restart output for the current grid.
    ///
    /// Emits the INTEHEAD, LOGIHEAD, and DOUBHEAD information for the current
    /// grid.  This should be called for all grids (main and local) since
    /// these headers are expected in the restart output
    void headers() const;

    /// Write the group section of the restart output for the current grid.
    ///
    /// Emits the IGRP, SGRP, XGRP, and ZGRP information for the current
    /// grid.  This should be called for all grids (main and local) since
    /// groups are expected in the restart output for all grids.
    void groups() const;

    /// Write the network section of the restart output for the current grid.
    ///
    /// No action unless the extended network model is active for the current
    /// report step.  Otherwise, emits network arrays like INODE, IBRAN, &c
    /// for the current grid.  This should be called for all grids (main and
    /// local) since network data is expected in the restart output for all grids.
    void networks() const;

    /// Write the well section of the restart output for the current grid.
    ///
    /// No action unless there are wells with valid data to write for the
    /// current grid.  Otherwise, emits segment and branch arrays for applicable
    /// multi-segment wells and regular well arrays like IWEL, SWEL, XWEL, and
    /// ZWEL.  Also includes well list (ZWLS, IWLS) and well connection arrays
    /// (ICON, SCON, XCON) as needed.  This should be called for all grids
    /// (main and local) since well data is expected in the restart output for
    /// all grids.
    void wells() const;

    /// Write the aquifer section of the restart output for the current grid.
    ///
    /// No action unless aquifers exist in the current run and this is the main
    /// grid since aquifer data is only relevant for the main grid.  Otherwise,
    /// emits analytic and numerical aquifer data as applicable for the current
    /// report step.
    ///
    /// Note: Member function aquifers() will mutate
    ///
    ///     writeContext_->state.get().aquiferData
    ///
    /// if it is engaged.
    void aquifers() const;

    /// Write the action section of the restart output for the current grid.
    ///
    /// Emits action-related information for the main grid if actions are
    /// active in the current run and the current report step.  No action
    /// for local grids since action information is only relevant for the
    /// main grid.
    void actions() const;

    /// Write the common solution arrays for the current grid.
    ///
    /// This function handles the "regular" solution arrays like "PRESSURE",
    /// "SGAS", "RS", "FLROILI+" &c.  It furthermore handles fluid-in-place
    /// arrays like "FIPOIL", "RFIPGAS", and "SFIPWAT" which require special
    /// handling in the restart output.  It finally handles tracer and
    /// temperature (TEMP) arrays which require special, per-array headers.
    ///
    /// \param[in] isFip Predicate for whether or not a particular solution
    /// array represents a fluid-in-place quantity.  Fluid-in-place arrays
    /// need special output logic.
    void solutionCommon(const IsFluidInPlace& isFip) const;

    /// Write solution arrays that require special handling beyond the
    /// "common" arrays.
    ///
    /// No action for local grids since the special arrays are only relevant
    /// for the main grid.
    ///
    /// This function will emit any applicable UDQ array as well as the
    /// threshold pressure array if the threshold pressure model is active.
    /// It will also emit extended solution arrays (e.g., properties related
    /// to CO2 storage) if the run requests extended solution arrays.
    void solutionSpecial() const;

    /// Write solution arrays for non-neighbouring connections (NNCs).
    ///
    /// This is the function for NNCs internal to a single grid--i.e.,
    /// across faults and pinch-outs.  Solution arrays for connections
    /// between the main grid and local grids are handled separately in
    /// solutionLocalGlobal().
    ///
    /// Emits FLOOILN+, FLOWATN+, FLOGASN+ and similar arrays if available.
    void solutionNNC() const;

    /// Write solution arrays for connections between the main grid and local grids.
    ///
    /// This is the function for NNCs between the main grid and local grids.
    /// Solution arrays for NNCs internal to a single grid are handled separately
    /// in solutionNNC().
    ///
    /// Emits FLOOILL+, FLOWATL+, FLOGASL+ and similar arrays if available
    /// for the connections between the main grid and local grids.
    void solutionLocalGlobal() const;

    /// Write solution arrays for connections between local grids.
    ///
    /// This is the function for NNCs between local grids. Solution arrays
    /// for NNCs internal to a single grid are handled separately in
    /// solutionNNC().
    ///
    /// Emits FLOOILA+, FLOWATA+, FLOGASA+ and similar arrays if available
    /// for the connections between local grids.
    void solutionLocalLocal() const;

    /// Emit an informational message to the run's log indicating that restart
    /// output has been completed for the current report step.
    ///
    /// The message format is as follows:
    ///
    ///   "Restart file written for report step {reportStep}/{total steps}, date = {date}."
    void logRestartOutputCompleted() const;

    // ----------------------------------------------------------------------
    // Second level helper functions for writing different parts of the restart output.
    // ----------------------------------------------------------------------

    /// Predicate for whether or not there are wells in the current grid.
    ///
    /// \return True if there are valid wells, false otherwise.
    bool hasValidWells() const;

    /// Write arrays for multi-segment wells.
    ///
    /// No action unless there are multi-segment wells with valid data to write
    /// for the current grid.  Otherwise, emits segment and branch arrays like
    /// ISEG, ILBS, ILBR, and RSEG for applicable multi-segment wells.
    void writeMultisegWellArrays() const;

    /// Write arrays for regular wells.
    ///
    /// No action unless there are wells with valid data to write for the current
    /// grid.  Otherwise, emits regular well arrays like IWEL, SWEL, XWEL, and ZWEL.
    ///
    /// For local grids, also emits the LGWEL array which maps local grid wells
    /// to their corresponding main grid wells.
    void writeWellArrays() const;

    /// Write well list arrays.
    ///
    /// No action unless there are wells with valid data to write for the current
    /// grid.  Otherwise, emits well list arrays like ZWLS and IWLS.
    ///
    /// Currently supported for the main grid only since well lists are (currently)
    /// not defined for local grids.
    void writeWellListArrays() const;

    /// Write well connection arrays.
    ///
    /// No action unless there are wells with valid data to write for the current
    /// grid.  Otherwise, emits well connection arrays like ICON, SCON, and XCON.
    ///
    /// Supported for both the main grid and local grids.
    void writeWellConnectionArrays() const;

    /// Write restart output for analytical aquifers.
    ///
    /// Applicable to the main grid only.  Omitted if there are no analytical
    /// aquifers in the run.
    void analyticalAquiferData() const;

    /// Write restart output for numerical aquifers.
    ///
    /// Applicable to the main grid only.  Omitted if there are no numerical
    /// aquifers in the run.
    void numericalAquiferData() const;

    /// Write the "common" solution arrays for the current grid.
    ///
    /// This function handles the "regular" solution arrays like "PRESSURE",
    /// "SGAS", "RS", "FLROILI+" &c.
    ///
    /// \param[in] isFip Predicate for whether or not a particular solution
    /// array represents a fluid-in-place quantity.  Fluid-in-place arrays
    /// are skipped in this function.  They require special handling in the
    /// restart output and are instead handled in fluidInPlaceArrays().
    void regularSolutionArrays(const IsFluidInPlace& isFip) const;

    /// Write the fluid-in-place solution arrays for the current grid.
    ///
    /// \param[in] isFip Predicate for whether or not a particular solution
    /// array represents a fluid-in-place quantity.  Fluid-in-place arrays
    /// require special handling in the restart output and are handled in
    /// this function.  They are skipped in regularSolutionArrays().
    void fluidInPlaceArrays(const IsFluidInPlace& isFip) const;

    /// Write tracer solution arrays for the current grid.
    ///
    /// Each tracer array requires a header that specifies the tracer name
    /// and the corresponding unit of measure (e.g., G/RM3).  This function
    /// is responsible for writing these headers as well as the per-cell
    /// tracer values themselves.
    ///
    /// This function will also write the temperature (TEMP) array if applicable
    /// which, while not strictly a tracer, has similar special handling
    /// requirements in the restart output.
    void tracerArrays() const;

    /// Write user-defined quantity (UDQ) solution arrays for the current grid.
    ///
    /// Applicable to the main grid only.  No action unless there are UDQ
    /// arrays with valid data to write for the main grid.
    void udqArrays() const;

    /// Write the threshold pressure array for the current grid.
    ///
    /// Applicable to the main grid only.  No action unless the threshold
    /// pressure model is active in the run and there is valid threshold
    /// pressure data to write for the main grid.
    void thresholdPressureArray() const;

    /// Write extended solution arrays for the current grid.
    ///
    /// This function emits extended solution arrays (e.g., properties related
    /// to CO2 storage) if the run requests extended solution arrays.
    void extendedSolutionArrays() const;

    /// Write singularly OPM-specific solution arrays for the current grid.
    ///
    /// This mostly applies to the OPMEXTRA array, although will also
    /// cover arrays configured with "addExtra()" and which have not been
    /// been emitted in the other solution array functions (e.g., NNC arrays).
    ///
    /// Note: If the client callback for writing solution arrays is *NOT*
    /// engaged, then this function will unconditionally emit the arrays as
    /// double precision floating point arrays ("DOUB") regardless of the
    /// original data type.  Otherwise, the client callback will be responsible
    /// for handling the data type of the arrays.
    void extraSolutionArrays() const;

    /// Emit names of LGRs connected to the current grid.
    void lgrNames() const;

    // ------------------------------------------------------------------------

    /// Default implementation of solution array output.
    ///
    /// Converts cell data to output units and emits floating point arrays
    /// as "REAL" (i.e., C++ "float") regardless of the original data type.
    /// Integer arrays are emitted as is.
    ///
    /// Used if client callback for writing solution arrays is not engaged.
    ///
    /// \param[in] arrayName Name of the solution array to write (e.g.,
    /// "PRESSURE", "SGAS", "FLROILI+").
    ///
    /// \param[in] value Cell data to write for the solution array.
    void defaultSolutionArrayBackend(const std::string&    arrayName,
                                     const data::CellData& value) const;

    template <typename CellDataRange>
    requires std::ranges::input_range<CellDataRange>
    void writeSolutionValues(CellDataRange&& cellDataRange) const
    {
        if (this->solutionArrayBackend_.has_value()) {
            std::ranges::for_each(cellDataRange, [this](const auto& array)
            {
                const auto& [name, data] = array;

                (*this->solutionArrayBackend_)
                    (name, data, this->writeContext_->rstFile);
            });
        }
        else {
            std::ranges::for_each(cellDataRange, [this](const auto& array)
            {
                const auto& [name, data] = array;

                this->defaultSolutionArrayBackend(name, data);
            });
        }
    }
};

} // namespace Opm::RestartIO

#endif // RESTART_OUTPUT_MANAGER_HPP
