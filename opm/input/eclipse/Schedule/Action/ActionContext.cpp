/*
  Copyright 2018 Equinor ASA.

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

#include <opm/input/eclipse/Schedule/Action/ActionContext.hpp>

#include <opm/common/utility/TimeService.hpp>

#include <opm/input/eclipse/Schedule/SummaryState.hpp>

void Opm::Action::Context::add(const std::string& func,
                               const std::string& arg,
                               const double       value)
{
    this->values[func + ":" + arg] = value;
}

Opm::Action::Context::Context(const SummaryState& summary_state_arg,
                              const WListManager& wlm_)
    : summary_state { summary_state_arg }
    , wlm           { wlm_ }
{
    for (const auto& [month, idx] : TimeService::eclipseMonthIndices()) {
        this->add(month, idx);
    }
}

void Opm::Action::Context::add(const std::string& func,
                               const double       value)
{
    this->values.insert_or_assign(func, value);
}

double Opm::Action::Context::get(const std::string& func,
                                 const std::string& arg) const
{
    return this->get(func + ":" + arg);
}

double Opm::Action::Context::get(const std::string& key) const
{
    auto iter = this->values.find(key);
    if (iter != this->values.end())
        return iter->second;

    return this->summary_state.get(key);
}

std::vector<std::string>
Opm::Action::Context::wells(const std::string& key) const
{
    return this->summary_state.wells(key);
}

const Opm::WListManager&
Opm::Action::Context::wlist_manager() const
{
    return this->wlm;
}
