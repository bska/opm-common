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

/// \file Component that gives a view into a sequence of (numerical) values
/// keyed by region sets and region indices.

namespace Opm::data {

    /// Linear sequence of read-only or read/write values associated to a
    /// collection of region sets and regions within each region set.
    ///
    /// The value range is expected to have one scalar element for each
    /// region in each region set.
    ///
    /// \tparam RandIt Random access iterator type.  Must be random access
    /// to support constant time element access.  If the iterator's value
    /// type is const, then this view models a read-only sequence of values.
    /// Otherwise the view is read/write.
    template <typename RandIt>
    class RegionVariableView
    {
    public:
        /// Constructor.
        ///
        /// \param[in] first Beginning of value range.
        ///
        /// \param[in] last One past the end of value range.
        ///
        /// \param[in] variableDescriptor Collection of region sets and
        /// associate region indices per region set.  The value range
        /// demarcated by \p first and \p last must have one scalar value
        /// for each region in each region set known to \p
        /// variableDescriptor.  If not, this constructor will throw an
        /// exception of type \code std::logic_error \endcode.
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

        /// Value range element type.
        using ElmT = std::remove_reference_t<
            typename std::iterator_traits<RandIt>::value_type>;

        /// Read/write access to an element in the value range.
        ///
        /// Inaccessible if \code ElmT is const.
        ///
        /// \param[in] regSetID Region set ID.  Must be in the range 0..#S-1
        /// with S being the number of region sets known to the variable
        /// descriptor from which view was constructed.
        ///
        /// \param[in] region Region index within the specific \p regSetID.
        ///
        /// \return Mutable element within the value range.
        template <typename Ret = ElmT>
        std::enable_if_t<! std::is_const_v<ElmT>, Ret&>
        element(const std::size_t regSetID, const std::size_t region)
        {
            return this->begin_[ this->index(regSetID, region) ];
        }

        /// Read-only access to an element in the value range.
        ///
        /// \param[in] regSetID Region set ID.  Must be in the range 0..#S-1
        /// with S being the number of region sets known to the variable
        /// descriptor from which view was constructed.
        ///
        /// \param[in] region Region index within the specific \p regSetID.
        ///
        /// \return Mutable element within the value range.
        ElmT element(const std::size_t regSetID, const std::size_t region) const
        {
            return this->begin_[ this->index(regSetID, region) ];
        }

    private:
        /// Convenience type alias for a wrapped variable descriptor.
        using VarDesc = std::reference_wrapper<const RegionsetVariableDescriptor>;

        /// Start of value range.
        RandIt begin_;

        /// Collection of region sets and associate region IDs.
        VarDesc variableDescriptor_;

        /// Translate a pair of region set and region indices to a linear index.
        ///
        /// \param[in] regSetID Region set ID.  Must be in the range 0..#S-1
        /// with S being the number of region sets known to the variable
        /// descriptor from which view was constructed.
        ///
        /// \param[in] region Region index within the specific \p regSetID.
        ///
        /// \return Linear index within value range corresponding to the
        /// index pair (\p regSetID, \p region).
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
