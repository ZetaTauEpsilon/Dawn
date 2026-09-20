#include "mission_launch.h"
#include "mission_launch_options.h"
#include "campaign_openings.h"
#include "campaign_dialogue.h"
#include "nightfall_player.h"
#include "../hooks/bootflow/mission_prelaunch.h"
#include "mission_launch_testing.h"
#include <Windows.h>
#include <array>
#include <cstring>
#include <cstdio>
#include "../../state/build_data/activities/activity_catalog.h"
#include "../../state/build_data/runtime.h"
#include "../../state/activity/forced/activity_forced_destination.h"
#include "../../state/activity/runtime.h"
#include "../../state/activity/gateway_intro.h"
#include "../../state/activity/vanilla/homecoming/prologue.h"
#include "../../state/activity/destination/activity_destination_snapshot.h"
#include "../../core/logging/log.h"

namespace dawn::client::activity::mission_launch {
namespace {
SRWLOCK g_lock{SRWLOCK_INIT};
Snapshot g_state{};
std::uint64_t g_requestedAt{};
// Game-thread-only receipt state, scoped to a successfully submitted request.
std::uint64_t g_previousSession{};
bool g_leftOrbit{};
ManualScratch g_manualScratch{}; // Game-frame owner only, outside the UI arena and native stack.
using World = std::uintptr_t(__fastcall*)();
using Ready = bool(__fastcall*)(std::uintptr_t);
using Record = std::uintptr_t(__fastcall*)(std::uint32_t);
using Construct = void*(__fastcall*)(void*, std::uint32_t, std::int16_t);
using Valid = bool(__fastcall*)(const void*);
using Name = const char*(__fastcall*)(std::int16_t);
using Clear = void(__fastcall*)();
using Select = void(__fastcall*)(std::uint8_t, const void*);
using Commit = void(__fastcall*)(std::int32_t);
using Step = std::int32_t(__fastcall*)();
template<class T> bool read(std::uintptr_t address, T& value) noexcept {
    SIZE_T copied{};
    return address != 0 && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address),
        &value, sizeof(value), &copied) != FALSE && copied == sizeof(value);
}
template<class F> F resolve(std::uintptr_t base, std::uintptr_t rva,
                            std::array<unsigned char, 8> expected) noexcept {
#if defined(DAWN_MISSION_LAUNCH_TESTS)
    (void)base; (void)expected;
    return reinterpret_cast<F>(testing::native_entry(rva));
#else
    std::array<unsigned char, 8> actual{};
    return read(base + rva, actual) && actual == expected ? reinterpret_cast<F>(base + rva) : nullptr;
#endif
}
std::uint64_t now() noexcept {
#if defined(DAWN_MISSION_LAUNCH_TESTS)
    return testing::now();
#else
    return GetTickCount64();
#endif
}
#include "gateway_intro_native.inl"
namespace prologue = state::activity::vanilla::homecoming::prologue;
/** @return True when the installed catalog carries the opening video activity before Homecoming. */
bool prologue_available() noexcept {
    return prologue::catalog_valid(state::build_data::activities::entries());
}
/** @return True when the loaded world is the requested opening with its authored arrival coordinates. */
bool arrival_matches(const Snapshot& state, const state::activity::destination::DestinationSelection& actual,
                     std::span<const state::build_data::activities::Definition> rows) noexcept {
    if (state.index >= rows.size()) { return false; }
    const auto expectedName = state.manual ? destination_name(state.destination) : rows[state.index].name();
    return ((state.manual && !state.opening) || actual.activityIndex == static_cast<std::int16_t>(state.index))
        && actual.packageNameLength == expectedName.size()
        && std::memcmp(actual.packageName.data(), expectedName.data(), expectedName.size()) == 0
        && (!state.manual || ((!state.destination.hasBubble || (actual.hasArrivalBubbleOverride
            && actual.arrivalBubbleOverride == state.destination.bubble))
            && (!state.destination.hasSliceSet || (actual.hasSliceSetOverride
                && actual.sliceSetOverride == state.destination.sliceSet))
            && (!state.destination.hasBubble || (actual.hasSpawnSetOverride
                && actual.spawnSetOverride == (state.destination.hasSpawnSetHash
                    ? state.destination.spawnSetHash : forced::kAbsentSpawnSetHash)))));
}
void finish(Status status) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if (g_state.status == status) { ReleaseSRWLockExclusive(&g_lock); return; }
    g_state.status = status;
    g_state.busy = status == Status::queued || status == Status::preparing || status == Status::cinematics;
    const auto state = g_state;
    ReleaseSRWLockExclusive(&g_lock);
    if (status != Status::queued && status != Status::arrived && status != Status::preparing && status != Status::cinematics) {
        if(intro::active()) intro::fail();
        if(prologue::active()) prologue::fail();
        g_gatewayExit={};
    }
    std::array<char, 512> line{};
    const int size = state.manual ? std::snprintf(line.data(), line.size(),
        "ev=mission_launch activity=%u manual=1 destination=%.*s bubble=%u slice=%u spawn=%08X current_destination=%.*s current_activity=%d in_mission=%u status=%u detail=%s",
        state.index, static_cast<int>(destination_name(state.destination).size()), state.destination.packageName.data(),
        state.destination.bubble, state.destination.sliceSet,
        state.destination.hasSpawnSetHash ? state.destination.spawnSetHash : forced::kAbsentSpawnSetHash,
        static_cast<int>(state.currentPackageLength), state.currentPackage.data(),
        static_cast<int>(state.currentIndex), static_cast<unsigned>(state.inMission),
        static_cast<unsigned>(status), description(status))
        : std::snprintf(line.data(), line.size(), "ev=mission_launch activity=%u status=%u detail=%s", state.index,
            static_cast<unsigned>(status), description(status));
    if (size > 0 && static_cast<std::size_t>(size) < line.size()) {
        core::log::write(core::log::Channel::client, core::log::Level::info,
            {line.data(), static_cast<std::size_t>(size)});
    }
}
}
Snapshot snapshot() noexcept {
    AcquireSRWLockShared(&g_lock);
    const auto result = g_state;
    ReleaseSRWLockShared(&g_lock);
    return result;
}
bool request(std::uint16_t index) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if (g_state.busy || g_state.inMission) { ReleaseSRWLockExclusive(&g_lock); return false; }
    g_state = {Status::requested, index, true};
    g_requestedAt = now();
    ReleaseSRWLockExclusive(&g_lock);
    return true;
}
bool request_manual(std::uint16_t index, const forced::ForcedDestination& destination) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if (g_state.busy || g_state.inMission) { ReleaseSRWLockExclusive(&g_lock); return false; }
    g_state = {Status::requested, index, true, true, destination};
    g_requestedAt = now();
    ReleaseSRWLockExclusive(&g_lock);
    return true;
}
bool request_opening(std::size_t mission) noexcept {
    return request_variant(mission, state::activity::strikes::Difficulty::standard);
}
bool request_variant(std::size_t mission, state::activity::strikes::Difficulty difficulty) noexcept {
    return request_variant(mission, difficulty, state::activity::nightfall::defaults(difficulty));
}
bool request_variant(std::size_t mission, state::activity::strikes::Difficulty difficulty,
                     state::activity::nightfall::Options options) noexcept {
    const auto route = openings::resolve(mission, state::build_data::activities::entries(), difficulty);
    if (!route.valid()) { return false; }
    AcquireSRWLockExclusive(&g_lock);
    if (g_state.busy || g_state.inMission) { ReleaseSRWLockExclusive(&g_lock); return false; }
    g_state = {Status::requested, route.transport, true, true, route.destination, true};
    g_state.nightfallOptions = state::activity::nightfall::sanitize(difficulty, options);
    g_requestedAt = now();
    ReleaseSRWLockExclusive(&g_lock);
    return true;
}
void poll() noexcept {
    const auto state = snapshot();
    // Requests are immutable until this owner completes them; the timestamp follows that lock.
    AcquireSRWLockShared(&g_lock);
    const auto started = g_requestedAt;
    ReleaseSRWLockShared(&g_lock);
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto step = resolve<Step>(base, 0xE2D510, {0x48,0x83,0xEC,0x28,0xE8,0x07,0x83,0x00});
    const auto currentStep = step ? step() : -1;
    const auto sessionId = state::activity::newest_joined_session();
    state::activity::destination::DestinationSelection actual{};
    const bool inMission = currentStep == 38 && sessionId != 0
        && state::activity::destination::snapshot(sessionId, actual)
        && actual.packageNameLength > 0 && actual.packageNameLength < actual.packageName.size();
    // Presence follows the loaded world, even after a request finishes or a Director launch.
    AcquireSRWLockExclusive(&g_lock);
    g_state.inMission = inMission;
    g_state.currentIndex = inMission ? actual.activityIndex : -1;
    g_state.currentPackage.fill(0);
    g_state.currentPackageLength = inMission ? actual.packageNameLength : 0;
    if (inMission) {
        std::memcpy(g_state.currentPackage.data(), actual.packageName.data(), actual.packageNameLength);
    }
    if (!g_state.busy && currentStep == 29
        && (g_state.status == Status::arrived || g_state.status == Status::unexpectedDestination)) {
        g_state.status = Status::idle;
    }
    ReleaseSRWLockExclusive(&g_lock);
    // Keep the native flag through loading and clear the lease once back in orbit.
    if (inMission) {
        (void)campaign_dialogue::select(actual.activityIndex);
        const auto run = state::activity::mission_run_generation();
        state::activity::nightfall::enter(sessionId, actual.activityIndex, run);
        const std::string_view actualPackage{reinterpret_cast<const char*>(actual.packageName.data()), actual.packageNameLength};
        // A queued launch can briefly expose the previous world. Bind only after its new
        // session and exact destination pass the arrival checks below.
        nightfall_player::poll(sessionId, run);
    } else if (!state.busy && currentStep==29) {
        (void)campaign_dialogue::select(-1);
        state::activity::nightfall::leave();
    }
    if (!state.busy) { return; }
    if (!step) { finish(Status::nativeUnavailable); return; }
    const auto rows = state::build_data::activities::entries();
    const bool gateway=state.index==intro::kMission && (state.opening || !state.manual);
    if(gateway && state.status==Status::cinematics) {
        intro::tick(now());
        if(intro::state().phase==intro::Phase::failed) {finish(Status::timedOut);return;}
        if(!gateway_video_poll(base,currentStep)) {finish(Status::nativeUnavailable);return;}
        if(intro::wanted()<0) return;
    }
    // The Red War opening: the client plays the opening video activity and advances into
    // Homecoming itself, exactly like Gateway's video chain. Without the installed video
    // entry the mission launches directly.
    const bool homecoming=state.index==prologue::kMission && state.opening
        && (state.status==Status::cinematics || prologue_available());
    if(homecoming && state.status==Status::cinematics) {
        prologue::tick(now());
        if(prologue::state().phase==prologue::Phase::failed) {finish(Status::timedOut);return;}
        if(!gateway_video_poll(base,currentStep,true)) {finish(Status::nativeUnavailable);return;}
        if(prologue::state().phase==prologue::Phase::missionLoading) {
            // Retail selected the mission from the video; the loaded world is the chain's arrival.
            if(!inMission || sessionId==g_previousSession) return;
            const bool matches=arrival_matches(state,actual,rows);
            if(matches) prologue::complete();
            finish(matches ? Status::arrived : Status::unexpectedDestination);return;
        }
        if(prologue::wanted()<0) return;
    }
    if (state.status == Status::queued) {
        if(gateway) gateway_depart(base,currentStep);
        g_leftOrbit = g_leftOrbit || currentStep != 29;
        if (g_leftOrbit && inMission && sessionId != g_previousSession && state.index < rows.size()) {
            const bool matches = arrival_matches(state, actual, rows);
            if(gateway && matches) intro::complete();
            if(homecoming && matches) prologue::complete();
            finish(matches ? Status::arrived : Status::unexpectedDestination); return;
        }
        if (now() - started > 120000) { finish(Status::timedOut); }
        return;
    }
    if (rows.empty()) { finish(Status::catalogUnavailable); return; }
    if (state.index >= rows.size() || rows[state.index].name().empty()) {
        finish(Status::entryUnavailable); return;
    }
    state::build_data::scenarios::Definition layout{};
    if (!state::build_data::find_scenario_layout(rows[state.index].name(), layout)) {
        finish(Status::entryUnavailable); return;
    }
    if(gateway) {
        state::build_data::scenarios::Definition briefing{};
        if(!intro::catalog_valid(rows) || !state::build_data::find_scenario_layout(intro::kPackage,briefing)
            || briefing.tag!=intro::kScenario) {finish(Status::entryUnavailable);return;}
    }
    if (state.manual) {
        if ((!state.opening && !manual_transport_valid(state.index, state.destination, rows))
            || validate_manual(state.destination, g_manualScratch) != ManualError::none) {
            finish(Status::manualRejected); return;
        }
    } else {
        forced::ForcedDestination effective{};
        forced::snapshot(effective);
        // A staged Homecoming override is not operational yet, but Chosen would activate it.
        if (forced::active(effective) || forced::override_active()) { finish(Status::overrideActive); return; }
    }
    // Captured retail setup:orbit is 29. In-world exit/reclassification belongs to the native
    // activity lifecycle and is deliberately not synthesized by this UI request adapter.
    const bool departingBriefing=gateway && state.status==Status::cinematics && intro::wanted()==intro::kMission;
    if (currentStep != 29 && !(departingBriefing && currentStep==38)) { finish(Status::returnToOrbit); return; }
    const auto world = resolve<World>(base,0xC03430,{0x40,0x56,0x48,0x83,0xEC,0x30,0x48,0x8B});
    const auto sessionReady = resolve<Ready>(base,0x1788810,{0x83,0xB9,0x6C,0x08,0x00,0x00,0x00,0x0F});
    const auto memberReady = resolve<Ready>(base,0x178D740,{0x4C,0x8B,0xC1,0x48,0x63,0x89,0x3C,0xE9});
    const auto record = resolve<Record>(base,0xBFA030,{0x40,0x53,0x48,0x83,0xEC,0x20,0x8B,0xD9});
    const auto construct = resolve<Construct>(base,0xC061D0,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74});
    const auto valid = resolve<Valid>(base,0x4D5460,{0x0F,0xB6,0x11,0xB0,0x01,0x80,0xFA,0xFF});
    const auto name = resolve<Name>(base,0xDDECA0,{0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83});
    const auto clear = resolve<Clear>(base,0xBF95D0,{0x48,0x83,0xEC,0x38,0xE8,0xE7,0xE0,0x7F});
    const auto select = resolve<Select>(base,0xBFB1F0,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74});
    const auto commit = resolve<Commit>(base,0xBF97D0,{0x89,0x4C,0x24,0x08,0x48,0x83,0xEC,0x38});
    if (!world || !sessionReady || !memberReady || !record || !construct || !valid || !name
        || !clear || !select || !commit) { finish(Status::nativeUnavailable); return; }
    const auto manager = world();
    std::int32_t primary{}, sessionState{}, member{};
    if (!manager || !read(manager + 0x10, primary) || primary < 0 || primary > 3) {
        finish(Status::notReady); return;
    }
    const auto session = manager + 0x18 + static_cast<std::uintptr_t>(primary) * 0x1C8A0;
    if (!read(session + 0x1AEF8, sessionState) || sessionState < 4 || sessionState > 9
        || !sessionReady(session) || !memberReady(session)
        || !read(session + 0xE93C, member) || member < 0 || member >= 12) {
        finish(Status::notReady); return;
    }
    const auto current = record(static_cast<std::uint32_t>(member));
    std::uint8_t launchState{};
    if (!current || !read(current + 0xA33, launchState) || launchState >= 3) {
        finish(Status::notReady); return;
    }
    const auto index = gateway ? (state.status==Status::cinematics ? intro::wanted() : intro::kIntroduction)
        : homecoming ? (state.status==Status::cinematics ? prologue::wanted() : prologue::kVideo)
        : static_cast<std::int16_t>(state.index);
    std::array<char, 40> nativeName{};
    const auto package = reinterpret_cast<std::uintptr_t>(name(index));
    bool nameMatches = package != 0;
    for (std::size_t i = 0; nameMatches && i <= rows[index].name().size(); ++i) {
        nameMatches = read(package + i, nativeName[i]) && nativeName[i] == rows[index].package[i];
    }
    if (!nameMatches) { finish(Status::descriptorRejected); return; }
    alignas(16) std::array<std::byte, 0x120> selection{};
    if (construct(selection.data(), 0, index) != selection.data()) {
        finish(Status::descriptorRejected); return;
    }
    std::int16_t source{}, destination{};
    std::memcpy(&source, selection.data() + 2, sizeof(source));
    std::memcpy(&destination, selection.data() + 4, sizeof(destination));
    if (source != index || destination != index || selection[0] != std::byte{}
        || !valid(selection.data())) { finish(Status::descriptorRejected); return; }
    // Wait for the native opening support before publishing the exact selected activity.
    if (state.manual && !hooks::bootflow::prepare_mission_prelaunch(state.destination)) {
        finish(now() - started > 10000 ? Status::prelaunchUnavailable : Status::preparing);
        return;
    }
    if (!campaign_dialogue::select((gateway && index!=intro::kMission) || (homecoming && index!=prologue::kMission) ? -1 : index)) {
        finish(now()-started>10000 ? Status::prelaunchUnavailable : Status::preparing);
        return;
    }
    if(gateway && index!=intro::kMission) {
        if(index<intro::kBriefing && !gateway_video_reset(base)) {finish(Status::nativeUnavailable);return;}
        if(state.status!=Status::cinematics) {intro::begin(now());forced::clear();}
        if(!intro::queued(index,now())) {finish(Status::descriptorRejected);return;}
        clear();select(0,selection.data());commit(1);
        std::array<char,96> line{};std::snprintf(line.data(),line.size(),"ev=gateway_intro stage=queued activity=%d",index);
        core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
        finish(Status::cinematics);return;
    }
    if(homecoming && index!=prologue::kMission) {
        if(!gateway_video_reset(base)) {finish(Status::nativeUnavailable);return;}
        if(state.status!=Status::cinematics) {prologue::begin(now());forced::clear();}
        if(!prologue::queued(index,now())) {finish(Status::descriptorRejected);return;}
        // The client advances the video into the mission itself, so the mission's opening
        // coordinates are published now, scoped to the mission and its chain source.
        if(!forced::publish_direct(state.destination,prologue::kMission,index)) {finish(Status::manualRejected);return;}
        g_previousSession = state::activity::newest_joined_session();
        g_leftOrbit = false;
        clear();select(0,selection.data());commit(1);
        std::array<char,96> line{};std::snprintf(line.data(),line.size(),"ev=homecoming stage=prologue_queued activity=%d",index);
        core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
        finish(Status::cinematics);return;
    }
    std::uint64_t departureNonce{};
    if(departingBriefing) {
        if(!gateway_nonce(session,departureNonce)) return;
        std::memcpy(selection.data()+0x10,&departureNonce,sizeof departureNonce);
        if(!valid(selection.data())) {finish(Status::descriptorRejected);return;}
    }
    g_previousSession = state::activity::newest_joined_session();
    g_leftOrbit = false;
    // Coordinates are scoped to the activity we constructed. Its identity and opaque native
    // launch descriptor remain intact, including campaign versus strike presentation.
    if (state.opening) {
        if (!forced::publish_direct(state.destination, index)) { finish(Status::manualRejected); return; }
    } else if (state.manual && !forced::publish(state.destination)) { finish(Status::manualRejected); return; }
    clear();
    state::activity::nightfall::arm(index, state.nightfallOptions);
    select(0, selection.data());
    commit(1);
    if(departingBriefing) {
        static_cast<void>(intro::queued(index,now()));
        g_gatewayExit={session,departureNonce,state::activity::mission_run_generation(),true};
        AcquireSRWLockExclusive(&g_lock);g_requestedAt=now();ReleaseSRWLockExclusive(&g_lock);
    }
    if(homecoming && state.status==Status::cinematics) {
        // The video ended in orbit without the native chain; the adapter's own launch follows.
        static_cast<void>(prologue::queued(index,now()));
        AcquireSRWLockExclusive(&g_lock);g_requestedAt=now();ReleaseSRWLockExclusive(&g_lock);
    }
    finish(Status::queued);
}
const char* description(Status status) noexcept {
    switch (status) {
    case Status::idle: return "Choose an activity, return to orbit, then launch.";
    case Status::requested: return "Checking the native launch request...";
    case Status::queued: return "Launching the selected activity. Waiting for arrival.";
    case Status::arrived: return "In mission.";
    case Status::preparing: return "Preparing the mission opening...";
    case Status::cinematics: return "Playing the opening cinematics...";
    case Status::prelaunchUnavailable: return "The mission opening could not be prepared. Wait in orbit and try again.";
    case Status::unexpectedDestination: return "The selected opening did not load. Return to orbit and try again.";
    case Status::catalogUnavailable: return "Activity catalog is still being extracted.";
    case Status::entryUnavailable: return "This entry has no available direct-launch scenario.";
    case Status::overrideActive: return "An Activity override is active. Disable it before launching this selection.";
    case Status::returnToOrbit: return "Return to orbit through the Director, then click Launch again.";
    case Status::nativeUnavailable: return "Native launch entry points are unavailable in this client build.";
    case Status::notReady: return "The fireteam is not ready to launch. Wait in orbit and try again.";
    case Status::descriptorRejected: return "The native client rejected this activity selection.";
    case Status::timedOut: return "No arrival confirmation was received. Check the Director before retrying.";
    case Status::manualRejected: return "This mission opening is unavailable in the installed content. Check the activity selection.";
    }
    return "Launch status unavailable.";
}
} // namespace dawn::client::activity::mission_launch
