/*
  Copyright (c) 2025 Equinor ASA

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

#include "config.h"

#define BOOST_TEST_MODULE StoreIO

#include <boost/test/unit_test.hpp>

#include <opm/output/eclipse/StoreIO.hpp>
#include <opm/output/eclipse/RestartIO.hpp>
#include <opm/output/eclipse/RestartValue.hpp>
#include <opm/output/eclipse/AggregateAquiferData.hpp>

#include <opm/io/eclipse/OutputStream.hpp>
#include <opm/io/eclipse/ERst.hpp>
#include <opm/io/eclipse/EclFile.hpp>
#include <opm/io/eclipse/EclIOdata.hpp>
#include <opm/io/eclipse/PaddedOutputString.hpp>

#include <opm/input/eclipse/Deck/Deck.hpp>
#include <opm/input/eclipse/EclipseState/EclipseState.hpp>
#include <opm/input/eclipse/EclipseState/Grid/EclipseGrid.hpp>
#include <opm/input/eclipse/EclipseState/SummaryConfig/SummaryConfig.hpp>

#include <opm/input/eclipse/Python/Python.hpp>

#include <opm/input/eclipse/Schedule/Action/State.hpp>
#include <opm/input/eclipse/Schedule/MSW/SegmentMatcher.hpp>
#include <opm/input/eclipse/Schedule/Schedule.hpp>
#include <opm/input/eclipse/Schedule/ScheduleState.hpp>
#include <opm/input/eclipse/Schedule/SummaryState.hpp>
#include <opm/input/eclipse/Schedule/UDQ/UDQConfig.hpp>
#include <opm/input/eclipse/Schedule/UDQ/UDQState.hpp>
#include <opm/input/eclipse/Schedule/Well/WellTestState.hpp>

#include <opm/input/eclipse/EclipseState/Grid/FIPRegionStatistics.hpp>
#include <opm/input/eclipse/EclipseState/Grid/RegionSetMatcher.hpp>

#include <opm/input/eclipse/Parser/Parser.hpp>

#include <opm/output/data/Cells.hpp>
#include <opm/output/data/Groups.hpp>
#include <opm/output/data/Wells.hpp>

#include <opm/common/utility/TimeService.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <opm/input/eclipse/Schedule/Well/WellMatcher.hpp>

#include <opm/common/utility/FileSystem.hpp>
#include <tests/WorkArea.hpp>

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

template <class Coll>
void check_is_close(const Coll& c1, const Coll& c2, double tol = 1.0e-7)
{
    using ElmType = typename std::remove_cv<
        typename std::remove_reference<
            typename std::iterator_traits<
                decltype(std::begin(c1))
            >::value_type
        >::type
    >::type;

    BOOST_REQUIRE_EQUAL(c1.size(), c2.size());
    for (auto b1 = c1.begin(), e1 = c1.end(), b2 = c2.begin(); b1 != e1; ++b1, ++b2) {
        BOOST_CHECK_CLOSE(*b1, *b2, static_cast<ElmType>(tol));
    }
}

// ---------------------------------------------------------------------------
// Temporary result-set directory manager
// ---------------------------------------------------------------------------

class RSet
{
public:
    explicit RSet(std::string base)
        : odir_(std::filesystem::temp_directory_path() /
                Opm::unique_path("store-rset-%%%%"))
        , base_(std::move(base))
    {
        std::filesystem::create_directories(this->odir_);
    }

    ~RSet()
    {
        std::filesystem::remove_all(this->odir_);
    }

    operator ::Opm::EclIO::OutputStream::ResultSet() const
    {
        return { this->odir_.string(), this->base_ };
    }

    std::string dir() const { return this->odir_.string(); }
    const std::string& base() const { return this->base_; }

private:
    std::filesystem::path odir_;
    std::string           base_;
};

// ---------------------------------------------------------------------------
// Minimal simulation fixtures (mirrors test_Restart.cpp helpers)
// ---------------------------------------------------------------------------

Opm::data::GroupAndNetworkValues mkGroups() { return {}; }

Opm::data::Wells mkWells()
{
    Opm::data::Rates r1, r2, rc1, rc2, rc3;
    r1.set(Opm::data::Rates::opt::wat, 5.67);
    r1.set(Opm::data::Rates::opt::oil, 6.78);
    r1.set(Opm::data::Rates::opt::gas, 7.89);

    r2.set(Opm::data::Rates::opt::wat, 8.90);
    r2.set(Opm::data::Rates::opt::oil, 9.01);
    r2.set(Opm::data::Rates::opt::gas, 10.12);

    rc1.set(Opm::data::Rates::opt::wat, 20.41);
    rc1.set(Opm::data::Rates::opt::oil, 21.19);
    rc1.set(Opm::data::Rates::opt::gas, 22.41);

    rc2.set(Opm::data::Rates::opt::wat, 23.19);
    rc2.set(Opm::data::Rates::opt::oil, 24.41);
    rc2.set(Opm::data::Rates::opt::gas, 25.19);

    rc3.set(Opm::data::Rates::opt::wat, 26.41);
    rc3.set(Opm::data::Rates::opt::oil, 27.19);
    rc3.set(Opm::data::Rates::opt::gas, 28.41);

    Opm::data::Well w1, w2;
    w1.rates = r1;
    w1.thp   = 1.0;
    w1.bhp   = 1.23;
    w1.temperature = 3.45;
    w1.control = 1;

    const auto lgr_grid = 0;
    Opm::data::ConnectionFiltrate cf{ 0.1, 1, 3, 0.4, 1.e-9, 0.2, 0.05, 10.0 };

    w1.connections.push_back({  88, rc1, 30.45, 123.4, 543.21, 0.62, 0.15, 1.0e3,  1.234, 0.0, 1.23, lgr_grid, cf });
    w1.connections.push_back({ 288, rc2, 33.19, 123.4, 432.1,  0.26, 0.45, 2.56,   2.345, 0.0, 0.98, lgr_grid, cf });

    w2.rates = r2;
    w2.thp   = 2.0;
    w2.bhp   = 2.34;
    w2.temperature = 4.56;
    w2.control = 2;
    w2.connections.push_back({ 188, rc3, 36.22, 123.4, 256.1,  0.55, 0.0125, 314.15, 3.456, 0.0, 2.46, lgr_grid, cf });

    Opm::data::Wells wellRates;
    wellRates["OP_1"] = w1;
    wellRates["OP_2"] = w2;

    return wellRates;
}

Opm::data::Solution mkSolution(int numCells)
{
    using Opm::UnitSystem;
    using Opm::data::CellData;
    using Opm::data::TargetType;
    using measure = UnitSystem::measure;

    auto sol = Opm::data::Solution {
        { "PRESSURE", CellData{ measure::pressure,    {}, TargetType::RESTART_SOLUTION } },
        { "SWAT",     CellData{ measure::identity,    {}, TargetType::RESTART_SOLUTION } },
        { "SGAS",     CellData{ measure::identity,    {}, TargetType::RESTART_SOLUTION } },
    };

    sol.data<double>("PRESSURE").assign(numCells, 150.0);
    sol.data<double>("SWAT").assign(numCells, 0.3);
    sol.data<double>("SGAS").assign(numCells, 0.2);

    return sol;
}

Opm::SummaryState makeSummaryState(const Opm::Schedule& sched)
{
    auto st = Opm::SummaryState{
        Opm::TimeService::now(),
        sched.back().udq().params().undefinedValue()
    };

    st.update_well_var("OP_1", "WOPR", 1.0);
    st.update_well_var("OP_1", "WWPR", 2.0);
    st.update_well_var("OP_1", "WBHP", 314.15);
    st.update_well_var("OP_2", "WWIR", 100.0);
    st.update_well_var("OP_2", "WBHP", 400.6);
    st.update_well_var("OP_3", "WOPR", 11.0);
    st.update_well_var("OP_3", "WBHP", 314.15);

    st.update_group_var("OP",    "GOPR", 110.0);
    st.update_group_var("FIELD", "FOPR", 1100.0);

    return st;
}

struct Setup
{
    Opm::EclipseState  es;
    const Opm::EclipseGrid& grid;
    Opm::Schedule      schedule;
    Opm::SummaryConfig summary_config;

    explicit Setup(const char* path)
        : Setup{ Opm::Parser{}.parseFile(path) }
    {}

    explicit Setup(const Opm::Deck& deck)
        : es             { deck }
        , grid           { es.getInputGrid() }
        , schedule       { deck, es, std::make_shared<Opm::Python>() }
        , summary_config { deck, schedule, es.fieldProps(), es.aquifer() }
    {
        es.getIOConfig().setEclCompatibleRST(false);
    }
};

} // anonymous namespace

// ===========================================================================
// Suite 1: OutputStream::Store stream-level tests
// ===========================================================================

BOOST_AUTO_TEST_SUITE(Class_Store)

BOOST_AUTO_TEST_CASE(Extension_Unformatted)
{
    const auto rset = RSet("CASE");
    const auto fname = Opm::EclIO::OutputStream::
        outputFileName(rset, "STORE");

    BOOST_CHECK(fname.ends_with(".STORE"));
}

BOOST_AUTO_TEST_CASE(Extension_Formatted)
{
    const auto rset = RSet("CASE");
    const auto fname = Opm::EclIO::OutputStream::
        outputFileName(rset, "FSTORE");

    BOOST_CHECK(fname.ends_with(".FSTORE"));
}

BOOST_AUTO_TEST_CASE(Unified_Unformatted_Creates_STORE_File)
{
    namespace OS = Opm::EclIO::OutputStream;

    const auto rset = RSet("CASE");
    const auto fmt  = OS::Formatted{ false };

    {
        auto store = OS::Store{ rset, 1, fmt };
        store.write("I", std::vector<int>   {1, 7, 2, 9});
        store.write("L", std::vector<bool>  {true, false, false, true});
        store.write("S", std::vector<float> {3.1f, 4.1f, 59.265f});
        store.write("D", std::vector<double>{2.71, 8.21});
    }

    const auto fname = OS::outputFileName(rset, "STORE");
    BOOST_CHECK(std::filesystem::exists(fname));

    auto rst = Opm::EclIO::ERst{ fname };
    rst.loadReportStepNumber(1);

    BOOST_CHECK(rst.hasReportStepNumber(1));

    const auto& I = rst.getRestartData<int>  ("I", 1, 0);
    const auto  expect_I = std::vector<int>{ 1, 7, 2, 9 };
    BOOST_CHECK_EQUAL_COLLECTIONS(I.begin(), I.end(),
                                  expect_I.begin(), expect_I.end());

    const auto& S = rst.getRestartData<float>("S", 1, 0);
    const auto  expect_S = std::vector<float>{ 3.1f, 4.1f, 59.265f };
    check_is_close(S, expect_S);

    const auto& D = rst.getRestartData<double>("D", 1, 0);
    const auto  expect_D = std::vector<double>{ 2.71, 8.21 };
    check_is_close(D, expect_D);
}

BOOST_AUTO_TEST_CASE(Unified_Formatted_Creates_FSTORE_File)
{
    namespace OS = Opm::EclIO::OutputStream;

    const auto rset = RSet("CASE");
    const auto fmt  = OS::Formatted{ true };

    {
        auto store = OS::Store{ rset, 1, fmt };
        store.write("I", std::vector<int>{ 42 });
    }

    const auto fname = OS::outputFileName(rset, "FSTORE");
    BOOST_CHECK(std::filesystem::exists(fname));
    BOOST_CHECK(!std::filesystem::exists(OS::outputFileName(rset, "STORE")));
}

BOOST_AUTO_TEST_CASE(SEQNUM_Is_Written)
{
    namespace OS = Opm::EclIO::OutputStream;

    const auto rset   = RSet("CASE");
    const auto fmt    = OS::Formatted{ false };
    const auto seqnum = 7;

    {
        auto store = OS::Store{ rset, seqnum, fmt };
        store.write("I", std::vector<int>{ 1, 2, 3 });
    }

    const auto fname = OS::outputFileName(rset, "STORE");
    auto rst = Opm::EclIO::ERst{ fname };

    BOOST_CHECK(rst.hasReportStepNumber(seqnum));

    const auto steps        = rst.listOfReportStepNumbers();
    const auto expect_steps = std::vector<int>{ seqnum };
    BOOST_CHECK_EQUAL_COLLECTIONS(steps.begin(), steps.end(),
                                  expect_steps.begin(), expect_steps.end());
}

BOOST_AUTO_TEST_CASE(Multi_Step_Append)
{
    namespace OS = Opm::EclIO::OutputStream;

    const auto rset = RSet("CASE");
    const auto fmt  = OS::Formatted{ false };

    for (const int seqnum : {1, 5, 10}) {
        auto store = OS::Store{ rset, seqnum, fmt };
        store.write("I", std::vector<int>{ seqnum * 10 });
    }

    const auto fname = OS::outputFileName(rset, "STORE");
    auto rst = Opm::EclIO::ERst{ fname };

    BOOST_CHECK(rst.hasReportStepNumber( 1));
    BOOST_CHECK(rst.hasReportStepNumber( 5));
    BOOST_CHECK(rst.hasReportStepNumber(10));

    const auto steps        = rst.listOfReportStepNumbers();
    const auto expect_steps = std::vector<int>{ 1, 5, 10 };
    BOOST_CHECK_EQUAL_COLLECTIONS(steps.begin(), steps.end(),
                                  expect_steps.begin(), expect_steps.end());

    rst.loadReportStepNumber(5);
    const auto& I5        = rst.getRestartData<int>("I", 5, 0);
    const auto  expect_I5 = std::vector<int>{ 50 };
    BOOST_CHECK_EQUAL_COLLECTIONS(I5.begin(), I5.end(),
                                  expect_I5.begin(), expect_I5.end());
}

BOOST_AUTO_TEST_CASE(Overwrite_Truncates_Later_Steps)
{
    namespace OS = Opm::EclIO::OutputStream;

    const auto rset = RSet("CASE");
    const auto fmt  = OS::Formatted{ false };

    // Write steps 1, 5, 10
    for (const int seqnum : {1, 5, 10}) {
        auto store = OS::Store{ rset, seqnum, fmt };
        store.write("I", std::vector<int>{ seqnum * 10 });
    }

    // Overwrite step 5: should discard step 10
    {
        auto store = OS::Store{ rset, 5, fmt };
        store.write("I", std::vector<int>{ 999 });
    }

    const auto fname = OS::outputFileName(rset, "STORE");

    // Eager-load to avoid use-after-temp issues
    auto rst = Opm::EclIO::ERst{ fname };

    BOOST_CHECK( rst.hasReportStepNumber( 1));
    BOOST_CHECK( rst.hasReportStepNumber( 5));
    BOOST_CHECK(!rst.hasReportStepNumber(10));

    rst.loadReportStepNumber(5);
    const auto& I5        = rst.getRestartData<int>("I", 5, 0);
    const auto  expect_I5 = std::vector<int>{ 999 };
    BOOST_CHECK_EQUAL_COLLECTIONS(I5.begin(), I5.end(),
                                  expect_I5.begin(), expect_I5.end());
}

BOOST_AUTO_TEST_CASE(Float_Double_Endianness_RoundTrip)
{
    namespace OS = Opm::EclIO::OutputStream;

    const auto rset = RSet("CASE");
    const auto fmt  = OS::Formatted{ false };

    const auto float_vals  = std::vector<float> { 1.23456789e10f, -9.8765e-4f, 3.14159265f };
    const auto double_vals = std::vector<double>{ 1.23456789012345e15, -9.87654321e-10, 2.71828182845904 };

    {
        auto store = OS::Store{ rset, 1, fmt };
        store.write("SF", float_vals);
        store.write("DF", double_vals);
    }

    const auto fname = OS::outputFileName(rset, "STORE");
    auto rst = Opm::EclIO::ERst{ fname };
    rst.loadReportStepNumber(1);

    check_is_close(rst.getRestartData<float> ("SF", 1, 0), float_vals);
    check_is_close(rst.getRestartData<double>("DF", 1, 0), double_vals);
}

BOOST_AUTO_TEST_CASE(Message_Is_Written)
{
    namespace OS = Opm::EclIO::OutputStream;

    const auto rset = RSet("CASE");
    const auto fmt  = OS::Formatted{ false };

    {
        auto store = OS::Store{ rset, 1, fmt };
        store.message("STARTSOL");
        store.write("I", std::vector<int>{ 42 });
        store.message("ENDSOL");
    }

    const auto fname = OS::outputFileName(rset, "STORE");
    auto rst = Opm::EclIO::ERst{ fname };

    const auto arrays        = rst.listOfRstArrays(1);
    const auto has_startsol  = std::ranges::any_of(arrays,
        [](const auto& e) { return std::get<0>(e) == "STARTSOL"; });
    const auto has_endsol    = std::ranges::any_of(arrays,
        [](const auto& e) { return std::get<0>(e) == "ENDSOL"; });

    BOOST_CHECK_MESSAGE(has_startsol, "Expected STARTSOL message in STORE file");
    BOOST_CHECK_MESSAGE(has_endsol,   "Expected ENDSOL message in STORE file");
}

BOOST_AUTO_TEST_SUITE_END() // Class_Store

// ===========================================================================
// Suite 2: StoreIO save/load round-trip tests
// ===========================================================================

BOOST_AUTO_TEST_SUITE(StoreIO_RoundTrip)

BOOST_AUTO_TEST_CASE(Save_Creates_STORE_File_With_Restart_Data)
{
    namespace OS = Opm::EclIO::OutputStream;

    WorkArea work("test_StoreIO_save");
    work.copyIn("BASE_SIM.DATA");

    Setup setup("BASE_SIM.DATA");

    const auto outputDir = work.currentWorkingDirectory();
    const auto numCells  = setup.grid.getNumActive();

    auto cells     = mkSolution(numCells);
    auto wells     = mkWells();
    auto groups    = mkGroups();
    auto sumState  = makeSummaryState(setup.schedule);
    auto udqState  = Opm::UDQState{ 1 };
    auto aquiferData = std::optional<Opm::RestartIO::Helpers::AggregateAquiferData>{ std::nullopt };

    Opm::Action::State   action_state;
    Opm::WellTestState   wtest_state;

    const int report_step = 1;

    {
        Opm::RestartValue restart_value(cells, wells, groups, {});

        auto& sched      = setup.schedule;
        auto& udqConfig  = sched.getUDQConfig(report_step);
        auto segFac = []() {
            return std::make_unique<Opm::SegmentMatcher>(Opm::ScheduleState{});
        };
        auto regFac = []() {
            return std::make_unique<Opm::RegionSetMatcher>(Opm::FIPRegionStatistics{});
        };
        udqConfig.eval(report_step,
                       sched.wellMatcher(report_step),
                       sched[report_step].group_order(),
                       segFac, regFac,
                       sumState, udqState);

        const auto seqnum = report_step;
        auto storeFile = OS::Store{
            OS::ResultSet{ outputDir, "STORE_CASE" },
            seqnum, OS::Formatted{ false }
        };

        Opm::StoreIO::save(storeFile, seqnum,
                           100.0,
                           restart_value,
                           setup.es,
                           setup.grid,
                           setup.schedule,
                           action_state,
                           wtest_state,
                           sumState,
                           udqState,
                           aquiferData,
                           /*write_double=*/false);
    }

    // Verify file was created with expected extension
    const auto storeFname = OS::outputFileName(
        { outputDir, "STORE_CASE" }, "STORE");

    BOOST_REQUIRE(std::filesystem::exists(storeFname));

    // Verify the ERst reader can access it
    auto rst = Opm::EclIO::ERst{ storeFname };
    BOOST_CHECK(rst.hasReportStepNumber(report_step));
    BOOST_CHECK(rst.hasKey("INTEHEAD"));
    BOOST_CHECK(rst.hasKey("SWAT"));
}

BOOST_AUTO_TEST_CASE(Load_Reads_Back_Solution)
{
    namespace OS = Opm::EclIO::OutputStream;

    WorkArea work("test_StoreIO_load");
    work.copyIn("BASE_SIM.DATA");

    Setup setup("BASE_SIM.DATA");

    const auto outputDir = work.currentWorkingDirectory();
    const auto numCells  = setup.grid.getNumActive();

    const auto pressure_val = 155.0;
    const auto swat_val     = 0.35;
    const auto sgas_val     = 0.25;

    // Build solution with known values
    auto cells = mkSolution(numCells);
    cells.data<double>("PRESSURE").assign(numCells, pressure_val);
    cells.data<double>("SWAT").assign(numCells, swat_val);
    cells.data<double>("SGAS").assign(numCells, sgas_val);

    auto wells       = mkWells();
    auto groups      = mkGroups();
    auto sumState    = makeSummaryState(setup.schedule);
    auto udqState    = Opm::UDQState{ 1 };
    auto aquiferData = std::optional<Opm::RestartIO::Helpers::AggregateAquiferData>{ std::nullopt };

    Opm::Action::State action_state;
    Opm::WellTestState wtest_state;

    const int report_step = 1;

    {
        Opm::RestartValue restart_value(cells, wells, groups, {});

        auto& sched     = setup.schedule;
        auto& udqConfig = sched.getUDQConfig(report_step);
        auto segFac = []() {
            return std::make_unique<Opm::SegmentMatcher>(Opm::ScheduleState{});
        };
        auto regFac = []() {
            return std::make_unique<Opm::RegionSetMatcher>(Opm::FIPRegionStatistics{});
        };
        udqConfig.eval(report_step,
                       sched.wellMatcher(report_step),
                       sched[report_step].group_order(),
                       segFac, regFac,
                       sumState, udqState);

        auto storeFile = OS::Store{
            OS::ResultSet{ outputDir, "STORE_CASE" },
            report_step, OS::Formatted{ false }
        };

        Opm::StoreIO::save(storeFile, report_step,
                           100.0, restart_value,
                           setup.es, setup.grid, setup.schedule,
                           action_state, wtest_state,
                           sumState, udqState, aquiferData,
                           /*write_double=*/true);
    }

    // Load back
    const auto storeFname = OS::outputFileName(
        { outputDir, "STORE_CASE" }, "STORE");

    const std::vector<Opm::RestartKey> solution_keys{
        { "PRESSURE", Opm::UnitSystem::measure::pressure },
        { "SWAT",     Opm::UnitSystem::measure::identity },
        { "SGAS",     Opm::UnitSystem::measure::identity },
    };

    auto load_sum = Opm::SummaryState{
        Opm::TimeService::now(),
        setup.schedule.back().udq().params().undefinedValue()
    };
    Opm::Action::State load_action;

    const auto rst_val = Opm::StoreIO::load(
        storeFname, report_step,
        load_action, load_sum,
        solution_keys,
        setup.es, setup.grid, setup.schedule);

    // Check that solution data round-trips within tolerance
    const auto& pressure  = rst_val.solution.data<double>("PRESSURE");
    const auto& swat      = rst_val.solution.data<double>("SWAT");
    const auto& sgas      = rst_val.solution.data<double>("SGAS");

    BOOST_REQUIRE_EQUAL(static_cast<decltype(numCells)>(pressure.size()), numCells);
    BOOST_REQUIRE_EQUAL(static_cast<decltype(numCells)>(swat.size()),     numCells);
    BOOST_REQUIRE_EQUAL(static_cast<decltype(numCells)>(sgas.size()),     numCells);

    for (auto i = 0*numCells; i != numCells; ++i) {
        BOOST_CHECK_CLOSE(pressure[i], pressure_val, 1.0e-5);
        BOOST_CHECK_CLOSE(swat[i],     swat_val,     1.0e-5);
        BOOST_CHECK_CLOSE(sgas[i],     sgas_val,     1.0e-5);
    }
}

BOOST_AUTO_TEST_CASE(Load_Nonexistent_File_Throws)
{
    Opm::Action::State   action_state;
    Opm::SummaryState    sum_state(Opm::TimeService::now(), 0.0);

    WorkArea work("test_StoreIO_missing");
    work.copyIn("BASE_SIM.DATA");

    Setup setup("BASE_SIM.DATA");

    const std::vector<Opm::RestartKey> keys{
        { "PRESSURE", Opm::UnitSystem::measure::pressure },
    };

    BOOST_CHECK_THROW(
        Opm::StoreIO::load("/nonexistent/path/CASE.STORE", 1,
                           action_state, sum_state,
                           keys,
                           setup.es, setup.grid, setup.schedule),
        std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END() // StoreIO_RoundTrip
