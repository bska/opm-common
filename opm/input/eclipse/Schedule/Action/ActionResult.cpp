/*
  Copyright 2019 Equinor ASA.

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

#include <opm/input/eclipse/Schedule/Action/ActionResult.hpp>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

Opm::Action::WellSet::WellSet(const std::vector<std::string>& wells)
{
    this->well_set.insert(wells.begin(), wells.end());
}

void Opm::Action::WellSet::add(const std::string& well)
{
    this->well_set.insert(well);
}

std::size_t Opm::Action::WellSet::size() const
{
    return this->well_set.size();
}

std::vector<std::string>
Opm::Action::WellSet::wells() const
{
    return { this->well_set.begin(), this->well_set.end() };
}

Opm::Action::WellSet&
Opm::Action::WellSet::intersect(const WellSet& other)
{
    auto well_iter = this->well_set.begin();
    while (well_iter != this->well_set.end()) {
        if (other.contains(*well_iter)) {
            ++well_iter;
        }
        else {
            well_iter = this->well_set.erase(well_iter);
        }
    }

    return *this;
}

Opm::Action::WellSet&
Opm::Action::WellSet::add(const WellSet& other)
{
    this->well_set.insert(other.well_set.begin(), other.well_set.end());

    return *this;
}

bool Opm::Action::WellSet::contains(const std::string& well) const
{
    return this->well_set.find(well) != this->well_set.end();
}

bool Opm::Action::WellSet::operator==(const WellSet& other) const
{
    return this->well_set == other.well_set;
}

Opm::Action::WellSet
Opm::Action::WellSet::serializationTestObject()
{
    WellSet ws;

    ws.well_set = {"W1", "W2", "W3"};

    return ws;
}

// ---------------------------------------------------------------------------

class Opm::Action::Result::Impl
{
};

// ---------------------------------------------------------------------------

Opm::Action::Result::Result(bool result_arg)
    : result(result_arg)
{}

Opm::Action::Result::Result(bool result_arg, const std::vector<std::string>& wells)
    : result(result_arg)
    , matching_wells(wells)
{}

Opm::Action::Result::Result(bool result_arg, const WellSet& wells)
    : result(result_arg)
    , matching_wells(wells)
{}

Opm::Action::Result::operator bool() const
{
    return this->result;
}

std::vector<std::string>
Opm::Action::Result::wells() const
{
    if (!this->result) {
        throw std::logic_error {
            "Programming error: trying to check wells "
            "in ActionResult which is false"
        };
    }

    if (this->matching_wells.has_value()) {
        return this->matching_wells->wells();
    }

    return {};
}

Opm::Action::Result&
Opm::Action::Result::operator|=(const Result& other)
{
    this->result = this->result || other.result;

    if (other.matching_wells.has_value()) {
        if (this->matching_wells.has_value()) {
            this->matching_wells->add( other.matching_wells.value() );
        }
        else {
            this->matching_wells = other.matching_wells.value();
        }
    }

    return *this;
}

Opm::Action::Result&
Opm::Action::Result::operator&=(const Result& other)
{
    this->result = this->result && other.result;

    if (other.matching_wells.has_value()) {
        if (this->matching_wells.has_value()) {
            this->matching_wells->intersect( other.matching_wells.value() );
        }
        else {
            this->matching_wells = other.matching_wells.value();
        }
    }

    return *this;
}

bool Opm::Action::Result::operator==(const Result& other) const
{
    return (this->result == other.result)
        && (this->matching_wells == other.matching_wells)
        ;
}

void Opm::Action::Result::assign(bool value)
{
    this->result = value;
}

void Opm::Action::Result::add_well(const std::string& well)
{
    if (!this->matching_wells.has_value()) {
        this->matching_wells = WellSet{};
    }

    this->matching_wells->add(well);
}

bool Opm::Action::Result::has_well(const std::string& well) const
{
    if (! this->result) {
        throw std::logic_error {
            "Programming error: trying to check "
            "wells in ActionResult which is false"
        };
    }

    return this->matching_wells.has_value()
        && this->matching_wells->contains(well);
}

Opm::Action::Result
Opm::Action::Result::serializationTestObject()
{
    Result rs;

    rs.result = false;
    rs.matching_wells = WellSet::serializationTestObject();

    return rs;
}
