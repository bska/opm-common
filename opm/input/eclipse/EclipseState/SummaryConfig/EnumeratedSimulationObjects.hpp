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

#ifndef OPM_ENUMERATED_SIMULATION_OBJECTS_HPP
#define OPM_ENUMERATED_SIMULATION_OBJECTS_HPP

#include <opm/input/eclipse/EclipseState/Grid/GridDims.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Opm {
    class AquiferConfig;
    class FieldPropsManager;
    class Schedule;
} // namespace Opm

namespace Opm {

/// Lightweight, piecemeal-constructible snapshot of the simulation objects
/// that SummaryConfig needs during keyword parsing.
///
/// Call add*() methods to populate the object, then call commit() to sort and
/// deduplicate internal vectors and build O(1) lookup structures.  All query
/// methods are valid only after commit().
class EnumeratedSimulationObjects
{
public:
    // -----------------------------------------------------------------------
    // Add methods (pre-commit)
    // -----------------------------------------------------------------------

    /// Register a well that lives on the global grid.
    void addWell(std::string name);

    /// Register a well that lives inside a local grid refinement (LGR).
    void addWell(std::string name, std::string lgrTag);

    /// Register a group name.
    void addGroup(std::string name);

    /// Register a production network node name.
    void addNetworkNode(std::string name);

    /// Register a well that belongs to a group that is also a network node.
    ///
    /// The name is added to the list used by node keywords that include
    /// group wells; it does not add a new well to the well list.
    void addNetworkGroupWell(std::string wellName);

    /// Register a connection (global grid cell) for a well.
    void addWellConnection(const std::string& wellName, std::size_t globalCellIndex);

    /// Register a possible future connection for a well.
    void addPossibleFutureConnection(const std::string& wellName, int globalCellIndex);

    /// Register one segment belonging to a multi-segment well.
    void addWellSegment(const std::string& wellName, int segmentID);

    /// Register a COMPLUMP-based completion ID for a well.
    void addWellCompletion(const std::string& wellName, int completionID);

    /// Register an analytic aquifer ID.
    void addAnalyticAquifer(int id);

    /// Register a numeric aquifer ID.
    void addNumericAquifer(int id);

    /// Register a per-active-cell integer region array.
    void addRegionArray(std::string regionName, std::vector<int> regIDs);

    /// Register a UDQ keyword definition.
    void addUDQDefinition(std::string keyword, bool hasUnit);

    /// Register the dimensions of a grid.
    ///
    /// Pass an empty string as \p gridID for the global grid; pass the LGR
    /// name for local grids.
    void addGrid(std::string gridID, GridDims dims);

    // -----------------------------------------------------------------------
    // Commit
    // -----------------------------------------------------------------------

    /// Finalise the object.
    ///
    /// Sorts and deduplicates all internally stored vectors, then builds
    /// hash-based lookup structures for O(1) membership queries.  Calling
    /// commit() more than once is a no-op after the first call.
    void commit();

    // -----------------------------------------------------------------------
    // Query methods (valid after commit())
    // -----------------------------------------------------------------------

    /// All well names, sorted lexicographically.
    const std::vector<std::string>& wellNames() const;

    /// Well names matching the shell-glob \p pattern.
    std::vector<std::string> wellNames(const std::string& pattern) const;

    /// Whether a well with this name was registered.
    bool hasWell(const std::string& name) const;

    /// LGR tag of the named well, or nullopt if the well lives on the global grid.
    std::optional<std::string> lgrTagForWell(const std::string& name) const;

    /// All group names, sorted lexicographically.
    const std::vector<std::string>& groupNames() const;

    /// Whether a group with this name was registered.
    bool hasGroup(const std::string& name) const;

    /// All production network node names, sorted lexicographically.
    const std::vector<std::string>& networkNodeNames() const;

    /// Well names that belong to groups that are network nodes, sorted
    /// lexicographically.
    const std::vector<std::string>& networkGroupWellNames() const;

    /// Sorted global cell indices of all connections registered for \p name.
    ///
    /// Returns an empty vector for unknown well names.
    const std::vector<std::size_t>& connectionsForWell(const std::string& name) const;

    /// Whether the named well has a connection at the given global cell index.
    bool wellHasGlobalConnection(const std::string& wellName,
                                 std::size_t        globalIndex) const;

    /// Map of well names to their sets of possible future connection cell indices.
    const std::unordered_map<std::string, std::set<int>>& possibleFutureConnections() const;

    /// Whether \p name is a multi-segment well.
    bool isMultiSegmentWell(const std::string& name) const;

    /// Number of segments in well \p name (0 for non-MSW wells).
    int segmentCount(const std::string& name) const;

    /// Whether well \p wellName has a COMPLUMP completion with ID \p completionID.
    bool wellHasCompletion(const std::string& wellName, int completionID) const;

    /// Sorted list of analytic aquifer IDs.
    const std::vector<int>& analyticAquiferIDs() const;

    /// Sorted list of numeric aquifer IDs.
    const std::vector<int>& numericAquiferIDs() const;

    /// Whether a region array named \p regionName was registered.
    bool hasRegionArray(const std::string& regionName) const;

    /// Per-active-cell integer values for region array \p regionName.
    ///
    /// Returns an empty vector for unknown names.
    const std::vector<int>& regionArray(const std::string& regionName) const;

    /// Whether a UDQ keyword definition for \p keyword was registered.
    bool hasUDQKeyword(const std::string& keyword) const;

    /// Whether the UDQ keyword \p keyword has a unit definition.
    bool udqHasUnit(const std::string& keyword) const;

    /// Grid dimensions for the grid identified by \p gridID.
    ///
    /// Pass an empty string for the global grid; pass the LGR name for local
    /// grids.  Returns a default-constructed GridDims (NX=NY=NZ=0) for
    /// unregistered grid IDs.
    GridDims gridDims(const std::string& gridID) const;

    /// Register a fallback callback for resolving grid dimensions.
    ///
    /// Used when a grid ID was not pre-registered through addGrid().
    void setGridDimsResolver(std::function<GridDims(const std::string&)> resolver);

private:
    bool committed_ { false };

    // Well data
    std::vector<std::string> wells_{};
    std::unordered_map<std::string, std::string> wellLgrTag_{};

    // Group data
    std::vector<std::string> groups_{};

    // Network node data (pre-commit: may contain duplicates; post-commit: sorted+unique)
    std::vector<std::string> networkNodes_{};
    std::vector<std::string> networkGroupWells_{};

    // Connection data: well -> sorted unique global cell indices
    std::unordered_map<std::string, std::vector<std::size_t>> wellConns_{};

    // Post-commit: set form for O(1) membership test
    std::unordered_map<std::string, std::unordered_set<std::size_t>> wellConnSet_{};

    // Post-commit: fast name-existence sets
    std::unordered_set<std::string> wellSet_{};
    std::unordered_set<std::string> groupSet_{};

    // Possible future connections (same type as Schedule::getPossibleFutureConnections)
    std::unordered_map<std::string, std::set<int>> possibleFutureConns_{};

    // Multi-segment well data: well -> sorted unique segment IDs
    std::unordered_map<std::string, std::vector<int>> wellSegments_{};

    // COMPLUMP completion IDs per well
    std::unordered_map<std::string, std::unordered_set<int>> wellCompletions_{};

    // Aquifer IDs
    std::vector<int> analyticAquiferIDs_{};
    std::vector<int> numericAquiferIDs_{};

    // Region arrays: name -> per-active-cell region IDs
    std::unordered_map<std::string, std::vector<int>> regionArrays_{};

    // UDQ keywords: keyword -> has_unit flag
    std::unordered_map<std::string, bool> udqKeywords_{};

    // Grid dimensions: gridID -> GridDims ("" = global grid)
    std::unordered_map<std::string, GridDims> gridDims_{};

    // Optional fallback for resolving unknown grid dimensions.
    std::function<GridDims(const std::string&)> gridDimsResolver_{};

    // Sentinel empty containers returned by query methods for unknown names
    static const std::vector<std::size_t> emptyConnections_;
    static const std::vector<int>         emptyInts_;
};

/// Build a fully committed EnumeratedSimulationObjects by iterating the
/// supplied simulation objects.
///
/// The returned object contains all wells, groups, network nodes, well
/// connections, segment counts, completion IDs, aquifer IDs, FIP region
/// arrays, UDQ definitions, and grid dimensions resolved through \p gridDims.
EnumeratedSimulationObjects
makeEnumeratedSimObjs(const Schedule&          schedule,
                      const FieldPropsManager& field_props,
                      const AquiferConfig&     aquiferConfig,
                      std::function<GridDims(const std::string&)> gridDims);

} // namespace Opm

#endif // OPM_ENUMERATED_SIMULATION_OBJECTS_HPP
