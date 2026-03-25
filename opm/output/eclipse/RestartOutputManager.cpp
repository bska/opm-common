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

#include <opm/common/OpmLog/OpmLog.hpp>
#include <opm/common/utility/Visitor.hpp>

#include <opm/output/data/Cells.hpp>
#include <opm/output/data/Solution.hpp>
#include <opm/output/data/Wells.hpp>

#include <opm/output/eclipse/UDQDims.hpp>
#include <opm/output/eclipse/WriteRestartHelpers.hpp>

#include <opm/output/eclipse/VectorItems/intehead.hpp>

#include <opm/io/eclipse/OutputStream.hpp>
#include <opm/io/eclipse/PaddedOutputString.hpp>

#include <opm/output/eclipse/AggregateAquiferData.hpp>
#include <opm/output/eclipse/AggregateGroupData.hpp>
#include <opm/output/eclipse/AggregateNetworkData.hpp>
#include <opm/output/eclipse/AggregateWellData.hpp>
#include <opm/output/eclipse/AggregateWListData.hpp>
#include <opm/output/eclipse/AggregateConnectionData.hpp>
#include <opm/output/eclipse/AggregateMSWData.hpp>
#include <opm/output/eclipse/AggregateUDQData.hpp>
#include <opm/output/eclipse/AggregateActionxData.hpp>
#include <opm/output/eclipse/InteHEAD.hpp>
#include <opm/output/eclipse/RestartValue.hpp>
#include <opm/output/eclipse/UDQDims.hpp>

#include <opm/input/eclipse/EclipseState/Aquifer/AquiferConfig.hpp>
#include <opm/input/eclipse/EclipseState/EclipseState.hpp>
#include <opm/input/eclipse/EclipseState/Grid/EclipseGrid.hpp>
#include <opm/input/eclipse/EclipseState/Grid/FieldProps.hpp>
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

#include <opm/input/eclipse/Units/UnitSystem.hpp>

#include <fmt/format.h>
#include <fmt/chrono.h>

#include <algorithm>
#include <cstddef>
#include <ctime>
#include <functional>
#include <optional>
#include <iterator>
#include <initializer_list>
#include <ranges>
#include <regex>
#include <stdexcept>
#include <span>
#include <string_view>
#include <string>
#include <tuple>
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

    double nextStepSize(const Opm::RestartValue& rst_value)
    {
        return rst_value.hasExtra("OPMEXTRA")
            ? rst_value.getExtra("OPMEXTRA")[0]
            : 0.0;
    }

} // Anonymous namespace

class Opm::RestartIO::RestartOutputManager::IsFluidInPlace
{
public:
    // Note: We take a string& because regex_match does not support string_view.
    bool operator()(const std::string& arrayName) const
    {
        return std::regex_match(arrayName, this->fipRegex_);
    }

private:
    std::regex fipRegex_ { R"([RS]?FIP(OIL|GAS|WAT))" };
};

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

Opm::RestartIO::RestartOutputManager&
Opm::RestartIO::RestartOutputManager::permitExtendedArrays(const bool permit)
{
    this->permitExtendedArrays_ = permit;
    return *this;
}

void
Opm::RestartIO::RestartOutputManager::
writeRestartStep(const int                         reportStep,
                 const double                      elapsedTime,
                 DynamicStateValues&               state,
                 EclIO::OutputStream::RestartBase& rstFile) const
{
    // Context guard to ensure that write context is properly cleaned up even
    // if an exception is thrown during writing.  Recall that objects with a
    // non-trivial destructor cannot be optimised away by the compiler.
    const auto contextGuard = ContextGuard{ [this] { this->popContext(); } };

    this->pushContext(reportStep, elapsedTime, state, rstFile);

    std::ranges::for_each(std::views::iota(std::size_t{0}, this->gridIndices_.size()),
                          [isFip = IsFluidInPlace{}, this](const auto& prtIdx)
    {
        this->writeContext_->printIndex = prtIdx;
        this->writeContext_->gridIndex = this->gridIndices_[prtIdx];
        this->writeContext_->gridName = this->gridNames_[prtIdx];

        this->prologue();

        this->headers();

        if (this->writeContext_->reportStep > 0) {
            this->groups();
            this->networks();
            this->wells();
            this->aquifers();
            this->actions();
        }

        this->message("STARTSOL");

        this->solutionCommon(isFip);
        this->solutionSpecial();
        this->solutionNNC();
        this->lgrNames();
        this->solutionLocalGlobal();
        this->solutionLocalLocal();

        this->message("ENDSOL");

        this->epilogue();
    });

    this->logRestartOutputCompleted();
}

// -------------------------------------------------------------------------
// RestartOutputManager private interface below separator
// -------------------------------------------------------------------------

namespace Opm::RestartIO {

    template <typename T>
    void
    RestartOutputManager::writeArray(std::string_view      arrayName,
                                     const std::vector<T>& arrayValue) const
    {
        // Explicitly convert arrayName to std::string since
        // RestartBase::write() does not support a string_view
        // array name.
        //
        // [April 2026] This is hopefully a temporary state of affairs...
        this->writeContext_->rstFile.get()
            .write(std::string {arrayName}, arrayValue);
    }

} // namespace Opm::RestartIO

void
Opm::RestartIO::RestartOutputManager::
pushContext(const int                         reportStep,
            const double                      elapsedTime,
            DynamicStateValues&               state,
            EclIO::OutputStream::RestartBase& rstFile) const
{
    this->writeContext_.emplace(WriteContext {
        .reportStep = reportStep,
        .elapsedTime = elapsedTime,
        .state = state,
        .rstFile = rstFile,
        .printIndex = 0, // Default to zero; may be updated by caller.
        .gridIndex = 0, // Default to zero; may be updated by caller.
        .gridName = {}, // Default to empty; may be updated by caller.
        .inteHead = {} // Default to empty; will be populated in headers() member function.
    });
}

void
Opm::RestartIO::RestartOutputManager::popContext() const
{
    this->writeContext_.reset();
}

void
Opm::RestartIO::RestartOutputManager::message(std::string_view msg) const
{
    // Explicitly convert msg to std::string since
    // RestartBase::message() does not support a string_view
    // message type.
    //
    // [April 2026] This is hopefully a temporary state of affairs...
    this->writeContext_->rstFile.get().message(std::string {msg});
}

void
Opm::RestartIO::RestartOutputManager::prologue() const
{
    if (this->writeContext_->gridIndex == 0) {
        return; // No prologue for main grid.
    }

    // If we get here, this is a local grid. Write LGR-specific headers.

    this->writeArray("LGR", std::vector { EclIO::PaddedOutputString<8>{ this->writeContext_->gridName } });

    this->writeArray("LGRHEADI", Helpers::createLgrHeadi(this->es_, this->writeContext_->gridIndex));

    this->writeArray("LGRHEADQ", Helpers::createLgrHeadq(this->es_));

    this->writeArray("LGRHEADD", Helpers::createLgrHeadd());
}

void
Opm::RestartIO::RestartOutputManager::epilogue() const
{
    if (this->writeContext_->gridIndex == 0) {
        return; // No epilogue for main grid.
    }

    // If we get here, this is a local grid. Write LGR-specific epilogue.

    this->writeArray("ENDLGR", std::vector { static_cast<int>(this->writeContext_->printIndex) });
}

void
Opm::RestartIO::RestartOutputManager::headers() const
{
    const auto sim_step = std::max(this->writeContext_->reportStep - 1, 0);

    // Write INTEHEAD to restart file
    this->writeContext_->inteHead =
        Helpers::createInteHead(this->es_, this->grid_, this->schedule_,
                                this->writeContext_->elapsedTime,
                                this->writeContext_->reportStep, // Should really be number of timesteps
                                this->writeContext_->reportStep,
                                sim_step);

    this->writeArray("INTEHEAD", this->writeContext_->inteHead);

    // Write LOGIHEAD to restart file
    {
        const auto logiHead = (this->writeContext_->reportStep == 0)
            ? Helpers::createLogiHead(this->es_)
            : Helpers::createLogiHead(this->es_, this->schedule_.get()[this->writeContext_->reportStep - 1]);

        this->writeArray("LOGIHEAD", logiHead);
    }

    // Write DOUBHEAD to restart file
    {
        const auto& rst_value = this->writeContext_->state.get()
            .solution[this->writeContext_->gridIndex];

        const auto dh = Helpers::createDoubHead
            (this->es_, this->schedule_, sim_step,
             this->writeContext_->reportStep,
             this->writeContext_->elapsedTime,
             nextStepSize(rst_value));

        this->writeArray("DOUBHEAD", dh);
    }
}

void
Opm::RestartIO::RestartOutputManager::groups() const
{
    const auto simStep = static_cast<std::size_t>
        (std::max(this->writeContext_->reportStep - 1, 0));

    auto groupData = Helpers::AggregateGroupData(this->writeContext_->inteHead);

    if (this->writeContext_->gridIndex == 0) {
        groupData.captureDeclaredGroupData(this->schedule_,
                                           this->schedule_.get().getUnits(),
                                           simStep,
                                           this->writeContext_->state.get().summaryState,
                                           this->writeContext_->inteHead);
    }
    else {
        const auto lgr_tag = std::string { this->writeContext_->gridName };

        groupData.captureDeclaredGroupDataLGR(this->schedule_,
                                              this->schedule_.get().getUnits(),
                                              simStep,
                                              this->writeContext_->state.get().summaryState,
                                              lgr_tag);
    }

    this->writeArray("IGRP", groupData.getIGroup());
    this->writeArray("SGRP", groupData.getSGroup());
    this->writeArray("XGRP", groupData.getXGroup());
    this->writeArray("ZGRP", groupData.getZGroup());
}

void
Opm::RestartIO::RestartOutputManager::networks() const
{
    const auto simStep = static_cast<std::size_t>
        (std::max(this->writeContext_->reportStep - 1, 0));

    if (! this->schedule_.get()[simStep].network().active()) {
        return; // No network data to write for this step.
    }

    auto networkData = Helpers::AggregateNetworkData(this->writeContext_->inteHead);

    networkData.captureDeclaredNetworkData(this->es_,
                                           this->schedule_,
                                           this->schedule_.get().getUnits(),
                                           simStep,
                                           this->writeContext_->state.get().summaryState,
                                           this->writeContext_->inteHead);

    this->writeArray("INODE", networkData.getINode());
    this->writeArray("IBRAN", networkData.getIBran());
    this->writeArray("INOBR", networkData.getINobr());
    this->writeArray("RNODE", networkData.getRNode());
    this->writeArray("RBRAN", networkData.getRBran());
    this->writeArray("ZNODE", networkData.getZNode());
}

void
Opm::RestartIO::RestartOutputManager::wells() const
{
    if (! this->hasValidWells()) {
        return; // No well data to write for this step.
    }

    // If we get here, there are valid wells for this grid.
    //
    // Write well-related data.

    this->writeMultisegWellArrays();
    this->writeWellArrays();
    this->writeWellListArrays();
    this->writeWellConnectionArrays();
}

void
Opm::RestartIO::RestartOutputManager::aquifers() const
{
    if (this->writeContext_->gridIndex != 0) {
        return; // Aquifer data is only relevant for the main grid.
    }

    const auto simStep = static_cast<std::size_t>
        (std::max(this->writeContext_->reportStep - 1, 0));

    const auto& aqConfig = this->es_.get().aquifer();

    if (!aqConfig.active() || !this->writeContext_->state.get().aquiferData.has_value()) {
        return; // No aquifer data to write for this step.
    }

    this->writeContext_->state.get().aquiferData->captureDynamicAquiferData
        (inferAquiferDimensions(this->es_, this->schedule_.get()[simStep]),
         aqConfig,
         this->schedule_.get()[simStep],
         this->writeContext_->state.get().solution[this->writeContext_->gridIndex].aquifer,
         this->writeContext_->state.get().summaryState,
         this->schedule_.get().getUnits());

    if (aqConfig.hasAnalyticalAquifer() ||
        this->schedule_.get()[simStep].hasAnalyticalAquifers())
    {
        this->analyticalAquiferData();
    }

    if (aqConfig.hasNumericalAquifer()) {
        this->numericalAquiferData();
    }
}

void
Opm::RestartIO::RestartOutputManager::actions() const
{
    if (this->writeContext_->gridIndex != 0) {
        return; // ACTIONX data is only relevant for the main grid.
    }

    const auto simStep = static_cast<std::size_t>
        (std::max(this->writeContext_->reportStep - 1, 0));

    if ((this->writeContext_->reportStep == 0) ||
        (this->schedule_.get()[simStep].actions().ecl_size() == 0))
    {
        return;
    }

    const auto actionxData = Helpers::AggregateActionxData {
        this->schedule_,
        this->writeContext_->state.get().actionState,
        this->writeContext_->state.get().summaryState,
        simStep
    };

    this->writeArray("IACT", actionxData.getIACT());
    this->writeArray("SACT", actionxData.getSACT());
    this->writeArray("ZACT", actionxData.getZACT());
    this->writeArray("ZLACT", actionxData.getZLACT());
    this->writeArray("ZACN", actionxData.getZACN());
    this->writeArray("IACN", actionxData.getIACN());
    this->writeArray("SACN", actionxData.getSACN());
}

void
Opm::RestartIO::RestartOutputManager::solutionCommon(const IsFluidInPlace& isFip) const
{
    this->regularSolutionArrays(isFip);
    this->fluidInPlaceArrays(isFip);
    this->tracerArrays();
}

void
Opm::RestartIO::RestartOutputManager::solutionSpecial() const
{
    if (this->writeContext_->gridIndex != 0) {
        return; // Special solution arrays apply to the main grid only.
    }

    this->udqArrays();
    this->thresholdPressureArray();

    if (this->permitExtendedArrays_) {
        this->extendedSolutionArrays();
    }
}

void
Opm::RestartIO::RestartOutputManager::solutionNNC() const
{
    const auto& allArrays = this->writeContext_->state.get()
        .solution[this->writeContext_->gridIndex].extra;

    auto nncArrays = allArrays | std::views::filter([nncRE = std::regex{ R"(FL[OR](OIL|GAS|WAT)N\+)"}](const auto& array)
    {
        return std::regex_match(array.first.key, nncRE);
    });

    if (this->solutionArrayBackend_.has_value()) {
        std::ranges::for_each(nncArrays, [this](const auto& array) {
            const auto& [key, nncValue] = array;

            auto data = data::CellData {
                key.dim, nncValue, data::TargetType::RESTART_SOLUTION
            };

            (*this->solutionArrayBackend_)
                (key.key, data, this->writeContext_->rstFile);
        });
    }
    else {
        std::ranges::for_each(nncArrays, [this](const auto& array) {
            const auto& [key, nncValue] = array;

            auto data = data::CellData {
                key.dim, nncValue, data::TargetType::RESTART_SOLUTION
            };

            this->defaultSolutionArrayBackend(key.key, data);
        });
    }
}

void
Opm::RestartIO::RestartOutputManager::solutionLocalGlobal() const
{
    // Currently unsupported.
}

void
Opm::RestartIO::RestartOutputManager::solutionLocalLocal() const
{
    // Currently unsupported.
}

void
Opm::RestartIO::RestartOutputManager::logRestartOutputCompleted() const
{
    using namespace fmt::literals;
    using Ix = Helpers::VectorItems::intehead;

    const auto numReports = this->schedule_.get().size() - 1;

    const auto& inteHD = this->writeContext_->inteHead;

    auto timepoint = std::tm {};
    timepoint.tm_year = inteHD[Ix::YEAR]  - 1900;
    timepoint.tm_mon  = inteHD[Ix::MONTH] -    1;
    timepoint.tm_mday = inteHD[Ix::DAY];

    timepoint.tm_hour = inteHD[Ix::IHOURZ];
    timepoint.tm_min  = inteHD[Ix::IMINTS];
    timepoint.tm_sec  = inteHD[Ix::ISECND] / 1'000'000;

    const auto msg = fmt::format("Restart file written for report step "
                                 "{report_step:>{width}}/{num_reports}, "
                                 "date = {timepoint:%d-%b-%Y %H:%M:%S}",
                                 "width"_a = fmt::formatted_size("{}", numReports),
                                 "report_step"_a = this->writeContext_->reportStep,
                                 "num_reports"_a = numReports,
                                 "timepoint"_a = timepoint);

    OpmLog::info(msg);
}

bool
Opm::RestartIO::RestartOutputManager::hasValidWells() const
{
    if (this->writeContext_->reportStep == 0) {
        return false; // No wells are active at time zero.
    }

    const auto& allWells = this->schedule_.get()
        [this->writeContext_->reportStep - 1].wells;

    if (this->writeContext_->gridIndex == 0) {
        // Wells valid for the main grid if there are any wells at all.
        return allWells.size() > 0;
    }

    // If we get here, this is a local grid. Valid wells in this case are those
    // that intersect the local grid and which are not multi-segmented wells.

    auto lgrWells = allWells |
        std::views::filter([this](const auto& well) {
            return well.second->get_lgr_well_tag().value_or("") == this->writeContext_->gridName;
        }) |
        std::views::transform([](const auto& well) {
            return well.second.get();
        });

    const auto lgrWellPtrs = std::vector<const Well*> {
        std::begin(lgrWells), std::end(lgrWells)
    };

    return ! lgrWellPtrs.empty()
        && std::ranges::none_of(lgrWellPtrs, &Well::isMultiSegment);
 }

void
Opm::RestartIO::RestartOutputManager::writeMultisegWellArrays() const
{
    if (this->writeContext_->gridIndex != 0) {
        // Multisegment well arrays are (currently) supported only for the main grid.
        return;
    }

    const auto simStep = static_cast<std::size_t>
        (std::max(this->writeContext_->reportStep - 1, 0));

    const auto haveMSW = std::ranges::any_of(this->schedule_.get()[simStep].wells,
        &Well::isMultiSegment,
        [](const auto& well) { return well.second.get(); });

    if (! haveMSW) {
        return; // No multisegment wells to write for this step.
    }

    const auto& wellSol = this->writeContext_->state.get()
        .solution[this->writeContext_->gridIndex].wells;

    auto MSWData = Helpers::AggregateMSWData {this->writeContext_->inteHead};
    MSWData.captureDeclaredMSWData(this->schedule_,
                                   simStep,
                                   this->schedule_.get().getUnits(),
                                   this->writeContext_->inteHead,
                                   this->grid_,
                                   this->writeContext_->state.get().summaryState,
                                   wellSol);

    this->writeArray("ISEG", MSWData.getISeg());
    this->writeArray("ILBS", MSWData.getILBs());
    this->writeArray("ILBR", MSWData.getILBr());
    this->writeArray("RSEG", MSWData.getRSeg());
}

void
Opm::RestartIO::RestartOutputManager::writeWellArrays() const
{
    const auto simStep = static_cast<std::size_t>
        (std::max(this->writeContext_->reportStep - 1, 0));

    const auto& wellSol = this->writeContext_->state.get()
        .solution[this->writeContext_->gridIndex].wells;

    const auto& smry = this->writeContext_->state.get().summaryState;
    const auto& tracers = this->es_.get().tracer();

    auto wellData = Helpers::AggregateWellData { this->writeContext_->inteHead };

    if (this->writeContext_->gridIndex == 0) {
        wellData.captureDeclaredWellData(this->schedule_,
                                         tracers,
                                         simStep,
                                         this->writeContext_->state.get().actionState,
                                         this->writeContext_->state.get().wellTestState,
                                         smry,
                                         this->writeContext_->inteHead);

        wellData.captureDynamicWellData(this->schedule_, tracers, simStep, wellSol, smry);
    }
    else {
        const auto lgr_tag = std::string {this->writeContext_->gridName};
        const auto& lgr_grid = this->grid_.get().getLGRCell(this->writeContext_->gridIndex - 1);

        wellData.captureDeclaredWellDataLGR(this->schedule_,
                                            lgr_grid,
                                            tracers,
                                            simStep,
                                            this->writeContext_->state.get().actionState,
                                            this->writeContext_->state.get().wellTestState,
                                            smry,
                                            this->writeContext_->inteHead,
                                            lgr_tag);

        wellData.captureDynamicWellDataLGR(this->schedule_, tracers, simStep,
                                           wellSol, smry, lgr_tag);
    }

    this->writeArray("IWEL", wellData.getIWell());
    this->writeArray("SWEL", wellData.getSWell());
    this->writeArray("XWEL", wellData.getXWell());
    this->writeArray("ZWEL", wellData.getZWell());

    if (this->writeContext_->gridIndex != 0) {
        // Well array specific to local grids.
        this->writeArray("LGWEL", wellData.getLGWell());
    }
}

void
Opm::RestartIO::RestartOutputManager::writeWellListArrays() const
{
    if (this->writeContext_->gridIndex != 0) {
        return; // No well list arrays for local grids.
    }

    const auto simStep = static_cast<std::size_t>
        (std::max(this->writeContext_->reportStep - 1, 0));

    auto wListData = Helpers::AggregateWListData{ this->writeContext_->inteHead };
    wListData.captureDeclaredWListData(this->schedule_, simStep, this->writeContext_->inteHead);

    this->writeArray("ZWLS", wListData.getZWls());
    this->writeArray("IWLS", wListData.getIWls());
}

void
Opm::RestartIO::RestartOutputManager::writeWellConnectionArrays() const
{
    const auto simStep = static_cast<std::size_t>
        (std::max(this->writeContext_->reportStep - 1, 0));

    const auto& wellSol = this->writeContext_->state.get()
        .solution[this->writeContext_->gridIndex].wells;

    auto connectionData = Helpers::AggregateConnectionData{ this->writeContext_->inteHead };

    if (this->writeContext_->gridIndex == 0) {
        connectionData.captureDeclaredConnData(this->schedule_,
                                               this->grid_,
                                               this->schedule_.get().getUnits(),
                                               wellSol,
                                               this->writeContext_->state.get().summaryState,
                                               simStep);
    }
    else {
        const auto lgr_tag = std::string { this->writeContext_->gridName };

        connectionData.captureDeclaredConnDataLGR(this->schedule_,
                                                  this->grid_,
                                                  this->schedule_.get().getUnits(),
                                                  wellSol,
                                                  this->writeContext_->state.get().summaryState,
                                                  simStep,
                                                  lgr_tag);
    }

    this->writeArray("ICON", connectionData.getIConn());
    this->writeArray("SCON", connectionData.getSConn());
    this->writeArray("XCON", connectionData.getXConn());
}

void
Opm::RestartIO::RestartOutputManager::analyticalAquiferData() const
{
    const auto& aquiferData = this->writeContext_->state.get().aquiferData.value();

    this->writeArray("IAAQ", aquiferData.getIntegerAquiferData());
    this->writeArray("SAAQ", aquiferData.getSinglePrecAquiferData());
    this->writeArray("XAAQ", aquiferData.getDoublePrecAquiferData());

    // Aquifer IDs in 1..maxID inclusive.
    const auto maxAquiferID = aquiferData.maximumActiveAnalyticAquiferID();
    std::ranges::for_each(std::views::iota(1) | std::views::take(maxAquiferID),
                          [this, &aquiferData](const int aquiferID)
    {
        // Note: The aquiferID is a one-based index. The get*AquiferConnectionData()
        // functions expect this and will convert to zero-based internally as needed.

        const auto xCAQnum = std::vector<int> {aquiferID};

        this->writeArray("ICAQNUM", xCAQnum);
        this->writeArray("ICAQ", aquiferData.getIntegerAquiferConnectionData(aquiferID));

        this->writeArray("SCAQNUM", xCAQnum);
        this->writeArray("SCAQ", aquiferData.getSinglePrecAquiferConnectionData(aquiferID));

        this->writeArray("ACAQNUM", xCAQnum);
        this->writeArray("ACAQ", aquiferData.getDoublePrecAquiferConnectionData(aquiferID));
    });
}

void
Opm::RestartIO::RestartOutputManager::numericalAquiferData() const
{
    const auto& aquiferData = this->writeContext_->state.get().aquiferData.value();

    this->writeArray("IAQN", aquiferData.getNumericAquiferIntegerData());
    this->writeArray("RAQN", aquiferData.getNumericAquiferDoublePrecData());
}

void
Opm::RestartIO::RestartOutputManager::regularSolutionArrays(const IsFluidInPlace& isFip) const
{
    const auto& allArrays = this->writeContext_->state.get()
        .solution[this->writeContext_->gridIndex].solution;

    auto regularArrays = allArrays | std::views::filter([&isFip](const auto& array)
    {
        const auto& [name, data] = array;

        return (data.target == data::TargetType::RESTART_SOLUTION)
            && (name != "TEMP") // TEMP is written along with the tracers
            && !isFip(name);
    });

    if (this->solutionArrayBackend_.has_value()) {
        std::ranges::for_each(regularArrays, [this](const auto& array) {
            const auto& [name, data] = array;

            (*this->solutionArrayBackend_)
                (name, data, this->writeContext_->rstFile);
        });
    }
    else {
        std::ranges::for_each(regularArrays, [this](const auto& array) {
            const auto& [name, data] = array;

            this->defaultSolutionArrayBackend(name, data);
        });
    }
}

void
Opm::RestartIO::RestartOutputManager::fluidInPlaceArrays(const IsFluidInPlace& isFip) const
{
    const auto& allArrays = this->writeContext_->state.get()
        .solution[this->writeContext_->gridIndex].solution;

    auto fipArrays = allArrays | std::views::filter([&isFip](const auto& array)
    {
        const auto& [name, data] = array;

        return (data.target == data::TargetType::RESTART_SOLUTION)
            && isFip(name);
    });

    if (std::ranges::empty(fipArrays)) {
        return; // No fluid-in-place arrays to write for this step.
    }

    {
        auto regSets = this->es_.get().fieldProps().fip_regions();

        std::ranges::sort(regSets);

        this->writeArray("FIPFAMNA", regSets);
    }

    if (this->solutionArrayBackend_.has_value()) {
        std::ranges::for_each(fipArrays, [this](const auto& array) {
            const auto& [name, data] = array;

            (*this->solutionArrayBackend_)
                (name, data, this->writeContext_->rstFile);
        });
    }
    else {
        std::ranges::for_each(fipArrays, [this](const auto& array) {
            const auto& [name, data] = array;
            this->defaultSolutionArrayBackend(name, data);
        });
    }

    const auto haveRSFip = std::ranges::any_of(fipArrays | std::views::transform([](const auto& array) {
        return array.first.front();
    }), [](const auto& firstChar) {
        return (firstChar == 'R') || (firstChar == 'S');
    });

    if (haveRSFip) {
        // The simulator provides properly tagged in-place arrays.  No
        // further action needed.
        return;
    }

    // If we get here, all fluid-in-place vectors have the name FIP* and
    // represent surface condition volumes.  Output the same vectors
    // using the corresponding SFIP name as well.
    if (this->solutionArrayBackend_.has_value()) {
        std::ranges::for_each(fipArrays, [this](const auto& array) {
            const auto& [name, data] = array;
            (*this->solutionArrayBackend_)
                ('S' + name, data, this->writeContext_->rstFile);
        });
    }
    else {
        std::ranges::for_each(fipArrays, [this](const auto& array) {
            const auto& [name, data] = array;
            this->defaultSolutionArrayBackend('S' + name, data);
        });
    }
}

void
Opm::RestartIO::RestartOutputManager::tracerArrays() const
{
    auto tracerHeaderTemp = [tempUnit = this->schedule_.get().getUnits().name(UnitSystem::measure::temperature)]()
        -> std::pair<std::string_view, std::vector<EclIO::PaddedOutputString<8>>>
    {
        return {
            std::piecewise_construct,
            std::forward_as_tuple("ZATRACER"),
            std::forward_as_tuple(std::initializer_list<EclIO::PaddedOutputString<8>> {
                EclIO::PaddedOutputString<8>{ "TEMP" },
                EclIO::PaddedOutputString<8>{ tempUnit }
            })
        };
    };

    auto tracerHeaderNormal = [volUnit = this->schedule_.get().getUnits().name(UnitSystem::measure::volume),
                               &trConfig = this->es_.get().tracer()](std::string_view tracerName)
        -> std::pair<std::string_view, std::vector<EclIO::PaddedOutputString<8>>>
    {
        // Trim trailing 'F'/'S' from tracer name to get the corresponding tracer unit.
        tracerName.remove_suffix(1);
        const auto& tracerUnit = trConfig[std::string { tracerName }].unit_string;

        return {
            std::piecewise_construct,
            std::forward_as_tuple("ZTRACER"),
            std::forward_as_tuple(std::initializer_list<EclIO::PaddedOutputString<8>> {
                EclIO::PaddedOutputString<8>{ tracerName },
                EclIO::PaddedOutputString<8>{ fmt::format("{}/{}", tracerUnit, volUnit) }
            })
        };
    };

    auto tracerHeader = [&tracerHeaderNormal, &tracerHeaderTemp](std::string_view tracerName)
        -> std::pair<std::string_view, std::vector<EclIO::PaddedOutputString<8>>>
    {
        if (tracerName == "TEMP") {
            return tracerHeaderTemp();
        }
        else {
            return tracerHeaderNormal(tracerName);
        }
    };

    const auto& allArrays = this->writeContext_->state.get()
        .solution[this->writeContext_->gridIndex].solution;

    auto tracerArrays = allArrays | std::views::filter([](const auto& array)
    {
        const auto& [name, data] = array;

        return (data.target == data::TargetType::RESTART_TRACER_SOLUTION)
            || (name == "TEMP");
    });

    if (this->solutionArrayBackend_.has_value()) {
        std::ranges::for_each(tracerArrays, [this, &tracerHeader](const auto& array) {
            const auto& [name, data] = array;

            const auto& [headerName, headerValues] = tracerHeader(name);
            this->writeArray(headerName, headerValues);

            (*this->solutionArrayBackend_)
                (name, data, this->writeContext_->rstFile);
        });
    }
    else {
        std::ranges::for_each(tracerArrays, [this, &tracerHeader](const auto& array) {
            const auto& [name, data] = array;

            const auto& [headerName, headerValues] = tracerHeader(name);
            this->writeArray(headerName, headerValues);

            this->defaultSolutionArrayBackend(name, data);
        });
    }
}

void
Opm::RestartIO::RestartOutputManager::udqArrays() const
{
    if (this->writeContext_->reportStep == 0) {
        return; // No UDQ data to write for initial condition.
    }

    const auto simStep = static_cast<std::size_t>
        (this->writeContext_->reportStep - 1);

    const auto& udqConfig = this->schedule_.get()[simStep].udq();

    if (udqConfig.size() == 0) {
        // Initial condition or no UDQs in run.
        return;
    }

    const auto udqDims = UDQDims { udqConfig, this->writeContext_->inteHead };

    // UDQs are active in run.  Write UDQ related data to restart file.
    auto udqData = Helpers::AggregateUDQData(udqDims);
    udqData.captureDeclaredUDQData(this->schedule_,
                                   simStep,
                                   this->writeContext_->state.get().udqState,
                                   this->writeContext_->inteHead);

    this->writeArray("ZUDN", udqData.getZUDN());
    this->writeArray("ZUDL", udqData.getZUDL());
    this->writeArray("IUDQ", udqData.getIUDQ());

    if (const auto& dudf = udqData.getDUDF(); dudf.has_value()) {
        this->writeArray("DUDF", dudf->data());
    }

    if (const auto& dudg = udqData.getDUDG(); dudg.has_value()) {
        this->writeArray("DUDG", dudg->data());
    }

    if (const auto& duds = udqData.getDUDS(); duds.has_value()) {
        this->writeArray("DUDS", duds->data());
    }

    if (const auto& dudw = udqData.getDUDW(); dudw.has_value()) {
        this->writeArray("DUDW", dudw->data());
    }

    if (const auto& iuad = udqData.getIUAD(); iuad.has_value()) {
        this->writeArray("IUAD", iuad->data());
    }

    if (const auto& iuap = udqData.getIUAP(); iuap.has_value()) {
        this->writeArray("IUAP", iuap->data());
    }

    if (const auto& igph = udqData.getIGPH(); igph.has_value()) {
        this->writeArray("IGPH", igph->data());
    }
}

void
Opm::RestartIO::RestartOutputManager::thresholdPressureArray() const
{
    const auto arrayName = std::string {"THRESHPR"};

    const auto& solution = this->writeContext_->state.get()
        .solution[this->writeContext_->gridIndex].extra;

    auto thpressArrays = solution | std::views::filter([&arrayName](const auto& array)
    {
        return array.first.key == arrayName;
    });

    if (this->solutionArrayBackend_.has_value()) {
        std::ranges::for_each(thpressArrays, [this, &arrayName](const auto& array) {
            const auto& [key, thpressInput] = array;

            auto thpress = data::CellData {
                key.dim, thpressInput, data::TargetType::RESTART_SOLUTION
            };

            (*this->solutionArrayBackend_)
                (arrayName, thpress, this->writeContext_->rstFile.get());
        });
    }
    else {
        std::ranges::for_each(thpressArrays, [this, &arrayName](const auto& array) {
            const auto& [key, thpressInput] = array;

            auto convertedThresholdPr = thpressInput;

            // Note: We always write THRESHPR as a double precision array.
            this->schedule_.get().getUnits().from_si(key.dim, convertedThresholdPr);

            this->writeArray(arrayName, convertedThresholdPr);
        });
    }
}

void
Opm::RestartIO::RestartOutputManager::extendedSolutionArrays() const
{
    const auto& allArrays = this->writeContext_->state.get()
        .solution[this->writeContext_->gridIndex].solution;

    auto extendedArrays = allArrays | std::views::filter([](const auto& array)
    {
        const auto target = array.second.target;

        return (target == data::TargetType::RESTART_OPM_EXTENDED)
            || (target == data::TargetType::RESTART_AUXILIARY);
    });

    if (this->solutionArrayBackend_.has_value()) {
        std::ranges::for_each(extendedArrays, [this](const auto& array) {
            const auto& [name, data] = array;

            (*this->solutionArrayBackend_)
                (name, data, this->writeContext_->rstFile);
        });
    }
    else {
        std::ranges::for_each(extendedArrays, [this](const auto& array) {
            const auto& [name, data] = array;

            this->defaultSolutionArrayBackend(name, data);
        });
    }
}

void
Opm::RestartIO::RestartOutputManager::lgrNames() const
{
    if (! this->grid_.get().is_lgr()) {
        return; // No LGR names to write if this is not an LGR case.
    }

    const auto lgrLabels = (this->writeContext_->gridIndex == 0)
        ? this->grid_.get().get_all_lgr_labels() // Main grid
        : this->grid_.get().getLGRCell(this->writeContext_->gridIndex - 1).get_all_lgr_labels();

    if (lgrLabels.empty()) {
        return; // No LGR names to write for this grid.
    }

    this->writeArray("LGRNAMES", lgrLabels);
}

void
Opm::RestartIO::RestartOutputManager::defaultSolutionArrayBackend(const std::string&    arrayName,
                                                                  const data::CellData& value) const
{
    const auto& usys = this->schedule_.get().getUnits();

    value.visit(VisitorOverloadSet {
        MonoThrowHandler<std::logic_error> {
            fmt::format("{} does not have an associate value", arrayName)
        },
        [&arrayName, &usys, unit = value.dim, this](const std::vector<double>& v)
        {
            auto outputValue = v;
            usys.from_si(unit, outputValue);

            this->writeArray(arrayName, std::vector<float> { outputValue.begin(), outputValue.end() });
        },
        [&arrayName, this](const std::vector<int>& v)
        {
            this->writeArray(arrayName, v);
        }
    });
}
