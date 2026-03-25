/*
  Copyright (c) 2018 Statoil ASA

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

#ifndef OPM_WRITE_RESTART_HELPERS_HPP
#define OPM_WRITE_RESTART_HELPERS_HPP

#include <opm/output/eclipse/AggregateAquiferData.hpp>
#include <opm/output/eclipse/RestartValue.hpp>

#include <cstddef>
#include <optional>
#include <vector>

// Forward declarations

namespace Opm {

    class Runspec;
    class EclipseGrid;
    class EclipseState;
    class Schedule;
    class ScheduleState;
    class SummaryState;
    class UDQState;
    class Well;
    class WellTestState;
    class UnitSystem;
    class UDQActive;
    class Actdims;

} // Opm

namespace Opm::Action { class State; }

namespace Opm::EclIO::OutputStream { class RestartBase; }

namespace Opm::RestartIO::Helpers {

    /// Validate solution vector sizes against the active grid.
    /// Throws std::logic_error or std::runtime_error on mismatch.
    void checkSaveArguments(const EclipseState& es,
                            const RestartValue& value,
                            const EclipseGrid&  grid);

    std::vector<double>
    createDoubHead(const EclipseState& es,
                   const Schedule&     sched,
                   const std::size_t   sim_step,
                   const std::size_t   report_step,
                   const double        simTime,
                   const double        nextTimeStep);

    std::vector<int>
    createInteHead(const EclipseState& es,
                   const EclipseGrid&  grid,
                   const Schedule&     sched,
                   const double        simTime,
                   const int           num_solver_steps,
                   const int           report_step,
                   const int           lookup_step);

    std::vector<int>
    createLgrHeadi(const EclipseState& es,
                   const int           lgr_index);

    std::vector<bool>
    createLgrHeadq(const EclipseState& es);

    std::vector<double>
    createLgrHeadd();

    /// Static flags, mostly for INIT file purposes.
    ///
    /// \param[in] es Static properties and run settings.
    ///
    /// \return Output file's logical flag settings.
    std::vector<bool>
    createLogiHead(const EclipseState& es);

    /// Dynamic flags, mostly for restart file purposes.
    ///
    /// \param[in] es Static properties and run settings.
    ///
    /// \param[in] sched Dynamic simulation objects and settings.
    ///
    /// \return Output file's logical flag settings.
    std::vector<bool>
    createLogiHead(const EclipseState& es, const ScheduleState& sched);

    std::size_t
    entriesPerSACT();

    std::size_t
    entriesPerIACT();

    std::size_t
    entriesPerZACT();

    std::size_t
    entriesPerZACN(const Opm::Actdims& actdims);

    std::size_t
    entriesPerIACN(const Opm::Actdims& actdims);

    std::size_t
    entriesPerSACN(const Opm::Actdims& actdims);

    std::vector<int>
    createActionRSTDims(const Schedule&     sched,
                        const std::size_t   simStep);

    /// Shared single-grid save pipeline used by both RestartIO and StoreIO.
    ///
    /// Caller is responsible for prior validation (checkSaveArguments) and
    /// for unit conversion (value.convertFromSI).
    ///
    /// \param[in,out] stream    Writable restart-like output stream.
    ///
    /// \param[in] report_step   One-based report step number.
    ///
    /// \param[in] sim_step      Zero-based simulation step (= max(report_step-1,0)).
    ///
    /// \param[in] seconds_elapsed Elapsed simulation time in seconds.
    ///
    /// \param[in] value         Restart value (already converted to user units).
    ///
    /// \param[in] es            Static eclipse state (grid, unit system, ...).
    ///
    /// \param[in] grid          Active grid.
    ///
    /// \param[in] schedule      Dynamic simulation schedule.
    ///
    /// \param[in] action_state  Current action state.
    ///
    /// \param[in] wtest_state   Current well-test state.
    ///
    /// \param[in] sumState      Summary-state accumulator.
    ///
    /// \param[in] udqState      UDQ state.
    ///
    /// \param[in,out] aquiferData  Aggregate aquifer data helper (may be empty).
    ///
    /// \param[in] ecl_compatible_rst Whether to apply ECL output restrictions.
    ///
    /// \param[in] write_double  Whether to write solution data as double precision.
    ///
    /// \return INTEHEAD vector written to stream.
    std::vector<int>
    saveSingleGrid(EclIO::OutputStream::RestartBase&             stream,
                   int                                           report_step,
                   int                                           sim_step,
                   double                                        seconds_elapsed,
                   const RestartValue&                           value,
                   const EclipseState&                           es,
                   const EclipseGrid&                            grid,
                   const Schedule&                               schedule,
                   const Action::State&                          action_state,
                   const WellTestState&                          wtest_state,
                   const SummaryState&                           sumState,
                   const UDQState&                               udqState,
                   std::optional<AggregateAquiferData>&          aquiferData,
                   bool                                          ecl_compatible_rst,
                   bool                                          write_double);

} // Opm::RestartIO::Helpers

#endif  // OPM_WRITE_RESTART_HELPERS_HPP
