/*
  Copyright 2026 Equinor ASA.

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

#include <opm/input/eclipse/Schedule/Well/ConnectionOrdering.hpp>

#include <opm/input/eclipse/Schedule/Well/Connection.hpp>
#include <opm/input/eclipse/Schedule/Well/WellMatcher.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

bool Opm::ConnectionOrdering::update(const Connection::Order order,
                                     const std::string&      pattern)
{
    if (order == Connection::Order::TRACK) {
        return this->setTrackOrdering(pattern);
    }
    else if (order == Connection::Order::INPUT) {
        return this->setInputOrdering(pattern);
    }
    else if (order == Connection::Order::DEPTH) {
        return this->setDepthOrdering(pattern);
    }

    return false;
}

Opm::Connection::Order
Opm::ConnectionOrdering::getConnectionOrder(const std::string& wellName,
                                            const WellMatcher& wellMatcher) const
{
    if (!this->hasAnyNonDefaultPatterns_) {
        // Default order.  Don't check whether 'wellName' is even an active well.
        return Connection::Order::TRACK;
    }

    for (const auto& [type, order] : {
        std::pair {PatternType::Input, Connection::Order::INPUT},
        std::pair {PatternType::Depth, Connection::Order::DEPTH},
    })
    {
        if (this->wellHasSpecificType(type, wellName, wellMatcher)) {
            return order;
        }
    }

    // If no specific type matches, return the default order.  Don't check
    // whether 'wellName' is even an active well.
    return Connection::Order::TRACK;
}

Opm::ConnectionOrdering Opm::ConnectionOrdering::serializationTestObject()
{
    ConnectionOrdering obj;

    // Add some test patterns for serialization testing
    obj.setInputOrdering("W1*");
    obj.setDepthOrdering("PROD");
    obj.setTrackOrdering("INJ");
    obj.setDepthOrdering("RFT-*");
    obj.setInputOrdering("*WL2*");

    return obj;
}

bool Opm::ConnectionOrdering::operator==(const ConnectionOrdering& that) const noexcept
{
    return (this->patterns_ == that.patterns_)
        && (this->hasAnyNonDefaultPatterns_ == that.hasAnyNonDefaultPatterns_)
        ;
}

// ==========================================================================
// Private member functions below separator
// ==========================================================================

bool Opm::ConnectionOrdering::setTrackOrdering(const std::string& pattern)
{
    const auto input = this->removePattern(PatternType::Input, pattern);
    const auto depth = this->removePattern(PatternType::Depth, pattern);

    return input || depth;
}

bool Opm::ConnectionOrdering::setInputOrdering(const std::string& pattern)
{
    const auto input = this->addPattern(PatternType::Input, pattern);
    const auto depth = this->removePattern(PatternType::Depth, pattern);

    return input || depth;
}

bool Opm::ConnectionOrdering::setDepthOrdering(const std::string& pattern)
{
    const auto depth = this->addPattern(PatternType::Depth, pattern);
    const auto input = this->removePattern(PatternType::Input, pattern);

    return depth || input;
}

bool Opm::ConnectionOrdering::addPattern(const PatternType  type,
                                         const std::string& pattern)
{
    auto& vec = this->patterns_.at(static_cast<std::size_t>(type));

    if (std::ranges::find(vec, pattern) != vec.end()) {
        return false;
    }

    vec.push_back(pattern);

    this->setNonDefaultPatternFlag();

    return true;
}

bool
Opm::ConnectionOrdering::removePattern(const PatternType  type,
                                       const std::string& pattern)
{
    auto& vec = this->patterns_.at(static_cast<std::size_t>(type));

    const auto it = std::ranges::find(vec, pattern);
    if (it == vec.end()) {
        return false;
    }

    vec.erase(it);

    this->setNonDefaultPatternFlag();

    return true;
}

void Opm::ConnectionOrdering::setNonDefaultPatternFlag()
{
    this->hasAnyNonDefaultPatterns_ = std::ranges::any_of
        (this->patterns_, [](const auto& vec) { return !vec.empty(); });
}

bool
Opm::ConnectionOrdering::wellHasSpecificType(const PatternType  type,
                                             const std::string& wellName,
                                             const WellMatcher& wellMatcher) const
{
    const auto& vec = this->patterns_.at(static_cast<std::size_t>(type));

    return std::ranges::any_of(vec, [&wellMatcher, &wellName](const auto& pattern) {
        const auto match = WellMatcher::ActiveWellMatch {
            .wellName = std::cref(wellName),
            .pattern = std::cref(pattern),
        };

        return wellMatcher.activeWellMatches(match);
    });
}
