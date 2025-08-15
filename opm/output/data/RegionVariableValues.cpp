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

#include <config.h>

#include <opm/output/data/RegionVariableValues.hpp>

#include <opm/output/data/RegionsetVariableDescriptor.hpp>
#include <opm/output/data/RegionVariableView.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <vector>

std::unique_ptr<Opm::data::RegionVariableValues>
Opm::data::RegionVariableValues::clone() const
{
    return std::make_unique<RegionVariableValues>(*this);
}

void
Opm::data::RegionVariableValues::
defineVariables(const RegionsetVariableDescriptor& descr,
                const std::vector<bool>&           is_cumulative)
{
    this->descr_.emplace(std::cref(descr));

    this->partitionVariables(is_cumulative);
    this->allocateValues();
}

void Opm::data::RegionVariableValues::prepareValueAccumulation()
{
    std::fill(this->increment_.begin(), this->increment_.end(), 0.0);
}

void Opm::data::RegionVariableValues::commitValues()
{
    this->communicateIncrement();

    const auto end_cum = this->end_cum_ * this->descr_->get().numVariableSlots();

    // values += increment for cumulative quantities.
    std::transform(this->increment_.begin(), this->increment_.begin() + end_cum,
                   this->values_   .begin(), this->values_   .begin(), std::plus<>{});

    // values = increment for non-cumulative quantities.
    std::copy(this->increment_.begin() + end_cum, this->increment_.end(),
              this->values_   .begin() + end_cum);
}

void
Opm::data::RegionVariableValues::
addRegionValue(const std::size_t var_ix,
               const std::size_t regset_ix,
               const std::size_t region_ix,
               const double      x)
{
    if (! (var_ix < this->storage_ix_.size())) {
        return;
    }

    const auto num_slots = this->descr_->get().numVariableSlots();
    const auto view_ix   = this->storage_ix_[var_ix];

    RegionVariableView {
        this->increment_.begin() + num_slots*(view_ix + 0),
        this->increment_.begin() + num_slots*(view_ix + 1),
        *this->descr_
    }.element(regset_ix, region_ix) += x;
}

std::optional<Opm::data::RegionVariableView<std::vector<double>::const_iterator>>
Opm::data::RegionVariableValues::
values(const std::size_t var_ix) const
{
    if (! (var_ix < this->storage_ix_.size())) {
        return {};
    }

    const auto num_slots = this->descr_->get().numVariableSlots();
    const auto view_ix   = this->storage_ix_[var_ix];

    return {
        RegionVariableView {
            this->values_.begin() + num_slots*(view_ix + 0),
            this->values_.begin() + num_slots*(view_ix + 1),
            *this->descr_
        }
    };
}

// ===========================================================================
// Protected member functions below separator
// ===========================================================================

void Opm::data::RegionVariableValues::communicateIncrement()
{
    // Nothing to do in the default case (sequential run).
}

// ===========================================================================
// Private member functions below separator
// ===========================================================================

void
Opm::data::RegionVariableValues::
partitionVariables(const std::vector<bool>& is_cumulative)
{
    auto i = std::vector<std::vector<bool>::size_type>(is_cumulative.size());
    std::iota(i.begin(), i.end(), std::vector<bool>::size_type{0});

    const auto end_cum_pos =
        std::stable_partition(i.begin(), i.end(),
                              [&is_cumulative](const auto ix)
                              { return is_cumulative[ix]; });

    this->end_cum_ = std::distance(i.begin(), end_cum_pos);

    this->storage_ix_.assign(i.size(), std::size_t{0});
    std::for_each(i.begin(), i.end(),
                  [n = std::size_t{0}, this]
                  (const auto var) mutable
                  { this->storage_ix_[var] = n++; });
}

void Opm::data::RegionVariableValues::allocateValues()
{
    const auto num_elem = this->storage_ix_.size() *
        this->descr_->get().numVariableSlots();

    this->values_.assign(num_elem, 0.0);
    this->increment_.resize(num_elem);
}
