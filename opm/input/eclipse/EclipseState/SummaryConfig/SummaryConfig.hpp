/*
  Copyright 2015 Statoil ASA.

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

#ifndef OPM_SUMMARY_CONFIG_HPP
#define OPM_SUMMARY_CONFIG_HPP

#include <opm/io/eclipse/SummaryNode.hpp>

#include <opm/common/OpmLog/KeywordLocation.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace Opm {
    class AquiferConfig;
    class Deck;
    class EclipseState;
    class ErrorGuard;
    class FieldPropsManager;
    class GridDims;
    class ParseContext;
    class Schedule;
} // namespace Opm

namespace Opm {

    class SummaryConfigNode
    {
    public:
        using Category = Opm::EclIO::SummaryNode::Category;
        using Type = Opm::EclIO::SummaryNode::Type;

        SummaryConfigNode() = default;
        explicit SummaryConfigNode(std::string keyword,
                                   const Category cat,
                                   KeywordLocation loc_arg);

        static SummaryConfigNode serializationTestObject();

        SummaryConfigNode& parameterType(const Type type);
        SummaryConfigNode& namedEntity(std::string name);
        SummaryConfigNode& number(const int num);
        SummaryConfigNode& isUserDefined(const bool userDefined);
        SummaryConfigNode& fip_region(const std::string& fip_region);

        const std::string& keyword() const { return this->keyword_; }
        Category category() const { return this->category_; }
        Type type() const { return this->type_; }
        const std::string& namedEntity() const { return this->name_; }
        int number() const { return this->number_; }
        bool isUserDefined() const { return this->userDefined_; }
        const std::string& fip_region() const { return *this->fip_region_ ; }

        std::string uniqueNodeKey() const;
        const KeywordLocation& location() const { return this->loc; }

        operator Opm::EclIO::SummaryNode() const {
            return { keyword_, category_, type_, name_, number_, fip_region_, {}};
        }

        template<class Serializer>
        void serializeOp(Serializer& serializer)
        {
            serializer(keyword_);
            serializer(category_);
            serializer(loc);
            serializer(type_);
            serializer(name_);
            serializer(number_);
            serializer(fip_region_);
            serializer(userDefined_);
        }

    private:
        std::string                keyword_{};
        Category                   category_{};
        KeywordLocation            loc{};
        Type                       type_{ Type::Undefined };
        std::string                name_{};
        int                        number_{std::numeric_limits<int>::min()};
        std::optional<std::string> fip_region_;
        bool                       userDefined_{false};
    };

    SummaryConfigNode::Category parseKeywordCategory(const std::string& keyword);
    SummaryConfigNode::Type parseKeywordType(std::string keyword);

    bool operator==(const SummaryConfigNode& lhs, const SummaryConfigNode& rhs);
    bool operator<(const SummaryConfigNode& lhs, const SummaryConfigNode& rhs);

    inline bool operator!=(const SummaryConfigNode& lhs, const SummaryConfigNode& rhs)
    {
        return ! (lhs == rhs);
    }

    inline bool operator<=(const SummaryConfigNode& lhs, const SummaryConfigNode& rhs)
    {
        return ! (rhs < lhs);
    }

    inline bool operator>(const SummaryConfigNode& lhs, const SummaryConfigNode& rhs)
    {
        return rhs < lhs;
    }

    inline bool operator>=(const SummaryConfigNode& lhs, const SummaryConfigNode& rhs)
    {
        return ! (lhs < rhs);
    }

    class SummaryConfig
    {
        public:
            using keyword_list = std::vector<SummaryConfigNode>;

            SummaryConfig() = default;
            SummaryConfig(const Deck&,
                          const Schedule&,
                          const FieldPropsManager&,
                          const AquiferConfig&,
                          const ParseContext&,
                          ErrorGuard&);

            template <typename T>
            SummaryConfig(const Deck&,
                          const Schedule&,
                          const FieldPropsManager&,
                          const AquiferConfig&,
                          const ParseContext&,
                          T&&);

            SummaryConfig(const Deck&,
                          const Schedule&,
                          const FieldPropsManager&,
                          const AquiferConfig&);

            SummaryConfig(const keyword_list& kwds,
                          const std::set<std::string>& shortKwds,
                          const std::set<std::string>& smryKwds);

            static SummaryConfig serializationTestObject();

            auto begin() const { return this->m_keywords.begin(); }
            auto end() const { return this->m_keywords.end(); }

            std::size_t size() const { return this->m_keywords.size(); }

            const keyword_list& extraFracturingVectors() const
            {
                return this->extraFracturingVectors_;
            }

            SummaryConfig& merge(const SummaryConfig&);
            SummaryConfig& merge(SummaryConfig&&);

            keyword_list
            registerRequisiteUDQorActionSummaryKeys(const std::vector<std::string>& extraKeys,
                                                    const EclipseState&             es,
                                                    const Schedule&                 sched);

            // The hasKeyword() method will consult the internal set
            // 'short_keywords', i.e. the query should be based on pure
            // keywords like 'WWCT' and 'BPR' - and *not* fully identifiers
            // like 'WWCT:OPX' and 'BPR:10,12,3'.
            bool hasKeyword(const std::string& keyword ) const;

            // Will check if the SummaryConfig object contains any keyword
            // matching the pattern argument. The matching is performed as
            // if by the Posix function fnmatch().
            bool match(const std::string& keywordPattern) const;

            keyword_list keywords(const std::string& keywordPattern) const;

            // The hasSummaryKey() method will look for fully qualified keys
            // like 'RPR:3' and 'BPR:10,15,20.
            bool hasSummaryKey(const std::string& keyword ) const;

            // Can be used to query if a certain 3D field, e.g. PRESSURE, is
            // required to calculate the summary variables.
            bool require3DField(const std::string& keyword) const;

            std::set<std::string> fip_regions() const;
            std::set<std::string> fip_regions_interreg_flow() const;

            bool operator==(const SummaryConfig& data) const;

            template<class Serializer>
            void serializeOp(Serializer& serializer)
            {
               serializer(m_keywords);
               serializer(short_keywords);
               serializer(summary_keywords);
            }

            bool createRunSummary() const
            { return runSummaryConfig.create; }

            const SummaryConfigNode& operator[](std::size_t index) const;

        private:
            SummaryConfig(const Deck& deck,
                          const Schedule& schedule,
                          const FieldPropsManager& field_props,
                          const AquiferConfig& aquiferConfig,
                          const ParseContext& parseContext,
                          ErrorGuard& errors,
                          const GridDims& dims);

            keyword_list m_keywords{};
            keyword_list extraFracturingVectors_{};

            // The short_keywords set contains only the pure keyword part,
            // e.g. "WWCT", and not the qualification with well/group name
            // or a numerical value.
            std::set<std::string> short_keywords;
            std::set<std::string> summary_keywords;

            struct {
                bool create { false };
                bool narrow { false };
                bool separate { true };
            } runSummaryConfig;

            void handleProcessingInstruction(const std::string& keyword);
    };

} // namespace Opm

#endif // OPM_SUMMARY_CONFIG_HPP
