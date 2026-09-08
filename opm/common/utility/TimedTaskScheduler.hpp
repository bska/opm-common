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

#ifndef OPM_TIMED_TASK_SCHEDULER_HPP
#define OPM_TIMED_TASK_SCHEDULER_HPP

#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <queue>
#include <type_traits>
#include <utility>
#include <vector>

namespace Opm {

/// A scheduler for tasks that should be executed at specific times.
///
/// Note that even though this facility is defined in terms of future/promise,
/// it does not create any background threads.  Tasks are only executed when
/// runReadyTasks is called, and is intended to be used in a synchronous manner.
///
/// \tparam TimePoint The type representing the time points at which tasks are
/// scheduled.  Must support comparison operators (e.g., operator<=) to determine
/// the order of execution based on time.
///
/// A common choice for TimePoint is std::chrono::steady_clock::time_point or
/// std::chrono::system_clock::time_point, or even Opm::time_point from
/// TimeService.hpp.  For testing purposes clients may choose something simple
/// like an integer type.  We suggest not using floating-point types however,
/// due to potential precision issues.
///
/// \tparam Ret The return type of the scheduled tasks.  Can be void.
///
/// \tparam Args The argument types of the scheduled tasks.
template <typename TimePoint, typename Ret, typename... Args>
class TimedTaskScheduler
{
public:
    /// The type representing the completion status of a scheduled task.
    using CompletionStatus = std::future<Ret>;

    /// The type representing the callback function of a scheduled task.
    using Callback = std::function<Ret(const Args&...)>;

    /// Schedule a task to be executed at a specific time with an optional priority.
    ///
    /// \param[in] callback The callback function representing the task.
    ///
    /// \param[in] time The time point at which the task should be executed.
    ///
    /// \param[in] priority The priority of the task. Higher priority (larger
    /// numerical  value) tasks are executed first if scheduled for the same time.
    void schedule(Callback callback, const TimePoint time, const int priority = 0)
    {
        auto task = std::make_unique<ScheduledTask>
            (std::move(callback), time, priority,
             this->sequence_id_counter_++);

        this->task_queue_.push(std::move(task));
    }

    /// Execute all tasks that are ready at the given time.
    ///
    /// \param[in] time The current time point used to determine which
    /// tasks are ready.
    ///
    /// \param[in] args The arguments to be passed to the task callbacks.
    ///
    /// \return A vector of futures representing the completion status of
    /// the executed tasks.
    std::vector<CompletionStatus>
    runReadyTasks(const TimePoint time, const Args&... args)
    {
        auto completed_tasks = std::vector<CompletionStatus>{};

        while (!this->task_queue_.empty() &&
               (this->task_queue_.top()->time <= time))
        {
            auto& task = this->task_queue_.top();

            try {
                if constexpr (std::is_void_v<Ret>) {
                    task->callback(args...);
                    task->promise.set_value();
                }
                else {
                    task->promise.set_value(task->callback(args...));
                }
            }
            catch (...) {
                task->promise.set_exception(std::current_exception());
            }

            completed_tasks.push_back(task->promise.get_future());

            this->task_queue_.pop();
        }

        return completed_tasks;
    }

private:
    ///  Structure representing a scheduled task.
    struct ScheduledTask
    {
        ///  The callback function representing the task to be executed.
        Callback callback{};

        ///  The scheduled time for the task.
        TimePoint time{};

        ///  The priority of the task.
        ///
        /// Higher priority tasks (greater numerical value) are executed
        /// first if scheduled for the same time.
        int priority{};

        ///  The sequence ID used for tie-breaking when tasks
        /// have the same time and priority.
        ///
        /// Tasks with lower sequence IDs are considered older and are executed first.
        std::uint_least64_t sequence_id{};

        ///  The promise used to set the completion status of the task.
        std::promise<Ret> promise{};
    };

    /// Container for a single scheduled task.
    ///
    /// This is a pointer type since std::promise is move-only and therefore
    /// poorly suited for value semantics in a std::priority_queue<>.
    using TaskPtr = std::unique_ptr<ScheduledTask>;

    /// Comparator for scheduled tasks used in the priority queue.
    ///
    /// \details Orders tasks first by scheduled time (earlier time comes first),
    /// then by priority (higher priority comes first),  and finally by sequence
    /// ID (older/smaller sequence ID comes first) for tie-breaking.
    struct CompareTasks
    {
        bool operator()(const TaskPtr& lhs, const TaskPtr& rhs) const
        {
            // 1. Check trigger time (earlier time comes first -> belongs at the top)
            if (lhs->time != rhs->time) {
                return lhs->time > rhs->time;
            }

            // 2. Check priority (higher priority comes first)
            if (lhs->priority != rhs->priority) {
                return lhs->priority < rhs->priority;
            }

            // 3. Tie-breaker: Registration order (older/smaller sequence ID comes first)
            return lhs->sequence_id > rhs->sequence_id;
        }
    };

    /// Convenience type alias for the priority queue of scheduled tasks.
    ///
    /// \details Uses the CompareTasks comparator to maintain the correct
    /// ordering of tasks based on time, priority, and sequence ID.
    using TaskQueue = std::priority_queue<TaskPtr, std::vector<TaskPtr>, CompareTasks>;

    /// The priority queue holding the scheduled tasks.
    ///
    /// \details Tasks are ordered based on their scheduled time,
    /// priority, and sequence ID using the CompareTasks comparator.
    TaskQueue task_queue_{};

    /// Counter for generating unique sequence IDs for tasks.
    ///
    /// \details Ensures that each task has a unique sequence ID
    /// for tie-breaking when tasks have the same time and priority.
    std::uint_least64_t sequence_id_counter_{};
};

} // namespace Opm

#endif // OPM_TIMED_TASK_SCHEDULER_HPP
