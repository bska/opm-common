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

#ifndef OPM_OUTPUT_DATA_REGION_VARIABLE_VIEW_HPP
#define OPM_OUTPUT_DATA_REGION_VARIABLE_VIEW_HPP

#include <opm/output/data/RegionsetVariableDescriptor.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <type_traits>

namespace Opm::data {

    template <typename RandIt>
    class RegionVariableView
    {
    public:
        explicit RegionVariableView(RandIt                             first,
                                    RandIt                             last,
                                    const RegionsetVariableDescriptor& variableDescriptor)
            : begin_              { first }
            , variableDescriptor_ { std::cref(variableDescriptor) }
        {
            const auto numElements = static_cast<std::size_t>(std::distance(first, last));

            if (numElements != this->variableDescriptor_.get().numVariableSlots()) {
                throw std::logic_error {
                    "Element range does not match expected number of values"
                };
            }
        }

        using ElmT = std::remove_reference_t<
            typename std::iterator_traits<RandIt>::value_type>;

        template <typename Ret = ElmT>
        std::enable_if_t<! std::is_const_v<ElmT>, Ret&>
        element(const std::size_t regSetID, const std::size_t region)
        {
            return this->begin_[ this->index(regSetID, region) ];
        }

        ElmT element(const std::size_t regSetID, const std::size_t region) const
        {
            return this->begin_[ this->index(regSetID, region) ];
        }

    private:
        using VarDesc = std::reference_wrapper<const RegionsetVariableDescriptor>;

        RandIt begin_;

        VarDesc variableDescriptor_;

        std::size_t index(const std::size_t regSetID, const std::size_t region) const
        {
            const auto& d = this->variableDescriptor_.get();

            assert (regSetID < d.numRegionSets());
            assert (d.startIndex(regSetID) + region < d.startIndex(regSetID + 1));

            return d.startIndex(regSetID) + region;
        }
    };

} // namespace Opm::data

#endif // OPM_OUTPUT_DATA_REGION_VARIABLE_VIEW_HPP
