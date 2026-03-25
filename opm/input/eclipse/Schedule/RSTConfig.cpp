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

#include <opm/input/eclipse/Schedule/RSTConfig.hpp>

#include <opm/common/utility/OpmInputError.hpp>

#include <opm/input/eclipse/Schedule/RPTKeywordNormalisation.hpp>
#include <opm/input/eclipse/Schedule/RptschedKeywordNormalisation.hpp>

#include <opm/input/eclipse/Utility/Functional.hpp>

#include <opm/input/eclipse/Deck/DeckKeyword.hpp>
#include <opm/input/eclipse/Deck/DeckSection.hpp>

#include <opm/input/eclipse/Parser/ErrorGuard.hpp>
#include <opm/input/eclipse/Parser/ParseContext.hpp>
#include <opm/input/eclipse/Parser/ParserKeywords/R.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <fmt/format.h>

namespace {

    /// Translate sequence of RPTRST integer controls to RPTRST mnemonics.
    class RptRstIntegerControlHandler
    {
    public:
        /// Default constructor.
        RptRstIntegerControlHandler();

        /// Translate integer controls to mnemonics.
        ///
        /// \param[in] controlValues RPTRST integer control values.
        ///
        /// \return Normalised RPTRST mnemonics and associate mnemonic
        /// values.
        Opm::RPTKeywordNormalisation::MnemonicMap
        operator()(const std::vector<int>& controlValues) const;

    private:
        /// Mnemonic strings for RPTRST integer controls.
        std::vector<std::string> keywords_{};
    };

    RptRstIntegerControlHandler::RptRstIntegerControlHandler()
        : keywords_ {
                "BASIC",      //  1
                "FLOWS",      //  2
                "FIP",        //  3
                "POT",        //  4
                "PBPD",       //  5
                "FREQ",       //  6
                "PRES",       //  7
                "VISC",       //  8
                "DEN",        //  9
                "DRAIN",      // 10
                "KRO",        // 11
                "KRW",        // 12
                "KRG",        // 13
                "PORO",       // 14
                "NOGRAD",     // 15
                "NORST",      // 16 NORST - not supported
                "SAVE",       // 17
                "SFREQ",      // 18 SFREQ=?? - not supported
                "ALLPROPS",   // 19
                "ROCKC",      // 20
                "SGTRAP",     // 21
                "",           // 22 - Blank - ignored.
                "RSSAT",      // 23
                "RVSAT",      // 24
                "GIMULT",     // 25
                "SURFBLK",    // 26
                "",           // 27 - PCOW, PCOG, special cased
                "STREAM",     // 28 STREAM=?? - not supported
                "RK",         // 29
                "VELOCITY",   // 30
                "COMPRESS",   // 31
            }
    {}

    Opm::RPTKeywordNormalisation::MnemonicMap
    RptRstIntegerControlHandler::
    operator()(const std::vector<int>& controlValues) const
    {
        auto mnemonics = Opm::RPTKeywordNormalisation::MnemonicMap{};

        const auto PCO_index   = std::vector<int>::size_type{26};
        const auto BASIC_index = std::vector<int>::size_type{ 0};
        const auto numValues   =
            std::min(controlValues.size(), this->keywords_.size());

        if ((numValues > BASIC_index) &&
            ((numValues >= PCO_index) || (controlValues[BASIC_index] != 0)))
        {
            // Special case.  Leave BASIC untouched if number of control
            // values is small and the control value itself is zero.
            //
            // Needed for RestartConfigTests.cpp:RPTSCHED_INTEGER2.
            mnemonics.emplace_back(this->keywords_[BASIC_index],
                                   controlValues  [BASIC_index]);
        }

        for (auto i = BASIC_index + 1; i < std::min(PCO_index, numValues); ++i) {
            mnemonics.emplace_back(this->keywords_[i], controlValues[i]);
        }

        // Item 27 (index 26) sets both PCOW and PCOG, so we special case it
        // here.
        if (numValues > PCO_index) {
            mnemonics.emplace_back("PCOW", controlValues[PCO_index]);
            mnemonics.emplace_back("PCOG", controlValues[PCO_index]);
        }

        for (auto i = PCO_index + 1; i < numValues; ++i) {
            mnemonics.emplace_back(this->keywords_[i], controlValues[i]);
        }

        return mnemonics;
    }

    // -----------------------------------------------------------------------

    std::vector<std::string> rptRstBaseMnemonics()
    {
        return {
            "ACIP",     "ACIS",     "ALLPROPS", "BASIC",    "BG",      "BO",
            "BW",       "CELLINDX", "COMPRESS", "CONV",     "DEN",     "DENG",
            "DENO",     "DENW",     "DRAIN",    "DRAINAGE", "DYNREG",  "FIP",
            "FLORES",   "FLORES-",  "FLOWS",    "FLOWS-",   "FREQ",    "GIMULT",
            "HYDH",     "HYDHFW",   "KRG",      "KRO",      "KRW",     "NOGRAD",
            "NORST",    "NPMREB",   "PBPD",     "PCGW",     "PCOG",    "PCOW",
            "PERMREDN", "POIS",     "PORO",     "PORV",     "POT",     "PRES",
            "RESIDUAL", "RFIP",     "RK",       "ROCKC",    "RPORV",   "RSSAT",
            "RSWSAT",   "RVSAT",    "RVWSAT",   "SAVE",     "SDENO",   "SFIP",
            "SFREQ",    "SGTRAP",   "SIGM_MOD", "STREAM",   "SURFBLK", "TEMP",
            "TRAS",     "VELGAS",   "VELOCITY", "VELOIL",   "VELWAT",  "VGAS",
            "VISC",     "VOIL",     "VWAT",
        };
    }

    std::vector<std::string> rptRstCompositionalMnemonics()
    {
        return {
            "AIM",      "ALSTML",   "ALSURF",   "AMF",      "AQPH",    "AQSP",
            "AREAC",    "ASPADS",   "ASPDOT",   "ASPENT",   "ASPFLO",  "ASPFLT",
            "ASPFRD",   "ASPKDM",   "ASPLIM",   "ASPLUG",   "ASPRET",  "ASPREW",
            "ASPVEL",   "ASPVOM",   "BASIC",    "BFORO",    "BG",      "BGAS",
            "BO",       "BOIL",     "BSOL",     "BTFORG",   "BTFORO",  "BW",
            "BWAT",     "CELLINDX", "CFL",      "CGAS",     "COILR",   "COLR",
            "CONV",     "DENG",     "DENO",     "DENS",     "DENW",    "DYNREG",
            "ENERGY",   "ESALTP",   "ESALTS",   "FFACTG",   "FFACTO",  "FFORO",
            "FIP",      "FLOE",     "FLOGAS",   "FLOOIL",   "FLORES",  "FLORES-",
            "FLOWAT",   "FMISC",    "FOAM",     "FOAMCNM",  "FOAMMOB", "FOAMST",
            "FPC",      "FREQ",     "FUGG",     "FUGO",     "GASPOT",  "HGAS",
            "HOIL",     "HSOL",     "HWAT",     "JV",       "KRG",     "KRGDM",
            "KRO",      "KRODM",    "KRW",      "KRWDM",    "LGLCHC",  "LGLCWAT",
            "MLSC",     "MWAT",     "NCNG",     "NCNO",     "NPMREB",  "OILPOT",
            "PART",     "PCGW",     "PCOG",     "PCOW",     "PERM_MDX","PERM_MDY",
            "PERM_MDZ", "PERM_MOD", "PGAS",     "PKRG",     "PKRGR",   "PKRO",
            "PKRORG",   "PKRORW",   "PKRW",     "PKRWR",    "POIL",    "POLY",
            "POLYVM",   "PORV",     "PORV_MOD", "PPCG",     "PPCW",    "PRES",
            "PRESMIN",  "PRESSURE", "PRES_EFF", "PSAT",     "PSGCR",   "PSGL",
            "PSGU",     "PSOGCR",   "PSOWCR",   "PSWCR",    "PSWL",    "PSWU",
            "PVDPH",    "PWAT",     "RATP",     "RATS",     "RATT",    "REAC",
            "RESTART",  "RFIP",     "ROCKC",    "ROMLS",    "RPORV",   "RS",
            "RSSAT",    "RSW",      "RV",       "RVSAT",    "SFIP",    "SFIPGAS",
            "SFIPOIL",  "SFIPWAT",  "SFOIL",    "SFSOL",    "SGAS",    "SGASMAX",
            "SGCRH",    "SGTRAP",   "SGTRH",    "SIGM_MOD", "SMF",     "SMMULT",
            "SOIL",     "SOILM",    "SOILMAX",  "SOILR",    "SOLADS",  "SOLADW",
            "SOLWET",   "SSFRAC",   "SSOLID",   "STATE",    "STEN",    "SUBG",
            "SURF",     "SURFCNM",  "SURFCP",   "SURFKR",   "SURFST",  "SWAT",
            "SWATMIN",  "TCBULK",   "TCMULT",   "TEMP",     "TOTCOMP", "TREACM",
            "TSUB",     "VGAS",     "VMF",      "VOIL",     "VWAT",    "WATPOT",
            "XFW",      "XGAS",     "XMF",      "XWAT",     "YFW",     "YMF",
            "ZMF",
        };
    }

    class IsRptRstSchedMnemonic
    {
    public:
        explicit IsRptRstSchedMnemonic(const bool isCompositional)
            : mnemonics_ { isCompositional
                           ? rptRstCompositionalMnemonics()
                           : rptRstBaseMnemonics() }
        {
            std::ranges::sort(mnemonics_);
        }

        bool operator()(const std::string& mnemonic) const
        {
            return std::ranges::binary_search(this->mnemonics_, mnemonic);
        }

    private:
        std::vector<std::string> mnemonics_{};
    };

    // -----------------------------------------------------------------------

    void expand_RPTRST_mnemonics(std::map<std::string, int, std::less<>>& mnemonics)
    {
        auto allprops_iter = mnemonics.find("ALLPROPS");
        if (allprops_iter == mnemonics.end()) {
            return;
        }

        const auto value = allprops_iter->second;
        mnemonics.erase(allprops_iter);

        for (const auto& kw : {
                "BG"  , "BO"  , "BW"  ,
                "KRG" , "KRO" , "KRW" ,
                "VOIL", "VGAS", "VWAT",
                "DEN" ,
            })
        {
            mnemonics.insert_or_assign(kw, value);
        }
    }

    std::optional<int>
    extract(std::string_view                         key,
            std::map<std::string, int, std::less<>>& mnemonics)
    {
        auto iter = mnemonics.find(key);
        if (iter == mnemonics.end()) {
            return {};
        }

        const auto value = iter->second;
        mnemonics.erase(iter);

        return value;
    }

    std::map<std::string, int, std::less<>>
    asMap(const std::vector<std::pair<std::string, int>>& mnemonic_list)
    {
        return { mnemonic_list.begin(), mnemonic_list.end() };
    }

    std::tuple<std::map<std::string, int, std::less<>>, std::optional<int>, std::optional<int>>
    RPTRST(const Opm::DeckKeyword&  keyword,
           const Opm::ParseContext& parseContext,
           Opm::ErrorGuard&         errors,
           const bool               compositional = false)
    {
        auto mnemonic_list = Opm::RPTKeywordNormalisation {
            RptRstIntegerControlHandler{},
            IsRptRstSchedMnemonic {compositional}
        }.normaliseKeyword(keyword, parseContext, errors);

        auto mnemonics = asMap(mnemonic_list);

        const auto basic = extract("BASIC", mnemonics);
        const auto freq  = extract("FREQ" , mnemonics);

        expand_RPTRST_mnemonics(mnemonics);

        return { mnemonics, basic, freq };
    }

    template <typename T>
    void update_optional(const std::optional<T>& src,
                         std::optional<T>&       target)
    {
        if (src.has_value()) {
            target = src;
        }
    }

} // Anonymous namespace

// ---------------------------------------------------------------------------

namespace Opm {

RSTConfig::RSTConfig(const SOLUTIONSection& solution_section,
                     const ParseContext&    parseContext,
                     const bool             compositional_arg,
                     ErrorGuard&            errors)
    : write_rst_file_ { true }
    , compositional_  { compositional_arg }
{
    for (const auto& keyword : solution_section) {
        if (keyword.name() == ParserKeywords::RPTRST::keywordName) {
            this->handleRPTRSTSOLUTION(keyword, parseContext, errors);
        }
        else if (keyword.name() == ParserKeywords::RPTSOL::keywordName) {
            this->handleRPTSOL(keyword, parseContext, errors);
        }
    }
}

void RSTConfig::update(const DeckKeyword&  keyword,
                       const ParseContext& parseContext,
                       ErrorGuard&         errors)
{
    if (keyword.name() == ParserKeywords::RPTRST::keywordName) {
        this->handleRPTRST(keyword, parseContext, errors);
    }
    else if (keyword.name() == ParserKeywords::RPTSCHED::keywordName) {
        this->handleRPTSCHED(keyword, parseContext, errors);
    }
    else {
        throw std::logic_error("The RSTConfig object can only use RPTRST and RPTSCHED keywords");
    }
}

// The RPTRST keyword semantics differs between the SOLUTION and SCHEDULE
// sections.  This function assumes that the current object is constructed
// from SOLUTION section information and creates a transformed copy suitable
// as the first RSTConfig object in the SCHEDULE section.
RSTConfig RSTConfig::makeFirstStepConfig() const
{
    auto first = *this;

    first.solution_only_keywords_.clear();
    for (const auto& kw : this->solution_only_keywords_) {
        first.mnemonics_.erase(kw);
    }

    const auto basic = first.basic_;
    if (!basic.has_value()) {
        first.write_rst_file_ = false;
        return first;
    }

    const auto basic_value = basic.value();
    if (basic_value == 0) {
        first.write_rst_file_ = false;
    }
    else if ((basic_value == 1) || (basic_value == 2)) {
        first.write_rst_file_ = true;
    }
    else if (basic_value >= 3) {
        first.write_rst_file_.reset();
    }

    return first;
}

RSTConfig RSTConfig::serializationTestObject()
{
    RSTConfig rst_config;

    rst_config.basic_.emplace(10);
    rst_config.freq_.emplace(42);
    rst_config.write_rst_file_.emplace(true);
    rst_config.save_ = true;
    rst_config.store_ = true;
    rst_config.compositional_ = false;
    rst_config.mnemonics_ = {{"S1", 1}, {"S2", 2}};
    rst_config.solution_only_keywords_ = { "FIP" };

    return rst_config;
}

bool RSTConfig::operator==(const RSTConfig& other) const
{
    return (this->mnemonics_ == other.mnemonics_)
        && (this->write_rst_file_ == other.write_rst_file_)
        && (this->basic_ == other.basic_)
        && (this->freq_ == other.freq_)
        && (this->save_ == other.save_)
        && (this->store_ == other.store_)
        && (this->compositional_ == other.compositional_)
        && (this->solution_only_keywords_ == other.solution_only_keywords_)
        ;
}

bool RSTConfig::clearSaveStore()
{
    const auto was_updated = this->save_ || this->store_;

    this->save_ = this->store_ = false;

    return was_updated;
}

void RSTConfig::recordSaveEvent()
{
    this->save_  = true;
    this->store_ = false;
}

void RSTConfig::recordStoreEvent()
{
    this->save_  = false;
    this->store_ = true;
}

void RSTConfig::setMnemonicForTesting(const std::string& mnemonic,
                                      const int          value)
{
    this->mnemonics_.insert_or_assign(mnemonic, value);
}

std::optional<int> RSTConfig::getMnemonic(std::string_view mnemonic) const
{
    const auto mnemonicPos = this->mnemonics_.find(mnemonic);
    if (mnemonicPos == this->mnemonics_.end()) {
        return {};
    }

    return { mnemonicPos->second };
}

std::optional<RSTConfig::FileType>
RSTConfig::resultFileType(const ReportStepDescriptor& descr) const
{
    if (this->store_) {
        return { FileType::Store };
    }

    if (this->save_) {
        return { FileType::Save };
    }

    if (this->write_rst_file_.has_value() && *this->write_rst_file_) {
        return { FileType::Restart };
    }

    const auto basic = this->basic_.value_or(0);

    if ((basic == 0) || (basic > 5)) {
        // No output (0) or unsupported setting (6+)
        return {};
    }

    if ((basic == 1) || (basic == 2)) {
        // Restart file output at every report step.
        return { FileType::Restart };
    }

    if (basic == 3) {
        // Restart file output at every 'FREQ' report step.
        return this->shouldWriteResultFileFreq(descr.simStep);
    }

    const auto& [year_diff, month_diff] = descr.dateDiff();

    if (basic == 4) {
        // Restart file output at first report step of every 'FREQ'-ed year.
        return this->shouldWriteResultFileFreqDate
            (year_diff, descr.firstInYear);
    }

    // BASIC = 5.  Restart file output at first report step of every
    // 'FREQ'-ed month.
    return this->shouldWriteResultFileFreqDate
        (month_diff, descr.firstInMonth);
}

bool RSTConfig::writeSaveOrStoreFile() const
{
    return this->save_ || this->store_;
}

// ---------------------------------------------------------------------------

// Recall that handleRPTSOL() is private and invoked only from the
// RSTConfig constructor processing SOLUTION section information.

void RSTConfig::handleRPTSOL(const DeckKeyword&  keyword,
                             const ParseContext& parseContext,
                             ErrorGuard&         errors)
{
    // Note: We intentionally use RPTSCHED mnemonic handling here.  While
    // potentially misleading, this process does do what we want for the
    // typical cases.  Older style integer controls are however only
    // partially handled and we may choose to refine this logic by
    // introducing predicates specific to the RPTSOL keyword later.
    auto mnemonics =
        asMap(normaliseRptSchedKeyword(keyword, parseContext, errors));

    const auto restart = extract("RESTART", mnemonics);
    const auto request_restart =
        (restart.has_value() && (*restart > 1));

    this->write_rst_file_ =
        (this->write_rst_file_.has_value() && *this->write_rst_file_)
        || request_restart;

    if (request_restart) {
        // RPTSOL's RESTART flag is set.  Internalise new flags, as
        // "SOLUTION" only properties, from 'mnemonics'.
        this->mnemonics_.swap(mnemonics);
        for (const auto& kw : this->mnemonics_) {
            this->solution_only_keywords_.insert(kw.first);
        }

        for (const auto& [key, value] : mnemonics) {
            // Note: Using emplace() after swap() means that the mnemonics
            // from RPTSOL overwrite those that might already have been
            // present in 'mnemonics_'.  Recall that emplace() will not
            // insert a new element with the same key as an existing
            // element.
            this->mnemonics_.emplace(key, value);
        }
    }
}

void RSTConfig::handleRPTRST(const DeckKeyword&  keyword,
                             const ParseContext& parseContext,
                             ErrorGuard&         errors)
{
    const auto& [mnemonics, basic, freq] =
        RPTRST(keyword, parseContext, errors, this->compositional_);

    this->update_schedule(basic, freq);

    for (const auto& [kw, num] : mnemonics) {
        // Insert_or_assign() to overwrite existing 'kw' elements.
        this->mnemonics_.insert_or_assign(kw, num);
    }
}

void RSTConfig::handleRPTRSTSOLUTION(const DeckKeyword&  keyword,
                                     const ParseContext& parseContext,
                                     ErrorGuard&         errors)
{
    const auto& [mnemonics, basic, freq] =
        RPTRST(keyword, parseContext, errors, this->compositional_);

    update_optional(basic, this->basic_);
    update_optional(freq , this->freq_ );

    for (const auto& [kw, num] : mnemonics) {
        // Insert_or_assign() to overwrite existing 'kw' elements.
        this->mnemonics_.insert_or_assign(kw, num);
    }

    // We're processing RPTRST in the SOLUTION section.  Mnemonics from
    // RPTRST should persist beyond the SOLUTION section in this case so
    // prune these from the list of solution-only keywords.
    for (const auto& kw : mnemonics) {
        this->solution_only_keywords_.erase(kw.first);
    }

    if (this->basic_.has_value() && (*this->basic_ == 0)) {
        this->write_rst_file_ = false;
    }
}

void RSTConfig::handleRPTSCHED(const DeckKeyword&  keyword,
                               const ParseContext& parseContext,
                               ErrorGuard&         errors)
{
    auto mnemonic_list = normaliseRptSchedKeyword(keyword, parseContext, errors);

    auto nothingPos = std::ranges::find_if(mnemonic_list,
                                           [](const auto& mnemonicPair)
                                           { return mnemonicPair.first == "NOTHING"; });

    if (nothingPos != mnemonic_list.end()) {
        this->basic_.reset();
        this->mnemonics_.clear();

        mnemonic_list.erase(mnemonic_list.begin(), nothingPos + 1);
    }

    auto mnemonics = asMap(mnemonic_list);

    if (this->basic_.value_or(2) <= 2) {
        const auto restart = extract("RESTART", mnemonics);

        if (restart.has_value()) {
            const auto basic_value = std::min(2, restart.value());
            this->update_schedule(std::make_optional(basic_value),
                                  std::make_optional(1));
        }
    }

    for (const auto& [kw, num] : mnemonics) {
        this->mnemonics_.insert_or_assign(kw, num);
    }
}

void RSTConfig::update_schedule(const std::optional<int>& basic,
                                const std::optional<int>& freq)
{
    update_optional(basic, this->basic_);
    update_optional(freq , this->freq_ );

    if (this->basic_.has_value()) {
        const auto basic_value = *this->basic_;

        if (basic_value == 0) {
            this->write_rst_file_ = false;
        }
        else if ((basic_value == 1) || (basic_value == 2)) {
            this->write_rst_file_ = true;
        }
        else {
            this->write_rst_file_.reset();
        }
    }
}

std::optional<RSTConfig::FileType>
RSTConfig::shouldWriteResultFileFreq(const std::size_t count) const
{
    const auto freq = this->freq_.value_or(1);

    if ((freq > 0) && (count % freq == 0)) {
        return { FileType::Restart };
    }

    return {};
}

std::optional<RSTConfig::FileType>
RSTConfig::shouldWriteResultFileFreqDate(const std::size_t count,
                                         const bool        is_first) const
{
    const auto freq = static_cast<std::size_t>
        (this->freq_.value_or(1));

    if ((freq > 0) && is_first && (count >= freq)) {
        return { FileType::Restart };
    }

    return {};
}

} // namespace Opm
