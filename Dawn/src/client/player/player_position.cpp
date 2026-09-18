#include "../../state/activity/vanilla/one_au/runtime.h"
#include "../../state/activity/gateway/runtime.h"
#include "../../state/activity/deadly_trial/runtime.h"
#include "../../state/activity/beyond_infinity/runtime.h"
#include "../../state/activity/deep_storage/runtime.h"
#include "../../state/activity/hijacked/runtime.h"
#include "../../state/activity/Newlight/launchpad/runtime.h"
#include "../../state/activity/strike_bond/runtime.h"
/**
 * The local player's published world position.
 * The game threads write it and the interface reads it, so a seqlock guards the vector.
 */

#include "player_position.h"
#include "../../state/activity/omega_presentation.h"
#include "../../state/activity/omega_first_lair_runtime.h"
#include "../../state/activity/omega_crown_transit_geometry.h"
#include "../../state/activity/runtime.h"
#include "../../core/logging/log.h"
#include "../hooks/bootflow/public_event_participant_observer.h"

#include <atomic>
#include <Windows.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <mutex>

namespace {
std::atomic_uint64_t g_crownRouteRejectRun{};
std::atomic_uint64_t g_crownRouteRejectMask{};
std::atomic_uint64_t g_crownRouteNoPlayerRun{};
}

namespace dawn::client::player::position {
namespace {

namespace teleport = hooks::teleport;

/** Odd while a write is in progress, so a reader that sees one retries. */
std::atomic_uint32_t g_sequence{0};
/** Written between two sequence bumps, and read between two equal even reads. */
teleport::Vector g_position{};
std::atomic_bool g_present{false};
/**
 * The player's physics component, found on the sync tick.
 * It is kept here rather than taken from the teleport hook, because that hook only caches one
 * while the teleport feature is switched on.
 */
std::atomic<void*> g_component{nullptr};
std::mutex g_physicsObserverMutex;
PhysicsObserver g_physicsObserver{};

/** Native local-player route observations; geometry never moves the player or
 * manufactures pickup/dunk. The encounter owner deduplicates salted tokens. */
void observe_crown_route(void* component,const teleport::Vector& position) noexcept {
    namespace lair=state::activity::omega_first_lair;
    namespace transit=state::activity::omega_crown_transit;
    const auto nav=state::activity::omega_presentation::navigation();
    if(!nav.enabled || nav.run==0 || nav.run!=state::activity::mission_run_generation()) { return; }
    const auto status=lair::status(nav.run);
    if(!status.enabled || status.failed || !status.token.valid()
        || status.token.boss.run!=nav.run) { return; }
    const bool route=status.crownStage==lair::CrownStage::route
        || status.crownStage==lair::CrownStage::carrying;
    const bool finalApproach=status.crownStage==lair::CrownStage::finalArrival
        || (status.crownStage==lair::CrownStage::relocation
            && status.phase==lair::Phase::mechanicRequested);
    if(!route && !finalApproach) { return; }
    std::uint32_t player=UINT32_MAX;
    if(!teleport::read_local_player_entity(component,player)) {
        if(g_crownRouteNoPlayerRun.exchange(nav.run,std::memory_order_acq_rel)!=nav.run) {
            std::array<char,200> line{};
            const int count=std::snprintf(line.data(),line.size(),
                "ev=omega_crown_transit stage=reject reason=no_local_player_entity run=%llu cycle=%u crown_stage=%u phase=%u mutation=observe_only",
                static_cast<unsigned long long>(nav.run),static_cast<unsigned>(status.token.cycle),
                static_cast<unsigned>(status.crownStage),static_cast<unsigned>(status.phase));
            if(count>0 && static_cast<std::size_t>(count)<line.size()) {
                core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(count)});
            }
        }
        return;
    }
    const transit::Point point{position[0],position[1],position[2]};
    const auto offer=[&](std::size_t index,lair::GateMilestone milestone) noexcept {
        const auto& volume=transit::kRouteVolumes[index];
        const bool inside=index==6?transit::contains_final_cannon(point):transit::contains(volume,point);
        if(!inside) { return; }
        if(!lair::observe_gate_arrival(status.token,milestone,player)) {
            if(g_crownRouteRejectRun.exchange(nav.run,std::memory_order_acq_rel)!=nav.run) {
                g_crownRouteRejectMask.store(0,std::memory_order_relaxed);
            }
            const std::uint64_t bit=UINT64_C(1)<<(index*8U+static_cast<unsigned>(milestone));
            if((g_crownRouteRejectMask.fetch_or(bit,std::memory_order_acq_rel)&bit)!=0) { return; }
            std::array<char,320> rejectLine{};
            const int rejectCount=std::snprintf(rejectLine.data(),rejectLine.size(),
                "ev=omega_crown_transit stage=reject reason=gate_rejected run=%llu cycle=%u player=%08X volume=%08X/60/%u milestone=%u crown_stage=%u phase=%u position=%.3f,%.3f,%.3f mutation=observe_only",
                static_cast<unsigned long long>(nav.run),static_cast<unsigned>(status.token.cycle),player,
                volume.registry,static_cast<unsigned>(volume.slot),static_cast<unsigned>(milestone),
                static_cast<unsigned>(status.crownStage),static_cast<unsigned>(status.phase),
                static_cast<double>(point.x),static_cast<double>(point.y),static_cast<double>(point.z));
            if(rejectCount>0 && static_cast<std::size_t>(rejectCount)<rejectLine.size()) {
                core::log::write(core::log::Channel::client,core::log::Level::info,{rejectLine.data(),static_cast<std::size_t>(rejectCount)});
            }
            return;
        }
        std::array<char,320> line{};
        const int count=std::snprintf(line.data(),line.size(),
            "ev=omega_crown_transit stage=route_arrival run=%llu cycle=%u player=%08X volume=%08X/60/%u milestone=%u position=%.3f,%.3f,%.3f mutation=observe_only",
            static_cast<unsigned long long>(nav.run),static_cast<unsigned>(status.token.cycle),
            player,volume.registry,static_cast<unsigned>(volume.slot),static_cast<unsigned>(milestone),
            static_cast<double>(point.x),static_cast<double>(point.y),static_cast<double>(point.z));
        if(count>0 && static_cast<std::size_t>(count)<line.size()) {
            core::log::write(core::log::Channel::client,core::log::Level::info,
                {line.data(),static_cast<std::size_t>(count)});
        }
    };
    if(finalApproach) {
        offer(6,lair::GateMilestone::finalCannon);
        offer(5,lair::GateMilestone::finalPlatform);return;
    }
    if(status.token.cycle<3) {
        offer(status.token.cycle-1U,lair::GateMilestone::chargePlatform);
    }
    // These exact authored pm_teleport_complete volumes are the receiving eye
    // platforms. They are not the portal source or proof of holding the charge.
    if(status.crownStage==lair::CrownStage::carrying) {
        offer(status.token.cycle+1U,lair::GateMilestone::eyePlatform);
    }
}

/**
 * Reads one component's body position and publishes it.
 * @param component Component already proved to be the player's.
 * @return True when the body was read. A failed read leaves the last position published.
 */
[[nodiscard]] bool publish_from(void* component) noexcept {
    teleport::Vector position{};
    std::uint32_t before=UINT32_MAX,after=UINT32_MAX;
    // A retired component can retain the player's low pool index. Check full
    // ownership on both sides of the body read, including a respawn during it.
    if(!teleport::read_local_player_entity(component,before)
        || !teleport::read_position(component,position)
        || !teleport::read_local_player_entity(component,after) || before!=after
        || !std::isfinite(position[0]) || !std::isfinite(position[1]) || !std::isfinite(position[2])) {
        return false;
    }
    g_sequence.fetch_add(1, std::memory_order_acq_rel);
    g_position = position;
    g_sequence.fetch_add(1, std::memory_order_release);
    g_present.store(true, std::memory_order_release);
    hooks::bootflow::public_event_participant_observer::poll_local_identity();
    state::activity::omega_presentation::observe_position({position[0], position[1], position[2]});
    state::activity::gateway::observe_position(position[0],position[1],position[2]);
    state::activity::deadly_trial::observe_position(position[0],position[1],position[2]);
    state::activity::beyond_infinity::observe_position(position[0],position[1],position[2]);
    state::activity::deep_storage::observe_position(position[0],position[1],position[2]);
    state::activity::hijacked::observe_position(position[0],position[1],position[2]);
    state::activity::newlight::launchpad::observe_position(position[0],position[1],position[2]);
    state::activity::strike_bond::observe_position(position[0],position[1],position[2]);
    {
        const std::lock_guard lock(g_physicsObserverMutex);
        if(g_physicsObserver) g_physicsObserver(component,before,GetTickCount64());
    }
    state::activity::vanilla::one_au::observe_position(position[0],position[1],position[2]);
    state::activity::vanilla::one_au::observe_native_player(before);
    observe_crown_route(component,position);
    return true;
}

} // namespace

void set_physics_observer(PhysicsObserver observer) noexcept {
    const std::lock_guard lock(g_physicsObserverMutex);
    g_physicsObserver=observer;
}

/** Publishes the position of the component the physics sync is running for. */
void observe(void* component) noexcept {
    if (component == nullptr) {
        return;
    }
    void* known = g_component.load(std::memory_order_relaxed);
    if(known!=nullptr && known!=component) {
        if(teleport::owns_local_player(known)) { return; }
        g_component.compare_exchange_strong(known,nullptr,std::memory_order_relaxed);
        g_present.store(false,std::memory_order_release);
        server::runtime::activity::public_event::participant_bridge::invalidate_local_identity();
    }
    if(!publish_from(component)) {
        void* expected=component;
        if(g_component.compare_exchange_strong(expected,nullptr,std::memory_order_relaxed)) {
            g_present.store(false,std::memory_order_release);
            server::runtime::activity::public_event::participant_bridge::invalidate_local_identity();
        }
        return;
    }
    g_component.store(component,std::memory_order_relaxed);
}

/** Refreshes the position for a player at rest, and drops a component that is no longer theirs. */
void poll() noexcept {
    void* component = g_component.load(std::memory_order_relaxed);
    if (component == nullptr) {
        // The teleport hook keeps one too whenever its own feature is on.
        component = teleport::local_player_component();
    }
    if (component == nullptr) {
        return;
    }
    if (!publish_from(component)) {
        g_component.store(nullptr, std::memory_order_relaxed);
        g_present.store(false, std::memory_order_release);
        server::runtime::activity::public_event::participant_bridge::invalidate_local_identity();
        return;
    }
    g_component.store(component, std::memory_order_relaxed);
}

/** Drops the published position. */
void reset() noexcept {
    g_component.store(nullptr, std::memory_order_relaxed);
    g_present.store(false, std::memory_order_release);
    server::runtime::activity::public_event::participant_bridge::invalidate_local_identity();
}

/** @return The last published position. */
Snapshot snapshot() noexcept {
    Snapshot value{};
    if (!g_present.load(std::memory_order_acquire)) {
        return value;
    }
    for (;;) {
        const std::uint32_t before = g_sequence.load(std::memory_order_acquire);
        if ((before & 1U) != 0U) {
            continue;
        }
        value.position = g_position;
        if (g_sequence.load(std::memory_order_acquire) == before) {
            break;
        }
    }
    value.present = true;
    return value;
}

} // namespace dawn::client::player::position
