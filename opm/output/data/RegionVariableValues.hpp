/*
  Copyright 2025 Equinor ASA.

  This file is part of the Open Porous Media Project (OPM).

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

#ifndef OPM_OUTPUT_DATA_REGION_VARIABLE_CONTAINER_HPP
#define OPM_OUTPUT_DATA_REGION_VARIABLE_CONTAINER_HPP

#include <opm/output/data/RegionsetVariableDescriptor.hpp>
#include <opm/output/data/RegionVariableView.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace Opm::data {

    class RegionVariableValues
    {
    public:
        using RegsetIDs = std::function<const std::vector<int>&()>;

        virtual ~RegionVariableValues() = default;

        void prepareRegistration();
        void commitStructure(const RegionsetVariableDescriptor& descr);

        void addRegionSet(RegsetIDs ids);
        void addVariable(const bool is_cumulative);

        void prepareValueAccumulation();
        void commitValues();
        void addCellValue(const std::size_t var_ix,
                          const std::size_t cell_ix,
                          const double      x);

        std::optional<RegionVariableView<std::vector<double>::const_iterator>>
        values(const std::size_t variable) const;

    protected:
        std::vector<double> increment_{};

        virtual void communicateIncrement();

    private:
        using DescrRef = std::reference_wrapper<const RegionsetVariableDescriptor>;

        std::vector<RegsetIDs> regset_ids_{};
        std::vector<bool> is_cumulative_{};
        std::vector<std::vector<bool>::size_type> storage_ix_{};

        std::optional<DescrRef> descr_{};

        std::vector<double>::iterator var_type_partition_point_{};

        std::vector<double> values_{};

        void reorderVariables();
        void allocateValues();

        void ensureRegistrationPossible(std::string_view type,
                                        std::string_view name);
    };

} // namespace Opm::data

#endif // OPM_OUTPUT_DATA_REGION_VARIABLE_CONTAINER_HPP
