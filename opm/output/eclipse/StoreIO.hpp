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
#ifndef STORE_IO_HPP
#define STORE_IO_HPP

#include <opm/output/eclipse/AggregateAquiferData.hpp>
#include <opm/output/eclipse/RestartValue.hpp>

#include <optional>
#include <string>
#include <vector>

namespace Opm {

    class EclipseGrid;
    class EclipseState;
    class Schedule;
    class UDQState;
    class SummaryState;
    class WellTestState;

} // namespace Opm

namespace Opm { namespace EclIO { namespace OutputStream {

    class Store;

}}}

namespace Opm { namespace Action {

    class State;

}}

/*
  The two free functions StoreIO::save() and StoreIO::load() provide
  save and load access to OPM unified store files (.STORE / .FSTORE).

  The store format mirrors the restart format: it is unified (always a
  single file), uses SEQNUM-based report-step framing, and stores the
  same class of solution/well/group/aquifer data as a restart file.

  StoreIO::save() accepts an already-constructed EclIO::OutputStream::Store
  object that handles file creation, seeking, and SEQNUM emission.  The
  caller is responsible for instantiating the Store with the correct result-
  set descriptor, sequence number, and format flag.

  StoreIO::load() takes a filename (including extension) and a report step,
  matching the interface of RestartIO::load().  The file must be a unified
  STORE or FSTORE file produced by StoreIO::save().
*/
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
              bool                                                    write_double = false);

    RestartValue load(const std::string&             filename,
                      int                            report_step,
                      Action::State&                 action_state,
                      SummaryState&                  summary_state,
                      const std::vector<RestartKey>& solution_keys,
                      const EclipseState&            es,
                      const EclipseGrid&             grid,
                      const Schedule&                schedule,
                      const std::vector<RestartKey>& extra_keys = {});

} // namespace Opm::StoreIO

#endif  // STORE_IO_HPP
