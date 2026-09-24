#include "activity_forced_destination.h"
#include "prelaunch_profile.h"

#include <Windows.h>

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdio>
#include <span>
#include <string_view>

#include "../../../core/logging/log.h"
#include "../runtime.h"
#include "../../runtime/storage/internal.h"

namespace dawn::state::activity::forced {
namespace {

/** Authored investment activity indices recovered from the pinned client's activity table. */
constexpr std::int16_t kTowerCinematicActivityIndex = 2;
constexpr std::int16_t kChosenActivityIndex = 282;
constexpr std::int16_t kOmegaActivityIndex = 299;

/** Hidden missions stay staged until their supported native launch boundary. */
std::atomic_bool g_prelaunchCommitted{};
/** Bounds the staged-fallback diagnostic to one line per configured destination. */
std::atomic_bool g_prelaunchStagedReported{};
/** Final host-migration acknowledgement for the current authored opening launch. */
std::atomic_bool g_openingHostReady{};
/** Protected with the stored configuration by g_stateLock. A completed Omega
 * launch must remain released even if native teardown visits idle before svc6.
 * Only a changed operator configuration or Clear re-arms the override. */
bool g_omegaCompletionSuspended{};
std::int16_t g_directActivity{destination::kAbsentActivityIndex};
std::int16_t g_directChainSource{destination::kAbsentActivityIndex};

[[nodiscard]] bool same_configuration(const ForcedDestination& left,
                                      const ForcedDestination& right) noexcept {
    return left.packageName == right.packageName
           && left.packageNameLength == right.packageNameLength && left.bubble == right.bubble
           && left.sliceSet == right.sliceSet && left.spawnSetHash == right.spawnSetHash
           && left.hasBubble == right.hasBubble && left.hasSliceSet == right.hasSliceSet
           && left.hasSpawnSetHash == right.hasSpawnSetHash
           && left.bubbleless == right.bubbleless && left.enabled == right.enabled;
}

[[nodiscard]] bool homecoming(const ForcedDestination& value) noexcept {
    return std::string_view(value.packageName.data(), value.packageNameLength)
           == "mission_towerfall";
}

[[nodiscard]] bool authored_chosen_selection(
    const destination::DestinationSelection& selection) noexcept {
    return selection.descriptorBitLength != 0
           && (selection.activityIndex == kChosenActivityIndex
               || selection.previousActivityIndex == kChosenActivityIndex);
}

/** Resolves the archived campaign destinations whose exact investment rows are known. */
[[nodiscard]] constexpr std::int16_t
opening_activity_index(std::string_view packageName) noexcept {
    if (packageName == "cine_110_twr") {
        return kTowerCinematicActivityIndex;
    }
    if (const auto* profile = prelaunch::find(packageName)) {
        return profile->activity;
    }
    if (packageName == "mission_scot") {
        return kOmegaActivityIndex;
    }
    return destination::kAbsentActivityIndex;
}

/** Bits in one byte. The name field is 40 of them, back to back. */
constexpr std::size_t kBitsPerByte = 8;
/** The captured descriptor starts with reason, source activity and destination activity. */
constexpr std::size_t kReasonBit = 0;
constexpr std::size_t kSourceActivityBit = 4;
constexpr std::size_t kActivityBit = 16;
constexpr std::size_t kReasonWidth = 4;
constexpr std::size_t kActivityWidth = 12;
/** All three descriptor-prefix scalars use a wire bias of one. */
constexpr unsigned kScalarBias = 1;
/** Every name byte is encoded with this bias, and padding is a biased zero. */
constexpr unsigned kPackageNameBias = 128;
/** The most significant bit of a byte, where each packed field starts. */
constexpr unsigned kHighBit = 0x80;

/**
 * Writes one scalar into bit-packed storage at a bit offset.
 * @param bits Storage large enough to hold the complete field at that offset.
 * @param bitOffset First bit of the field.
 * @param width Number of bits in the field.
 * @param value Unsigned scalar to write, most significant bit first.
 */
void write_bits(std::span<std::byte> bits,
                std::size_t bitOffset,
                std::size_t width,
                unsigned value) noexcept {
    for (std::size_t index = 0; index < width; ++index) {
        const std::size_t bit = bitOffset + index;
        std::byte& target = bits[bit / kBitsPerByte];
        const unsigned mask = kHighBit >> (bit % kBitsPerByte);
        const bool set = (value >> (width - 1 - index) & 1U) != 0;
        target = static_cast<std::byte>(set ? static_cast<unsigned>(target) | mask
                                            : static_cast<unsigned>(target) & ~mask);
    }
}

/**
 * Rewrites the captured descriptor's selection prefix and package name with the forced values.
 * @param selection Committed destination holding the captured bits.
 * @param value Forced destination whose name replaces the captured one.
 * @return True when the whole 40-byte field sat inside the captured bits.
 */
[[nodiscard]] bool rewrite_descriptor(destination::DestinationSelection& selection,
                                      const ForcedDestination& value) noexcept {
    const std::size_t nameBits = destination::kPackageNameCapacity * kBitsPerByte;
    if (!selection.hasDescriptorName || selection.descriptorBitLength == 0
        || kActivityBit + kActivityWidth > selection.descriptorBitLength
        || selection.descriptorNameBit + nameBits > selection.descriptorBitLength) {
        return false;
    }
    write_bits(selection.descriptorBits,
               kReasonBit,
               kReasonWidth,
               static_cast<unsigned>(selection.reason + kScalarBias));
    write_bits(selection.descriptorBits,
               kSourceActivityBit,
               kActivityWidth,
               static_cast<unsigned>(selection.previousActivityIndex + kScalarBias));
    write_bits(selection.descriptorBits,
               kActivityBit,
               kActivityWidth,
               static_cast<unsigned>(selection.activityIndex + kScalarBias));
    for (std::size_t index = 0; index < destination::kPackageNameCapacity; ++index) {
        // Past the name the field is padding, and a biased zero decodes outside the name charset,
        // which is what ends the name.
        const unsigned character = index < value.packageNameLength
                                       ? static_cast<unsigned char>(value.packageName[index])
                                       : 0U;
        write_bits(selection.descriptorBits,
                   selection.descriptorNameBit + index * kBitsPerByte,
                   kBitsPerByte,
                   character + kPackageNameBias & 0xFFU);
    }
    return true;
}

} // namespace

/** Replaces the forced destination. */
bool publish(const ForcedDestination& value) noexcept {
    if (!storable(value)) {
        return false;
    }
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    const bool changed =
        !same_configuration(runtime::storage::g_state.activity.forced, value);
    runtime::storage::g_state.activity.forced = value;
    g_directActivity = destination::kAbsentActivityIndex;
    g_directChainSource = destination::kAbsentActivityIndex;
    if (changed || !active(value)) { g_omegaCompletionSuspended = false; }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    if (changed || !active(value) || !prelaunch::configured(value)) {
        g_prelaunchCommitted.store(false, std::memory_order_release);
        g_prelaunchStagedReported.store(false, std::memory_order_release);
    }
    // Readiness belongs to the complete forced-destination configuration, not to Homecoming.
    // An unchanged mission_scot publish must not erase the final peer-reestablish acknowledgement.
    if (changed || !active(value)) {
        g_openingHostReady.store(false, std::memory_order_release);
    }
    if (changed && active(value) && prelaunch::configured(value)) {
        std::array<char, 256> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=activity_profile stage=selection result=staged package=%.*s bubble=%u slice=%u spawn=%s commit=awaiting_chosen_activity_282",
            static_cast<int>(value.packageNameLength), value.packageName.data(),
            static_cast<unsigned>(value.bubble),
            static_cast<unsigned>(value.sliceSet),
            value.hasSpawnSetHash ? "forced" : "authored");
        if (length > 0) {
            core::log::write(core::log::Channel::server,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
    return true;
}

bool publish_direct(const ForcedDestination& value, std::int16_t activity,
                    std::int16_t chainSource) noexcept {
    if (!active(value) || !storable(value) || activity < 0) { return false; }
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    runtime::storage::g_state.activity.forced = value;
    g_directActivity = activity;
    g_directChainSource = chainSource;
    g_omegaCompletionSuspended = false;
    g_prelaunchCommitted.store(true, std::memory_order_release);
    g_openingHostReady.store(false, std::memory_order_release);
    g_prelaunchStagedReported.store(false, std::memory_order_release);
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    return true;
}

std::int16_t direct_snapshot(ForcedDestination& value) noexcept {
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const auto suspended = g_omegaCompletionSuspended;
    value = suspended ? ForcedDestination{} : runtime::storage::g_state.activity.forced;
    const auto activity = suspended ? destination::kAbsentActivityIndex : g_directActivity;
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    return activity;
}

/** Copies the operator's raw stored panel selection, empty until the panel sets one. */
void stored(ForcedDestination& value) noexcept {
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    value = runtime::storage::g_state.activity.forced;
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
}

/**
 * Copies the effective forced destination: exactly the operator's stored override-panel
 * selection. There is no built-in auto-forced Omega default, so an empty panel means no forced
 * destination and every map pick loads normally; Omega launches only when the panel selects
 * mission_scot. The panel display also reads stored().
 */
void snapshot(ForcedDestination& value) noexcept {
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    value = g_omegaCompletionSuspended ? ForcedDestination{}
                                     : runtime::storage::g_state.activity.forced;
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
}

static bool suspend_completed_run(std::uint64_t run,std::string_view package) noexcept {
    if (run == 0 || run != mission_run_generation()) { return false; }
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    const auto& value = runtime::storage::g_state.activity.forced;
    const bool matches = value.packageNameLength <= value.packageName.size()
        && std::string_view(value.packageName.data(), value.packageNameLength) == package;
    const bool accepted = run == mission_run_generation() && (!active(value) || matches);
    if (accepted && active(value)) { g_omegaCompletionSuspended = true; }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    if (accepted) {
        g_openingHostReady.store(false, std::memory_order_release);
        core::log::write(core::log::Channel::server, core::log::Level::info,
            "ev=activity_override stage=native_complete result=paused_until_configuration_changes");
    }
    return accepted;
}

bool suspend_omega_for_completed_run(std::uint64_t run) noexcept {return suspend_completed_run(run,"mission_scot");}
bool suspend_launchpad_for_completed_run(std::uint64_t run) noexcept {return suspend_completed_run(run,"mission_launchpad");}

bool omega_completion_suspended() noexcept {
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const bool paused = g_omegaCompletionSuspended;
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    return paused;
}

/** Drops the selection and the switch, the same as the interface's clear action. */
void clear() noexcept {
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    runtime::storage::g_state.activity.forced = {};
    g_directActivity = destination::kAbsentActivityIndex;
    g_directChainSource = destination::kAbsentActivityIndex;
    g_omegaCompletionSuspended = false;
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    g_prelaunchCommitted.store(false, std::memory_order_release);
    g_prelaunchStagedReported.store(false, std::memory_order_release);
    g_openingHostReady.store(false, std::memory_order_release);
}

bool release_haunted_forest_for_native_selection(std::int16_t source,
    std::int16_t destination,std::string_view package) noexcept {
    if (!prelaunch::native_haunted_forest(source,destination,package)) return false;
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& value=runtime::storage::g_state.activity.forced;
    const bool matching=prelaunch::configured(value)==&prelaunch::kHauntedForest;
    if (matching) {
        value.enabled=false;
        g_prelaunchCommitted.store(false,std::memory_order_release);
        g_prelaunchStagedReported.store(false,std::memory_order_release);
        g_openingHostReady.store(false,std::memory_order_release);
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    return matching;
}

bool commit_prelaunch_authored_selection(std::int16_t sourceActivityIndex,
                                          std::int16_t destinationActivityIndex,
                                          ForcedDestination& committed) noexcept {
    // Select and commit under the same lock as panel changes. Return the exact profile
    // that was committed so the native hook cannot resnapshot a different mission.
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    const auto value = runtime::storage::g_state.activity.forced;
    const bool valid = !g_omegaCompletionSuspended && prelaunch::configured(value)
        && prelaunch::donor(sourceActivityIndex, destinationActivityIndex);
    if (valid) {
        committed = value;
        g_prelaunchCommitted.store(true, std::memory_order_release);
        g_openingHostReady.store(false, std::memory_order_release);
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    if (valid) {
        std::array<char, 256> line{};
        const int length = std::snprintf(line.data(), line.size(),
            "ev=activity_override stage=activation result=committed destination=%.*s trigger=authored_chosen_prelaunch_282",
            static_cast<int>(value.packageNameLength), value.packageName.data());
        if (length > 0) { core::log::write(core::log::Channel::server,
            core::log::Level::info, {line.data(), static_cast<std::size_t>(length)}); }
    }
    return valid;
}

bool commit_homecoming_authored_selection(std::int16_t sourceActivityIndex,
                                          std::int16_t destinationActivityIndex) noexcept {
    ForcedDestination value{};
    snapshot(value);
    if (!homecoming(value)) { return false; }
    return commit_prelaunch_authored_selection(sourceActivityIndex, destinationActivityIndex, value);
}

/** @return True while the stored selection is complete and operationally committed. */
bool override_active() noexcept {
    ForcedDestination value{};
    snapshot(value);
    return active(value)
           && (!prelaunch::configured(value)
               || g_prelaunchCommitted.load(std::memory_order_acquire));
}

bool mission_host_reestablishment_enabled() noexcept {
    if (!override_active()) {
        return false;
    }
    ForcedDestination value{};
    snapshot(value);
    const std::size_t length = value.packageNameLength <= value.packageName.size()
                                   ? value.packageNameLength
                                   : value.packageName.size();
    const std::string_view package(value.packageName.data(), length);
    return package == "cine_110_twr" || package == "mission_towerfall"
           || package == "mission_scot";
}

bool mark_opening_host_ready() noexcept {
    if (!mission_host_reestablishment_enabled()) {
        return false;
    }
    return !g_openingHostReady.exchange(true, std::memory_order_acq_rel);
}

bool mark_towerfall_native_ready() noexcept {
    if (!override_active()) {
        return false;
    }
    ForcedDestination value{};
    snapshot(value);
    const std::size_t length = value.packageNameLength <= value.packageName.size()
                                   ? value.packageNameLength
                                   : value.packageName.size();
    if (std::string_view(value.packageName.data(), length) != "mission_towerfall") {
        return false;
    }
    return !g_openingHostReady.exchange(true, std::memory_order_acq_rel);
}

bool opening_host_ready() noexcept {
    return mission_host_reestablishment_enabled()
           && g_openingHostReady.load(std::memory_order_acquire);
}

/** Overwrites one committed destination with the forced one. */
bool apply(destination::DestinationSelection& selection) noexcept {
    ForcedDestination value{};
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    value = g_omegaCompletionSuspended ? ForcedDestination{} : runtime::storage::g_state.activity.forced;
    const auto directActivity = g_directActivity;
    const auto chainSource = g_directChainSource;
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    if (!active(value)) return false;
    if (directActivity != destination::kAbsentActivityIndex) {
        const std::string_view incoming(reinterpret_cast<const char*>(selection.packageName.data()),
            selection.packageNameLength <= selection.packageName.size() ? selection.packageNameLength : 0);
        // A selection retail chains from the published source may carry that source or no
        // previous activity at all; the exact activity and package still gate the override.
        const bool chained = chainSource != destination::kAbsentActivityIndex
            && (selection.previousActivityIndex == chainSource
                || selection.previousActivityIndex == destination::kAbsentActivityIndex);
        if (selection.activityIndex != directActivity
            || (selection.previousActivityIndex != directActivity && !chained)
            || incoming != std::string_view(value.packageName.data(), value.packageNameLength)) { return false; }
        selection.arrivalBubbleOverride=value.bubble;selection.hasArrivalBubbleOverride=value.hasBubble;
        selection.sliceSetOverride=value.sliceSet;selection.hasSliceSetOverride=value.hasSliceSet;
        selection.spawnSetOverride=value.hasSpawnSetHash?value.spawnSetHash:kAbsentSpawnSetHash;
        selection.hasSpawnSetOverride=value.hasBubble;
        if (incoming == "mission_scot") {
            g_openingHostReady.store(false, std::memory_order_release);
            state::activity::reset_mission_authority_runtime_initialization();
        }
        return true;
    }

    if (const auto* profile=prelaunch::configured(value); profile==&prelaunch::kAdieu || profile==&prelaunch::kGateway || profile==&prelaunch::kDeadlyTrial || profile==&prelaunch::kBeyondInfinity || profile==&prelaunch::kDeepStorage || profile==&prelaunch::kHauntedForest || profile==&prelaunch::kOneAu) {
        const std::string_view incoming(reinterpret_cast<const char*>(selection.packageName.data()),
            selection.packageNameLength <= selection.packageName.size() ? selection.packageNameLength : 0);
        if (!g_prelaunchCommitted.load(std::memory_order_acquire)
            || selection.descriptorBitLength == 0 || !selection.hasDescriptorName
            || selection.descriptorBitLength > selection.descriptorBits.size() * 8
            || selection.descriptorNameBit < 28
            || selection.descriptorNameBit + destination::kPackageNameCapacity * 8 > selection.descriptorBitLength
            || !prelaunch::matches(*profile, selection.previousActivityIndex,
                selection.activityIndex, incoming)) {
            if (!g_prelaunchStagedReported.exchange(true, std::memory_order_acq_rel)) {
                core::log::write(core::log::Channel::server, core::log::Level::info,
                    profile==&prelaunch::kGateway ? "ev=activity_override stage=activation result=staged destination=mission_abs trigger=awaiting_native_gateway_contract_292" : profile==&prelaunch::kHauntedForest ? "ev=activity_override stage=activation result=staged destination=infinite_abyss trigger=awaiting_native_haunted_forest_contract_78" : "ev=activity_override stage=activation result=staged destination=adventure_ginger trigger=awaiting_native_trial_contract_293");
            }
            return false;
        }
    }

    if (prelaunch::configured(value)
        && !g_prelaunchCommitted.load(std::memory_order_acquire)) {
        if (!authored_chosen_selection(selection)) {
            if (!g_prelaunchStagedReported.exchange(true, std::memory_order_acq_rel)) {
                core::log::write(
                    core::log::Channel::server,
                    core::log::Level::info,
                    "ev=activity_override stage=activation result=staged destination=mission_towerfall trigger=awaiting_chosen_activity_282");
            }
            return false;
        }
        g_prelaunchCommitted.store(true, std::memory_order_release);
        g_openingHostReady.store(false, std::memory_order_release);
        core::log::write(
            core::log::Channel::server,
            core::log::Level::info,
            "ev=activity_override stage=activation result=committed destination=mission_towerfall trigger=authored_chosen_activity_282");
    }

    // A forced archived campaign mission can retain the opaque route fields only from another
    // authored mission. The Tower landing-zone descriptor is social-shaped: replaying it after
    // changing the visible activity fields still makes the client start city_tower_social_d2.
    const std::string_view sourcePackageName(
        reinterpret_cast<const char*>(selection.packageName.data()),
        selection.packageNameLength);
    const bool sourceIsTowerSocial = sourcePackageName == "city_tower_social_d2";
    const std::int8_t sourceReason = selection.reason;
    const std::int16_t sourceActivityIndex = selection.previousActivityIndex;
    const std::string_view packageName(value.packageName.data(), value.packageNameLength);
    // Service 6 allocates a new private activity instance. Clear only the generic host-ready latch
    // and the prior instance's type-18 acknowledgement at Omega's launch boundary.
    if (packageName == "mission_scot") {
        g_openingHostReady.store(false, std::memory_order_release);
        state::activity::reset_mission_authority_runtime_initialization();
    }
    const bool openingActivity =
        opening_activity_index(packageName) != destination::kAbsentActivityIndex;
    const bool retainAuthoredSource =
        openingActivity && !sourceIsTowerSocial && selection.descriptorBitLength != 0
        && sourceActivityIndex != destination::kAbsentActivityIndex;

    selection.packageName = {};
    for (std::size_t index = 0; index < value.packageNameLength; ++index) {
        selection.packageName[index] = static_cast<std::int8_t>(value.packageName[index]);
    }
    selection.packageNameLength = value.packageNameLength;
    // All three are absent when a destination is forced rather than picked. Many package names
    // map to several definitions, so no index can be derived from a name.
    // Preserve the campaign node's transition origin when it supplied a real authored descriptor.
    // Clearing this prefix changed CHOSEN's source activity 282 to absent even though all 620
    // opaque authored bits survived, and the client consequently retained the local route.
    selection.reason = retainAuthoredSource ? sourceReason : destination::kMinimumReason;
    selection.previousActivityIndex =
        retainAuthoredSource ? sourceActivityIndex : destination::kAbsentActivityIndex;
    // Retain the authored indices recovered from the pinned client's investment table. A permanent
    // implementation will derive this for every activity during build-data extraction instead of
    // naming the archived campaign packages here.
    selection.activityIndex = opening_activity_index(packageName);
    selection.elementIndex = destination::kAbsentElementIndex;
    selection.hasElementIndex = false;
    // The client named its arrival for the destination it picked, so both wire hashes go with it.
    selection.arrivalBubbleHash = 0;
    selection.hasArrivalBubbleHash = false;
    selection.spawnSetHash = 0;
    selection.hasSpawnSetHash = false;
    selection.arrivalBubbleOverride = value.bubble;
    selection.hasArrivalBubbleOverride = value.hasBubble;
    selection.sliceSetOverride = value.sliceSet;
    selection.hasSliceSetOverride = value.hasSliceSet;
    // With no set chosen the absent hash goes out, so the Client searches the loaded world itself.
    // A map-wide set is not proof that the arrival bubble holds one of its points.
    selection.spawnSetOverride = value.hasSpawnSetHash ? value.spawnSetHash : kAbsentSpawnSetHash;
    selection.hasSpawnSetOverride = value.hasBubble;
    // The mission may be launched from an authored campaign node so its mission-shaped opaque
    // fields survive while the visible destination is rewritten. A Tower-social source is the one
    // known-bad exception and deliberately falls back to the clean descriptor.
    if (!rewrite_descriptor(selection, value) || (openingActivity && sourceIsTowerSocial)) {
        // Nothing usable was captured, so the reconstructed descriptor goes out instead.
        selection.reason = destination::kMinimumReason;
        selection.previousActivityIndex = destination::kAbsentActivityIndex;
        selection.descriptorBits = {};
        selection.descriptorBitLength = 0;
        selection.descriptorNameBit = 0;
        selection.hasDescriptorName = false;
    }
    return true;
}

} // namespace dawn::state::activity::forced
