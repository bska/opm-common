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

#define BOOST_TEST_MODULE ConnectionOrdering_Manager

#include <boost/test/unit_test.hpp>

#include <opm/input/eclipse/Schedule/Well/ConnectionOrdering.hpp>

#include <opm/input/eclipse/Schedule/Well/Connection.hpp>
#include <opm/input/eclipse/Schedule/Well/NameOrder.hpp>
#include <opm/input/eclipse/Schedule/Well/WellMatcher.hpp>
#include <opm/input/eclipse/Schedule/Well/WListManager.hpp>

BOOST_AUTO_TEST_SUITE(Wells_Only)

namespace {

  Opm::NameOrder basicWells()
  {
      auto nameOrder = Opm::NameOrder {};

      nameOrder.add("WELL1");
      nameOrder.add("WELL2");
      nameOrder.add("WELL3");

      return nameOrder;
  }

} // Anonymous namespace

BOOST_AUTO_TEST_CASE(All_Default_Ordering)
{
    const auto mgr = Opm::ConnectionOrdering {};

    const auto wm = Opm::WellMatcher { basicWells() };

    BOOST_CHECK_MESSAGE(mgr.getConnectionOrder("WELL1", wm) == Opm::Connection::Order::TRACK,
                        R"("WELL1" should have a default connection order ("TRACK"))");

    BOOST_CHECK_MESSAGE(mgr.getConnectionOrder("WELL2", wm) == Opm::Connection::Order::TRACK,
                        R"("WELL2" should have a default connection order ("TRACK"))");

    BOOST_CHECK_MESSAGE(mgr.getConnectionOrder("WELL3", wm) == Opm::Connection::Order::TRACK,
                        R"("WELL3" should have a default connection order ("TRACK"))");

    BOOST_CHECK_MESSAGE(mgr.getConnectionOrder("No-Such-Well", wm) == Opm::Connection::Order::TRACK,
                        R"("No-Such-Well" should have a default connection order ("TRACK"))");
}

BOOST_AUTO_TEST_CASE(Per_Well_Specific_Ordering)
{
    const auto wm = Opm::WellMatcher { basicWells() };

    auto mgr = Opm::ConnectionOrdering {};

    BOOST_CHECK_MESSAGE(mgr.update(Opm::Connection::Order::INPUT, "WELL1"),
                        R"("WELL1" should be updated to have a custom connection order ("INPUT"))");

    BOOST_CHECK_MESSAGE(mgr.update(Opm::Connection::Order::DEPTH, "WELL2"),
                        R"("WELL2" should be updated to have a custom connection order ("DEPTH"))");

    BOOST_CHECK_MESSAGE(mgr.getConnectionOrder("WELL1", wm) == Opm::Connection::Order::INPUT,
                        R"("WELL1" should have a custom connection order ("INPUT"))");

    BOOST_CHECK_MESSAGE(mgr.getConnectionOrder("WELL2", wm) == Opm::Connection::Order::DEPTH,
                        R"("WELL2" should have a custom connection order ("DEPTH"))");

    BOOST_CHECK_MESSAGE(mgr.getConnectionOrder("WELL3", wm) == Opm::Connection::Order::TRACK,
                        R"("WELL3" should have a default connection order ("TRACK"))");

    BOOST_CHECK_MESSAGE(!mgr.update(Opm::Connection::Order::INPUT, "WELL1"),
                        R"("WELL1" should not be updated to have the same connection order ("INPUT") again)");

    BOOST_CHECK_MESSAGE(mgr.getConnectionOrder("WELL1", wm) == Opm::Connection::Order::INPUT,
                        R"("WELL1" should have a custom connection order ("INPUT"))");

    BOOST_CHECK_MESSAGE(mgr.update(Opm::Connection::Order::TRACK, "WELL1"),
                        R"("WELL1" should be updated to have a custom connection order ("TRACK"))");

    BOOST_CHECK_MESSAGE(mgr.getConnectionOrder("WELL1", wm) == Opm::Connection::Order::TRACK,
                        R"("WELL1" should have connection order "TRACK")");
}

BOOST_AUTO_TEST_SUITE_END() // Wells_Only
