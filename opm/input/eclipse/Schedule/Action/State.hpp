/*
  Copyright 2020 Equinor ASA.

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

#ifndef ACTION_STATE_HPP
#define ACTION_STATE_HPP

#include <cstddef>
#include <ctime>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Opm::RestartIO {
    struct RstState;
} // namespace Opm::RestartIO

namespace Opm::Action {

    class ActionX;
    class Actions;
    class PyAction;
    class Result;

} // namespace Opm::Action

namespace Opm::Action {

class State
{
public:
    class MatchSet
    {
    public:
        bool hasWell(const std::string& well) const;

        static MatchSet serializationTestObject();

        bool operator==(const MatchSet& that) const;

        template <class Serializer>
        void serializeOp(Serializer& serializer)
        {
            serializer(this->wells_);
        }

        friend class State;

    private:
        std::vector<std::string> wells_{};
    };

    void add_run(const ActionX& action, std::time_t sim_time, const Result& result);
    void add_run(const PyAction& action, bool result);

    std::size_t run_count(const ActionX& action) const;
    std::time_t run_time(const ActionX& action) const;

    const MatchSet* result(const std::string& action) const;
    std::optional<bool> python_result(const std::string& action) const;

    void load_rst(const Actions& action_config, const RestartIO::RstState& rst_state);

    template <class Serializer>
    void serializeOp(Serializer& serializer)
    {
        serializer(this->run_state);
        serializer(this->last_result);
        serializer(this->m_python_result);
    }

    static State serializationTestObject();
    bool operator==(const State& other) const;

private:
    struct RunState
    {
        RunState() = default;

        explicit RunState(std::time_t sim_time)
            : run_count(1)
            , last_run(sim_time)
        {}

        void add_run(std::time_t sim_time)
        {
            this->last_run = sim_time;
            this->run_count += 1;
        }

        static RunState serializationTestObject()
        {
            RunState rs;

            rs.run_count = 100;
            rs.last_run = 123456;

            return rs;
        }

        bool operator==(const RunState& other) const
        {
            return (this->run_count == other.run_count)
                && (this->last_run == other.last_run);
        }

        template <class Serializer>
        void serializeOp(Serializer& serializer)
        {
            serializer(this->run_count);
            serializer(this->last_run);
        }

        std::size_t run_count{};
        std::time_t last_run{};
    };

    using action_id = std::pair<std::string, std::size_t>;
    static action_id make_id(const ActionX& action);

    std::map<action_id, RunState> run_state;
    std::map<std::string, MatchSet> last_result;
    std::map<std::string, bool> m_python_result;
};

} // namespace Opm::Action

#endif // ACTION_STATE_HPP
