/*
  Copyright 2018 Statoil ASA.

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

#ifndef UDQASSIGN_HPP_
#define UDQASSIGN_HPP_

#include <opm/input/eclipse/Schedule/UDQ/UDQEnums.hpp>
#include <opm/input/eclipse/Schedule/UDQ/UDQSet.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Opm {

class UDQAssign
{
public:
    using VString = std::vector<std::string>;
    using VEnumItems = std::vector<UDQSet::EnumeratedItems>;
    using WGNameMatcher = std::function<VString(const VString&)>;
    using ItemMatcher = std::function<VEnumItems(const VString&)>;

    // If the same keyword is assigned several times the different
    // assignment records are assembled in one UDQAssign instance.  This is
    // an attempt to support restart in a situation where a full UDQ ASSIGN
    // statement can be swapped with a UDQ DEFINE statement.
    struct AssignRecord
    {
        VString input_selector{};
        std::unordered_set<std::string> rst_selector{};
        VEnumItems numbered_selector{};
        double value{};
        std::size_t report_step{};

        AssignRecord() = default;

        AssignRecord(const VString&    selector,
                     const double      value_arg,
                     const std::size_t report_step_arg)
            : input_selector(selector)
            , value         (value_arg)
            , report_step   (report_step_arg)
        {}

        AssignRecord(const std::unordered_set<std::string>& selector,
                     const double                           value_arg,
                     const std::size_t                      report_step_arg)
            : rst_selector(selector)
            , value       (value_arg)
            , report_step (report_step_arg)
        {}

        AssignRecord(const VEnumItems& selector,
                     const double      value_arg,
                     const std::size_t report_step_arg)
            : numbered_selector(selector)
            , value            (value_arg)
            , report_step      (report_step_arg)
        {}

        AssignRecord(VEnumItems&&      selector,
                     const double      value_arg,
                     const std::size_t report_step_arg)
            : numbered_selector(std::move(selector))
            , value            (value_arg)
            , report_step      (report_step_arg)
        {}

        void eval(UDQSet& values) const;
        void eval(WGNameMatcher matcher, UDQSet& values) const;
        void eval(ItemMatcher matcher, UDQSet& values) const;

        bool operator==(const AssignRecord& data) const;

        template<class Serializer>
        void serializeOp(Serializer& serializer)
        {
            serializer(this->input_selector);
            serializer(this->rst_selector);
            serializer(this->numbered_selector);
            serializer(this->value);
            serializer(this->report_step);
        }

    private:
        void assignEnumeration(const VEnumItems& items, UDQSet& values) const;
    };

    UDQAssign() = default;
    UDQAssign(const std::string& keyword,
              const VString&     selector,
              double             value,
              std::size_t        report_step);

    UDQAssign(const std::string&                     keyword,
              const std::unordered_set<std::string>& selector,
              double                                 value,
              std::size_t                            report_step);

    UDQAssign(const std::string& keyword,
              const VEnumItems&  selector,
              double             value,
              std::size_t        report_step);

    UDQAssign(const std::string& keyword,
              VEnumItems&&       selector,
              double             value,
              std::size_t        report_step);

    static UDQAssign serializationTestObject();

    const std::string& keyword() const;
    UDQVarType var_type() const;

    void add_record(const VString& selector,
                    double         value,
                    std::size_t    report_step);

    void add_record(const std::unordered_set<std::string>& rst_selector,
                    double                                 value,
                    std::size_t                            report_step);

    void add_record(const VEnumItems& selector,
                    double            value,
                    std::size_t       report_step);

    void add_record(VEnumItems&& selector,
                    double       value,
                    std::size_t  report_step);

    UDQSet eval(const VEnumItems& items) const;
    UDQSet eval(const VString& wells) const;
    UDQSet eval() const;

    UDQSet eval(const VString& wgNames, WGNameMatcher matcher) const;
    UDQSet eval(const VEnumItems& items, ItemMatcher matcher) const;

    std::size_t report_step() const;

    bool operator==(const UDQAssign& data) const;

    template<class Serializer>
    void serializeOp(Serializer& serializer)
    {
        serializer(m_keyword);
        serializer(m_var_type);
        serializer(records);
    }

private:
    std::string m_keyword{};
    UDQVarType m_var_type{UDQVarType::NONE};
    std::vector<AssignRecord> records{};
};

} // namespace Opm

#endif // UDQASSIGN_HPP_
