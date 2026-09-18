#include "coo_native_components.h"
#include "internal.h"
#include "beyond_infinity_plate_timer.h"
#include "../../../state/activity/gateway/service_bindings.h"
#include <Windows.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>

#include "omega_arc_charge_receipts.h"
#include "native_hook_ownership.h"
#include "native_capture_observer.h"
#include "public_event_deferred_placement_observer.h"
#include "omega_arc_charge_native.h"
#include "gateway_native_read.h"
#include "gateway_module_native_path.h"
#include "beyond_infinity_native_receipts.h"
#include "../../../state/activity/beyond_infinity/runtime.h"
#include "../../../state/activity/deep_storage/runtime.h"
#include "../../../state/activity/hijacked/runtime.h"
#include "../../../state/activity/Newlight/launchpad/runtime.h"
#include "../../../state/activity/hijacked/controller.h"
#include "../../../state/activity/hijacked/plate_presentation.h"
#include "../../../state/activity/strike_bond/runtime.h"
#include "../../../state/activity/deep_storage/controller.h"
#include "../../../state/activity/deep_storage/plate_presentation.h"
#include "../../../state/activity/deep_storage/hologram_owner.h"
#include "gateway_module_damage.h"
#include "beyond_infinity_lens_damage.h"
#include "deep_storage_lens_damage.h"
#include "hijacked_boss_damage.h"
#include "damage_meter_probe.h"
#include "strike_pact_boss_damage.h"
#include "../../../state/activity/strike_pact/runtime.h"
#include "strike_bond_boss_damage.h"
#include "../../../state/activity/gateway/runtime.h"
#include "../../../state/activity/deadly_trial/runtime.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../core/logging/log.h"
#include "../../../state/activity/omega_arc_charge_authority.h"
#include "../../../state/activity/omega_crown_transit_authority.h"
#include "../../../state/activity/omega_first_lair_runtime.h"
#include "../../../state/activity/omega_rescue_marker_authority.h"
#include "../../../state/activity/omega_presentation.h"
#include "../../../server/runtime/activity/public_event_native_bridge.h"
#include "../../../server/runtime/activity/public_event_key_bridge.h"
#include "../../../server/runtime/activity/native_activity_runtime.h"
#include "../../../state/activity/runtime.h"
#include "../../../server/runtime/activity/lost_sector_destructible_definitions.h"

namespace dawn::client::hooks::bootflow {
namespace {
namespace native = omega_arc_charge_native;
namespace probe = damage_meter_probe;
namespace catalog = state::activity::omega_arc_charge;
namespace lair = state::activity::omega_first_lair;
namespace transit = state::activity::omega_crown_transit;
namespace rescueMarkers = state::activity::omega_rescue_markers;
namespace eventKeys=server::runtime::activity::public_event::keys;
using Carry = void(__fastcall*)(void*, std::uint8_t, const void*) noexcept;
using Dunk = void(__fastcall*)(void*) noexcept;
/** Original 9EFFC0: creates a source's world entity, stores the weak pair at
 * +440 and commits +2F0 = +180. Deferred (definition+94 != 0) objects reach it
 * only from tick 9F2F30, after the type-4 apply has returned. */
using Create = bool(__fastcall*)(void*) noexcept;
using Association = std::uint32_t*(__fastcall*)(const void*, std::uint32_t*) noexcept;
using Holder = std::uint32_t*(__fastcall*)(const void*, std::uint32_t*) noexcept;
/** Original 4AFE80: the local player's typed player-table handle. */
using LocalPlayer = std::uint32_t*(__fastcall*)(std::uint32_t*) noexcept;
/** Original 4B2260: the local player's controlled world entity (player record +54). */
using ControlledEntity = std::uint32_t*(__fastcall*)(std::uint32_t*) noexcept;
/** Original F32CD0 evaluates the native hold-to-use interaction. All authored
 * predicates, range/angle/LOS checks and its output remain with the original. */
using Interaction = void(__fastcall*)(void*, std::uint32_t, std::uint8_t, const void*,
    std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint8_t, void*) noexcept;
hooking::CallGate g_gate;
std::array<hooking::detour::Handle, 9> g_handles{};
std::atomic<Carry> g_carry{};
std::atomic<Dunk> g_dunk{};
std::atomic<Create> g_create{};
std::atomic<Interaction> g_interaction{};
Association g_association{};
Holder g_holder{};
LocalPlayer g_localPlayer{};
ControlledEntity g_controlled{};
std::uintptr_t g_image{};
SRWLOCK g_lock = SRWLOCK_INIT;
native::Bindings g_bindings;
std::uint64_t g_run{};
lair::CrownToken g_heldToken{};
unsigned g_lines{};
unsigned g_gatewayModuleLines{};
beyond_infinity_lens_damage::Candidate g_beyondLensCandidate{};
deep_storage_lens_damage::Candidate g_deepLensCandidate{};
thread_local bool g_dunkInFlight{};
/** Last reported native state of each deferred Crown transit object (platform,
 * bridge, rings, portal, destinations, final FX/disk): one line per change, so
 * the next run proves whether 9F2F30/9EFFC0 created the slab the player lands on. */
struct TransitSeen final { std::uint32_t applied{}, entity{}; std::uint8_t active{}; bool seen{}; };
std::array<TransitSeen, transit::kSources.size()> g_transitSeen{};
unsigned g_transitLines{};
inline constexpr unsigned kTransitLines = 160;
struct PromptSeen final {
    std::uint32_t controller{},entity{},player{},label0{},label1{};
    std::uint8_t flags{},status{},eligible{},blocked{},used{};
    bool seen{};
    friend bool operator==(const PromptSeen&,const PromptSeen&) noexcept = default;
};
std::array<PromptSeen,catalog::kCycles.size()> g_promptSeen{};
unsigned g_promptLines{};

// Player table descriptor of the pinned image (DAT_1F90E10): +8 base, +10 stride.
// Read by originals 4AFE80/4B2260/4D3AC0/F2F9F0; +54 of a record is the controlled entity.
inline constexpr std::uintptr_t kPlayerTableBase = 0x1F90E18U;
inline constexpr std::uintptr_t kPlayerTableStride = 0x1F90E20U;
inline constexpr std::size_t kPlayerControlledEntity = 0x54U;

// Bounded rejection diagnostics: once per (site, reason, handle) and at most
// 64 lines per run, so the next stall explains itself without flooding.
inline constexpr unsigned kRejectLines = 64;
struct RejectKey final { std::uint32_t handle{}; std::uint8_t site{}, reason{}; };
std::array<RejectKey, kRejectLines> g_rejects{};
unsigned g_rejectCount{}, g_rejectLines{};

// Outgoing-damage probe for the damage HUD. B7E3C0 carries the attacker, target and
// absolute amount that HUD needs, but its coverage is unestablished: every existing
// consumer matched a single boss entity, so nothing says whether it observes ordinary
// outgoing hits at all. Raw rows are capped per run, so the opening of an encounter
// stays readable, while the window totals keep running afterwards and report a whole
// encounter's coverage without flooding. All of it is observe-only.
inline constexpr unsigned kDamageProbeRows = 96;
inline constexpr std::uint64_t kDamageProbeWindow = 128;
probe::Window g_damageWindow{};
std::uint64_t g_damageCalls{}, g_damageWindowStart{}, g_damageRun{};
unsigned g_damageRows{};

bool copy(const void* source, std::span<std::byte> destination) noexcept {
    const auto address = reinterpret_cast<std::uintptr_t>(source);
    if (address < 0x10000 || address > UINTPTR_MAX - destination.size()) { return false; }
    SIZE_T copied{};
    return ReadProcessMemory(GetCurrentProcess(), source, destination.data(), destination.size(), &copied)
        && copied == destination.size();
}
template<class T> T at(const std::byte* source) noexcept {
    T result{}; std::memcpy(&result, source, sizeof result); return result;
}
template<class T> bool read_at(std::uintptr_t address, T& value) noexcept {
    std::array<std::byte, sizeof(T)> bytes{};
    if (!copy(reinterpret_cast<const void*>(address), bytes)) { return false; }
    value = at<T>(bytes.data()); return true;
}
bool prefix(const std::byte* source, std::uint32_t resource,
            std::uint32_t type, std::uint64_t offset) noexcept {
    return at<std::uint32_t>(source) == resource && at<std::uint32_t>(source + 4) == type
        && at<std::uint64_t>(source + 8) == offset;
}
template<class... Args> void report_capped(unsigned& lines, unsigned cap, const char* format, Args... args) noexcept {
    std::array<char, 640> line{};
    const int length = std::snprintf(line.data(), line.size(), format, args...);
    if (length <= 0 || static_cast<std::size_t>(length) >= line.size()) { return; }
    AcquireSRWLockExclusive(&g_lock);
    const bool emit = lines++ < cap;
    ReleaseSRWLockExclusive(&g_lock);
    if (emit) {
        core::log::write(core::log::Channel::client, core::log::Level::info,
                        {line.data(), static_cast<std::size_t>(length)});
    }
}
template<class... Args> void report(const char* format, Args... args) noexcept {
    report_capped(g_lines, 256U, format, args...);
}
const char* text(transit::Role role) noexcept {
    switch (role) {
    case transit::Role::core: return "core";
    case transit::Role::ring: return "ring";
    case transit::Role::platform: return "platform";
    case transit::Role::bridge: return "bridge";
    case transit::Role::portal: return "portal";
    case transit::Role::destinationA: return "destination_a";
    case transit::Role::destinationB: return "destination_b";
    case transit::Role::dpsPlatformA: return "dps_platform_a";
    case transit::Role::dpsPlatformB: return "dps_platform_b";
    case transit::Role::returnLauncher: return "return_launcher";
    case transit::Role::finalCore: return "final_core";
    case transit::Role::finalFx: return "final_fx";
    default: return "final_disk";
    }
}
enum class Site : std::uint8_t { source, pickup, dunk_before, dunk_after };
enum class Reason : std::uint8_t {
    navigation, inactive, no_entity, ambiguous_entity, bad_generation, not_carried, no_holder,
    no_binding, token, duplicate, unknown_sink, no_held, no_sink_binding, cycle, identity,
    stale_run, requester, not_consumed, inactive_after, not_current, encounter, ownership
};
const char* text(Site site) noexcept {
    switch (site) {
    case Site::source: return "native_source";
    case Site::pickup: return "native_pickup";
    case Site::dunk_before: return "native_dunk_request";
    default: return "native_dunk";
    }
}
const char* text(Reason reason) noexcept {
    switch (reason) {
    case Reason::navigation: return "navigation_or_run";
    case Reason::inactive: return "carrier_inactive";
    case Reason::no_entity: return "no_created_entity";
    case Reason::ambiguous_entity: return "entity_bound_twice";
    case Reason::bad_generation: return "bad_generation";
    case Reason::not_carried: return "state_not_carried";
    case Reason::no_holder: return "no_holder_entity";
    case Reason::no_binding: return "item_not_bound";
    case Reason::token: return "crown_token_or_cycle";
    case Reason::duplicate: return "duplicate";
    case Reason::unknown_sink: return "not_authored_sink";
    case Reason::no_held: return "no_held_charge";
    case Reason::no_sink_binding: return "sink_not_bound";
    case Reason::cycle: return "sink_cycle_mismatch";
    case Reason::identity: return "identity_changed";
    case Reason::stale_run: return "stale_run";
    case Reason::requester: return "requester_not_holder";
    case Reason::not_consumed: return "request_not_consumed";
    case Reason::inactive_after: return "sink_inactive_after";
    case Reason::not_current: return "bindings_not_current";
    case Reason::encounter: return "encounter_rejected";
    default: return "ownership";
    }
}
/** Emits one bounded diagnostic per distinct (site, reason, handle) in a run. */
template<class... Args>
void reject(Site site, Reason reason, std::uint32_t handle, const char* detailFormat, Args... args) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    bool emit = g_rejectLines < kRejectLines;
    for (unsigned i = 0; emit && i < g_rejectCount; ++i) {
        const auto& key = g_rejects[i];
        if (key.handle == handle && key.site == static_cast<std::uint8_t>(site)
            && key.reason == static_cast<std::uint8_t>(reason)) { emit = false; }
    }
    if (emit) {
        if (g_rejectCount < g_rejects.size()) {
            g_rejects[g_rejectCount++] = {handle, static_cast<std::uint8_t>(site),
                                          static_cast<std::uint8_t>(reason)};
        }
        ++g_rejectLines;
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (!emit) { return; }
    std::array<char, 320> detail{};
    const int detailLength = std::snprintf(detail.data(), detail.size(), detailFormat, args...);
    std::array<char, 640> line{};
    const int length = std::snprintf(line.data(), line.size(),
        "ev=omega_charge stage=reject site=%s reason=%s handle=%08X %s mutation=observe_only",
        text(site), text(reason), handle, detailLength > 0 ? detail.data() : "");
    if (length > 0 && static_cast<std::size_t>(length) < line.size()) {
        core::log::write(core::log::Channel::client, core::log::Level::info,
                        {line.data(), static_cast<std::size_t>(length)});
    }
}
void ensure_run(std::uint64_t run) noexcept {
    if (g_run == run) { return; }
    g_run = run; g_bindings.reset(run); g_heldToken = {}; g_lines = 0;
    g_rejects = {}; g_rejectCount = 0; g_rejectLines = 0;
    g_transitSeen = {}; g_transitLines = 0;
    g_promptSeen = {}; g_promptLines = 0;
}
lair::ChargeReceipt receipt(const native::Held& held, const lair::CrownToken& token,
                            std::uint32_t sink = UINT32_MAX) noexcept {
    const auto& cycle = catalog::kCycles[held.binding.cycle];
    return {token, held.binding.source, held.binding.generation, held.binding.entity,
            held.player, sink, cycle.registry, cycle.carrySlot, cycle.sinkSlot};
}
bool current_token(const lair::CrownToken& token, std::uint64_t run, std::uint8_t cycle) noexcept {
    return token.valid() && token.boss.run == run && token.cycle == cycle + 1U;
}
std::uint32_t local_controlled_entity() noexcept {
    std::uint32_t entity = UINT32_MAX;
    if (g_controlled != nullptr) { g_controlled(&entity); }
    return entity;
}

/** Resolves the sink's stored requester to a world entity when its kind allows. */
struct Requester final {
    std::uint32_t raw{UINT32_MAX}, entity{UINT32_MAX}, local{UINT32_MAX}, localEntity{UINT32_MAX};
    const char* match{"none"};
};
Requester resolve_requester(const void* association, std::uint32_t holder) noexcept {
    Requester result{};
    if (g_association == nullptr) { return result; }
    g_association(association, &result.raw);
    if (result.raw == UINT32_MAX) { return result; }
    if (g_localPlayer != nullptr) { g_localPlayer(&result.local); }
    result.localEntity = local_controlled_entity();
    if (result.raw == holder || (holder != UINT32_MAX
            && native::registry_row(result.raw) == native::registry_row(holder))) {
        result.entity = result.raw; result.match = "entity"; return result;
    }
    if (result.local == UINT32_MAX || result.localEntity == UINT32_MAX
        || native::registry_row(result.raw) != native::registry_row(result.local)) {
        result.match = "unresolved_kind"; return result;
    }
    std::uint64_t base{}; std::uint32_t stride{}, check{}, entity{};
    // The same arithmetic original 4B2260 applies to the local handle must
    // reproduce its answer before it is trusted for another player-table handle.
    if (!read_at(g_image + kPlayerTableBase, base) || !read_at(g_image + kPlayerTableStride, stride)
        || base < 0x10000 || stride == 0 || stride > 0x10000
        || !read_at(static_cast<std::uintptr_t>(base) + (result.local & 0x1FFFU) * stride + kPlayerControlledEntity, check)
        || check != result.localEntity
        || !read_at(static_cast<std::uintptr_t>(base) + (result.raw & 0x1FFFU) * stride + kPlayerControlledEntity, entity)) {
        result.match = "player_table_unverified"; return result;
    }
    result.entity = entity; result.match = "player_table"; return result;
}

/** Binds an authored carry/sink source to the world entity it created. Called
 * after original 9F19F0 (retransmissions) and after original 9EFFC0 (creation). */
void observe_carrier(void* component, std::uint64_t run, const char* origin) noexcept {
    if (run == 0 || g_association == nullptr) { return; }
    std::array<std::byte, 0x448> bytes{};
    std::array<std::byte, 16> header{};
    if (!copy(component, header)) { return; }
    const auto authored = catalog::definition(at<std::uint32_t>(header.data()));
    if (authored.cycle == nullptr || authored.object == catalog::Object::effect
        || !prefix(header.data(), at<std::uint32_t>(header.data()), 0x80809928U, 0x4C8U)
        || !copy(component, bytes)) { return; }
    const auto self = at<std::uint32_t>(bytes.data() + 0x24);
    const auto applied = at<std::uint32_t>(bytes.data() + 0x180);
    const auto active = at<std::uint8_t>(bytes.data() + 0x188);
    const auto committed = at<std::uint32_t>(bytes.data() + 0x2F0);
    const auto rawPair = at<std::uint64_t>(bytes.data() + 0x440);
    const auto nav = state::activity::omega_presentation::navigation();
    if (!nav.enabled || nav.run != run) {
        reject(Site::source, Reason::navigation, self, "origin=%s definition=%08X nav_enabled=%u nav_run=%llu run=%llu",
               origin, at<std::uint32_t>(header.data()), nav.enabled ? 1U : 0U,
               static_cast<unsigned long long>(nav.run), static_cast<unsigned long long>(run));
        return;
    }
    if (active == 0) {
        // Dormant preparation is expected once per source; a later inactive apply
        // with committed==applied is the native "already created" state.
        reject(Site::source, Reason::inactive, self, "origin=%s definition=%08X applied=%u committed=%u pair=%016llX",
               origin, at<std::uint32_t>(header.data()), applied, committed,
               static_cast<unsigned long long>(rawPair));
        return;
    }
    std::uint32_t entity = UINT32_MAX;
    g_association(static_cast<const std::byte*>(component) + 0x440, &entity);
    const auto generation = catalog::base_generation(applied);
    if (entity == UINT32_MAX) {
        // Deferred objects are created by tick 9F2F30 only while committed < applied.
        reject(Site::source, Reason::no_entity, self, "origin=%s definition=%08X applied=%u committed=%u pair=%016llX creatable=%u",
               origin, at<std::uint32_t>(header.data()), applied, committed,
               static_cast<unsigned long long>(rawPair),
               static_cast<int>(committed) < static_cast<int>(applied) ? 1U : 0U);
        return;
    }
    if (generation == 0 || generation >= catalog::kMaximumGeneration) {
        reject(Site::source, Reason::bad_generation, self, "origin=%s definition=%08X applied=%u committed=%u",
               origin, at<std::uint32_t>(header.data()), applied, committed);
        return;
    }
    native::Binding binding{run, self, generation, entity, authored.cycle->index, authored.object};
    AcquireSRWLockExclusive(&g_lock);
    ensure_run(run);
    const bool changed = g_bindings.observe(binding);
    const bool ambiguous = g_bindings.ambiguous(entity, authored.object);
    ReleaseSRWLockExclusive(&g_lock);
    if (changed) {
        report("ev=omega_charge stage=native_source origin=%s run=%llu cycle=%u source=%08X registry=%08X slot=%u generation=%u applied=%u committed=%u entity=%08X role=%s mutation=observe_only",
               origin, static_cast<unsigned long long>(run), static_cast<unsigned>(binding.cycle + 1U), binding.source,
               authored.cycle->registry, static_cast<unsigned>(authored.object == catalog::Object::carry
                   ? authored.cycle->carrySlot : authored.cycle->sinkSlot), binding.generation, applied, committed,
               binding.entity, authored.object == catalog::Object::carry ? "pickup" : "dunk");
    }
    if (ambiguous) {
        reject(Site::source, Reason::ambiguous_entity, entity, "origin=%s source=%08X cycle=%u",
               origin, self, static_cast<unsigned>(binding.cycle + 1U));
    }
}

/** Observe-only receipt for the deferred Crown transit objects (same 80809928
 * +4C8 instance layout as the Arc sources): applied revision +180, native active
 * +188, committed +2F0 and the created world entity behind the +440 weak pair.
 * Reported after original 9F19F0 (apply) and original 9EFFC0 (create), once per
 * state change per object, so a missing platform is explained as never enabled
 * (active=0), enabled but never created (entity=FFFFFFFF, committed==applied) or
 * created (origin=create, entity valid). Nothing is written. */
void observe_transit(void* component, std::uint64_t run, const char* origin) noexcept {
    if (run == 0 || g_association == nullptr) { return; }
    std::array<std::byte, 16> header{};
    if (!copy(component, header)) { return; }
    const auto definition = at<std::uint32_t>(header.data());
    const transit::Source* item{};
    std::size_t index{};
    for (std::size_t i = 0; i < transit::kSources.size(); ++i) {
        if (transit::kSources[i].definition == definition) { item = &transit::kSources[i]; index = i; break; }
    }
    if (item == nullptr || (!item->deferred && item->role != transit::Role::dpsPlatformA
            && item->role != transit::Role::dpsPlatformB && item->role != transit::Role::returnLauncher)
        || !prefix(header.data(), definition, 0x80809928U, 0x4C8U)) { return; }
    std::array<std::byte, 0x448> bytes{};
    if (!copy(component, bytes)) { return; }
    const auto nav = state::activity::omega_presentation::navigation();
    if (!nav.enabled || nav.run != run) { return; }
    const auto self = at<std::uint32_t>(bytes.data() + 0x24);
    const auto applied = at<std::uint32_t>(bytes.data() + 0x180);
    const auto active = at<std::uint8_t>(bytes.data() + 0x188);
    const auto committed = at<std::uint32_t>(bytes.data() + 0x2F0);
    std::uint32_t entity = UINT32_MAX;
    g_association(static_cast<const std::byte*>(component) + 0x440, &entity);
    // Both deferred slabs and their immediate back pieces must exist at this
    // run's exact active revision before the receiving platform is usable.
    const auto status = lair::status(run);
    if (status.enabled && !status.failed && status.boss.run == run
        && native::created_revision(status.boss.generation, item->deferred,
            applied, committed, active != 0, entity)) {
        lair::observe_transit_created(run, static_cast<std::uint8_t>(index), entity);
    }
    AcquireSRWLockExclusive(&g_lock);
    ensure_run(run);
    auto& seen = g_transitSeen[index];
    const bool changed = !seen.seen || seen.applied != applied || seen.active != active || seen.entity != entity;
    seen = {applied, entity, active, true};
    ReleaseSRWLockExclusive(&g_lock);
    if (!changed) { return; }
    report_capped(g_transitLines, kTransitLines,
        "ev=omega_crown_transit stage=native_object origin=%s run=%llu cycle=%u registry=%08X slot=%u role=%s definition=%08X source=%08X applied=%u active=%u committed=%u entity=%08X creatable=%u mutation=observe_only",
        origin, static_cast<unsigned long long>(run), static_cast<unsigned>(item->cycle), item->registry,
        static_cast<unsigned>(item->slot), text(item->role), definition, self, applied,
        static_cast<unsigned>(active), committed, entity,
        active != 0 && static_cast<int>(committed) < static_cast<int>(applied) ? 1U : 0U);
}

/** Scene role overrides resolve the actual type4 entity. Do not publish a
 * selector until all its exact transform providers have committed creation. */
void observe_rescue_marker(void* component,std::uint64_t run,const char* origin) noexcept {
    if(run==0 || g_association==nullptr) { return; }
    std::array<std::byte,16> header{};
    if(!copy(component,header)) { return; }
    const auto* marker=rescueMarkers::definition(at<std::uint32_t>(header.data()));
    if(marker==nullptr || !prefix(header.data(),marker->definition,0x80809928U,0x4C8U)) { return; }
    std::array<std::byte,0x448> bytes{};
    if(!copy(component,bytes)) { return; }
    const auto nav=state::activity::omega_presentation::navigation();
    const auto status=lair::status(run);
    if(!nav.enabled || nav.run!=run || !status.enabled || status.failed || status.boss.run!=run) { return; }
    const auto applied=at<std::uint32_t>(bytes.data()+0x180);
    const auto committed=at<std::uint32_t>(bytes.data()+0x2F0);
    const auto active=at<std::uint8_t>(bytes.data()+0x188);
    std::uint32_t entity=UINT32_MAX;
    g_association(static_cast<const std::byte*>(component)+0x440,&entity);
    if(!g_gate.accepting() || !rescueMarkers::created(status.boss.generation,
            applied,committed,active!=0,entity)) { return; }
    if(lair::observe_rescue_marker_created(run,status.boss.generation,marker->slot)) {
        report("ev=omega_rescue stage=marker_created origin=%s run=%llu scene=%u registry=99BD2FEB slot=%u definition=%08X generation=%u applied=%u committed=%u entity=%08X mutation=observe_only",
            origin,static_cast<unsigned long long>(run),static_cast<unsigned>(marker->scene),
            static_cast<unsigned>(marker->slot),marker->definition,status.boss.generation,
            applied,committed,entity);
    }
}

void observe_carry(void* component, std::uint64_t enteredRun) noexcept {
    std::array<std::byte, 0x4A0> bytes{};
    if (!copy(component, bytes) || !prefix(bytes.data(), catalog::kCarryController, 0x80804221U, 0x598U)) {
        return;
    }
    const auto self = at<std::uint32_t>(bytes.data() + 0x24);
    const auto entity = at<std::uint32_t>(bytes.data() + 0x2C);
    if (self == UINT32_MAX || entity == UINT32_MAX) { return; }
    const auto nav = state::activity::omega_presentation::navigation();
    if (!nav.enabled || nav.run == 0 || nav.run != enteredRun) {
        reject(Site::pickup, Reason::navigation, entity, "controller=%08X nav_enabled=%u nav_run=%llu run=%llu",
               self, nav.enabled ? 1U : 0U, static_cast<unsigned long long>(nav.run),
               static_cast<unsigned long long>(enteredRun));
        return;
    }
    const auto status = lair::status(nav.run);
    const auto state = at<std::uint8_t>(bytes.data() + 0x470);
    // Live 2026-09-06 (PID 8292): a player carrying the relic sits in state 1 with the
    // five-qword holder context at +0x478 populated (state 0 leaves it all -1). Native
    // D90A70/D97B40 resolve the parent entity (597B10) only for state 3, the socketed
    // attachment case. Both are "carried"; the holder for state 1 is the local controlled
    // entity when 597B10 has no parent to report.
    const auto contextWord = at<std::uint64_t>(bytes.data() + 0x478);
    const bool carried = state == 3 || (state == 1 && contextWord != UINT64_MAX);
    std::uint32_t player = UINT32_MAX;
    if (carried && g_holder != nullptr) { g_holder(component, &player); }
    const auto local = local_controlled_entity();
    if (carried && player == UINT32_MAX && state == 1) { player = local; }

    native::Held held{};
    lair::CrownToken token{};
    bool accepted{}, bound{}, tokenOk{};
    AcquireSRWLockExclusive(&g_lock);
    ensure_run(nav.run);
    const auto binding = g_bindings.find(entity, catalog::Object::carry);
    const bool ambiguous = g_bindings.ambiguous(entity, catalog::Object::carry);
    bound = binding.valid();
    tokenOk = bound && current_token(status.token, nav.run, binding.cycle);
    if (carried && player != UINT32_MAX && bound && tokenOk) {
        held = {binding, self, player}; token = status.token;
        accepted = g_bindings.carry(held);
        if (accepted) { g_heldToken = token; }
    } else if (!carried) {
        held = g_bindings.release(entity, self); token = g_heldToken;
        accepted = held.valid();
        if (accepted) { g_heldToken = {}; }
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (!accepted) {
        if (!carried) {
            // Any non-carried transition of an unheld item is routine; only a
            // carried item that never bound is worth a line.
            return;
        }
        if (player == UINT32_MAX) {
            reject(Site::pickup, Reason::no_holder, entity, "controller=%08X state=%u local_entity=%08X", self,
                   static_cast<unsigned>(state), local);
        } else if (!bound) {
            reject(Site::pickup, ambiguous ? Reason::ambiguous_entity : Reason::no_binding, entity,
                   "controller=%08X state=%u holder=%08X local_entity=%08X crown_stage=%u cycle=%u",
                   self, static_cast<unsigned>(state), player, local,
                   static_cast<unsigned>(status.crownStage), static_cast<unsigned>(status.token.cycle));
        } else if (!tokenOk) {
            reject(Site::pickup, Reason::token, entity,
                   "controller=%08X binding_cycle=%u token_cycle=%u token_valid=%u token_run=%llu enabled=%u crown_stage=%u",
                   self, static_cast<unsigned>(binding.cycle + 1U), static_cast<unsigned>(status.token.cycle),
                   status.token.valid() ? 1U : 0U, static_cast<unsigned long long>(status.token.boss.run),
                   status.enabled ? 1U : 0U, static_cast<unsigned>(status.crownStage));
        } else {
            reject(Site::pickup, Reason::duplicate, entity, "controller=%08X holder=%08X", self, player);
        }
        return;
    }
    if (!carried && g_dunkInFlight) { return; }
    const auto event = carried ? lair::ChargeMilestone::pickedUp : lair::ChargeMilestone::dropped;
    const bool observed = lair::observe_charge(receipt(held, token), event);
    report("ev=omega_charge stage=%s run=%llu cycle=%u source=%08X registry=%08X slot=%u generation=%u item=%08X controller=%08X state=%u player=%08X local_entity=%08X context=%016llX,%016llX accepted=%u mutation=observe_only",
           carried ? "native_pickup" : "native_drop", static_cast<unsigned long long>(nav.run),
           static_cast<unsigned>(token.cycle), held.binding.source,
           catalog::kCycles[held.binding.cycle].registry,
           static_cast<unsigned>(catalog::kCycles[held.binding.cycle].carrySlot),
           held.binding.generation, entity, self, static_cast<unsigned>(state), held.player, local,
           static_cast<unsigned long long>(contextWord),
           static_cast<unsigned long long>(at<std::uint64_t>(bytes.data() + 0x490)),
           observed ? 1U : 0U);
    if (!observed) {
        const auto after = lair::status(nav.run);
        reject(Site::pickup, Reason::encounter, entity,
               "event=%s crown_stage=%u phase=%u enabled=%u failed=%u cycle=%u generation=%u boss_generation=%u",
               state == 3 ? "pickedUp" : "dropped", static_cast<unsigned>(after.crownStage),
               static_cast<unsigned>(after.phase), after.enabled ? 1U : 0U, after.failed ? 1U : 0U,
               static_cast<unsigned>(after.cycle), held.binding.generation, after.boss.generation);
    }
}

struct PendingDunk final {
    native::Dunk proof{};
    lair::CrownToken token{};
    std::uint32_t resource{};
    Requester requester{};
};
PendingDunk before_dunk(void* component, std::uint64_t run) noexcept {
    PendingDunk result{};
    std::array<std::byte, 0x2E8> bytes{};
    if (!copy(component, bytes)) { return {}; }
    const catalog::Cycle* cycle{};
    for (const auto& row : catalog::kCycles) {
        if (prefix(bytes.data(), row.sinkController, 0x80804FB2U, 0x388U)) { cycle = &row; break; }
    }
    if (cycle == nullptr || g_association == nullptr) { return {}; }
    const auto entity = at<std::uint32_t>(bytes.data() + 0x2C);
    const auto controller = at<std::uint32_t>(bytes.data() + 0x24);
    AcquireSRWLockExclusive(&g_lock);
    const bool sameRun = g_run == run;
    if (sameRun) {
        result.proof.held = g_bindings.held();
        result.proof.sink = g_bindings.find(entity, catalog::Object::sink);
        result.token = g_heldToken;
    }
    ReleaseSRWLockExclusive(&g_lock);
    result.requester = resolve_requester(static_cast<const std::byte*>(component) + 0x2E0,
                                         result.proof.held.player);
    if (!sameRun) {
        reject(Site::dunk_before, Reason::stale_run, controller, "sink=%08X run=%llu hook_run=%llu",
               entity, static_cast<unsigned long long>(run), static_cast<unsigned long long>(g_run));
        return {};
    }
    if (!result.proof.held.valid()) {
        reject(Site::dunk_before, Reason::no_held, controller,
               "sink=%08X cycle=%u requester=%08X requester_entity=%08X match=%s local_entity=%08X",
               entity, static_cast<unsigned>(cycle->index + 1U), result.requester.raw,
               result.requester.entity, result.requester.match, result.requester.localEntity);
        return {};
    }
    if (!result.proof.sink.valid()) {
        reject(Site::dunk_before, Reason::no_sink_binding, controller, "sink=%08X cycle=%u held_cycle=%u",
               entity, static_cast<unsigned>(cycle->index + 1U),
               static_cast<unsigned>(result.proof.held.binding.cycle + 1U));
        return {};
    }
    if (result.proof.sink.cycle != cycle->index) {
        reject(Site::dunk_before, Reason::cycle, controller, "sink=%08X authored_cycle=%u bound_cycle=%u",
               entity, static_cast<unsigned>(cycle->index + 1U),
               static_cast<unsigned>(result.proof.sink.cycle + 1U));
        return {};
    }
    if (!current_token(result.token, run, cycle->index)) {
        reject(Site::dunk_before, Reason::token, controller, "sink=%08X cycle=%u token_cycle=%u token_valid=%u",
               entity, static_cast<unsigned>(cycle->index + 1U), static_cast<unsigned>(result.token.cycle),
               result.token.valid() ? 1U : 0U);
        return {};
    }
    result.resource = cycle->sinkController;
    result.proof.controller = controller;
    result.proof.requester = result.requester.raw;
    result.proof.requesterEntity = result.requester.entity;
    result.proof.requested = at<std::int32_t>(bytes.data() + 0x2DC);
    result.proof.consumedBefore = at<std::int32_t>(bytes.data() + 0x2D8);
    return result;
}
void after_dunk(void* component, PendingDunk pending) noexcept {
    if (!pending.proof.held.valid()) { return; }
    const auto controller = pending.proof.controller;
    std::array<std::byte, 0x2E8> bytes{};
    if (!copy(component, bytes) || !prefix(bytes.data(), pending.resource, 0x80804FB2U, 0x388U)
        || at<std::uint32_t>(bytes.data() + 0x24) != controller
        || at<std::uint32_t>(bytes.data() + 0x2C) != pending.proof.sink.entity) {
        reject(Site::dunk_after, Reason::identity, controller, "sink=%08X", pending.proof.sink.entity);
        return;
    }
    pending.proof.activeAfter = at<std::uint8_t>(bytes.data() + 0x2D0) != 0;
    pending.proof.consumedAfter = at<std::int32_t>(bytes.data() + 0x2D8);
    const auto nav = state::activity::omega_presentation::navigation();
    if (!nav.enabled || nav.run != pending.proof.held.binding.run) {
        reject(Site::dunk_after, Reason::navigation, controller, "nav_enabled=%u nav_run=%llu held_run=%llu",
               nav.enabled ? 1U : 0U, static_cast<unsigned long long>(nav.run),
               static_cast<unsigned long long>(pending.proof.held.binding.run));
        return;
    }
    AcquireSRWLockShared(&g_lock);
    const bool current = g_bindings.current(pending.proof);
    ReleaseSRWLockShared(&g_lock);
    if (!current) {
        const auto& p = pending.proof;
        const Reason reason = p.requester == UINT32_MAX
                || (p.requesterEntity != UINT32_MAX && p.requesterEntity != p.held.player) ? Reason::requester
            : !(p.requested > p.consumedBefore && p.consumedBefore >= 0 && p.consumedAfter == p.requested) ? Reason::not_consumed
            : !p.activeAfter ? Reason::inactive_after
            : p.valid() ? Reason::not_current : Reason::ownership;
        reject(Site::dunk_after, reason, controller,
               "sink=%08X item=%08X holder=%08X requester=%08X requester_entity=%08X match=%s local=%08X local_entity=%08X requested=%d consumed_before=%d consumed_after=%d active_after=%u",
               p.sink.entity, p.held.binding.entity, p.held.player, p.requester, p.requesterEntity,
               pending.requester.match, pending.requester.local, pending.requester.localEntity,
               p.requested, p.consumedBefore, p.consumedAfter, p.activeAfter ? 1U : 0U);
        return;
    }
    const bool accepted = lair::observe_charge(receipt(pending.proof.held, pending.token,
                                                       pending.proof.controller), lair::ChargeMilestone::dunked);
    report("ev=omega_charge stage=native_dunk run=%llu cycle=%u source=%08X item=%08X player=%08X requester=%08X requester_entity=%08X player_match=%s sink=%08X sink_entity=%08X requested=%d consumed_before=%d consumed_after=%d accepted=%u mutation=observe_only",
           static_cast<unsigned long long>(nav.run), static_cast<unsigned>(pending.token.cycle),
           pending.proof.held.binding.source, pending.proof.held.binding.entity, pending.proof.held.player,
           pending.proof.requester, pending.proof.requesterEntity,
           pending.proof.requesterEntity == UINT32_MAX ? "holder_predicate" : pending.requester.match,
           pending.proof.controller, pending.proof.sink.entity, pending.proof.requested,
           pending.proof.consumedBefore, pending.proof.consumedAfter, accepted ? 1U : 0U);
    if (!accepted) {
        const auto after = lair::status(nav.run);
        reject(Site::dunk_after, Reason::encounter, controller,
               "crown_stage=%u phase=%u enabled=%u failed=%u cycle=%u token_cycle=%u generation=%u boss_generation=%u",
               static_cast<unsigned>(after.crownStage), static_cast<unsigned>(after.phase),
               after.enabled ? 1U : 0U, after.failed ? 1U : 0U, static_cast<unsigned>(after.cycle),
               static_cast<unsigned>(pending.token.cycle), pending.proof.held.binding.generation,
               after.boss.generation);
    }
}
// The same generic interaction template is used by unrelated world objects.
// Bind it only through this mission's current native source, weak entity, and
// controller. F36640 must consume the local player's real hold request.
void observe_trial_object(void* raw) noexcept {
    namespace trial=state::activity::deadly_trial;namespace gn=gateway_native;
    const auto request=trial::request();if(!request.enabled || request.interaction.valid()) { return; }
    gn::Read read{g_image};const auto source=reinterpret_cast<std::uintptr_t>(raw);std::array<std::byte,16> header{};
    if(!read.copy(source,header) || !prefix(header.data(),0x80B2EBA7U,0x80809928U,0x4C8U)) { return; }
    std::uint32_t generation{},committed{},bundle{};std::uint8_t active{};gn::Weak entity{},after{};
    if(!read.value(source+0x180,generation) || generation!=request.owner.value || !read.value(source+0x2F0,committed) || committed!=generation
        || !read.value(source+0x188,active) || active!=1 || !read.value(source+0x440,entity)) { return; }
    std::uintptr_t row{},controller{};std::uint32_t handle{},parent{};
    if(!read.entity_row(entity,row) || !read.value(row+0x4C,bundle)
        || !coo_native::component(read,bundle,entity.handle,0x80804FB2U,controller)
        || !read.copy(controller,header) || !prefix(header.data(),0x80FEAB33U,0x80804FB2U,0x388U)
        || !read.value(controller+0x24,handle) || handle==UINT32_MAX || !read.value(controller+0x2C,parent) || parent!=entity.handle) { return; }
    if(!read.value(source+0x440,after) || after!=entity || !read.weak(after)
        || !read.value(source+0x180,generation) || generation!=request.owner.value || !read.value(source+0x2F0,committed) || committed!=generation) { return; }
    trial::observe_binding({request.owner,source,entity.handle,entity.serial,handle});
}
struct TrialUse { state::activity::deadly_trial::InteractionBinding binding{};std::int32_t requested{},before{};std::uint32_t player{UINT32_MAX}; };
TrialUse before_trial_use(void* component) noexcept {
    namespace trial=state::activity::deadly_trial;const auto request=trial::request();const auto& b=request.interaction;
    if(!request.enabled || !b.valid()) { return {}; }
    std::array<std::byte,0x2E8> bytes{};if(!copy(component,bytes) || !prefix(bytes.data(),0x80FEAB33U,0x80804FB2U,0x388U)
        || at<std::uint32_t>(bytes.data()+0x24)!=b.controller || at<std::uint32_t>(bytes.data()+0x2C)!=b.entity) { return {}; }
    gateway_native::Read read{g_image};gateway_native::Weak entity{};std::uint32_t generation{};
    if(!read.value(b.source+0x440,entity) || entity.handle!=b.entity || entity.serial!=b.serial || !read.weak(entity)
        || !read.value(b.source+0x180,generation) || generation!=b.owner.value) { return {}; }
    const auto player=local_controlled_entity();if(player==UINT32_MAX) { return {}; }
    const auto who=resolve_requester(static_cast<const std::byte*>(component)+0x2E0,player);
    if(who.entity!=player || who.localEntity!=player) { return {}; }
    return {b,at<std::int32_t>(bytes.data()+0x2DC),at<std::int32_t>(bytes.data()+0x2D8),player};
}
void after_trial_use(void* component,const TrialUse& before) noexcept {
    if(!before.binding.valid()) { return; }const auto after=before_trial_use(component);
    if(after.binding!=before.binding || after.player!=before.player || after.requested!=before.requested) { return; }
    std::uint8_t active{};if(!read_at(reinterpret_cast<std::uintptr_t>(component)+0x2D0,active) || active>1) { return; }
    state::activity::deadly_trial::observe_interaction(before.binding,before.requested,before.before,after.before,active==1);
}
/**
 * Totals one B7E3C0 damage summary and emits the bounded probe lines it completes.
 * Runs on the damage thread inside the owner's CallGate scope, before the native
 * forward. It never reads the native regions argument: that layout is unrecovered,
 * so the capture records only that one was supplied.
 */
void observe_damage_summary(std::uint32_t attacker, std::uint32_t target, bool killed, bool mode,
                            const void* regions, float amount) noexcept {
    // Original 4B2260 is the same local-entity call the interaction receipts already make
    // from this thread. A player that does not resolve classifies as unknown, never as a
    // match, so an unresolved frame cannot inflate the outgoing total.
    const auto local = local_controlled_entity();
    const auto direction = probe::classify(attacker, target, local);
    const auto now = GetTickCount64();
    probe::Window completed{};
    std::uint64_t sequence{}, span{}, windowRun{}, run{};
    bool emitRow{}, emitWindow{};
    AcquireSRWLockExclusive(&g_lock);
    run = g_run;
    if (g_damageWindowStart == 0) { g_damageWindowStart = now; }
    // A run change closes the window that was open, so an encounter keeps its tail
    // instead of losing it to the next run's first summary.
    if (run != g_damageRun) {
        if (g_damageWindow.calls != 0) {
            completed = g_damageWindow;
            span = now - g_damageWindowStart;
            windowRun = g_damageRun;
            emitWindow = true;
        }
        g_damageWindow = {};
        g_damageCalls = 0;
        g_damageRows = 0;
        g_damageWindowStart = now;
        g_damageRun = run;
    }
    probe::observe(g_damageWindow, direction, killed, mode, regions != nullptr, amount);
    sequence = ++g_damageCalls;
    if (g_damageRows < kDamageProbeRows) { ++g_damageRows; emitRow = true; }
    if (!emitWindow && g_damageWindow.calls >= kDamageProbeWindow) {
        completed = g_damageWindow;
        span = now - g_damageWindowStart;
        windowRun = run;
        g_damageWindow = {};
        g_damageWindowStart = now;
        emitWindow = true;
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (emitRow) {
        std::array<char, 320> line{};
        const int length = std::snprintf(line.data(), line.size(),
            "ev=damage_probe stage=summary run=%llu seq=%llu dir=%s attacker=%08X target=%08X "
            "local=%08X killed=%u mode=%u regions=%u amount=%.6g finite=%u mutation=observe_only",
            static_cast<unsigned long long>(run), static_cast<unsigned long long>(sequence),
            probe::text(direction), attacker, target, local, killed ? 1U : 0U, mode ? 1U : 0U,
            regions != nullptr ? 1U : 0U, static_cast<double>(amount),
            std::isfinite(amount) ? 1U : 0U);
        if (length > 0 && static_cast<std::size_t>(length) < line.size()) {
            core::log::write(core::log::Channel::client, core::log::Level::info,
                            {line.data(), static_cast<std::size_t>(length)});
        }
    }
    if (!emitWindow) { return; }
    // The extremes mean nothing without a finite sample, so they report zero instead.
    const double minimum = completed.finite != 0 ? static_cast<double>(completed.amountMin) : 0.0;
    const double maximum = completed.finite != 0 ? static_cast<double>(completed.amountMax) : 0.0;
    std::array<char, 512> line{};
    const int length = std::snprintf(line.data(), line.size(),
        "ev=damage_probe stage=window run=%llu calls=%llu span_ms=%llu out=%llu out_row=%llu "
        "in=%llu in_row=%llu self=%llu other=%llu unknown=%llu killed=%llu mode=%llu regions=%llu "
        "finite=%llu nonfinite=%llu nonpositive=%llu sum=%.6g out_sum=%.6g min=%.6g max=%.6g "
        "mutation=observe_only",
        static_cast<unsigned long long>(windowRun),
        static_cast<unsigned long long>(completed.calls), static_cast<unsigned long long>(span),
        static_cast<unsigned long long>(completed.count(probe::Direction::outgoing)),
        static_cast<unsigned long long>(completed.count(probe::Direction::outgoingRow)),
        static_cast<unsigned long long>(completed.count(probe::Direction::incoming)),
        static_cast<unsigned long long>(completed.count(probe::Direction::incomingRow)),
        static_cast<unsigned long long>(completed.count(probe::Direction::self)),
        static_cast<unsigned long long>(completed.count(probe::Direction::other)),
        static_cast<unsigned long long>(completed.count(probe::Direction::unknown)),
        static_cast<unsigned long long>(completed.killed),
        static_cast<unsigned long long>(completed.modeSet),
        static_cast<unsigned long long>(completed.regionsPresent),
        static_cast<unsigned long long>(completed.finite),
        static_cast<unsigned long long>(completed.nonFinite),
        static_cast<unsigned long long>(completed.nonPositive),
        completed.amountSum, completed.outgoingSum, minimum, maximum);
    if (length > 0 && static_cast<std::size_t>(length) < line.size()) {
        core::log::write(core::log::Channel::client, core::log::Level::info,
                        {line.data(), static_cast<std::size_t>(length)});
    }
}
#include "beyond_infinity_plate_hooks.inl"
#include "beyond_infinity_object_receipts.inl"
#include "deep_storage_object_receipts.inl"
#include "hijacked_object_receipts.inl"
#include "strike_bond_tethers.inl"
#include "strike_bond_object_receipts.inl"
#include "lost_sector_object_receipts.inl"
#include "gateway_module_receipts.inl"
#include "gateway_module_damage_hooks.inl"

__declspec(noinline) void __fastcall carry_hook(void* component, std::uint8_t state, const void* holder) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    const auto nav = state::activity::omega_presentation::navigation();
    std::array<std::byte,0x480> before{};
    eventKeys::State key{};
    if(scope.accepts_side_effects() && !g_dunkInFlight && copy(component,before)
        && prefix(before.data(),at<std::uint32_t>(before.data()),0x80804221,0x598))
        key=eventKeys::bridge::carrier(at<std::uint32_t>(before.data()),at<std::uint32_t>(before.data()+0x2C));
    hooking::await_original(g_carry)(component, state, holder);
    if (scope.accepts_side_effects() && nav.enabled) { observe_carry(component, nav.run); }
    if(scope.accepts_side_effects() && key.epoch) {
        std::array<std::byte,0x480> after{};
        if(!copy(component,after) || std::memcmp(before.data(),after.data(),16)!=0
            || at<std::uint32_t>(before.data()+0x24)!=at<std::uint32_t>(after.data()+0x24)
            || at<std::uint32_t>(after.data()+0x2C)!=key.keyEntity
            || !public_event_deferred_placement_observer::live_entity(key.keyEntity))return;
        const auto mode=at<std::uint8_t>(after.data()+0x470);
        const bool held=mode==1 || mode==3;
        std::uint32_t owner=UINT32_MAX;
        if(held) {
            owner=public_event_deferred_placement_observer::holder_context(holder);
            if(owner==UINT32_MAX && g_holder)g_holder(component,&owner);
            if(!public_event_deferred_placement_observer::live_entity(owner))return;
        }
        const bool accepted=eventKeys::bridge::carry(key,at<std::uint32_t>(after.data()+0x24),owner,held);
        report("ev=public_event_key stage=native_carry item=%08X holder=%08X state=%u accepted=%u mutation=observe_only",key.keyEntity,owner,mode,accepted?1U:0U);
    }
}
namespace rallyBridge=server::runtime::activity::public_event::native_bridge;
namespace rallyUse=server::runtime::activity::public_event::rally_use;
struct PendingRally final {
    rallyUse::Binding binding{};
    std::array<std::byte,rallyUse::kBytes> before{};
    Requester requester{};
};
PendingRally before_rally(void* component) noexcept {
    PendingRally pending{};
    if(!copy(component,pending.before))return {};
    const auto* bytes=pending.before.data();
    if(at<std::uint32_t>(bytes+4)!=0x80804FB2)return {};
    pending.binding=rallyBridge::lookup_use(at<std::uint32_t>(bytes),at<std::uint32_t>(bytes+0x2C));
    if(!pending.binding.epoch)return {};
    pending.requester=resolve_requester(static_cast<const std::byte*>(component)+0x2E0,UINT32_MAX);
    // This local testing receipt must name the exact local player, including its
    // salt, and its currently controlled entity. A shared table kind is insufficient.
    if(pending.requester.raw==UINT32_MAX || pending.requester.raw!=pending.requester.local
        || pending.requester.entity==UINT32_MAX || pending.requester.entity!=pending.requester.localEntity)return {};
    return pending;
}
void after_rally(void* component,const PendingRally& pending) noexcept {
    if(!pending.binding.epoch)return;
    std::array<std::byte,rallyUse::kBytes> after{};rallyUse::Receipt receipt{};
    const bool qualified=copy(component,after) && rallyUse::qualify(pending.binding,pending.before,after,
        pending.requester.raw,pending.requester.entity,rallyUse::kProducerRva,receipt);
    const bool accepted=qualified && rallyBridge::submit_use(receipt);
    report("ev=public_event_rally stage=native_use owner=%016llX entity=%08X controller=%08X requested=%d consumed=%d qualified=%u accepted=%u mutation=observe_only",
        static_cast<unsigned long long>(pending.binding.ticket.lease.owner.sessionId),pending.binding.entity,
        at<std::uint32_t>(pending.before.data()+0x24),at<std::int32_t>(pending.before.data()+0x2DC),
        at<std::int32_t>(after.data()+0x2D8),qualified?1U:0U,accepted?1U:0U);
}
struct PendingEventKey final {eventKeys::Use use{};std::array<std::byte,eventKeys::kUseBytes> before{};};
PendingEventKey before_event_key(void* component) noexcept {
    PendingEventKey pending{};
    if(!copy(component,pending.before))return {};
    const auto sink=eventKeys::bridge::sink(at<std::uint32_t>(pending.before.data()),at<std::uint32_t>(pending.before.data()+0x2C));
    if(!sink.epoch || !public_event_deferred_placement_observer::live_entity(sink.sinkEntity))return {};
    const auto requester=resolve_requester(static_cast<const std::byte*>(component)+0x2E0,UINT32_MAX);
    const auto key=eventKeys::bridge::held(sink,requester.entity);
    if(requester.raw==UINT32_MAX || requester.entity==UINT32_MAX
        || !key.epoch || !public_event_deferred_placement_observer::live_entity(key.keyEntity)
        || !public_event_deferred_placement_observer::live_entity(requester.entity)
        || !eventKeys::before_use(key,sink,pending.before,requester.raw,requester.entity,pending.use))return {};
    return pending;
}
void after_event_key(void* component,const PendingEventKey& pending) noexcept {
    if(!pending.use.state.epoch)return;
    std::array<std::byte,eventKeys::kUseBytes> after{};
    const bool qualified=copy(component,after) && eventKeys::after_use(pending.use,pending.before,after);
    const bool accepted=qualified && eventKeys::bridge::deposit(pending.use);
    report("ev=public_event_key stage=native_deposit item=%08X sink=%08X holder=%08X requested=%d qualified=%u accepted=%u mutation=observe_only",
        pending.use.state.keyEntity,pending.use.sink.sinkEntity,pending.use.playerEntity,pending.use.requested,qualified?1U:0U,accepted?1U:0U);
}
__declspec(noinline) void __fastcall dunk_hook(void* component) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    const auto nav = state::activity::omega_presentation::navigation();
    const auto pending = scope.accepts_side_effects() && nav.enabled ? before_dunk(component, nav.run) : PendingDunk{};
    const auto trialUse=scope.accepts_side_effects()?before_trial_use(component):TrialUse{};
    const auto rally=scope.accepts_side_effects()?before_rally(component):PendingRally{};
    const auto key=scope.accepts_side_effects()?before_event_key(component):PendingEventKey{};
    const bool previous = g_dunkInFlight;
    g_dunkInFlight = pending.proof.held.valid() || key.use.state.epoch!=0;
    hooking::await_original(g_dunk)(component);
    g_dunkInFlight = previous;
    if (scope.accepts_side_effects()) { after_dunk(component, pending);after_trial_use(component,trialUse);after_rally(component,rally);after_event_key(component,key); }
}
__declspec(noinline) bool __fastcall create_hook(void* component) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    const auto publicEvent=scope.accepts_side_effects()?public_event_deferred_placement_observer::begin(component):public_event_deferred_placement_observer::Context{};
    const bool created = garden_tether::create(component,hooking::await_original(g_create),scope.accepts_side_effects());
    if(scope.accepts_side_effects())static_cast<void>(public_event_deferred_placement_observer::finish(component,publicEvent,created));
    if (created && scope.accepts_side_effects()) {
        observe_gateway_module(component);
        const auto nav = state::activity::omega_presentation::navigation();
        if (nav.enabled) {
            observe_carrier(component, nav.run, "create");
            observe_transit(component, nav.run, "create");
            observe_rescue_marker(component, nav.run, "create");
        }
    }
    return created;
}
__declspec(noinline) void __fastcall interaction_hook(void* component, std::uint32_t player,
    std::uint8_t flags, const void* view, std::uint64_t value5, std::uint64_t value6,
    std::uint64_t value7, std::uint64_t value8, std::uint8_t publish, void* output) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    hooking::await_original(g_interaction)(component, player, flags, view,
        value5, value6, value7, value8, publish, output);
    // Observe the original published prompt, never substitute its eligibility
    // or range. F32CD0 writes output+1 and component+295 status0 hidden/1
    // unavailable/2 ready; component+291 is eligibility before spatial checks.
    if (!scope.accepts_side_effects() || publish == 0) { return; }
    const auto nav = state::activity::omega_presentation::navigation();
    if (!nav.enabled) { return; }
    std::array<std::byte,16> header{};
    if (!copy(component,header)) { return; }
    const catalog::Cycle* cycle{};
    for (const auto& row:catalog::kCycles) {
        if (prefix(header.data(),row.sinkController,0x80804FB2U,0x388U)) { cycle=&row;break; }
    }
    if (cycle==nullptr) { return; }
    std::array<std::byte,0x2D1> bytes{};
    std::array<std::byte,0x29> published{};
    if (!copy(component,bytes) || !copy(output,published)) { return; }
    const PromptSeen now{at<std::uint32_t>(bytes.data()+0x24),at<std::uint32_t>(bytes.data()+0x2C),player,
        at<std::uint32_t>(published.data()+0x10),at<std::uint32_t>(published.data()+0x14),flags,
        at<std::uint8_t>(published.data()+1),at<std::uint8_t>(bytes.data()+0x291),
        at<std::uint8_t>(bytes.data()+0x2C0),at<std::uint8_t>(bytes.data()+0x2D0),true};
    bool changed{};
    AcquireSRWLockExclusive(&g_lock);
    if (g_run==nav.run && g_bindings.find(now.entity,catalog::Object::sink).valid()) {
        changed=now!=g_promptSeen[cycle->index];g_promptSeen[cycle->index]=now;
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (changed) {
        report_capped(g_promptLines,48U,
            "ev=omega_charge stage=native_prompt run=%llu cycle=%u controller=%08X entity=%08X player=%08X flags=%02X status=%u eligible_before_range=%u blocked=%u used=%u label=%08X:%08X mutation=observe_only",
            static_cast<unsigned long long>(nav.run),static_cast<unsigned>(cycle->index+1U),
            now.controller,now.entity,now.player,static_cast<unsigned>(now.flags),static_cast<unsigned>(now.status),
            static_cast<unsigned>(now.eligible),static_cast<unsigned>(now.blocked),static_cast<unsigned>(now.used),
            now.label0,now.label1);
    }
}
bool idle() noexcept { return g_gate.idle(); }
void* target(std::uintptr_t rva, const std::array<std::uint8_t, 16>& expected) noexcept {
    std::array<std::byte, 16> bytes{};
    const auto pointer = reinterpret_cast<void*>(g_image + rva);
    return copy(pointer, bytes) && std::memcmp(bytes.data(), expected.data(), expected.size()) == 0 ? pointer : nullptr;
}
} // namespace

void observe_omega_arc_charge_carrier(void* component, std::uint64_t run) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    if (!scope.accepts_side_effects()) { return; }
    observe_carrier(component, run, "apply");
    observe_transit(component, run, "apply");
    observe_rescue_marker(component, run, "apply");
}

bool install_omega_arc_charge_receipts() noexcept {
    if (g_handles[0].attached) { return g_gate.accepting(); }
    g_image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (g_image == 0) { return false; }
    static_assert(native_hook_ownership::kArcCharge[8]==plate_native::kTick.rva);
    const std::array<hooking::detour::Spec, 9> specs{{
        {target(native_hook_ownership::kArcCharge[0], {0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xEC,0x50,0x49}), reinterpret_cast<void*>(&carry_hook)},
        {target(native_hook_ownership::kArcCharge[1], {0x48,0x89,0x5C,0x24,0x18,0x57,0x48,0x83,0xEC,0x20,0x44,0x8B,0x01,0x48,0x8B,0xD9}), reinterpret_cast<void*>(&dunk_hook)},
        {target(native_hook_ownership::kArcCharge[2], {0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7C,0x24,0x20,0x41,0x56,0x48,0x83,0xEC,0x20}), reinterpret_cast<void*>(&create_hook)},
        {target(native_hook_ownership::kArcCharge[3], {0x48,0x89,0x5C,0x24,0x10,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57}), reinterpret_cast<void*>(&interaction_hook)},
        {target(native_hook_ownership::kArcCharge[4], {0x40,0x55,0x56,0x41,0x54,0x48,0x8D,0xAC,0x24,0x60,0xF4,0xFF,0xFF,0x48,0x81,0xEC}), reinterpret_cast<void*>(&gateway_sense_hook)},
        {target(native_hook_ownership::kArcCharge[5], {0xE9,0xA5,0xC5,0xAB,0x04,0x53,0x52,0x41,0x54,0x41,0x53,0x41,0x55,0x57,0x41,0x51}), reinterpret_cast<void*>(&gateway_damage_hook)},
        {target(native_hook_ownership::kArcCharge[6], {0x48,0x8B,0x41,0x08,0x0F,0xB6,0x80,0x38,0x03,0x00,0x00,0xD0,0xE8,0xF6,0xD0,0x24}), reinterpret_cast<void*>(&gateway_damage_gate_hook)},
        {target(native_hook_ownership::kArcCharge[7], {0x48,0x8B,0xC4,0x48,0x89,0x58,0x18,0x48,0x89,0x70,0x20,0x55,0x57,0x41,0x56,0x48}), reinterpret_cast<void*>(&gateway_damage_summary_hook)},
        {target(native_hook_ownership::kArcCharge[8],plate_native::kTick.signature),reinterpret_cast<void*>(&beyond_plate_tick_hook)},
    }};
    g_association = reinterpret_cast<Association>(target(0x352310U,
        {0x48,0x83,0xEC,0x08,0x44,0x8B,0x51,0x04,0x4C,0x8B,0xCA,0xC7,0x02,0xFF,0xFF,0xFF}));
    g_holder = reinterpret_cast<Holder>(target(0x597B10U,
        {0x40,0x53,0x48,0x83,0xEC,0x20,0x8B,0x41,0x2C,0x48,0x8B,0xDA,0xC7,0x02,0xFF,0xFF}));
    g_localPlayer = reinterpret_cast<LocalPlayer>(target(0x4AFE80U,
        {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x83}));
    g_controlled = reinterpret_cast<ControlledEntity>(target(0x4B2260U,
        {0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0xC7,0x01,0xFF,0xFF,0xFF,0xFF,0x48}));
    if (specs[0].target == nullptr || specs[1].target == nullptr || specs[2].target == nullptr
        || specs[3].target == nullptr || specs[4].target == nullptr || specs[5].target == nullptr
        || specs[6].target == nullptr || specs[7].target == nullptr
        || g_association == nullptr || g_holder == nullptr || g_localPlayer == nullptr
        || g_controlled == nullptr
        || !specs[8].target
        || !hooking::detour::install(specs, g_handles)) {
        core::log::write(core::log::Channel::client, core::log::Level::info,
                        "ev=omega_charge stage=install result=fail carry=D99620 dunk=F36640 create=9EFFC0 interaction=F32CD0 association=352310 holder=597B10 local_player=4AFE80 controlled=4B2260");
        g_association = nullptr; g_holder = nullptr; g_localPlayer = nullptr; g_controlled = nullptr;
        return false;
    }
    hooking::publish_original(g_carry, reinterpret_cast<Carry>(g_handles[0].original));
    hooking::publish_original(g_dunk, reinterpret_cast<Dunk>(g_handles[1].original));
    hooking::publish_original(g_create, reinterpret_cast<Create>(g_handles[2].original));
    hooking::publish_original(g_interaction, reinterpret_cast<Interaction>(g_handles[3].original));
    hooking::publish_original(g_gatewaySense, reinterpret_cast<SourceSense>(g_handles[4].original));
    hooking::publish_original(g_moduleDamage, reinterpret_cast<ModuleDamage>(g_handles[5].original));
    hooking::publish_original(g_moduleDamageGate, reinterpret_cast<ModuleDamageGate>(g_handles[6].original));
    hooking::publish_original(g_moduleDamageSummary, reinterpret_cast<ModuleDamageSummary>(g_handles[7].original));
    hooking::publish_original(g_plateTick,reinterpret_cast<PlateTick>(g_handles[8].original));
    g_gate.accept();
    core::log::write(core::log::Channel::client, core::log::Level::info,
                    "ev=omega_charge stage=install result=ok carry=D99620 dunk=F36640 create=9EFFC0 interaction=F32CD0 gateway_module_damage=B804E0 gate=CDCB60 summary=B7E3C0 mutation=native_source_interaction_authority_with_observed_receipts");
    return true;
}
void quiesce_omega_arc_charge_receipts() noexcept {
    g_gate.quiesce();
}
bool uninstall_omega_arc_charge_receipts() noexcept {
    quiesce_omega_arc_charge_receipts();
    if (!g_handles[0].attached) { return true; }
    const std::array<hooking::detour::ProtectedCodeEntry, 41> protectedCode{{
        {reinterpret_cast<void*>(&gateway_damage_hook)}, {reinterpret_cast<void*>(&gateway_damage_gate_hook)},
        {reinterpret_cast<void*>(&gateway_damage_summary_hook)}, {reinterpret_cast<void*>(&gateway_damage_receipt)},
        {reinterpret_cast<void*>(&gateway_damage_blocked)},
        {reinterpret_cast<void*>(&hijacked_damage::current)}, {reinterpret_cast<void*>(&hijacked_damage::query)},
        {reinterpret_cast<void*>(&hijacked_damage::immune)}, {reinterpret_cast<void*>(&hijacked_damage::before)},
        {reinterpret_cast<void*>(&hijacked_damage::after)},
        {reinterpret_cast<void*>(&strike_pact_damage::current)}, {reinterpret_cast<void*>(&strike_pact_damage::query)},
        {reinterpret_cast<void*>(&strike_pact_damage::immune)}, {reinterpret_cast<void*>(&strike_pact_damage::before)},
        {reinterpret_cast<void*>(&strike_pact_damage::after)},
        {reinterpret_cast<void*>(&gateway_sense_hook)}, {reinterpret_cast<void*>(&observe_gateway_module)},
        {reinterpret_cast<void*>(&observe_beyond_object)},
        {reinterpret_cast<void*>(&beyond_plate_tick_hook)}, {reinterpret_cast<void*>(&drive_plate)},
        {reinterpret_cast<void*>(&plate_consumed)}, {reinterpret_cast<void*>(&plate_addresses)},
        {reinterpret_cast<void*>(&same_plate_request)},
        {reinterpret_cast<void*>(&plate_pose_sample)}, {reinterpret_cast<void*>(&observe_beyond_plate_pose)},
        {reinterpret_cast<void*>(&carry_hook)}, {reinterpret_cast<void*>(&dunk_hook)},
        {reinterpret_cast<void*>(&create_hook)},
        {reinterpret_cast<void*>(&interaction_hook)},
        {reinterpret_cast<void*>(&observe_carry)}, {reinterpret_cast<void*>(&after_dunk)},
        {reinterpret_cast<void*>(&observe_carrier)}, {reinterpret_cast<void*>(&observe_transit)},
        {reinterpret_cast<void*>(&observe_rescue_marker)},
        {reinterpret_cast<void*>(&observe_omega_arc_charge_carrier)},
        {reinterpret_cast<void*>(&resolve_requester)},
        {reinterpret_cast<void*>(&observe_trial_object)}, {reinterpret_cast<void*>(&before_trial_use)},
        {reinterpret_cast<void*>(&after_trial_use)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::leave)},
    }};
    if (hooking::detour::uninstall(g_handles, protectedCode, idle) != hooking::detour::UninstallResult::removed) { return false; }
    g_carry.store(nullptr, std::memory_order_release); g_dunk.store(nullptr, std::memory_order_release);
    g_create.store(nullptr, std::memory_order_release);
    g_gatewaySense.store(nullptr, std::memory_order_release);g_gatewayModuleLines=0;
    g_gatewayModuleState.store(UINT64_MAX,std::memory_order_relaxed);
    g_moduleDamage.store(nullptr,std::memory_order_release);g_moduleDamageGate.store(nullptr,std::memory_order_release);
    g_moduleDamageSummary.store(nullptr,std::memory_order_release);
    g_plateTick.store(nullptr,std::memory_order_release);
    g_interaction.store(nullptr, std::memory_order_release);
    g_association = nullptr; g_holder = nullptr; g_localPlayer = nullptr; g_controlled = nullptr; g_image = 0;
    AcquireSRWLockExclusive(&g_lock); g_run = 0; g_bindings.reset(0); g_heldToken = {};
    g_rejects = {}; g_rejectCount = 0; g_rejectLines = 0;
    g_transitSeen = {}; g_transitLines = 0; ReleaseSRWLockExclusive(&g_lock);
    g_promptSeen = {}; g_promptLines = 0;g_beyondLensCandidate={};g_deepLensCandidate={};
    return true;
}
} // namespace dawn::client::hooks::bootflow
