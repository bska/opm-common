/*
  Copyright 2016 Statoil ASA.

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

#ifndef SUMMARY_STATE_H
#define SUMMARY_STATE_H

#include <opm/common/utility/TimeService.hpp>
#include <opm/io/eclipse/SummaryNode.hpp>

#include <cstddef>
#include <ctime>
#include <iosfwd>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace Opm {

class UDQSet;

// Opaque handle for O(1) access to summary values.
// Clients can cache these handles to avoid repeated string lookups.
struct SummaryHandle {
    std::size_t index{std::numeric_limits<std::size_t>::max()};
    
    bool is_valid() const noexcept { 
        return index != std::numeric_limits<std::size_t>::max(); 
    }
    
    bool operator==(const SummaryHandle&) const = default;
    
    template<class Serializer>
    void serializeOp(Serializer& serializer) {
        serializer(index);
    }
};

} // namespace Opm

// The purpose of the SummaryState class is to serve as a small container
// object for computed, ready to use summary values.  The values will
// typically be used by the UDQ, WTEST and ACTIONX calculations.  Observe
// that all values are stored in the run's output unit conventions.
//
// PERFORMANCE NOTE:
// For performance-critical code (e.g., inner loops), use the handle-based API:
//
//     auto handle = st.register_well_var("OPX", "WWCT");
//     // ... later, in a loop:
//     st.update(handle, new_value);
//     double val = st.get(handle);
//
// This provides O(1) access after the initial registration.
//
// For convenience and backward compatibility, string-based access is still supported:
//
//     st.update_well_var("OPX", "WWCT", 0.75);
//     double val = st.get_well_var("OPX", "WWCT");
//
// The string-based API performs a hash lookup on each call.

namespace Opm {

class SummaryState
{
public:
    using const_iterator = std::unordered_map<std::string, double>::const_iterator;

    explicit SummaryState(time_point sim_start_arg, double udqUndefined);

    // The std::time_t constructor is only for export to Python
    explicit SummaryState(std::time_t sim_start_arg);

    // Only used for testing purposes.
    SummaryState() : SummaryState(std::time_t{0}) {}
    ~SummaryState() = default;

    // ========================================================================
    // HANDLE-BASED API (NEW - for performance-critical code)
    // ========================================================================
    
    // Register a summary variable and get a handle for fast access.
    // If the variable is already registered, returns the existing handle.
    // These methods never throw and always return a valid handle.
    SummaryHandle register_key(const std::string& key);
    SummaryHandle register_well_var(const std::string& well, const std::string& var);
    SummaryHandle register_group_var(const std::string& group, const std::string& var);
    SummaryHandle register_conn_var(const std::string& well, const std::string& var, std::size_t global_index);
    SummaryHandle register_segment_var(const std::string& well, const std::string& var, std::size_t segment);
    SummaryHandle register_region_var(const std::string& regSet, const std::string& var, std::size_t region);
    
    // Fast O(1) read access using a handle.
    // Precondition: handle must be valid (from a register_* call on this SummaryState).
    double get(SummaryHandle handle) const;
    
    // Fast O(1) write access using a handle.
    // Returns a reference to the value for direct modification.
    // Precondition: handle must be valid.
    double& operator[](SummaryHandle handle);
    
    // Update a value using a handle.
    // Automatically handles totals (accumulation) vs. state variables (assignment).
    // Precondition: handle must be valid.
    void update(SummaryHandle handle, double value);
    
    // Set a value unconditionally (no total accumulation).
    // Precondition: handle must be valid.
    void set(SummaryHandle handle, double value);
    
    // Check if a handle is valid for this SummaryState.
    bool is_valid_handle(SummaryHandle handle) const noexcept {
        return handle.is_valid() && handle.index < values_.size();
    }
    
    // ========================================================================
    // STRING-BASED API (existing - kept for compatibility)
    // ========================================================================
    
    // The canonical way to update the SummaryState is through the
    // update_xxx() methods which will inspect the variable and either
    // accumulate or just assign, depending on whether it represents a total
    // or not. The set() method is low level and unconditionally do an
    // assignment.
    void set(const std::string& key, double value);

    bool erase(const std::string& key);
    bool erase_well_var(const std::string& well, const std::string& var);
    bool erase_group_var(const std::string& group, const std::string& var);

    bool has(const std::string& key) const;
    bool has_well_var(const std::string& well, const std::string& var) const;
    bool has_well_var(const std::string& var) const;
    bool has_group_var(const std::string& group, const std::string& var) const;
    bool has_group_var(const std::string& var) const;
    bool has_conn_var(const std::string& well, const std::string& var, std::size_t global_index) const;
    bool has_segment_var(const std::string& well, const std::string& var, std::size_t segment) const;
    bool has_region_var(const std::string& regSet, const std::string& var, std::size_t region) const;

    void update(const std::string& key, double value);
    void update_well_var(const std::string& well, const std::string& var, double value);
    void update_group_var(const std::string& group, const std::string& var, double value);
    void update_group_var(const std::string& group, const std::string& var, EclIO::SummaryNode::Type type, double value);
    void update_elapsed(double delta);
    void update_udq(const UDQSet& udq_set);
    void update_conn_var(const std::string& well, const std::string& var, std::size_t global_index, double value);
    void update_conn_var(const std::string& well, const std::string& var, EclIO::SummaryNode::Type type, std::size_t global_index, double value);

    void update_segment_var(const std::string& well, const std::string& var, std::size_t segment, double value);
    void update_region_var(const std::string& regSet, const std::string& var, std::size_t region, double value);

    double get(const std::string&) const;
    double get(const std::string&, double) const;
    double get_elapsed() const;
    double get_well_var(const std::string& well, const std::string& var) const;
    double get_group_var(const std::string& group, const std::string& var) const;
    double get_conn_var(const std::string& conn, const std::string& var, std::size_t global_index) const;
    double get_segment_var(const std::string& well, const std::string& var, std::size_t segment) const;
    double get_region_var(const std::string& regSet, const std::string& var, std::size_t region) const;
    double get_well_var(const std::string& well, const std::string& var, double) const;
    double get_group_var(const std::string& group, const std::string& var, double) const;
    double get_conn_var(const std::string& conn, const std::string& var, std::size_t global_index, double) const;
    double get_segment_var(const std::string& well, const std::string& var, std::size_t segment, double) const;
    double get_region_var(const std::string& regSet, const std::string& var, std::size_t region, double) const;
    double get_udq_undefined() const { return udq_undefined; }

    bool is_undefined_value(const double val) const { return val == udq_undefined; }

    const std::vector<std::string>& wells() const;
    std::vector<std::string> wells(const std::string& var) const;
    const std::vector<std::string>& groups() const;
    std::vector<std::string> groups(const std::string& var) const;
    void append(const SummaryState& buffer);
    const_iterator begin() const;
    const_iterator end() const;
    std::size_t num_wells() const;
    std::size_t size() const;
    bool operator==(const SummaryState& other) const;

    template<class Serializer>
    void serializeOp(Serializer& serializer)
    {
        serializer(sim_start);
        serializer(this->udq_undefined);
        serializer(elapsed);
        
        // Serialize new indexed storage
        serializer(values_);
        serializer(key_to_handle_);
        serializer(is_total_);
        
        // Entity tracking
        serializer(m_wells);
        serializer(well_names);
        serializer(m_groups);
        serializer(group_names);
    }

    static SummaryState serializationTestObject();

private:
    time_point sim_start;
    double udq_undefined{};
    double elapsed = 0;
    
    // ========================================================================
    // NEW INDEXED STORAGE - eliminates duplication and indirection
    // ========================================================================
    
    // Single source of truth for all summary values
    std::vector<double> values_;
    
    // Maps composite keys to handles (one lookup instead of 2-3)
    std::unordered_map<std::string, SummaryHandle> key_to_handle_;
    
    // Tracks whether each value is a total (needs accumulation) or state (assignment)
    std::vector<bool> is_total_;
    
    // ========================================================================
    // LEGACY DATA STRUCTURES (for backward compatibility during transition)
    // TODO: Remove these after full migration
    // ========================================================================
    
    // Kept for compatibility with existing serialization and legacy code
    std::unordered_map<std::string, double> values;  // Legacy - mirrored from values_
    
    // Entity tracking (still needed for wells(), groups() methods)
    std::set<std::string> m_wells;
    mutable std::optional<std::vector<std::string>> well_names;
    std::set<std::string> m_groups;
    mutable std::optional<std::vector<std::string>> group_names;

    // Reusable buffer for formatting keys
    mutable std::string key_buffer_;

    // ========================================================================
    // HELPER METHODS
    // ========================================================================
    
    // Internal method to register a key and allocate storage
    SummaryHandle make_handle(const std::string& key, bool is_total_var);
    
    // Sync legacy 'values' map from indexed storage (for backward compat)
    void sync_legacy_map(SummaryHandle handle, const std::string& key);
    
    // Extract entity name from a composite key (e.g., "WOPR:WELL_A" -> "WELL_A")
    void track_entity(const std::string& key);
};

std::ostream& operator<<(std::ostream& stream, const SummaryState& st);

} // namespace Opm

#endif  // SUMMARY_STATE_H
