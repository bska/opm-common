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

#include <opm/output/eclipse/RestartOutputManager.hpp>

#include <opm/output/data/Solution.hpp>
#include <opm/output/data/Wells.hpp>

#include <opm/output/eclipse/AggregateAquiferData.hpp>
#include <opm/output/eclipse/AggregateGroupData.hpp>
#include <opm/output/eclipse/AggregateNetworkData.hpp>
#include <opm/output/eclipse/AggregateWellData.hpp>
#include <opm/output/eclipse/AggregateWListData.hpp>
#include <opm/output/eclipse/AggregateConnectionData.hpp>
#include <opm/output/eclipse/AggregateMSWData.hpp>
#include <opm/output/eclipse/AggregateUDQData.hpp>
#include <opm/output/eclipse/AggregateActionxData.hpp>
#include <opm/output/eclipse/RestartValue.hpp>
#include <opm/output/eclipse/UDQDims.hpp>

#include <opm/input/eclipse/EclipseState/EclipseState.hpp>
#include <opm/input/eclipse/EclipseState/Grid/EclipseGrid.hpp>
#include <opm/input/eclipse/EclipseState/Tables/Eqldims.hpp>
#include <opm/input/eclipse/EclipseState/TracerConfig.hpp>

#include <opm/input/eclipse/Schedule/Action/Actions.hpp>
#include <opm/input/eclipse/Schedule/Network/ExtNetwork.hpp>
#include <opm/input/eclipse/Schedule/Schedule.hpp>
#include <opm/input/eclipse/Schedule/ScheduleState.hpp>
#include <opm/input/eclipse/Schedule/SummaryState.hpp>
#include <opm/input/eclipse/Schedule/Tuning.hpp>
#include <opm/input/eclipse/Schedule/UDQ/UDQConfig.hpp>
#include <opm/input/eclipse/Schedule/Well/Well.hpp>
#include <opm/input/eclipse/Schedule/Well/WellConnections.hpp>

#include <opm/common/OpmLog/OpmLog.hpp>

#include <algorithm>
#include <functional>
#include <optional>
#include <iterator>
#include <ranges>
#include <span>
#include <string_view>
#include <string>
#include <utility>
#include <vector>

namespace {

    template <typename CleanupFunc>
    class ContextGuard
    {
    public:
        ContextGuard(CleanupFunc&& cleanupFunc)
            : cleanupFunc_{ std::forward<CleanupFunc>(cleanupFunc) }
        {}

        ~ContextGuard()
        {
            this->cleanupFunc_();
        }

    private:
        CleanupFunc cleanupFunc_;
    };

    #if 0
    template <typename CleanupFunc>
    ContextGuard(CleanupFunc) -> ContextGuard<CleanupFunc>;
    #endif

    std::vector<std::size_t> collectGridIndices(const Opm::EclipseGrid& grid)
    {
        auto gridIndices = grid.get_print_order_lgr();

        // LGR indices are offset by 1 from main grid.
        std::ranges::transform(gridIndices, gridIndices.begin(),
                               [](const auto& lgrIndex) { return lgrIndex + 1; });
        
        // Note: The main grid is always present and assigned index 0.
        gridIndices.emplace(gridIndices.begin(), 0);
        
        return gridIndices;
    }

    std::vector<std::string> collectGridNames(const Opm::EclipseGrid& grid)
    {
        auto gridNames = grid.get_all_lgr_labels();
        
        // Note: The main grid is always present and named "GLOBAL".
        gridNames.emplace(gridNames.begin(), "GLOBAL");

        return gridNames;
    }

} // Anonymous namespace

// -------------------------------------------------------------------------
// RestartOutputManager public interface below separator
// -------------------------------------------------------------------------

Opm::RestartIO::RestartOutputManager::
RestartOutputManager(const EclipseState& es,
                     const EclipseGrid&  grid,
                     const Schedule&     schedule)
    : es_          { es }
    , grid_        { grid }
    , schedule_    { schedule }
    , gridIndices_ { collectGridIndices(grid) }
    , gridNames_   { collectGridNames(grid) }
{}

Opm::RestartIO::RestartOutputManager&
Opm::RestartIO::RestartOutputManager::
solutionArrayBackend(const WriteSolutionArray& backend)
{
    this->solutionArrayBackend_.emplace(backend);
    return *this;
}

void
Opm::RestartIO::RestartOutputManager::
writeRestartStep(const int                         reportStep,
                 const double                      elapsedTime,
                 DynamicStateValues&               state,
                 EclIO::OutputStream::RestartBase& rstFile) const
{
    const auto contextGuard = ContextGuard{ [this] { this->popContext(); } };
    this->pushContext(reportStep, elapsedTime, state, rstFile);

    for (this->writeContext_->printIndex = 0;
         this->writeContext_->printIndex < this->gridIndices_.size();
          ++this->writeContext_->printIndex)
    {
        this->writeContext_->gridIndex = this->gridIndices_[this->writeContext_->printIndex];
        this->writeContext_->gridName = this->gridNames_[this->writeContext_->printIndex];

        this->prologue();

        this->headers();
        this->groups();
        this->networks();
        this->wells();
        this->actions();
        this->aquifers();

        this->solutionCommon();
        this->solutionSpecial();
        this->solutionNNC();
        this->solutionLocalGlobal();
        this->solutionLocalLocal();

        this->epilogue();
    }
}

// -------------------------------------------------------------------------
// RestartOutputManager private interface below separator
// -------------------------------------------------------------------------

void
Opm::RestartIO::RestartOutputManager::
pushContext(const int                         reportStep,
            const double                      elapsedTime,
            DynamicStateValues&               state,
            EclIO::OutputStream::RestartBase& rstFile) const
{
    this->writeContext_.emplace(WriteContext{
        .reportStep = reportStep,
        .elapsedTime = elapsedTime,
        .state = state,
        .rstFile = rstFile,
        .printIndex = 0, // Default to zero; may be updated by caller.
        .gridIndex = 0, // Default to zero; may be updated by caller.
        .gridName = {}  // Default to empty; may be updated by caller.
    });
}

void
Opm::RestartIO::RestartOutputManager::popContext() const
{
    this->writeContext_.reset();
}
