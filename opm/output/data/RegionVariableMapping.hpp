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

#ifndef OPM_OUTPUT_DATA_REGION_VARIABLE_CONTAINER_HPP
#define OPM_OUTPUT_DATA_REGION_VARIABLE_CONTAINER_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Opm::data {

    class RegionVariableMapping
    {
    public:
        struct RegionSet   { std::string name{}; };
        struct Variable    { std::string name{}; };
        struct VariableIdx { std::size_t idx{}; };

        void prepareRegistration();
        void commitStructure();

        void add(const RegionSet& rset);
        void add(const Variable& var, const bool is_cumulative);

        auto numRegionSets() const { return this->regsets_.names().size(); }
        auto numVariables() const { return this->vars_.names().size(); }

        decltype(auto) regionSets() const { return this->regsets_.names(); }
        decltype(auto) variables() const { return this->vars_.names(); }

        auto index(const RegionSet& rset) const
        {
            this->ensureFinalStructure("region set", rset.name);
            return this->regsets_.index(rset.name);
        }

        auto index(const Variable& var) const
        {
            this->ensureFinalStructure("variable", var.name);
            return this->vars_.index(var.name);
        }

        std::optional<bool> isCumulative(const Variable& var) const
        {
            const auto i = this->index(var);
            if (! i.has_value()) { return {}; }

            return { this->isCumulative(VariableIdx { *i }) };
        }

        bool isCumulative(const VariableIdx& i) const
        { return this->is_cumulative_[i.idx]; }

    private:
        class NameLookup
        {
        public:
            void clear() { this->names_.clear(); }
            void add(const std::string& name) { this->names_.push_back(name); }
            std::vector<std::vector<std::string>::size_type> commit();

            const std::vector<std::string>& names() const { return this->names_; }

            std::optional<std::vector<std::string>::size_type>
            index(const std::string& name) const;

        private:
            std::vector<std::string> names_{};
        };

        NameLookup regsets_{};
        NameLookup vars_{};
        std::vector<bool> is_cumulative_{};

        bool is_final_{false};

        void ensureRegistrationPossible(std::string_view type,
                                        std::string_view name) const;

        void ensureFinalStructure(std::string_view type,
                                  std::string_view name) const;

        void makeUniqueCumulative(const std::vector<std::vector<std::string>::size_type>& i);
    };

} // namespace Opm::data

#endif // OPM_OUTPUT_DATA_REGION_VARIABLE_CONTAINER_HPP
