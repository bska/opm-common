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

#include <opm/output/data/RegionVariableContainer.hpp>

#include <opm/output/data/RegionsetVariableDescriptor.hpp>
#include <opm/output/data/RegionVariableView.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fmt/format.h>

Opm::data::RegionVariableContainer::RegionVariableContainer(const int declaredMaxRegionID)
    : declaredMaxRegionID_ { declaredMaxRegionID }
{}

void Opm::data::RegionVariableContainer::prepareRegistration()
{
    this->descr_.prepareDescriptorSet();
    this->id_func_ix_.clear();
    this->regset_ids_.clear();

    this->createFieldSlot();
}

void Opm::data::RegionVariableContainer::commitStructure()
{
    this->descr_.finaliseDescriptorSet();

    this->reorderVariables();
    this->allocateValues();

    this->is_cumulative_.clear();
}

void Opm::data::RegionVariableContainer::
addRegionSet(const std::string& regset, RegsetIDs ids)
{
    this->ensureRegistrationPossible("region set", regset);

    const auto& status = this->id_func_ix_
        .try_emplace(regset, this->regset_ids_.size());

    if (! status.second) {
        // We've already seen this region set.  Nothing to do.
        return;
    }

    const auto& regsetIDs = this->regset_ids_.emplace_back(std::move(ids))();

    this->descr_.addRegionSet(this->declaredMaxRegionID_,
                              regsetIDs.begin(),
                              regsetIDs.end());
}

void
Opm::data::RegionVariableContainer::
addVariable(const std::string& var_name, const bool is_cumulative)
{
    this->ensureRegistrationPossible("variable", var_name);

    const auto& status = this->var_ix_
        .try_emplace(var_name, this->is_cumulative_.size());

    if (! status.second) {
        // We've already seen this variable.  Unexpected, but nothing tod do.
        return;
    }

    this->is_cumulative_.push_back(is_cumulative);
}

void Opm::data::RegionVariableContainer::prepareValueAccumulation()
{
    std::fill(this->increment_.begin(), this->increment_.end(), 0.0);
}

void Opm::data::RegionVariableContainer::commitValues()
{
    this->communicateIncrement();

    const auto end_cum = this->end_cum_ * this->descr_.numVariableSlots();

    // values += increment for cumulative quantities.
    std::transform(this->increment_.begin(), this->increment_.begin() + end_cum,
                   this->values_   .begin(), this->values_   .begin(), std::plus<>{});

    // values = increment for non-cumulative quantities.
    std::copy(this->increment_.begin() + end_cum, this->increment_.end(),
              this->values_   .begin() + end_cum);
}

void
Opm::data::RegionVariableContainer::
addCellValue(const std::string& var_name, const std::size_t cell_ix, const double x)
{
    const auto varPos = this->var_ix_.find(var_name);
    if (varPos == this->var_ix_.end()) {
        return;
    }

    this->addCellValue(varPos->second, cell_ix, x);
}

std::optional<double>
Opm::data::RegionVariableContainer::
fieldValue(const std::string& variable) const
{
    const auto& varPos = this->var_ix_.find(variable);
    if (varPos == this->var_ix_.end()) {
        return {};
    }

    const auto num_slots = this->descr_.numVariableSlots();

    const auto view = RegionVariableView {
        this->values_.begin() + num_slots*(varPos->second + 0),
        this->values_.begin() + num_slots*(varPos->second + 1),
        this->descr_
    };

    return { view.element(0, 0) };
}

std::optional<Opm::data::RegionVariableView<std::vector<double>::const_iterator>>
Opm::data::RegionVariableContainer::
values(const std::string& variable) const
{
    const auto varPos = this->var_ix_.find(variable);
    if (varPos == this->var_ix_.end()) {
        return {};
    }

    const auto num_slots = this->descr_.numVariableSlots();

    return {
        RegionVariableView {
            this->values_.begin() + num_slots*(varPos->second + 0),
            this->values_.begin() + num_slots*(varPos->second + 1),
            this->descr_
        }
    };
}

// ===========================================================================
// Protected member functions below separator
// ===========================================================================

void Opm::data::RegionVariableContainer::communicateIncrement()
{
    // Nothing to do in the default case (sequential run).
}

// ===========================================================================
// Private member functions below separator
// ===========================================================================

void Opm::data::RegionVariableContainer::createFieldSlot()
{
    // Note: We do not go through this->addRegionSet() here, even if the
    // operation is very similar.  The reason is that the FIELD only has a
    // single slot whereas this->addRegionSet() will create at least
    // declaredMaxRegionID_ + 1 slots.

    assert (this->regset_ids_.size() == std::size_t{0});

    this->descr_.addRegionSet(0);
}

void Opm::data::RegionVariableContainer::reorderVariables()
{
    auto i = std::vector<std::vector<bool>::size_type>(this->is_cumulative_.size());
    std::iota(i.begin(), i.end(), std::vector<bool>::size_type{0});

    const auto end_cum_pos =
        std::stable_partition(i.begin(), i.end(),
                              [this](const auto ix)
                              { return this->is_cumulative_[ix]; });

    this->end_cum_ = std::distance(i.begin(), end_cum_pos);

    auto new_var_num = std::vector<std::size_t>(i.size());
    std::for_each(i.begin(), i.end(),
                  [n = std::size_t{0}, &new_var_num]
                  (const auto var) mutable
                  { new_var_num[var] = n++; });

    std::for_each(this->var_ix_.begin(), this->var_ix_.end(),
                  [&new_var_num](auto& var_ix)
                  { var_ix.second = new_var_num[var_ix.second]; });
}

void Opm::data::RegionVariableContainer::allocateValues()
{
    const auto num_elem = this->var_ix_.size() * this->descr_.numVariableSlots();

    this->values_.assign(num_elem, 0.0);
    this->increment_.resize(num_elem);
}

void
Opm::data::RegionVariableContainer::
ensureRegistrationPossible(std::string_view type,
                           std::string_view name)
{
    if (! this->structure_is_final_) {
        // Structure is not yet committed.  This is fine.
        return;
    }

    throw std::logic_error {
        fmt::format("Cannot register a {} named '{}' after the "
                    "container's structure is finalised", type, name)
    };
}

void
Opm::data::RegionVariableContainer::
addCellValue(const std::size_t var_ix,
             const std::size_t cell_ix,
             const double      x)
{
    assert (this->id_func_ix_.at("FIELD") == std::size_t{0});

    const auto num_slots = this->descr_.numVariableSlots();

    auto view = RegionVariableView {
        this->increment_.begin() + num_slots*(var_ix + 0),
        this->increment_.begin() + num_slots*(var_ix + 1),
        this->descr_
    };

    auto regset_ix = std::size_t{1};
    view.element(/* regSetID = */ regset_ix++, 0) += x;

    for (const auto& id_func : this->regset_ids_) {
        view.element(regset_ix++, id_func()[cell_ix]) += x;
    }
}
