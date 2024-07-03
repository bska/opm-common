/*
  Copyright 2021 Equinor ASA.

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

#include <opm/io/eclipse/rst/udq.hpp>

#include <opm/output/eclipse/UDQDims.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <fmt/format.h>

namespace Opm::RestartIO {

RstUDQ::RstDefine::RstDefine(const std::string& expression_arg,
                             const UDQUpdate    status_arg)
    : expression(expression_arg)
    , status    (status_arg)
{}

void RstUDQ::RstAssign::update_value(const std::string& name_arg,
                                     const double       new_value)
{
    if (const auto current_value = this->value.value_or(new_value);
        current_value != new_value)
    {
        throw std::logic_error {
            fmt::format("Internal error: the UDQ {} changes "
                        "value {} -> {} during restart load",
                        name_arg, current_value, new_value)
        };
    }

    this->value = new_value;
}

RstUDQ::RstUDQ(const std::string& name_arg,
               const std::string& unit_arg,
               const std::string& define_arg,
               const UDQUpdate    update_arg)
    : name    (name_arg)
    , unit    (unit_arg)
    , var_type(UDQ::varType(name_arg))
    , data    (RstDefine { define_arg, update_arg })
{}

RstUDQ::RstUDQ(const std::string& name_arg,
               const std::string& unit_arg)
    : name    (name_arg)
    , unit    (unit_arg)
    , var_type(UDQ::varType(name_arg))
    , data    (RstAssign{})
{}

bool RstUDQ::is_define() const
{
    return std::holds_alternative<RstDefine>(this->data);
}

UDQUpdate RstUDQ::updateStatus() const
{
    return this->is_define()
        ? std::get<RstDefine>(this->data).status
        : UDQUpdate::OFF;
}

void RstUDQ::add_value(const std::string& wgname, const double value)
{
    if (this->is_define()) {
        std::get<RstDefine>(this->data).values.emplace_back(wgname, value);
    }
    else {
        auto& assign = std::get<RstAssign>(this->data);

        assign.update_value(this->name, value);
        assign.selector.insert(wgname);
    }
}

void RstUDQ::add_value(double value)
{
    if (this->is_define()) {
        std::get<RstDefine>(this->data).field_value = value;
    }
    else {
        std::get<RstAssign>(this->data).update_value(this->name, value);
    }
}

double RstUDQ::assign_value() const
{
    return std::get<RstAssign>(this->data).value.value();
}

const std::unordered_set<std::string>& RstUDQ::assign_selector() const
{
    return std::get<RstAssign>(this->data).selector;
}

const std::string& RstUDQ::expression() const
{
    return std::get<RstDefine>(this->data).expression;
}

const std::vector<std::pair<std::string, double>>& RstUDQ::values() const
{
    return std::get<RstDefine>(this->data).values;
}

std::optional<double> RstUDQ::field_value() const
{
    return std::get<RstDefine>(this->data).field_value;
}

RstUDQActive::RstRecord::RstRecord(const UDAControl  c,
                                   const std::size_t i,
                                   const std::size_t u1,
                                   const std::size_t u2)
    : control    (c)
    , input_index(i)
    , use_count  (u1)
    , wg_offset  (u2)
{}

RstUDQActive::RstUDQActive(const std::vector<int>& iuad_arg,
                           const std::vector<int>& iuap,
                           const std::vector<int>& igph)
    : wg_index { iuap }
    , ig_phase (igph.size(), Phase::OIL)
{
    for (auto offset = 0*iuad_arg.size();
         offset < iuad_arg.size();
         offset += UDQDims::entriesPerIUAD())
    {
        const auto* uda = &iuad_arg[offset];

        this->iuad.emplace_back(UDQ::udaControl(uda[0]),
                                uda[1] - 1,
                                uda[3],
                                uda[4] - 1);
    }

    std::transform(this->wg_index.begin(),
                   this->wg_index.end(),
                   this->wg_index.begin(),
                   [](const int wgObjectID) { return wgObjectID - 1; });

    std::transform(igph.begin(), igph.end(), this->ig_phase.begin(),
                   [](const int phase)
                   {
                       if (phase == 1) { return Phase::OIL;   }
                       if (phase == 2) { return Phase::WATER; }
                       if (phase == 3) { return Phase::GAS;   }

                       return Phase::OIL;
                   });
}

} // namespace Opm::RestartIO
