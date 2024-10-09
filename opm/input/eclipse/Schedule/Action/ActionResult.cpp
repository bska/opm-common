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
#include <functional>
#include <iterator>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

template <typename T>
class SortedVectorSet
{
public:
    template <typename InputIt>
    void insert(InputIt first, InputIt last);
    void insert(const T& elem);
    void insert(T&& elem);

    void commit();
    bool hasElement(const T& elem) const;

    void makeIntersection(const SortedVectorSet& rhs);
    void makeUnion(const SortedVectorSet& rhs);

    template <typename Compare, typename Equivalent>
    void commit(Compare&& cmp, Equivalent&& eq);

    template <typename Compare>
    bool hasElement(const T& elem, Compare&& cmp) const;

    template <typename Compare>
    void makeIntersection(const SortedVectorSet& rhs, Compare&& cmp);

    template <typename Compare>
    void makeUnion(const SortedVectorSet& rhs, Compare&& cmp);

    auto begin() const { return this->elems_.begin(); }
    auto end() const { return this->elems_.end(); }

    const std::vector<T>& elems() const
    {
        return this->elems_;
    }

    bool empty() const
    {
        return this->elems_.empty();
    }

    bool operator==(const SortedVectorSet& that) const
    {
        return this->elems_ == that.elems_;
    }

private:
    std::vector<T> elems_{};
};

template <typename T>
template <typename InputIt>
void SortedVectorSet<T>::insert(InputIt first, InputIt last)
{
    this->elems_.insert(this->elems_.end(), first, last);
}

template <typename T>
void SortedVectorSet<T>::insert(const T& elem)
{
    this->elems_.push_back(elem);
}

template <typename T>
void SortedVectorSet<T>::insert(T&& elem)
{
    this->elems_.push_back(std::move(elem));
}

template <typename T>
void SortedVectorSet<T>::commit()
{
    this->commit(std::less<>{}, std::equal_to<>{});
}

template <typename T>
bool SortedVectorSet<T>::hasElement(const T& elem) const
{
    return this->hasElement(elem, std::less<>{});
}

template <typename T>
void SortedVectorSet<T>::makeIntersection(const SortedVectorSet& rhs)
{
    this->makeIntersection(rhs, std::less<>{});
}

template <typename T>
void SortedVectorSet<T>::makeUnion(const SortedVectorSet& rhs)
{
    this->makeUnion(rhs, std::less<>{});
}

template <typename T>
template <typename Compare, typename Equivalent>
void SortedVectorSet<T>::commit(Compare&& cmp, Equivalent&& eq)
{
    auto i = std::vector<typename std::vector<T>::size_type>(this->elems_.size());
    std::iota(i.begin(), i.end(), typename std::vector<T>::size_type{});

    std::sort(i.begin(), i.end(), [this, cmp](const auto& i1, const auto& i2)
    {
        return cmp(this->elems_[i1], this->elems_[i2]);
    });

    auto u = std::unique(i.begin(), i.end(), [this, eq](const auto& i1, const auto& i2)
    {
        return eq(this->elems_[i1], this->elems_[i2]);
    });

    i.erase(u, i.end());

    auto unique_sorted_elems = std::vector<T>{};
    unique_sorted_elems.reserve(i.size());

    std::transform(i.begin(), i.end(), std::back_inserter(unique_sorted_elems),
                   [this](const auto& ix) { return std::move(this->elems_[ix]); });

    this->elems_.swap(unique_sorted_elems);
}

template <typename T>
template <typename Compare>
bool SortedVectorSet<T>::hasElement(const T& elem, Compare&& cmp) const
{
    return std::binary_search(this->elems_.begin(), this->elems_.end(),
                              elem, std::forward<Compare>(cmp));
}

template <typename T>
template <typename Compare>
void SortedVectorSet<T>::makeIntersection(const SortedVectorSet& rhs, Compare&& cmp)
{
    auto i = std::vector<T>{};
    i.reserve(std::min(this->elems_.size(), rhs.elems_.size()));

    std::set_intersection(this->elems_.begin(), this->elems_.end(),
                          rhs  .elems_.begin(), rhs  .elems_.end(),
                          std::back_inserter(i),
                          std::forward<Compare>(cmp));

    this->elems_.swap(i);
}

template <typename T>
template <typename Compare>
void SortedVectorSet<T>::makeUnion(const SortedVectorSet& rhs, Compare&& cmp)
{
    auto u = std::vector<T>{};
    u.reserve(this->elems_.size() + rhs.elems_.size());

    std::set_union(this->elems_.begin(), this->elems_.end(),
                   rhs  .elems_.begin(), rhs  .elems_.end(),
                   std::back_inserter(u),
                   std::forward<Compare>(cmp));

    this->elems_.swap(u);
}

template <typename T>
void intersectWithEmptyHandling(const SortedVectorSet<T>& other,
                                SortedVectorSet<T>&       curr)
{
    // If 'other' is empty, then the result set (curr) should remain
    // unchanged.  Otherwise, if 'curr' is empty, then the result set should
    // be 'other'.  Otherwise, when both 'curr' and 'other' are non-empty,
    // the result set should be the set intersection of 'curr' and 'other'.
    //
    // These rules permit intersecting the match set of a scalar condition,
    // e.g., a condition that compares a field-level quantity to a scalar,
    // with a well (or group) condition, for instance something like
    //
    //    GGOR FIELD >  432.1 AND /
    //    WOPR 'OP*' <= 123.4 /
    //
    // and having the result set match that of the WOPR condition.

    if (other.empty()) {
        return;
    }

    if (curr.empty()) {
        curr = other;
    }
    else {
        curr.makeIntersection(other);
    }
}

} // Anonymous namespace

// ---------------------------------------------------------------------------

class Opm::Action::Result::MatchingEntities::Impl
{
public:
    bool hasWell(const std::string& well) const;

    ValueRange<std::string> wells() const;

    void addWell(const std::string& well);
    void addWells(const std::vector<std::string>& wnames);

    void makeIntersection(const Impl& rhs);
    void makeUnion(const Impl& rhs);

    bool operator==(const Impl& that) const;

private:
    SortedVectorSet<std::string> wells_{};
};

bool Opm::Action::Result::MatchingEntities::Impl::
hasWell(const std::string& well) const
{
    return this->wells_.hasElement(well);
}

Opm::Action::Result::ValueRange<std::string>
Opm::Action::Result::MatchingEntities::Impl::wells() const
{
    return ValueRange<std::string> {
        this->wells_.begin(), this->wells_.end(),
        /* isSorted = */ true
    };
}

void Opm::Action::Result::MatchingEntities::Impl::
addWell(const std::string& wname)
{
    this->wells_.insert(wname);
    this->wells_.commit();
}

void Opm::Action::Result::MatchingEntities::Impl::
addWells(const std::vector<std::string>& wnames)
{
    this->wells_.insert(wnames.begin(), wnames.end());
    this->wells_.commit();
}

void Opm::Action::Result::MatchingEntities::Impl::makeIntersection(const Impl& rhs)
{
    intersectWithEmptyHandling(rhs.wells_, this->wells_);
}

void Opm::Action::Result::MatchingEntities::Impl::makeUnion(const Impl& rhs)
{
    this->wells_.makeUnion(rhs.wells_);
}

bool Opm::Action::Result::MatchingEntities::Impl::operator==(const Impl& that) const
{
    return this->wells_ == that.wells_;
}

// ---------------------------------------------------------------------------

Opm::Action::Result::MatchingEntities::MatchingEntities()
    : pImpl_ { std::make_unique<Impl>() }
{}

Opm::Action::Result::MatchingEntities::
MatchingEntities(const MatchingEntities& rhs)
    : pImpl_ { std::make_unique<Impl>(*rhs.pImpl_) }
{}

Opm::Action::Result::MatchingEntities::MatchingEntities(MatchingEntities&& rhs)
    : pImpl_ { std::move(rhs.pImpl_) }
{}

Opm::Action::Result::MatchingEntities::~MatchingEntities() = default;

Opm::Action::Result::MatchingEntities&
Opm::Action::Result::MatchingEntities::operator=(const MatchingEntities& rhs)
{
    this->pImpl_ = std::make_unique<Impl>(*rhs.pImpl_);
    return *this;
}

Opm::Action::Result::MatchingEntities&
Opm::Action::Result::MatchingEntities::operator=(MatchingEntities&& rhs)
{
    this->pImpl_ = std::move(rhs.pImpl_);
    return *this;
}

Opm::Action::Result::ValueRange<std::string>
Opm::Action::Result::MatchingEntities::wells() const
{
    return this->pImpl_->wells();
}

bool Opm::Action::Result::MatchingEntities::hasWell(const std::string& well) const
{
    return this->pImpl_->hasWell(well);
}

bool Opm::Action::Result::MatchingEntities::operator==(const MatchingEntities& that) const
{
    return *this->pImpl_ == *that.pImpl_;
}

void Opm::Action::Result::MatchingEntities::addWell(const std::string& well)
{
    this->pImpl_->addWell(well);
}

void Opm::Action::Result::MatchingEntities::
addWells(const std::vector<std::string>& wells)
{
    this->pImpl_->addWells(wells);
}

void Opm::Action::Result::MatchingEntities::makeIntersection(const MatchingEntities& rhs)
{
    this->pImpl_->makeIntersection(*rhs.pImpl_);
}

void Opm::Action::Result::MatchingEntities::makeUnion(const MatchingEntities& rhs)
{
    this->pImpl_->makeUnion(*rhs.pImpl_);
}

// ---------------------------------------------------------------------------

class Opm::Action::Result::Impl
{
public:
    explicit Impl(const bool result) : result_ { result } {}

    MatchingEntities& matches() { return this->matches_; }
    const MatchingEntities& matches() const { return this->matches_; }
    bool result() const { return this->result_; }

    void makeSetUnion(const Impl& rhs);
    void makeSetIntersection(const Impl& rhs);

    bool operator==(const Impl& that) const;

private:
    bool result_{false};
    MatchingEntities matches_{};
};

void Opm::Action::Result::Impl::makeSetUnion(const Impl& rhs)
{
    this->result_ = this->result_ || rhs.result_;
    this->matches_.makeUnion(rhs.matches());
}

void Opm::Action::Result::Impl::makeSetIntersection(const Impl& rhs)
{
    this->result_ = this->result_ && rhs.result_;
    this->matches_.makeIntersection(rhs.matches());
}

bool Opm::Action::Result::Impl::operator==(const Impl& that) const
{
    return (this->result_  == that.result_)
        && (this->matches_ == that.matches_);
}

// ---------------------------------------------------------------------------

Opm::Action::Result::Result(const bool result)
    : pImpl_ { std::make_unique<Impl>(result) }
{}

Opm::Action::Result::Result(const Result& rhs)
    : pImpl_ { std::make_unique<Impl>(*rhs.pImpl_) }
{}

Opm::Action::Result::Result(Result&& rhs)
    : pImpl_ { std::move(rhs.pImpl_) }
{}

Opm::Action::Result::~Result() = default;

Opm::Action::Result&
Opm::Action::Result::operator=(const Result& rhs)
{
    this->pImpl_ = std::make_unique<Impl>(*rhs.pImpl_);
    return *this;
}

Opm::Action::Result&
Opm::Action::Result::operator=(Result&& rhs)
{
    this->pImpl_ = std::move(rhs.pImpl_);
    return *this;
}

Opm::Action::Result&
Opm::Action::Result::wells(const std::vector<std::string>& w)
{
    this->pImpl_->matches().addWells(w);
    return *this;
}

bool Opm::Action::Result::conditionSatisfied() const
{
    return this->pImpl_->result();
}

Opm::Action::Result&
Opm::Action::Result::makeSetUnion(const Result& rhs)
{
    this->pImpl_->makeSetUnion(*rhs.pImpl_);
    return *this;
}

Opm::Action::Result&
Opm::Action::Result::makeSetIntersection(const Result& rhs)
{
    this->pImpl_->makeSetIntersection(*rhs.pImpl_);
    return *this;
}

const Opm::Action::Result::MatchingEntities&
Opm::Action::Result::matches() const
{
    return this->pImpl_->matches();
}

bool Opm::Action::Result::operator==(const Result& that) const
{
    return *this->pImpl_ == *that.pImpl_;
}
