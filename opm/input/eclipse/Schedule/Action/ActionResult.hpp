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

#ifndef ACTION_RESULT_HPP
#define ACTION_RESULT_HPP

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace Opm::Action {

// The Action::Result class is used to hold the boolean result of a ACTIONX
// condition like:
//
//       WWCT > 0.75
//
//  and also the final evaluation of an ACTIONX condition comes as a
//  Action::Result instance.
//
//  In addition to the overall truthness of the expression an Action::Result
//  instance can optionally have a set of matching wells. For instance the
//  the result of:
//
//       FWCT > 0.75
//
//  Will not contain any wells, whereas the result of:
//
//       WWCT > 0.75
//
//  will contain a set of all the wells matching the condition. The set of
//  matching wells can be queries with the wells() method. When a result
//  with wells and a result without wells are combined as:
//
//       WWCT > 0.50 AND FPR > 1000
//
//  The result will have all the wells corresponding to the first condition,
//  when several results with wells are combined with logical operators AND
//  and OR the set of matching wells are combined correspondingly. It is
//  actually possible to have a result which evaluates to true and still
//  have and zero matching wells - e.g.
//
//      WWCT > 1.25 OR FOPT > 1
//
//  If the condition evaluates to true the set of matching wells will be
//  passed to the Schedule::applyAction() method, and will be used in place
//  of '?' in keywords like WELOPEN.

class Result
{
public:
    template <typename T>
    class ValueRange
    {
    public:
        using RandIt = typename std::vector<T>::const_iterator;

        explicit ValueRange(RandIt first, RandIt last, bool isSorted = false)
            : first_    { first }
            , last_     { last }
            , isSorted_ { isSorted }
        {}

        auto begin() const { return this->first_; }
        auto end() const { return this->last_; }

        auto empty() const { return this->begin() == this->end(); }
        auto size() const { return std::distance(this->begin(), this->end()); }

        std::vector<T> asVector() const
        {
            return { this->begin(), this->end() };
        }

        bool hasElement(const T& elem) const
        {
            return this->isSorted_
                ? this->hasElementSorted(elem)
                : this->hasElementUnsorted(elem);
        }

    private:
        RandIt first_{};
        RandIt last_{};
        bool isSorted_{false};

        bool hasElementSorted(const T& elem) const
        {
            return std::binary_search(this->begin(), this->end(), elem);
        }

        bool hasElementUnsorted(const T& elem) const
        {
            return std::find(this->begin(), this->end(), elem)
                != this->end();
        }
    };

    class MatchingEntities
    {
    public:
        MatchingEntities();
        MatchingEntities(const MatchingEntities& rhs);
        MatchingEntities(MatchingEntities&& rhs);

        ~MatchingEntities();

        MatchingEntities& operator=(const MatchingEntities& rhs);
        MatchingEntities& operator=(MatchingEntities&& rhs);

        ValueRange<std::string> wells() const;

        bool hasWell(const std::string& well) const;
        bool operator==(const MatchingEntities& that) const;

        friend class Result;

    private:
        class Impl;
        std::unique_ptr<Impl> pImpl_;

        void addWell(const std::string& well);
        void addWells(const std::vector<std::string>& wells);

        void makeIntersection(const MatchingEntities& rhs);
        void makeUnion(const MatchingEntities& rhs);
    };

    explicit Result(bool result_arg);

    Result(const Result& rhs);
    Result(Result&& rhs);

    ~Result();

    Result& operator=(const Result& rhs);
    Result& operator=(Result&& rhs);

    Result& wells(const std::vector<std::string>& w);

    bool conditionSatisfied() const;

    Result& makeSetUnion(const Result& rhs);
    Result& makeSetIntersection(const Result& rhs);

    const MatchingEntities& matches() const;

    bool operator==(const Result& that) const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_{};
};

} // Namespace Opm::Action

#endif // ACTION_RESULT_HPP
