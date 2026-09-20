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

#define BOOST_TEST_MODULE EnumeratedSimulationObjectsTests

#include <boost/test/unit_test.hpp>

#include <opm/input/eclipse/EclipseState/SummaryConfig/EnumeratedSimulationObjects.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

template <typename T, typename A>
std::vector<T, A> sorted(std::vector<T, A> values)
{
    std::ranges::sort(values);
    return values;
}

} // namespace

BOOST_AUTO_TEST_CASE(RoundTrip_Queries)
{
    using namespace std::string_literals;

    Opm::EnumeratedSimulationObjects eso;

    eso.addWell("W1"s);
    eso.addWell("W2"s, "LGR1"s);
    eso.addGroup("G1"s);
    eso.addGroup("FIELD"s);
    eso.addNetworkNode("NODE_A"s);
    eso.addNetworkGroupWell("W2"s);

    eso.addWellConnection("W1"s, 2);
    eso.addWellConnection("W1"s, 7);
    eso.addPossibleFutureConnection("W1"s, 11);

    eso.addWellSegment("W1"s, 1);
    eso.addWellSegment("W1"s, 3);
    eso.addWellCompletion("W1"s, 42);

    eso.addAnalyticAquifer(10);
    eso.addNumericAquifer(77);

    eso.addRegionArray("FIPNUM", {1, 1, 2, 3});

    eso.addUDQDefinition("WUAA", true);
    eso.addUDQDefinition("WUAB", false);

    eso.addGrid("", Opm::GridDims(10, 9, 8));
    eso.addGrid("LGR1", Opm::GridDims(3, 3, 1));

    eso.commit();

    BOOST_CHECK(eso.hasWell("W1"s));
    BOOST_CHECK(eso.hasWell("W2"s));
    BOOST_CHECK(!eso.hasWell("W3"s));

    BOOST_CHECK(eso.hasGroup("G1"s));
    BOOST_CHECK(eso.hasGroup("FIELD"s));
    BOOST_CHECK(!eso.hasGroup("NOPE"s));

    BOOST_CHECK_EQUAL(eso.wellNames().size(), 2U);
    BOOST_CHECK_EQUAL(eso.groupNames().size(), 2U);
    BOOST_CHECK_EQUAL(eso.networkNodeNames().size(), 1U);
    BOOST_CHECK_EQUAL(eso.networkGroupWellNames().size(), 1U);

    BOOST_CHECK_MESSAGE(!eso.lgrTagForWell("W1"s).has_value(), R"(Expected "W1" to have no LGR tag)");
    BOOST_REQUIRE(eso.lgrTagForWell("W2"s).has_value());
    BOOST_CHECK_EQUAL(*eso.lgrTagForWell("W2"s), "LGR1"s);

    const auto conn = eso.connectionsForWell("W1"s);
    const auto expectConn = std::array{std::size_t{2}, std::size_t{7}};
    BOOST_CHECK_EQUAL_COLLECTIONS(conn.begin(), conn.end(), expectConn.begin(), expectConn.end());

    BOOST_CHECK(eso.wellHasGlobalConnection("W1"s, 2));
    BOOST_CHECK(!eso.wellHasGlobalConnection("W1"s, 4));

    BOOST_REQUIRE(eso.possibleFutureConnections().contains("W1"s));
    BOOST_CHECK(eso.possibleFutureConnections().at("W1"s).contains(11));

    BOOST_CHECK(eso.isMultiSegmentWell("W1"s));
    BOOST_CHECK_EQUAL(eso.segmentCount("W1"s), 2);
    BOOST_CHECK(!eso.isMultiSegmentWell("W2"s));
    BOOST_CHECK_EQUAL(eso.segmentCount("W2"s), 0);

    BOOST_CHECK(eso.wellHasCompletion("W1"s, 42));
    BOOST_CHECK(!eso.wellHasCompletion("W1"s, 77));

    BOOST_CHECK_EQUAL(eso.analyticAquiferIDs().size(), 1U);
    BOOST_CHECK_EQUAL(eso.numericAquiferIDs().size(), 1U);

    BOOST_CHECK(eso.hasRegionArray("FIPNUM"));
    BOOST_CHECK_EQUAL(eso.regionArray("FIPNUM").size(), 4U);
    BOOST_CHECK(eso.regionArray("UNKNOWN").empty());

    BOOST_CHECK(eso.hasUDQKeyword("WUAA"s));
    BOOST_CHECK(eso.udqHasUnit("WUAA"s));
    BOOST_CHECK(eso.hasUDQKeyword("WUAB"s));
    BOOST_CHECK(!eso.udqHasUnit("WUAB"s));
    BOOST_CHECK(!eso.hasUDQKeyword("WUZZ"s));

    const auto globalDims = eso.gridDims(""s);
    BOOST_CHECK_EQUAL(globalDims.getNX(), 10U);
    BOOST_CHECK_EQUAL(globalDims.getNY(), 9U);
    BOOST_CHECK_EQUAL(globalDims.getNZ(), 8U);

    const auto lgrDims = eso.gridDims("LGR1"s);
    BOOST_CHECK_EQUAL(lgrDims.getNX(), 3U);
    BOOST_CHECK_EQUAL(lgrDims.getNY(), 3U);
    BOOST_CHECK_EQUAL(lgrDims.getNZ(), 1U);
}

BOOST_AUTO_TEST_CASE(Commit_Deduplicates_And_IsIdempotent)
{
    using namespace std::string_literals;

    Opm::EnumeratedSimulationObjects eso;

    eso.addWell("W2"s   );
    eso.addWell("W1"s);
    eso.addWell("W1"s);

    eso.addGroup("G2"s);
    eso.addGroup("G1"s);
    eso.addGroup("G1"s);

    eso.addNetworkNode("NODE"s);
    eso.addNetworkNode("NODE"s);
    eso.addNetworkGroupWell("W2"s);
    eso.addNetworkGroupWell("W2"s);

    eso.addWellConnection("W1"s, 9);
    eso.addWellConnection("W1"s, 4);
    eso.addWellConnection("W1"s, 4);

    eso.addWellSegment("W1"s, 3);
    eso.addWellSegment("W1"s, 1);
    eso.addWellSegment("W1"s, 1);

    eso.addAnalyticAquifer(7);
    eso.addAnalyticAquifer(7);
    eso.addAnalyticAquifer(5);

    eso.addNumericAquifer(9);
    eso.addNumericAquifer(9);

    eso.commit();

    const auto expectWells = sorted(std::vector {"W1"s, "W2"s});
    const auto expectGroups = sorted(std::vector {"G1"s, "G2"s});
    const auto expectNodeNames = sorted(std::vector {"NODE"s});
    const auto expectNodeGroupWells = sorted(std::vector {"W2"s});
    const auto expectConnections = std::array{std::size_t{4}, std::size_t{9}};
    const auto expectAnalyticAquifers = sorted(std::vector {5, 7});
    const auto expectNumericAquifers = sorted(std::vector {9});

    BOOST_CHECK_EQUAL_COLLECTIONS(
        eso.wellNames().begin(), eso.wellNames().end(),
        expectWells.begin(), expectWells.end());

    BOOST_CHECK_EQUAL_COLLECTIONS(
        eso.groupNames().begin(), eso.groupNames().end(),
        expectGroups.begin(), expectGroups.end());

    BOOST_CHECK_EQUAL_COLLECTIONS(
        eso.networkNodeNames().begin(), eso.networkNodeNames().end(),
        expectNodeNames.begin(), expectNodeNames.end());

    BOOST_CHECK_EQUAL_COLLECTIONS(
        eso.networkGroupWellNames().begin(), eso.networkGroupWellNames().end(),
        expectNodeGroupWells.begin(), expectNodeGroupWells.end());

    BOOST_CHECK_EQUAL_COLLECTIONS(
        eso.connectionsForWell("W1"s).begin(), eso.connectionsForWell("W1"s).end(),
        expectConnections.begin(), expectConnections.end());

    BOOST_CHECK_EQUAL(eso.segmentCount("W1"s), 2);

    BOOST_CHECK_EQUAL_COLLECTIONS(
        eso.analyticAquiferIDs().begin(), eso.analyticAquiferIDs().end(),
        expectAnalyticAquifers.begin(), expectAnalyticAquifers.end());

    BOOST_CHECK_EQUAL_COLLECTIONS(
        eso.numericAquiferIDs().begin(), eso.numericAquiferIDs().end(),
        expectNumericAquifers.begin(), expectNumericAquifers.end());

    // Must be safe and no-op.
    eso.commit();

    BOOST_CHECK_EQUAL(eso.wellNames().size(), 2U);
    BOOST_CHECK_EQUAL(eso.groupNames().size(), 2U);
    BOOST_CHECK_EQUAL(eso.connectionsForWell("W1"s).size(), 2U);
}

BOOST_AUTO_TEST_CASE(Commit_Prevents_Further_Additions)
{
    using namespace std::string_literals;

    Opm::EnumeratedSimulationObjects eso;

    eso.addWell("W1");
    eso.commit();

    BOOST_CHECK_THROW(eso.addWell("W2"s), std::logic_error);
    BOOST_CHECK_THROW(eso.addWell("W2"s, "LGR1"s), std::logic_error);
    BOOST_CHECK_THROW(eso.addGroup("G1"s), std::logic_error);
    BOOST_CHECK_THROW(eso.addNetworkNode("N1"s), std::logic_error);
    BOOST_CHECK_THROW(eso.addNetworkGroupWell("W1"s), std::logic_error);
    BOOST_CHECK_THROW(eso.addWellConnection("W1"s, 0), std::logic_error);
    BOOST_CHECK_THROW(eso.addPossibleFutureConnection("W1"s, 4), std::logic_error);
    BOOST_CHECK_THROW(eso.addWellSegment("W1"s, 1), std::logic_error);
    BOOST_CHECK_THROW(eso.addWellCompletion("W1"s, 1), std::logic_error);
    BOOST_CHECK_THROW(eso.addAnalyticAquifer(1), std::logic_error);
    BOOST_CHECK_THROW(eso.addNumericAquifer(1), std::logic_error);
    BOOST_CHECK_THROW(eso.addRegionArray("FIPNUM"s, {1}), std::logic_error);
    BOOST_CHECK_THROW(eso.addUDQDefinition("WUAA"s, true), std::logic_error);
    BOOST_CHECK_THROW(eso.addGrid(""s, Opm::GridDims(1, 1, 1)), std::logic_error);
}
