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

#include <opm/input/eclipse/EclipseState/SummaryConfig/EnumeratedSimulationObjects.hpp>

#include <opm/common/utility/shmatch.hpp>

#include <opm/input/eclipse/EclipseState/Aquifer/AquiferConfig.hpp>
#include <opm/input/eclipse/EclipseState/Grid/FieldPropsManager.hpp>

#include <opm/input/eclipse/Schedule/Group/Group.hpp>
#include <opm/input/eclipse/Schedule/MSW/WellSegments.hpp>
#include <opm/input/eclipse/Schedule/Schedule.hpp>
#include <opm/input/eclipse/Schedule/ScheduleState.hpp>
#include <opm/input/eclipse/Schedule/Network/Branch.hpp>
#include <opm/input/eclipse/Schedule/Network/ExtNetwork.hpp>
#include <opm/input/eclipse/Schedule/Network/Node.hpp>
#include <opm/input/eclipse/Schedule/UDQ/UDQConfig.hpp>
#include <opm/input/eclipse/Schedule/UDQ/UDQInput.hpp>
#include <opm/input/eclipse/Schedule/Well/Connection.hpp>
#include <opm/input/eclipse/Schedule/Well/Well.hpp>
#include <opm/input/eclipse/Schedule/Well/WellConnections.hpp>

#include <algorithm>
#include <functional>
#include <set>
#include <stdexcept>
#include <utility>
#include <unordered_map>
#include <vector>

namespace Opm {

namespace {

void requireMutable(const bool committed)
{
    if (committed) {
        throw std::logic_error {
            "EnumeratedSimulationObjects is committed; add*() is no longer allowed"
        };
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Static sentinel containers
// ---------------------------------------------------------------------------

const std::vector<std::size_t> EnumeratedSimulationObjects::emptyConnections_{};
const std::vector<int>         EnumeratedSimulationObjects::emptyInts_{};

// ---------------------------------------------------------------------------
// Add methods
// ---------------------------------------------------------------------------

void EnumeratedSimulationObjects::addWell(std::string name)
{
    requireMutable(committed_);
    wells_.push_back(std::move(name));
}

void EnumeratedSimulationObjects::addWell(std::string name, std::string lgrTag)
{
    requireMutable(committed_);
    wellLgrTag_.insert_or_assign(name, std::move(lgrTag));
    wells_.push_back(std::move(name));
}

void EnumeratedSimulationObjects::addGroup(std::string name)
{
    requireMutable(committed_);
    groups_.push_back(std::move(name));
}

void EnumeratedSimulationObjects::addNetworkNode(std::string name)
{
    requireMutable(committed_);
    networkNodes_.push_back(std::move(name));
}

void EnumeratedSimulationObjects::addNetworkGroupWell(std::string wellName)
{
    requireMutable(committed_);
    networkGroupWells_.push_back(std::move(wellName));
}

void EnumeratedSimulationObjects::addWellConnection(const std::string& wellName,
                                                    std::size_t        globalCellIndex)
{
    requireMutable(committed_);
    wellConns_[wellName].push_back(globalCellIndex);
}

void EnumeratedSimulationObjects::addPossibleFutureConnection(const std::string& wellName,
                                                              int                globalCellIndex)
{
    requireMutable(committed_);
    possibleFutureConns_[wellName].insert(globalCellIndex);
}

void EnumeratedSimulationObjects::addWellSegment(const std::string& wellName,
                                                 int                segmentID)
{
    requireMutable(committed_);
    wellSegments_[wellName].push_back(segmentID);
}

void EnumeratedSimulationObjects::addWellCompletion(const std::string& wellName,
                                                    int                completionID)
{
    requireMutable(committed_);
    wellCompletions_[wellName].insert(completionID);
}

void EnumeratedSimulationObjects::addAnalyticAquifer(int id)
{
    requireMutable(committed_);
    analyticAquiferIDs_.push_back(id);
}

void EnumeratedSimulationObjects::addNumericAquifer(int id)
{
    requireMutable(committed_);
    numericAquiferIDs_.push_back(id);
}

void EnumeratedSimulationObjects::addRegionArray(std::string      regionName,
                                                 std::vector<int> regIDs)
{
    requireMutable(committed_);
    regionArrays_.insert_or_assign(std::move(regionName), std::move(regIDs));
}

void EnumeratedSimulationObjects::addUDQDefinition(std::string keyword,
                                                   bool        hasUnit)
{
    requireMutable(committed_);
    udqKeywords_.insert_or_assign(std::move(keyword), hasUnit);
}

void EnumeratedSimulationObjects::addGrid(std::string gridID, GridDims dims)
{
    requireMutable(committed_);
    gridDims_.insert_or_assign(std::move(gridID), dims);
}

// ---------------------------------------------------------------------------
// Commit
// ---------------------------------------------------------------------------

void EnumeratedSimulationObjects::commit()
{
    if (this->committed_) {
        return;
    }

    auto sortUnique = []<typename T, typename A>(std::vector<T, A>& v) {
        std::ranges::sort(v);
        v.erase(std::unique(v.begin(), v.end()), v.end());
    };

    sortUnique(this->wells_);
    sortUnique(this->groups_);
    sortUnique(this->networkNodes_);
    sortUnique(this->networkGroupWells_);
    sortUnique(this->analyticAquiferIDs_);
    sortUnique(this->numericAquiferIDs_);

    for (auto& wellConnPair : this->wellConns_) {
        sortUnique(wellConnPair.second);
    }

    for (auto& wellSegPair : this->wellSegments_) {
        sortUnique(wellSegPair.second);
    }

    // Build O(1) lookup sets
    this->wellSet_.insert(this->wells_.begin(), this->wells_.end());
    this->groupSet_.insert(this->groups_.begin(), this->groups_.end());

    for (const auto& [wname, conns] : this->wellConns_) {
        this->wellConnSet_[wname].insert(conns.begin(), conns.end());
    }

    this->committed_ = true;
}

// ---------------------------------------------------------------------------
// Query methods
// ---------------------------------------------------------------------------

const std::vector<std::string>& EnumeratedSimulationObjects::wellNames() const
{
    return wells_;
}

std::vector<std::string>
EnumeratedSimulationObjects::wellNames(const std::string& pattern) const
{
    std::vector<std::string> result;

    std::ranges::copy_if(wells_, std::back_inserter(result),
                         [&pattern](const std::string& name)
                         { return shmatch(pattern, name); });

    return result;
}

bool EnumeratedSimulationObjects::hasWell(const std::string& name) const
{
    return wellSet_.contains(name);
}

std::optional<std::string>
EnumeratedSimulationObjects::lgrTagForWell(const std::string& name) const
{
    const auto it = wellLgrTag_.find(name);
    if (it == wellLgrTag_.end()) {
        return std::nullopt;
    }
    return it->second;
}

const std::vector<std::string>& EnumeratedSimulationObjects::groupNames() const
{
    return groups_;
}

bool EnumeratedSimulationObjects::hasGroup(const std::string& name) const
{
    return groupSet_.contains(name);
}

const std::vector<std::string>& EnumeratedSimulationObjects::networkNodeNames() const
{
    return networkNodes_;
}

const std::vector<std::string>&
EnumeratedSimulationObjects::networkGroupWellNames() const
{
    return networkGroupWells_;
}

const std::vector<std::size_t>&
EnumeratedSimulationObjects::connectionsForWell(const std::string& name) const
{
    const auto it = wellConns_.find(name);
    return (it != wellConns_.end()) ? it->second : emptyConnections_;
}

bool EnumeratedSimulationObjects::wellHasGlobalConnection(const std::string& wellName,
                                                          std::size_t        globalIndex) const
{
    const auto it = wellConnSet_.find(wellName);
    return (it != wellConnSet_.end()) && it->second.contains(globalIndex);
}

const std::unordered_map<std::string, std::set<int>>&
EnumeratedSimulationObjects::possibleFutureConnections() const
{
    return possibleFutureConns_;
}

bool EnumeratedSimulationObjects::isMultiSegmentWell(const std::string& name) const
{
    const auto it = wellSegments_.find(name);
    return (it != wellSegments_.end()) && !it->second.empty();
}

int EnumeratedSimulationObjects::segmentCount(const std::string& name) const
{
    const auto it = wellSegments_.find(name);
    return (it != wellSegments_.end())
        ? static_cast<int>(it->second.size())
        : 0;
}

bool EnumeratedSimulationObjects::wellHasCompletion(const std::string& wellName,
                                                    int                completionID) const
{
    const auto it = wellCompletions_.find(wellName);
    return (it != wellCompletions_.end()) && it->second.contains(completionID);
}

const std::vector<int>& EnumeratedSimulationObjects::analyticAquiferIDs() const
{
    return analyticAquiferIDs_;
}

const std::vector<int>& EnumeratedSimulationObjects::numericAquiferIDs() const
{
    return numericAquiferIDs_;
}

bool EnumeratedSimulationObjects::hasRegionArray(const std::string& regionName) const
{
    return regionArrays_.contains(regionName);
}

const std::vector<int>&
EnumeratedSimulationObjects::regionArray(const std::string& regionName) const
{
    const auto it = regionArrays_.find(regionName);
    return (it != regionArrays_.end()) ? it->second : emptyInts_;
}

bool EnumeratedSimulationObjects::hasUDQKeyword(const std::string& keyword) const
{
    return udqKeywords_.contains(keyword);
}

bool EnumeratedSimulationObjects::udqHasUnit(const std::string& keyword) const
{
    const auto it = udqKeywords_.find(keyword);
    return (it != udqKeywords_.end()) && it->second;
}

GridDims EnumeratedSimulationObjects::gridDims(const std::string& gridID) const
{
    const auto it = gridDims_.find(gridID);
    if (it != gridDims_.end()) {
        return it->second;
    }

    if (gridDimsResolver_) {
        return gridDimsResolver_(gridID);
    }

    return GridDims{};
}

void EnumeratedSimulationObjects::setGridDimsResolver(std::function<GridDims(const std::string&)> resolver)
{
    gridDimsResolver_ = std::move(resolver);
}

// ---------------------------------------------------------------------------
// Factory function
// ---------------------------------------------------------------------------

EnumeratedSimulationObjects
makeEnumeratedSimObjs(const Schedule&          schedule,
                      const FieldPropsManager& field_props,
                      const AquiferConfig&     aquiferConfig,
                      std::function<GridDims(const std::string&)> gridDims)
{
    EnumeratedSimulationObjects eso;

    eso.setGridDimsResolver(gridDims);

    // Global grid dimensions
    eso.addGrid("", gridDims(""));

    // Wells, connections, segment data, and completion IDs (at end of schedule)
    for (const auto& well : schedule.getWellsatEnd()) {
        const auto lgr_tag = well.get_lgr_well_tag();
        if (lgr_tag.has_value()) {
            eso.addWell(well.name(), *lgr_tag);

            const auto lgrDims = gridDims(*lgr_tag);
            if (lgrDims.getNX() > 0) {
                eso.addGrid(*lgr_tag, lgrDims);
            }
        }
        else {
            eso.addWell(well.name());
        }

        for (const auto& conn : well.getConnections()) {
            eso.addWellConnection(well.name(), conn.global_index());
            eso.addWellCompletion(well.name(), conn.complnum());
        }

        if (well.isMultiSegment()) {
            const auto nSeg = static_cast<int>(well.getSegments().size());
            for (auto seg = 1; seg <= nSeg; ++seg) {
                eso.addWellSegment(well.name(), seg);
            }
        }
    }

    // Possible future connections (same map type as Schedule exposes)
    for (const auto& [wellName, connSet] : schedule.getPossibleFutureConnections()) {
        for (const int ci : connSet) {
            eso.addPossibleFutureConnection(wellName, ci);
        }
    }

    // Groups (all groups including FIELD)
    for (const auto& groupName : schedule.groupNames()) {
        eso.addGroup(groupName);
    }

    // Network nodes: iterate all report steps to capture nodes from any step
    {
        const auto nstep = schedule.size() - 1;
        for (auto step = 0 * nstep; step < nstep; ++step) {
            for (const auto& nodeName : schedule[step].network.get().node_names()) {
                eso.addNetworkNode(nodeName);
                if (!schedule.hasGroup(nodeName, step)) {
                    continue;
                }
                for (const auto& wellName : schedule.getGroup(nodeName, step).wells()) {
                    eso.addNetworkGroupWell(wellName);
                }
            }
        }
    }

    // Aquifer IDs
    for (const int id : analyticAquiferIDs(aquiferConfig)) {
        eso.addAnalyticAquifer(id);
    }
    for (const int id : numericAquiferIDs(aquiferConfig)) {
        eso.addNumericAquifer(id);
    }

    // Region arrays: all known FIP regions
    auto fipnum = field_props.get_global_int("FIPNUM");
    if (fipnum.empty()) {
        fipnum.assign(field_props.active_size(), 1);
    }
    eso.addRegionArray("FIPNUM", std::move(fipnum));

    for (const auto& regionName : field_props.fip_regions()) {
        eso.addRegionArray(regionName, field_props.get_global_int(regionName));
    }

    // UDQ definitions at end of schedule
    if (schedule.size() > 0) {
        const auto& udq = schedule.getUDQConfig(schedule.size() - 1);
        for (const auto& inp : udq.input()) {
            eso.addUDQDefinition(inp.keyword(), udq.has_unit(inp.keyword()));
        }
    }

    eso.commit();
    return eso;
}

} // namespace Opm
