/*
  Copyright (c) 2026 Equinor ASA

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

#define BOOST_TEST_MODULE utility_TimedTaskScheduler

#include <boost/test/unit_test.hpp>

#include <opm/common/utility/TimedTaskScheduler.hpp>

#include <cstddef>
#include <future>
#include <stdexcept>
#include <vector>

BOOST_AUTO_TEST_SUITE(Integer_Time_Point)

using TimePoint = int;

BOOST_AUTO_TEST_SUITE(Void_No_Args)

using Scheduler = Opm::TimedTaskScheduler<TimePoint, void>;

BOOST_AUTO_TEST_CASE(Execute_In_Order)
{
    auto executionOrder = std::vector<int>{};
    auto scheduler = Scheduler{};

    scheduler.schedule([&executionOrder]() { executionOrder.push_back(1); }, 1);
    scheduler.schedule([&executionOrder]() { executionOrder.push_back(2); }, 2);
    scheduler.schedule([&executionOrder]() { executionOrder.push_back(3); }, 3);

    const auto completionStatus = scheduler.runReadyTasks(3);

    BOOST_REQUIRE_EQUAL(completionStatus.size(), std::size_t{3});

    for (const auto& status : completionStatus) {
        BOOST_CHECK_MESSAGE(status.valid(), "Task's future must have a valid state");
    }

    const auto expectOrder = std::vector{1, 2, 3};

    BOOST_CHECK_EQUAL_COLLECTIONS(executionOrder.begin(), executionOrder.end(),
                                  expectOrder.begin(), expectOrder.end());
}

BOOST_AUTO_TEST_CASE(Execute_With_Same_Time_Different_Priorities)
{
    auto executionOrder = std::vector<int>{};
    auto scheduler = Scheduler{};

    scheduler.schedule([&executionOrder]() { executionOrder.push_back(1); }, 1, 1);
    scheduler.schedule([&executionOrder]() { executionOrder.push_back(2); }, 1, 2);
    scheduler.schedule([&executionOrder]() { executionOrder.push_back(3); }, 1, 3);

    const auto completionStatus = scheduler.runReadyTasks(1);

    BOOST_REQUIRE_EQUAL(completionStatus.size(), std::size_t{3});

    for (const auto& status : completionStatus) {
        BOOST_CHECK_MESSAGE(status.valid(), "Task's future must have a valid state");
    }

    const auto expectOrder = std::vector{3, 2, 1};

    BOOST_CHECK_EQUAL_COLLECTIONS(executionOrder.begin(), executionOrder.end(),
                                  expectOrder.begin(), expectOrder.end());
}

BOOST_AUTO_TEST_CASE(Execute_With_Same_Time_Same_Priorities)
{
    auto executionOrder = std::vector<int>{};
    auto scheduler = Scheduler{};

    scheduler.schedule([&executionOrder]() { executionOrder.push_back(1); }, 1);
    scheduler.schedule([&executionOrder]() { executionOrder.push_back(2); }, 2);
    scheduler.schedule([&executionOrder]() { executionOrder.push_back(3); }, 2);

    {
        const auto completionStatus = scheduler.runReadyTasks(1);

        BOOST_REQUIRE_EQUAL(completionStatus.size(), std::size_t {1});

        for (const auto& status : completionStatus) {
            BOOST_CHECK_MESSAGE(status.valid(), "Task's future must have a valid state");
        }

        const auto expectOrder = std::vector {1};

        BOOST_CHECK_EQUAL_COLLECTIONS(executionOrder.begin(), executionOrder.end(),
                                      expectOrder.begin(), expectOrder.end());
    }

    {
        const auto completionStatus = scheduler.runReadyTasks(2);

        BOOST_REQUIRE_EQUAL(completionStatus.size(), std::size_t {2});

        for (const auto& status : completionStatus) {
            BOOST_CHECK_MESSAGE(status.valid(), "Task's future must have a valid state");
        }

        const auto expectOrder = std::vector {1, 2, 3};

        BOOST_CHECK_EQUAL_COLLECTIONS(executionOrder.begin(), executionOrder.end(),
                                      expectOrder.begin(), expectOrder.end());
    }
}

BOOST_AUTO_TEST_CASE(Throwing_Task)
{
    auto scheduler = Scheduler{};

    scheduler.schedule([]() { throw std::runtime_error("Task failed"); }, 1);

    auto completionStatus = scheduler.runReadyTasks(1);

    BOOST_REQUIRE_EQUAL(completionStatus.size(), std::size_t{1});

    for (const auto& status : completionStatus) {
        BOOST_CHECK_MESSAGE(status.valid(), "Future must hold a valid "
                            "state even if the task threw an exception");
    }

    BOOST_CHECK_THROW(completionStatus.front().get(), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END() // Void_No_Args

// --------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(Int_No_Args)

using Scheduler = Opm::TimedTaskScheduler<TimePoint, int>;

BOOST_AUTO_TEST_CASE(Execute_In_Order)
{
    auto executionOrder = std::vector<int>{};
    auto scheduler = Scheduler{};

    scheduler.schedule([&executionOrder]() { executionOrder.push_back(1); return 10; }, 1);
    scheduler.schedule([&executionOrder]() { executionOrder.push_back(2); return 20; }, 2);
    scheduler.schedule([&executionOrder]() { executionOrder.push_back(3); return 30; }, 3);

    {
        auto completionStatus = scheduler.runReadyTasks(1);

        BOOST_REQUIRE_EQUAL(completionStatus.size(), std::size_t {1});

        BOOST_CHECK_MESSAGE(completionStatus.front().valid(), "Task's future must have a valid state");
        BOOST_CHECK_EQUAL(completionStatus.front().get(), 10);

        const auto expectOrder = std::vector {1};

        BOOST_CHECK_EQUAL_COLLECTIONS(executionOrder.begin(), executionOrder.end(),
                                      expectOrder.begin(), expectOrder.end());
    }

    {
        auto completionStatus = scheduler.runReadyTasks(3);

        BOOST_REQUIRE_EQUAL(completionStatus.size(), std::size_t {2});

        BOOST_CHECK_MESSAGE(completionStatus.front().valid(), "Task's future must have a valid state");
        BOOST_CHECK_EQUAL(completionStatus.front().get(), 20);

        BOOST_CHECK_MESSAGE(completionStatus.back().valid(), "Task's future must have a valid state");
        BOOST_CHECK_EQUAL(completionStatus.back().get(), 30);

        const auto expectOrder = std::vector {1, 2, 3};

        BOOST_CHECK_EQUAL_COLLECTIONS(executionOrder.begin(), executionOrder.end(),
                                      expectOrder.begin(), expectOrder.end());
    }
}

BOOST_AUTO_TEST_CASE(Execute_With_Same_Time_Different_Priorities)
{
    auto executionOrder = std::vector<int>{};
    auto scheduler = Scheduler{};

    scheduler.schedule([&executionOrder]() { executionOrder.push_back(1); return 10; }, 1, 1);
    scheduler.schedule([&executionOrder]() { executionOrder.push_back(2); return 20; }, 1, 2);
    scheduler.schedule([&executionOrder]() { executionOrder.push_back(3); return 30; }, 1, 3);

    auto completionStatus = scheduler.runReadyTasks(3);

    BOOST_REQUIRE_EQUAL(completionStatus.size(), std::size_t {3});

    BOOST_CHECK_MESSAGE(completionStatus[0].valid(), "Task's future must have a valid state");
    BOOST_CHECK_EQUAL(completionStatus[0].get(), 30);

    BOOST_CHECK_MESSAGE(completionStatus[1].valid(), "Task's future must have a valid state");
    BOOST_CHECK_EQUAL(completionStatus[1].get(), 20);

    BOOST_CHECK_MESSAGE(completionStatus[2].valid(), "Task's future must have a valid state");
    BOOST_CHECK_EQUAL(completionStatus[2].get(), 10);

    const auto expectOrder = std::vector {3, 2, 1};

    BOOST_CHECK_EQUAL_COLLECTIONS(executionOrder.begin(), executionOrder.end(),
                                  expectOrder.begin(), expectOrder.end());
}

BOOST_AUTO_TEST_CASE(Execute_With_Same_Time_Same_Priorities)
{
    auto executionOrder = std::vector<int>{};
    auto scheduler = Scheduler{};

    scheduler.schedule([&executionOrder]() { executionOrder.push_back(1); return 10; }, 1, 1);
    scheduler.schedule([&executionOrder]() { executionOrder.push_back(2); return 20; }, 2);
    scheduler.schedule([&executionOrder]() { executionOrder.push_back(3); return 30; }, 2);

    auto completionStatus = scheduler.runReadyTasks(3);

    BOOST_REQUIRE_EQUAL(completionStatus.size(), std::size_t {3});

    BOOST_CHECK_MESSAGE(completionStatus[0].valid(), "Task's future must have a valid state");
    BOOST_CHECK_EQUAL(completionStatus[0].get(), 10);

    BOOST_CHECK_MESSAGE(completionStatus[1].valid(), "Task's future must have a valid state");
    BOOST_CHECK_EQUAL(completionStatus[1].get(), 20);

    BOOST_CHECK_MESSAGE(completionStatus[2].valid(), "Task's future must have a valid state");
    BOOST_CHECK_EQUAL(completionStatus[2].get(), 30);

    const auto expectOrder = std::vector {1, 2, 3};

    BOOST_CHECK_EQUAL_COLLECTIONS(executionOrder.begin(), executionOrder.end(),
                                  expectOrder.begin(), expectOrder.end());
}

BOOST_AUTO_TEST_CASE(Throwing_Task)
{
    auto scheduler = Scheduler{};

    scheduler.schedule([]() { throw std::runtime_error("Task failed"); return 1729; }, 1);

    auto completionStatus = scheduler.runReadyTasks(1);

    BOOST_REQUIRE_EQUAL(completionStatus.size(), std::size_t {1});

    BOOST_CHECK_MESSAGE(completionStatus[0].valid(), "Task's future must have a valid state");
    BOOST_CHECK_THROW(completionStatus[0].get(), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END() // Int_No_Args

BOOST_AUTO_TEST_SUITE_END() // Integer_Time_Point
