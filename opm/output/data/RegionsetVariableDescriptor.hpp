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

#ifndef OPM_OUTPUT_DATA_REGIONSET_VARIABLE_DESCRIPTOR_HPP
#define OPM_OUTPUT_DATA_REGIONSET_VARIABLE_DESCRIPTOR_HPP

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

namespace Opm::data {

    class RegionsetVariableDescriptor
    {
    public:
        RegionsetVariableDescriptor() = default;
        virtual ~RegionsetVariableDescriptor() = default;

        RegionsetVariableDescriptor(const RegionsetVariableDescriptor&) = default;
        RegionsetVariableDescriptor(RegionsetVariableDescriptor&&) = default;

        RegionsetVariableDescriptor& operator=(const RegionsetVariableDescriptor&) = default;
        RegionsetVariableDescriptor& operator=(RegionsetVariableDescriptor&&) = default;

        void prepareDescriptorSet();

        template <typename FwdIt>
        void addRegionSet(const int declaredMaxRegionID,
                          FwdIt     begin,
                          FwdIt     end)
        {
            const auto maxIdPos = std::max_element(begin, end);
            if (maxIdPos == end) {
                // Empty collection.  Unexpected.
                this->addRegionSet(declaredMaxRegionID);
            }
            else {
                this->addRegionSet(std::max(declaredMaxRegionID,
                                            *maxIdPos));
            }
        }

        void addRegionSet(const int maxRegionID);

        void finaliseDescriptorSet();

        std::size_t startIndex(const std::size_t regSet) const
        {
            return this->startPtr_[regSet];
        }

        std::size_t numVariableSlots() const
        {
            return this->startPtr_.back();
        }

        std::size_t numRegionSets() const
        {
            return this->startPtr_.size() - 1;
        }

    protected:
        std::optional<std::vector<int>> regsetMaxID_{};
        virtual void communicateGlobalRegsetMaxIDs();

    private:
        std::vector<std::size_t> startPtr_{};
        void defineStartPointers();
    };

} // namespace Opm::data

#endif // OPM_OUTPUT_DATA_REGIONSET_VARIABLE_DESCRIPTOR_HPP
