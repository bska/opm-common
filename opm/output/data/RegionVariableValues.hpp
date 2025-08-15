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
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Opm::data {

    class RegionVariableContainer
    {
    public:
        using RegsetIDs = std::function<const std::vector<int>&()>;

        explicit RegionVariableContainer(const int declaredMaxRegionID);

        virtual ~RegionVariableContainer() = default;

        void prepareRegistration();
        void commitStructure();

        void addRegionSet(const std::string& regset, RegsetIDs ids);
        void addVariable(const std::string& var_name, const bool is_cumulative);

        void prepareValueAccumulation();
        void commitValues();
        void addCellValue(const std::string& var_name, const std::size_t cell_ix, const double x);

        std::optional<double> fieldValue(const std::string& variable) const;

        std::optional<RegionVariableView<std::vector<double>::const_iterator>>
        values(const std::string& variable) const;

    protected:
        std::vector<double> increment_{};

        virtual void communicateIncrement();

    private:
        int declaredMaxRegionID_{};
        RegionsetVariableDescriptor descr_{};
        std::unordered_map<std::string, std::vector<RegsetIDs>::size_type> id_func_ix_{};
        std::vector<RegsetIDs> regset_ids_{};

        std::unordered_map<std::string, std::size_t> var_ix_{};
        std::vector<bool> is_cumulative_{};

        std::size_t end_cum_{};

        bool structure_is_final_{false};

        std::vector<double> values_{};

        void createFieldSlot();
        void reorderVariables();
        void allocateValues();

        void ensureRegistrationPossible(std::string_view type, std::string_view name);

        void addCellValue(const std::size_t var_ix, const std::size_t cell_ix, const double x);
    };

} // namespace Opm::data

#endif // OPM_OUTPUT_DATA_REGION_VARIABLE_CONTAINER_HPP
