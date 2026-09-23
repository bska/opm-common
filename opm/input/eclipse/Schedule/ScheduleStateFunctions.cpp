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

#include <opm/input/eclipse/Schedule/ScheduleStateFunctions.hpp>

#include <opm/input/eclipse/Schedule/Events.hpp>
#include <opm/input/eclipse/Schedule/GasLiftOpt.hpp>
#include <opm/input/eclipse/Schedule/Group/Group.hpp>
#include <opm/input/eclipse/Schedule/Network/Branch.hpp>
#include <opm/input/eclipse/Schedule/Network/ExtNetwork.hpp>
#include <opm/input/eclipse/Schedule/Network/Node.hpp>
#include <opm/input/eclipse/Schedule/UDQ/UDQConfig.hpp>
#include <opm/input/eclipse/Schedule/Well/WListManager.hpp>
#include <opm/input/eclipse/Schedule/Well/WellMatcher.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Opm {

// ---- Group A: trivial direct-field access ----

bool hasWell(const ScheduleState& state, const std::string& wellName)
{
    return state.wells.has(wellName);
}

bool hasGroup(const ScheduleState& state, const std::string& groupName)
{
    return state.groups.has(groupName);
}

bool isWList(const ScheduleState& state, const std::string& pattern)
{
    return state.wlist_manager().hasList(pattern);
}

const std::vector<std::string>& wellNames(const ScheduleState& state)
{
    return state.well_order().names();
}

const std::vector<std::string>& groupNames(const ScheduleState& state)
{
    return state.group_order().names();
}

const Well& getWell(const ScheduleState& state, const std::string& wellName)
{
    return state.wells.get(wellName);
}

const Group& getGroup(const ScheduleState& state, const std::string& groupName)
{
    return state.groups.get(groupName);
}

WellProducerCMode getGlobalWhistctlMmode(const ScheduleState& state)
{
    return state.whistctl();
}

const UDQConfig& getUDQConfig(const ScheduleState& state)
{
    return state.udq.get();
}

const GasLiftOpt& glo(const ScheduleState& state)
{
    return state.glo();
}

// ---- Group B: small computations ----

std::size_t numWells(const ScheduleState& state)
{
    return wellNames(state).size();
}

std::vector<std::string> groupNames(const ScheduleState& state,
                                    const std::string& pattern)
{
    return state.group_order().names(pattern);
}

const Well& getWell(const ScheduleState& state, const std::size_t well_index)
{
    const auto find_pred = [well_index](const auto& well_pair) -> bool {
        return well_pair.second->seqIndex() == well_index;
    };

    const auto* well_ptr = state.wells.find(find_pred);
    if (well_ptr == nullptr) {
        throw std::invalid_argument(
            fmt::format("There is no well with well_index:{}", well_index));
    }

    return *well_ptr;
}

std::vector<Well> getWells(const ScheduleState& state)
{
    auto wells = std::vector<Well>{};

    const auto& well_order = state.well_order();
    std::ranges::transform(well_order, std::back_inserter(wells),
                           [&state_wells = state.wells]
                           (const auto& wname) -> decltype(auto)
                           { return state_wells.get(wname); });

    return wells;
}

std::vector<const Group*> restart_groups(const ScheduleState& state)
{
    const auto rg = state.group_order().restart_groups();

    std::vector<const Group*> rst_groups(rg.size(), nullptr);
    for (std::size_t i = 0; i < rg.size(); ++i) {
        if (rg[i].has_value()) {
            rst_groups[i] = &getGroup(state, rg[i].value());
        }
    }

    return rst_groups;
}

// ---- Group C: delegate to other free functions ----

WellMatcher wellMatcher(const ScheduleState& state)
{
    return { &state.well_order(), state.wlist_manager() };
}

std::vector<std::string>
wellNames(const ScheduleState&            state,
          const std::string&              pattern,
          const std::vector<std::string>& matching_wells)
{
    const auto wm = wellMatcher(state);

    return (pattern == "?")
        ? wm.sort(matching_wells)  // ACTIONX handler
        : wm.wells(pattern);       // Normal well name pattern matching
}

void addWell(Well&& well, ScheduleState& state)
{
    const std::string wname = well.name();

    state.events().addEvent(ScheduleEvents::NEW_WELL);
    state.wellgroup_events().addWell(wname);

    {
        auto wo = state.well_order();
        wo.add(wname);

        state.well_order.update(std::move(wo));
    }

    well.setInsertIndex(state.wells.size());

    state.wells.update(std::move(well));
}

void addGroup(Group&& group, ScheduleState& state)
{
    const auto group_name = group.name();

    state.groups.update(std::move(group));

    state.events().addEvent(ScheduleEvents::NEW_GROUP);
    state.wellgroup_events().addGroup(group_name);

    {
        auto go = state.group_order();
        go.add(group_name);

        state.group_order.update(std::move(go));
    }

    if (group_name != "FIELD") {
        // All newly created groups are attached to the field group,
        // can then be relocated with the GRUPTREE keyword.

        addGroupToGroup({.parent = "FIELD", .child = group_name}, state);
    }
}

void addGroupToGroup(const GroupTreeBranch& branch, ScheduleState& state)
{
    if (auto parent = state.groups(branch.parent);
        parent.addGroup(branch.child))
    {
        state.groups.update(std::move(parent));
    }

    // Check and update backreference in child
    if (const auto& child = state.groups(branch.child);
        child.parent() != branch.parent)
    {
        {
            auto old_parent = state.groups(child.parent());
            old_parent.delGroup(child.name());

            state.groups.update(std::move(old_parent));
        }

        auto new_child_group = Group {child};
        new_child_group.updateParent(branch.parent);

        state.groups.update(std::move(new_child_group));
    }

    // Update standard network if required
    if (auto network = state.network(); !network.is_standard_network()) {
        return;
    }
    else if (network.has_node(branch.child)) {
        if (auto old_branch = network.uptree_branch(branch.child);
            old_branch.has_value())
        {
            auto new_branch = old_branch.value();
            new_branch.set_uptree_node(branch.parent);

            network.add_or_replace_branch(new_branch);

            state.network.update(std::move(network));
        }

        // If no previous uptree branch the child is a fixed-pressure node,
        // so no need to update network
    }
}

void addWellToGroup(const GroupTreeBranch& branch, ScheduleState& state)
{
    if (auto well = getWell(state, branch.child); well.groupName() != branch.parent) {
        // Remove well child reference from original group
        {
            auto orig_group = state.groups(well.groupName());
            orig_group.delWell(branch.child);

            state.groups.update(std::move(orig_group));
        }

        well.updateGroup(branch.parent);

        state.wells.update(std::move(well));
        state.wellgroup_events()
            .addEvent(branch.child, ScheduleEvents::WELL_WELSPECS_UPDATE);
    }

    // Add well child reference to new group
    auto group = state.groups(branch.parent);
    group.addWell(branch.child);

    state.groups.update(std::move(group));
    state.events().addEvent(ScheduleEvents::GROUP_CHANGE);
}

} // namespace Opm
