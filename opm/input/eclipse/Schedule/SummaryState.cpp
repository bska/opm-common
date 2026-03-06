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

#include <opm/input/eclipse/Schedule/SummaryState.hpp>


#include <opm/common/utility/TimeService.hpp>

#include <opm/input/eclipse/EclipseState/SummaryConfig/SummaryConfig.hpp>
#include <opm/input/eclipse/Schedule/UDQ/UDQSet.hpp>

#include <opm/io/eclipse/SummaryNode.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <limits>
#include <ostream>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>

namespace {

    bool is_udq(std::string_view keyword)
    {
        // Does 'keyword' match one of the patterns
        //   AU*, BU*, CU*, FU*, GU*, RU*, SU*, or WU*?
        using sz_t = std::string_view::size_type;

        return (keyword.size() > sz_t{1})
            && (keyword[1] == 'U')
            && (keyword.find_first_of("WGFCRBSA") == sz_t{0});
    }

    bool is_well_udq(std::string_view keyword)
    {
        // Does 'keyword' match the pattern
        //   WU*?
        using sz_t = std::string_view::size_type;

        return (keyword.size() > sz_t{1})
            && (keyword.compare(0, 2, "WU") == 0);
    }

    bool is_group_udq(std::string_view keyword)
    {
        // Does 'keyword' match the pattern
        //   GU*?
        using sz_t = std::string_view::size_type;

        return (keyword.size() > sz_t{1})
            && (keyword.compare(0, 2, "GU") == 0);
    }

    bool is_segment_udq(std::string_view keyword)
    {
        // Does 'keyword' match the pattern
        //   SU*?
        using sz_t = std::string_view::size_type;

        return (keyword.size() > sz_t{1})
            && (keyword.compare(0, 2, "SU") == 0);
    }

    bool is_total(const std::string& key)
    {
        static const std::vector<std::string> totals = {
            "OPT"  , "GPT"  , "WPT" , "GIT", "WIT", "GLIT", "OPTF" , "OPTS" , "OIT"  , "OVPT" , "OVIT" , "MWT" ,
            "WVPT" , "WVIT" , "GMT"  , "GPTF" , "SGT"  , "GST" , "FGT" , "GCT" , "GIMT" ,
            "WGPT" , "WGIT" , "EGT"  , "EXGT" , "GVPT" , "GVIT" , "LPT" , "VPT" , "VIT" , "NPT" , "NIT" , "LIT",
            "TPT", "TIT", "CPT", "CIT", "SPT", "SIT", "EPT", "EIT", "TPTHEA", "TITHEA",
            "MMIT", "MOIT", "MUIT", "MMPT", "MOPT", "MUPT",
            "OFT", "OFT+", "OFT-", "OFTG", "OFTL",
            "GFT", "GFT+", "GFT-", "GFTG", "GFTL",
            "WFT", "WFT+", "WFT-", "GMIT", "GMPT", "AMIT", "AMPT"
            // TODO: Add AQT and NQT when the aquifer code is modified
            // to produce incremental rather than cumulative aquifer quantities.
            // Currently the aquifer code does the cumulation internally and reports
            // those cumulative values to the summary. Also in is_total() from SummaryConfig.cpp.
        };

        auto sep_pos = key.find(':');

        // Starting with ':' - that is probably broken?!
        if (sep_pos == 0)
            return false;

        if (sep_pos == std::string::npos) {
            return std::ranges::any_of(totals,
                                       [&key](const auto& total)
                                       { return key.compare(1, total.size(), total) == 0; });
        } else {
            return is_total(key.substr(0, sep_pos));
        }
    }

    std::string normalise_region_set_name(const std::string& regSet)
    {
        if (regSet.empty()) {
            return "NUM";       // "" -> FIPNUM
        }

        // Discard initial "FIP" prefix if it exists.
        const auto maxchar = std::string::size_type{3};
        const auto prefix = std::string_view { "FIP" };
        const auto start = prefix.size() * (regSet.find(prefix) == std::string::size_type{0});
        return regSet.substr(start, maxchar);
    }

    std::string region_key(const std::string& variable,
                           const std::string& regSet,
                           const std::size_t  region)
    {
        auto node = Opm::EclIO::SummaryNode {
            variable, Opm::EclIO::SummaryNode::Category::Region
        };

        node.number = static_cast<int>(region);

        if (! regSet.empty() && (regSet != "NUM") && (regSet != "FIPNUM")) {
            // Generate summary vector names of the forms
            //   * RPR__ABC
            //   * ROPT_ABC
            //   * RODENABC
            // to uniquely identify vectors in the 'FIPABC' region set.
            node.keyword = fmt::format("{:_<5}{}", node.keyword,
                                       normalise_region_set_name(regSet));
        }

        return node.unique_key();
    }

} // Anonymous namespace

namespace Opm
{
    SummaryState::SummaryState(const time_point sim_start_arg,
                               const double     udqUndefined)
        : sim_start     { sim_start_arg }
        , udq_undefined { udqUndefined }
    {
        this->update_elapsed(0);
    }

    SummaryState::SummaryState(const std::time_t sim_start_arg)
        : SummaryState { TimeService::from_time_t(sim_start_arg),
                         std::numeric_limits<double>::lowest() }
    {}

    // ========================================================================
    // HANDLE-BASED API IMPLEMENTATION
    // ========================================================================

    SummaryHandle SummaryState::make_handle(const std::string& key, bool is_total_var)
    {
        auto [it, inserted] = key_to_handle_.try_emplace(key, SummaryHandle{});
        
        if (inserted) {
            // Allocate new storage for this variable
            it->second.index = values_.size();
            values_.push_back(0.0);
            is_total_.push_back(is_total_var);
        }
        
        return it->second;
    }

    SummaryHandle SummaryState::register_key(const std::string& key)
    {
        return make_handle(key, is_total(key));
    }

    SummaryHandle SummaryState::register_well_var(const std::string& well, 
                                                   const std::string& var)
    {
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}", var, well);
        
        // Track this well
        if (m_wells.count(well) == 0) {
            m_wells.insert(well);
            well_names.reset();
        }
        
        return make_handle(key_buffer_, is_total(var));
    }

    SummaryHandle SummaryState::register_group_var(const std::string& group,
                                                    const std::string& var)
    {
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}", var, group);
        
        // Track this group
        if (m_groups.count(group) == 0) {
            m_groups.insert(group);
            group_names.reset();
        }
        
        return make_handle(key_buffer_, is_total(var));
    }

    SummaryHandle SummaryState::register_conn_var(const std::string& well,
                                                   const std::string& var,
                                                   const std::size_t  global_index)
    {
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}:{}", var, well, global_index);
        return make_handle(key_buffer_, is_total(var));
    }

    SummaryHandle SummaryState::register_segment_var(const std::string& well,
                                                      const std::string& var,
                                                      const std::size_t  segment)
    {
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}:{}", var, well, segment);
        return make_handle(key_buffer_, is_total(var));
    }

    SummaryHandle SummaryState::register_region_var(const std::string& regSet,
                                                     const std::string& var,
                                                     const std::size_t  region)
    {
        const auto key = region_key(var, regSet, region);
        return make_handle(key, is_total(var));
    }

    double SummaryState::get(SummaryHandle handle) const
    {
        assert(is_valid_handle(handle));
        return values_[handle.index];
    }

    double& SummaryState::operator[](SummaryHandle handle)
    {
        assert(is_valid_handle(handle));
        return values_[handle.index];
    }

    void SummaryState::update(SummaryHandle handle, double value)
    {
        assert(is_valid_handle(handle));
        
        if (is_total_[handle.index]) {
            values_[handle.index] += value;
        } else {
            values_[handle.index] = value;
        }
    }

    void SummaryState::set(SummaryHandle handle, double value)
    {
        assert(is_valid_handle(handle));
        values_[handle.index] = value;
    }

    // ========================================================================
    // STRING-BASED API - now implemented using handles internally
    // ========================================================================

    void SummaryState::set(const std::string& key, double value)
    {
        auto handle = register_key(key);
        set(handle, value);
        
        // Maintain legacy map for backward compatibility
        this->values[key] = value;
    }

    void SummaryState::update(const std::string& key, double value)
    {
        auto handle = register_key(key);
        update(handle, value);
        
        // Maintain legacy map
        if (is_total(key)) {
            this->values[key] += value;
        } else {
            this->values[key] = value;
        }
    }

    void SummaryState::update_well_var(const std::string& well,
                                       const std::string& var,
                                       const double       value)
    {
        auto handle = register_well_var(well, var);
        update(handle, value);
        
        // Maintain legacy map
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}", var, well);
        if (is_total(var)) {
            this->values[key_buffer_] += value;
        } else {
            this->values[key_buffer_] = value;
        }
    }

    void SummaryState::update_group_var(const std::string& group,
                                        const std::string& var,
                                        const double       value)
    {
        auto handle = register_group_var(group, var);
        update(handle, value);
        
        // Maintain legacy map
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}", var, group);
        if (is_total(var)) {
            this->values[key_buffer_] += value;
        } else {
            this->values[key_buffer_] = value;
        }
    }

    void SummaryState::update_group_var(const std::string& group,
                                        const std::string& var,
                                        const EclIO::SummaryNode::Type type,
                                        const double       value)
    {
        auto handle = register_group_var(group, var);
        
        if (type == EclIO::SummaryNode::Type::Total) {
            values_[handle.index] += value;
        } else {
            values_[handle.index] = value;
        }
        
        // Maintain legacy map
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}", var, group);
        if (type == EclIO::SummaryNode::Type::Total) {
            this->values[key_buffer_] += value;
        } else {
            this->values[key_buffer_] = value;
        }
    }

    void SummaryState::update_conn_var(const std::string& well,
                                       const std::string& var,
                                       const std::size_t  global_index,
                                       const double       value)
    {
        auto handle = register_conn_var(well, var, global_index);
        update(handle, value);
        
        // Maintain legacy map
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}:{}", var, well, global_index);
        if (is_total(var)) {
            this->values[key_buffer_] += value;
        } else {
            this->values[key_buffer_] = value;
        }
    }

    void SummaryState::update_conn_var(const std::string& well,
                                       const std::string& var,
                                       const SummaryConfigNode::Type type,
                                       const std::size_t  global_index,
                                       const double       value)
    {
        auto handle = register_conn_var(well, var, global_index);
        
        if (type == EclIO::SummaryNode::Type::Total) {
            values_[handle.index] += value;
        } else {
            values_[handle.index] = value;
        }
        
        // Maintain legacy map
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}:{}", var, well, global_index);
        if (type == EclIO::SummaryNode::Type::Total) {
            this->values[key_buffer_] += value;
        } else {
            this->values[key_buffer_] = value;
        }
    }

    void SummaryState::update_segment_var(const std::string& well,
                                          const std::string& var,
                                          const std::size_t  segment,
                                          const double       value)
    {
        auto handle = register_segment_var(well, var, segment);
        update(handle, value);
        
        // Maintain legacy map
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}:{}", var, well, segment);
        if (is_total(var)) {
            this->values[key_buffer_] += value;
        } else {
            this->values[key_buffer_] = value;
        }
    }

    void SummaryState::update_region_var(const std::string& regSet,
                                         const std::string& var,
                                         const std::size_t  region,
                                         const double       value)
    {
        auto handle = register_region_var(regSet, var, region);
        update(handle, value);
        
        // Maintain legacy map
        const auto key = region_key(var, regSet, region);
        if (is_total(var)) {
            this->values[key] += value;
        } else {
            this->values[key] = value;
        }
    }

    double SummaryState::get_well_var(const std::string& well,
                                      const std::string& var) const
    {
        const auto use_udq_fallback = is_well_udq(var);
        
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}", var, well);
        
        auto it = key_to_handle_.find(key_buffer_);
        if (it == key_to_handle_.end()) {
            if (!use_udq_fallback) {
                throw std::invalid_argument {
                    fmt::format("Summary vector {} does not exist at the well level for well {}", var, well)
                };
            }
            return this->udq_undefined;
        }
        
        return values_[it->second.index];
    }

    double SummaryState::get_group_var(const std::string& group,
                                       const std::string& var) const
    {
        const auto use_udq_fallback = is_group_udq(var);
        
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}", var, group);
        
        auto it = key_to_handle_.find(key_buffer_);
        if (it == key_to_handle_.end()) {
            if (!use_udq_fallback) {
                throw std::invalid_argument {
                    fmt::format("Summary vector {} does not exist at the group level for group {}", var, group)
                };
            }
            return this->udq_undefined;
        }
        
        return values_[it->second.index];
    }

    double SummaryState::get_conn_var(const std::string& well,
                                      const std::string& var,
                                      const std::size_t  global_index) const
    {
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}:{}", var, well, global_index);
        
        auto it = key_to_handle_.find(key_buffer_);
        if (it == key_to_handle_.end()) {
            throw std::invalid_argument {
                fmt::format("Summary vector {} does not exist for connection {} in well {}", 
                           var, global_index, well)
            };
        }
        
        return values_[it->second.index];
    }

    double SummaryState::get_segment_var(const std::string& well,
                                         const std::string& var,
                                         const std::size_t  segment) const
    {
        const auto use_udq_fallback = is_segment_udq(var);
        
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}:{}", var, well, segment);
        
        auto it = key_to_handle_.find(key_buffer_);
        if (it == key_to_handle_.end()) {
            if (!use_udq_fallback) {
                throw std::invalid_argument {
                    fmt::format("Summary vector {} does not exist for segment {} in well {}", 
                               var, segment, well)
                };
            }
            return this->udq_undefined;
        }
        
        return values_[it->second.index];
    }

    double SummaryState::get_region_var(const std::string& regSet,
                                        const std::string& var,
                                        const std::size_t  region) const
    {
        const auto key = region_key(var, regSet, region);
        
        auto it = key_to_handle_.find(key);
        if (it == key_to_handle_.end()) {
            throw std::invalid_argument {
                fmt::format("Summary vector {} does not exist for region {} in region set {}", 
                           var, region, regSet)
            };
        }
        
        return values_[it->second.index];
    }

    // Versions with default values
    double SummaryState::get_well_var(const std::string& well,
                                      const std::string& var,
                                      const double       default_value) const
    {
        const auto fallback = is_well_udq(var) ? this->udq_undefined : default_value;
        
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}", var, well);
        
        auto it = key_to_handle_.find(key_buffer_);
        return (it == key_to_handle_.end()) ? fallback : values_[it->second.index];
    }

    double SummaryState::get_group_var(const std::string& group,
                                       const std::string& var,
                                       const double       default_value) const
    {
        const auto fallback = is_group_udq(var) ? this->udq_undefined : default_value;
        
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}", var, group);
        
        auto it = key_to_handle_.find(key_buffer_);
        return (it == key_to_handle_.end()) ? fallback : values_[it->second.index];
    }

    double SummaryState::get_conn_var(const std::string& well,
                                      const std::string& var,
                                      const std::size_t  global_index,
                                      const double       default_value) const
    {
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}:{}", var, well, global_index);
        
        auto it = key_to_handle_.find(key_buffer_);
        return (it == key_to_handle_.end()) ? default_value : values_[it->second.index];
    }

    double SummaryState::get_segment_var(const std::string& well,
                                         const std::string& var,
                                         const std::size_t  segment,
                                         const double       default_value) const
    {
        key_buffer_.clear();
        fmt::format_to(std::back_inserter(key_buffer_), "{}:{}:{}", var, well, segment);
        
        auto it = key_to_handle_.find(key_buffer_);
        return (it == key_to_handle_.end()) ? default_value : values_[it->second.index];
    }

    double SummaryState::get_region_var(const std::string& regSet,
                                        const std::string& var,
                                        const std::size_t  region,
                                        const double       default_value) const
    {
        const auto key = region_key(var, regSet, region);
        
        auto it = key_to_handle_.find(key);
        return (it == key_to_handle_.end()) ? default_value : values_[it->second.index];
    }

    // ... Rest of the existing implementation (has, erase, etc.) would follow similar patterns

} // namespace Opm
