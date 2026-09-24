#pragma once
#include "controller.h"
#include "../../coo/mission_script.h"

namespace dawn::state::activity::vanilla::one_au {
inline constexpr coo::script::Capability kEntryCapabilities[]{
    {"mission.native", "composition", {coo::Operation::mechanic, kModule, 1, coo::Wait::requested}},
    {"mission.finished", "composition", {coo::Operation::observation, {}, 0, coo::Wait::observed}},
};
inline constexpr coo::script::ModuleCapability kEntryModules[]{{"native", {kModule, 1}}};
inline constexpr coo::script::FactCapability kEntryFacts[]{{"mission.finished", 0}};
inline constexpr coo::script::Profile kEntryProfile{
    "one_au.native.v1", "otherMissions", coo::Schema::otherMissions,
    kEntryCapabilities, kEntryModules, kEntryFacts, {}, {}, {}, {}, {}};

inline bool valid_document(const coo::script::Views& views) noexcept {
    const auto* root = views.role("mission");
    if (!views.valid || views.missionId != "one_au" || views.profileId != kEntryProfile.id
        || views.graphs.size() != 1 || !root || root->domain != "composition"
        || root->definition.steps.data() != views.mission.sequence.steps.data()
        || views.mission.modules.size() != 1 || views.mission.modules[0].asset != kModule
        || views.mission.modules[0].id != 1 || views.mission.observations.size() != 1
        || views.mission.observations[0].fact != 0 || !views.phases.empty()
        || !views.conditions.empty() || views.observationStart) { return false; }
    // The module must start at selection, before opening-camera preparation.
    // A completion wait before its request would deadlock the native handoff.
    for (const auto& step : root->definition.steps) {
        for (const auto& command : step.commands) {
            if (command.operation == coo::Operation::mechanic && step.dependencies) { return false; }
        }
    }
    return coo::script::authorized(views, kEntryProfile) && coo::MissionRuntime::valid(views.mission);
}

// A small composition adapter; native checkpoint resets remain inside Controller
// and cannot reset the process-owned Lua document or its mission completion wait.
class Entry final {
public:
    bool select(const coo::script::Views& views, Controller& native,
                std::uint64_t run, std::uint64_t now = 0) noexcept {
        if (!run || !valid_document(views)) { reset(); native.reset(); return false; }
        if (run_ == run) { return views_ == &views; }
        reset(); views_ = &views; run_ = run;
        Ports ports{native, true};
        const auto status = composition_.update(views.mission, {run, now, 0, false, true}, ports);
        if (!status.selected) { reset(); native.reset(); return false; }
        return true;
    }
    Frame update(Controller& native, std::uint64_t run, std::uint64_t now, bool ready) noexcept {
        if (!views_ || run != run_ || !ready) { return {}; }
        Ports ports{native, false};
        const auto status = composition_.update(views_->mission, {run, now, 0, false, true}, ports);
        return status.selected ? native.frame() : Frame{};
    }
    void reset() noexcept { composition_.reset(); views_ = nullptr; run_ = 0; }
    coo::Diagnostics diagnostics() const noexcept { return composition_.diagnostics(); }
private:
    struct Status { bool selected{}, finished{}; };
    struct Ports final : coo::MissionPorts<Status> {
        Controller& native;
        bool selecting;
        Ports(Controller& controller, bool select) noexcept : native(controller), selecting(select) {}
        void update_module(std::uint32_t id, const coo::MissionInput& input, Status& out) noexcept override {
            if (id != 1) { return; }
            if (selecting) { out.selected = native.select(input.run, input.now); }
            else { out.selected = native.advance(input.run, input.now, true); }
            out.finished = out.selected && native.frame().finished;
        }
        std::uint32_t observations(std::uint64_t run, const Status& out) noexcept override {
            return run == native.owner().run && out.finished ? 1U : 0U;
        }
    };
    coo::MissionRuntime composition_{};
    const coo::script::Views* views_{};
    std::uint64_t run_{};
};
}
