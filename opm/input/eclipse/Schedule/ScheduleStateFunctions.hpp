/*
  Copyright 2026 Equinor ASA.

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

#ifndef SCHEDULE_STATE_FUNCTIONS_HPP
#define SCHEDULE_STATE_FUNCTIONS_HPP

#include <opm/input/eclipse/Schedule/ScheduleState.hpp>

#include <opm/input/eclipse/Schedule/Well/Well.hpp>
#include <opm/input/eclipse/Schedule/Well/WellMatcher.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace Opm {

class GasLiftOpt;
class UDQConfig;

struct GroupTreeBranch
{
    std::string parent{};
    std::string child{};
};

} // namespace Opm

namespace Opm {

// ---- Group A: trivial direct-field access ----

bool hasWell(const ScheduleState& state, const std::string& wellName);
bool hasGroup(const ScheduleState& state, const std::string& groupName);
bool isWList(const ScheduleState& state, const std::string& pattern);

const std::vector<std::string>& wellNames(const ScheduleState& state);
const std::vector<std::string>& groupNames(const ScheduleState& state);

const Well& getWell(const ScheduleState& state, const std::string& wellName);
const Group& getGroup(const ScheduleState& state, const std::string& groupName);

WellProducerCMode getGlobalWhistctlMmode(const ScheduleState& state);
const UDQConfig& getUDQConfig(const ScheduleState& state);
const GasLiftOpt& glo(const ScheduleState& state);

// ---- Group B: small computations ----

std::size_t numWells(const ScheduleState& state);

std::vector<std::string> groupNames(const ScheduleState& state,
                                    const std::string& pattern);

const Well& getWell(const ScheduleState& state, std::size_t well_index);

std::vector<Well> getWells(const ScheduleState& state);

std::vector<const Group*> restart_groups(const ScheduleState& state);

// ---- Group C: delegate to other free functions ----

WellMatcher wellMatcher(const ScheduleState& state);

std::vector<std::string> wellNames(const ScheduleState& state,
                                   const std::string& pattern,
                                   const std::vector<std::string>& matching_wells = {});

// --- Mutating operations

void addWell(Well&& well, ScheduleState& state);
void addGroup(Group&& group, ScheduleState& state);
void addGroupToGroup(const GroupTreeBranch& branch, ScheduleState& state);
void addWellToGroup(const GroupTreeBranch& branch, ScheduleState& state);

} // namespace Opm

#endif // SCHEDULE_STATE_FUNCTIONS_HPP
