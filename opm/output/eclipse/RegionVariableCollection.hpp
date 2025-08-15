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

#ifndef OPM_OUTPUT_REGION_VARIABLE_COLLECTION_HPP
#define OPM_OUTPUT_REGION_VARIABLE_COLLECTION_HPP

#include <opm/output/data/RegionVariableMapping.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace Opm {
    class FieldPropsManager;
} // namespace Opm

namespace Opm::data {
    class RegionsetVariableDescriptor;
    class RegionVariableValues;
} // namespace Opm::data

/// \file Component that gives a view into a sequence of (numerical) values
/// keyed by region sets and region indices.

namespace Opm {

    class RegionVariableCollection
    {
    public:
        /// Constructor.
        explicit RegionVariableCollection(std::unique_ptr<data::RegionsetVariableDescriptor> descr,
                                          std::unique_ptr<data::RegionVariableValues>        vals);

        RegionVariableCollection(const RegionVariableCollection& rhs);
        RegionVariableCollection(RegionVariableCollection&& rhs);

        RegionVariableCollection& operator=(const RegionVariableCollection& rhs);
        RegionVariableCollection& operator=(RegionVariableCollection&& rhs);

        virtual ~RegionVariableCollection();

        void initialise(const int                          declaredMaxRegID,
                        const FieldPropsManager&           fpMgr,
                        const data::RegionVariableMapping& rvarMap);

        void addCellValue(const std::size_t var_ix,
                          const std::size_t cell_ix,
                          const double      x);

        void prepareValueAccumulation();
        void commitValues();

        std::optional<std::size_t>
        regionSetIndex(const data::RegionVariableMapping& varMap,
                       const std::string&                 regionSet) const;

        std::optional<std::size_t>
        variableIndex(const data::RegionVariableMapping& varMap,
                      const std::string&                 variable) const;

        const data::RegionVariableValues& regionVariableValues() const;

    private:
        using RegIDIterator = std::vector<int>::const_iterator;

        std::unique_ptr<data::RegionsetVariableDescriptor> descr_{};
        std::unique_ptr<data::RegionVariableValues> vals_{};

        std::vector<RegIDIterator> regSet_{};

        void initialiseRegionDescriptors(const int                          declaredMaxRegID,
                                         const FieldPropsManager&           fpMgr,
                                         const data::RegionVariableMapping& rvarMap);

        void initialiseRegionValues(const data::RegionVariableMapping& rvarMap);
    };

} // namespace Opm

#endif // OPM_OUTPUT_REGION_VARIABLE_COLLECTION_HPP
