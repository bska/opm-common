/*
  Copyright (c) 2025 Equinor ASA

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

#include <opm/output/eclipse/StoreIO.hpp>

#include <opm/output/eclipse/RestartIO.hpp>
#include <opm/output/eclipse/WriteRestartHelpers.hpp>

#include <opm/io/eclipse/OutputStream.hpp>

#include <opm/input/eclipse/EclipseState/EclipseState.hpp>
#include <opm/input/eclipse/EclipseState/Grid/EclipseGrid.hpp>
#include <opm/input/eclipse/Schedule/Action/State.hpp>
#include <opm/input/eclipse/Schedule/Schedule.hpp>
#include <opm/input/eclipse/Schedule/SummaryState.hpp>
#include <opm/input/eclipse/Schedule/Well/WellTestState.hpp>

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>

namespace Opm::StoreIO {

void save(EclIO::OutputStream::Store&                             storeFile,
          int                                                     report_step,
          double                                                  seconds_elapsed,
          RestartValue                                            value,
          const EclipseState&                                     es,
          const EclipseGrid&                                      grid,
          const Schedule&                                         schedule,
          const Action::State&                                    action_state,
          const WellTestState&                                    wtest_state,
          const SummaryState&                                     sumState,
          const UDQState&                                         udqState,
          std::optional<RestartIO::Helpers::AggregateAquiferData>& aquiferData,
          bool                                                    write_double)
{
    // Validate solution vector sizes against the active grid.
    RestartIO::Helpers::checkSaveArguments(es, value, grid);

    // Store files always write all data; ECL compatibility mode restricts
    // to single precision, which is undesirable for store output.  We
    // still honour the caller's write_double preference, but skip the
    // IOConfig ECL-compatibility override so store files may carry double
    // precision data when requested.
    const bool ecl_compatible_rst = false;

    const auto  sim_step = std::max(report_step - 1, 0);
    const auto& units    = es.getUnits();

    // Convert solution fields and extra values from SI to user units.
    value.convertFromSI(units);

    RestartIO::Helpers::saveSingleGrid(storeFile, report_step, sim_step,
                                       seconds_elapsed, value, es, grid,
                                       schedule, action_state, wtest_state,
                                       sumState, udqState, aquiferData,
                                       ecl_compatible_rst, write_double);
}

RestartValue load(const std::string&             filename,
                  int                            report_step,
                  Action::State&                 action_state,
                  SummaryState&                  summary_state,
                  const std::vector<RestartKey>& solution_keys,
                  const EclipseState&            es,
                  const EclipseGrid&             grid,
                  const Schedule&                schedule,
                  const std::vector<RestartKey>& extra_keys)
{
    // The STORE file uses the same SEQNUM-framed binary layout as the
    // unified restart file (UNRST/FUNRST).  The existing RestartIO::load
    // path (ERst + RestartFileView) is therefore directly reusable.
    // Explicitly verify that the file exists to provide a Store-specific
    // error message before delegating to the generic loader.
    {
        std::ifstream probe(filename);
        if (! probe) {
            throw std::runtime_error {
                "StoreIO::load: unable to open store file '" + filename + "'"
            };
        }
    }

    return RestartIO::load(filename, report_step, action_state, summary_state,
                           solution_keys, es, grid, schedule, extra_keys);
}

} // namespace Opm::StoreIO
