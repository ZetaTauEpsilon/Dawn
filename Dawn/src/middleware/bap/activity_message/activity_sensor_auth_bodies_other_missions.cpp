#include "../../../state/activity/vanilla/one_au/authority.h"
#include "../../../state/activity/vanilla/homecoming/authority.h"
#include "../../../state/activity/omega/omega_ending_authority.h"
#include <array>
#include "native/omega_activity_script.h"
#include "../../../state/activity/gateway/authority.h"
#include "../../../state/activity/beyond_infinity/authority.h"
#include "../../../state/activity/deep_storage/authority.h"
#include "../../../state/activity/hijacked/authority.h"
#include "../../../state/activity/Newlight/launchpad/authority.h"
#include "../../../state/activity/beyond_infinity/forest_selection.h"
#include "../../../state/activity/deadly_trial/authority.h"
#include "../../../state/activity/strike_pact/authority.h"
#include "../../../state/activity/strike_bond/authority.h"

#include "sensor_auth_update.h"
#include "../../../state/activity/coo/native_clock_authority.h"
#include "../../../state/activity/omega/omega_progression.h"
#include "../../../state/activity/omega/omega_forest_encounters.h"
#include "../../../state/activity/omega/omega_portal_entry.h"
#include "../../../state/activity/omega/omega_boss_authority.h"
#include "../../../state/activity/omega/omega_ikora_authority.h"
#include "../../../state/activity/omega/omega_lair_authority.h"
#include "../../../state/activity/omega/omega_mission_authority.h"
#include "../../../state/activity/omega/omega_mission_devices.h"
#include "../../../state/activity/omega/omega_rescue_authority.h"
#include "../../../state/activity/omega/omega_transit_authority.h"

namespace dawn::middleware::bap::activity_message::sensor_auth_update {
namespace {

namespace bits = encoding::bits;
namespace encounters = state::activity::omega::forest_encounters;
namespace boss = state::activity::omega::boss_authority;
namespace ikora = state::activity::omega::ikora;
namespace lair = state::activity::omega::lair_authority;
namespace mission_auth = state::activity::omega::mission_authority;
namespace mission = state::activity::omega::mission;
namespace mission_devices = state::activity::omega::mission_devices;
namespace rescue = state::activity::omega::rescue;
namespace transit = state::activity::omega::transit;
namespace round = native::round_authority;
namespace status_effect = native::status_effect;
namespace forest_switches = native::forest_switches;
namespace player_trigger = state::activity::coo::native_player_trigger;

/** Slot types whose auth body this module fills. Every other block is seed-only. */
constexpr std::uint8_t kSlotTypeParticipation = 13;
constexpr std::uint8_t kSlotTypeLifetime = 17;
constexpr std::uint8_t kSlotTypeActivityScript = 18;
constexpr std::uint8_t kSlotTypeNavigation = 19;
constexpr std::uint8_t kSlotTypeMissionDirector = 35;
constexpr std::uint8_t kSlotTypeConfiguration = 8;
constexpr std::uint8_t kSlotTypePackage = 16;
constexpr std::uint8_t kSlotTypeQueues = 41;
constexpr std::uint8_t kSlotTypeSpawnKeys = 67;
/** scene_ikora_opens_portal's authored Scene controller. */
constexpr std::uint32_t kOmegaOpeningRegistry = 0xD00142CFU;
/** Authored portal transport placed in the Lighthouse mission group. */
constexpr std::uint32_t kOmegaTeleportRegistry = 0xBA5F26EFU;
/** Schema 0x8080626B: selector, one 55-bit active reference, empty word array, mode. */
// Both bounded arrays use five-bit counts. The first active trial using 4 + 6 decoded count 1
// as 2 and the registry as `registry << 1`, proving the one-bit boundary displacement.
constexpr std::size_t kSceneEntryCountBits = 5U;
constexpr std::size_t kSceneEntryReferenceBits = 32U + 7U + 16U;
constexpr std::size_t kSceneWordCountBits = 5U;
constexpr std::size_t kAuthoredActiveSceneBits =
    32U + kSceneEntryCountBits + kSceneEntryReferenceBits + kSceneWordCountBits + 32U;
static_assert(kAuthoredActiveSceneBits == 129U);
constexpr std::uint8_t kOmegaPortalVisualSlotType = 4;
constexpr std::uint16_t kOmegaTeleportSlotIndex = 0;
constexpr std::uint16_t kOmegaPortalVisualFirstIndex = 2;
constexpr std::uint16_t kOmegaPortalVisualLastIndex = 4;
constexpr std::uint16_t kOmegaGateControllerSlotIndex = 16;
/** The nine _o_gateway_extended_push[0..8] barrier-wall panels (D00142CF/4/7..15). */
constexpr std::uint16_t kOmegaGatewayPushFirstIndex = 7;
constexpr std::uint16_t kOmegaGatewayPushLastIndex = 15;
constexpr std::uint8_t kOmegaEngagementSlotType = 70;
constexpr std::uint16_t kOmegaEngagementSlotIndex = 17;
constexpr std::uint8_t kOmegaMonitorSlotType = 30;
constexpr std::uint16_t kOmegaOpeningMonitorSlotIndex = 20;
constexpr std::uint16_t kOmegaEntranceMonitorSlotIndex = 24;
/** Both type-4 definitions use auth schema 0x8080992F. */
constexpr std::size_t kOmegaPortalComponentBits = 253;
/** Exact native contact carrier has zero dynamic overrides in 80809AEA. */
constexpr std::size_t kOmegaPortalEntryBits = 252;
/** Schema 0x80804F48: three {float32, biased-int16, bool} link-state records. */
constexpr std::size_t kOmegaPortalGateBits = 147;
/** Ghost pre-roll dialogue (type 53). Schema 0x80804F77 tag-reflection body: a 55-bit root
 * reference then 128 records; record index N selects bank row N, so record 0 = Ghost row 0
 * (AD60F465). Body = 55 + 128*154 = 19,767 bits. */
constexpr std::uint32_t kOmegaDialogueRegistry = 0x82FB58B7U;
constexpr std::uint8_t kOmegaDialogueSlotType = 53;
constexpr std::uint16_t kOmegaDialogueSlotIndex = 2;
constexpr std::size_t kOmegaDialogueRecords = 128;
constexpr std::size_t kOmegaDialogueRecordBits = 65 + 55 + 32 + 2;
/** Record 0 carries its optional u64 time: the scan's validity check (+0xA70F50 -> +0xDD5B70)
 * is literally `*(u64*)(record + 8) != 0`, so an omitted time is rejected every tick. */
constexpr std::size_t kOmegaDialogueRecord0Bits = 129 + 55 + 32 + 2;
constexpr std::size_t kOmegaDialogueBits =
    55 + kOmegaDialogueRecord0Bits
    + (kOmegaDialogueRecords - 1) * kOmegaDialogueRecordBits;
/** Opening objective tracker (type 68). Schema 0x80804F67: two 55-bit references, three
 * rotating 1,563-bit directive records (stride 0xF8), then a 3-bit ring selector. Record 0
 * active: event C252E306 ("Hunt down and destroy Panoptes"), discriminator 0 (native FNV ->
 * HUD identity 32678D66), lifecycle 0 (active/installable); records 1-2 lifecycle -1 (absent);
 * ring entry 0. Total exactly 4,802 bits. */
constexpr std::uint8_t kOmegaDirectiveSlotType = 68;
constexpr std::uint16_t kOmegaDirectiveSlotIndex = 0;
constexpr std::uint32_t kOmegaOpeningObjectiveEvent = 0xC252E306U;
/** Forest-phase objective: "Destroy Panoptes, the Infinite Mind / Avert the future where the
 *  Vex win" -- directive bank event 2, published once the player's region is a forest bubble.
 *  Replacing the active identity removes the old lifecycle-0 entry before installing this one. */
constexpr std::uint32_t kOmegaForestObjectiveEvent = 0x1EBF4621U;
/** Dialogue record/bank row of the gate-tunnel Ghost line (bank selector AE2495AC, 8.09 s). */
constexpr std::size_t kOmegaTunnelDialogueRecord = 6U;
/** Infinite Forest map generator component 2763EC97/37/1: authority schema 0x80805007,
 *  fixed 11,350-bit body (calibrated against the live-proven directive schema: 1-bit bools,
 *  inline arrays always fully serialized).
 *  PARKED: a body the client cannot consume marks the slot's sync record unseeded forever,
 *  and ClientRosterSync_AllRecordsInBubbleSeeded then vetoes bubble 11's seed commit â€” which
 *  is the very sweep that instantiates the network-replicated map-generator WORKER
 *  (kind 0x80805017). The worker self-generates on authored defaults (seed 9001) with no
 *  sensor authority at all, so an empty-body record (payload-present=0, seeds trivially,
 *  mask B clears, worker constructs) is strictly better until the decode is proven. */
constexpr bool kOmegaForestGeneratorBodyReady = false;
constexpr std::uint32_t kOmegaForestGeneratorRegistry = 0x2763EC97U;
constexpr std::uint8_t kOmegaForestGeneratorSlotType = 37U;
constexpr std::uint16_t kOmegaForestGeneratorSlotIndex = 1U;
constexpr std::size_t kOmegaForestGeneratorBits = 11350U;
constexpr std::size_t kOmegaDirectiveTargetBits = 55 + 55 + 128 + 1;
constexpr std::size_t kOmegaDirectiveProgressBits = 1 + 5 * 64 + 32;
constexpr std::size_t kOmegaDirectiveRecordBits =
    32 + 32 + 2 + kOmegaDirectiveProgressBits + 128 + 2 + 55 + 3
    + 4 * kOmegaDirectiveTargetBits;
constexpr std::size_t kOmegaDirectiveBits = 55 + 55 + 3 * kOmegaDirectiveRecordBits + 3;
/** Body widths, each checked against the writer after the body is written. */
constexpr std::size_t kParticipationBits = 192;
constexpr std::size_t kParticipationRegionBits = 32;
constexpr std::size_t kLifetimeBits = 520;
constexpr std::size_t kOmegaLifetimeBits = kLifetimeBits + encounters::kSwitchBits;
static_assert(kOmegaLifetimeBits == 617);
/** Select the mission's typed population inputs before its generator creates encounters. */
[[nodiscard]] std::size_t forest_switch_count(const Snapshot& snapshot) noexcept {
    if (snapshot.nativeRound.controlled) {
        return forest_switches::valid(snapshot.nativeForestSwitches)
            ? snapshot.nativeForestSwitches.count : 0;
    }
    if(snapshot.strike_bond.enabled) { return std::size(state::activity::strike_bond::kForestHashSwitches); }
    return snapshot.strike_pact.services
        ? state::activity::strike_pact::kForestHashSwitches.size()
        : (snapshot.omegaForestVexEncounters ? 1U : 0U);
}
/** Schema 0x808099C4: one bool, five unsigned 64-bit fields, then one 32-bit scalar. */
constexpr std::size_t kSharedMissionStateBits = 1 + 5 * 64 + 32;
/** Schema 0x808099BF: two bools, two bias-1 two-bit enums, then shared state. */
constexpr std::size_t kMissionDirectorBits = 2 + 2 * 2 + kSharedMissionStateBits;
/** Schema 0x80809919: shared state, one bool, then one biased signed 32-bit scalar. */
constexpr std::size_t kActivityScriptBits = kSharedMissionStateBits + 1 + 32;
/**
 * The mission director was recovered independently before enabling the activity script. The
 * runtime descriptor proves their shared tail ends in 32 bits, not the provisional 64; that prior
 * extra word shifted the proven lifetime and participation objects out of alignment.
 */
constexpr bool kInitializeActivityScript = true;
constexpr bool kInitializeMissionDirector = true;
constexpr std::size_t kConfigurationBits = 35;
constexpr std::size_t kPackageBits = 7;
constexpr std::size_t kQueueBits = 12;
constexpr std::size_t kSpawnKeyBits = 32 * 32 + 1 + 32;

/** Signed fields in these bodies carry a -2^31 bias, so this wire value stores zero. */
constexpr std::uint32_t kSignedZero = 0x80000000;
/** The same bias wraps at the top of the field, so this wire value stores -1. */
constexpr std::uint32_t kSignedMinusOne = 0x7FFFFFFF;
/** The region index rides the same bias, so its wire value is the bias plus the index. */
constexpr std::uint32_t kRegionBias = 0x80000000;
/** Message 52's team-state byte 1, where bit 1 is `awaiting_client_sync`. */
constexpr std::uint32_t kAwaitingClientSync = 2;
/** Neutral runtime-i32 override that forces the type-17 waiting selector to zero. */
constexpr std::uint32_t kWaitingSwitchKey = 0xB3C1251B;
constexpr std::uint32_t kWaitingSwitchClass = 0x80800007;
/** Type 17 carries 3 spawn overrides. Wire zero stores index -1 and disables one. */
constexpr std::size_t kSpawnOverrideCount = 3;
constexpr std::uint8_t kSpawnOverrideIndexWidth = 10;
constexpr std::uint32_t kSpawnOverrideIndexBias = 1;
/** Type 67 maps the 32 spawn-key ordinals to themselves, matching its constructor. */
constexpr std::size_t kSpawnKeyCount = 32;

/**
 * Writes the participation body, which binds the player and latches the region. Zero-fill is not
 * safe here. Every biased field must carry its bias, or a stored zero decodes to the smallest
 * signed value.
 * @param writer Body writer.
 * @param snapshot Message input.
 * @return True when the body fits.
 */
[[nodiscard]] bool write_participation(bits::Writer& writer, const Snapshot& snapshot) noexcept {
    // An optional field's value follows its presence bit, so sending +0 shifts everything below.
    bool encoded = writer.write(snapshot.hasRegion ? 1U : 0U, kPresenceWidth);
    if (encoded && snapshot.hasRegion) {
        encoded = writer.write(kRegionBias + snapshot.region, kParticipationRegionBits);
    }
    // The participation record is this body's head, so struct +8 and +10 are record +8 and +10.
    // Record +8 is step 36 task 9's own term and +10 is the spawn gate's.
    encoded = encoded && writer.write(0, kPresenceWidth) && writer.write(1, kPresenceWidth)
           && writer.write(1, kPresenceWidth) && writer.write(1, kPresenceWidth)
           && writer.write(0, kPresenceWidth) && writer.write(1, 3) && writer.write(1, 2)
           && writer.write(0, 3) && writer.write(0, 32) && writer.write(1, 5)
           && writer.write(0, kPresenceWidth) && writer.write(0, 3)
           && writer.write(1, kPresenceWidth) && writer.write(snapshot.playerKey, 64)
           && writer.write(0, 5) && writer.write(3, 6);
    // 80804F30+48 ->808094DD+0C ->808094E1: authored predicate names.
    const auto names=native::player_predicates::compose(snapshot.playerPredicates,snapshot.omegaPortalPlayerHash,
        state::activity::omega::portal_entry::kRequiredPlayerHash);
    encoded=encoded && names && native::player_predicates::write(writer,*names);
    encoded = encoded && writer.write(0, 6)
           && writer.write(1, kPresenceWidth)
           && writer.write(snapshot.awaitClientSync ? kAwaitingClientSync : 0U, 4)
           && writer.write(0, 1) && writer.write(snapshot.one_au.enabled || snapshot.homecoming.enabled || snapshot.nativeRespawnRestricted ? 1U : 0U, 1);
    // 1AU keeps its three-second individual respawn delay even when the darkness
    // presentation is active. Other restricted activities retain thirty seconds.
    if(snapshot.one_au.enabled || snapshot.homecoming.enabled || snapshot.nativeRespawnRestricted)
        encoded=encoded && writer.write(snapshot.one_au.enabled || snapshot.homecoming.enabled ? 0x4200U : 0x4F80U,16);
    return encoded && writer.write(0,1) && writer.write(0,kPresenceWidth) && writer.write(128,8)
           && writer.write(kSignedZero,32);
}

/**
 * Writes the lifetime body, which is the activity state the roster reports.
 * @param writer Body writer.
 * @param snapshot Message input.
 * @return True when the body fits.
 */
[[nodiscard]] bool write_lifetime(bits::Writer& writer, const Snapshot& snapshot,
                                  std::uint32_t restrictionOrdinal,
                                  std::optional<std::uint32_t> scenarioOrdinal) noexcept {
    if (snapshot.nativeRound.controlled
        && !forest_switches::valid(snapshot.nativeForestSwitches)) return false;
    if (scenarioOrdinal && *scenarioOrdinal > kMaximumGrantBubble) { return false; }
    const auto ordinal=restrictionOrdinal?restrictionOrdinal:scenarioOrdinal.value_or(0U);
    // Shared terminal publication: native mission-complete phase 6 / success 1.
    const bool completed=!snapshot.nightfallFailed && snapshot.missionCompletion.valid();
    const auto lifetime=snapshot.nightfallFailed?8U:completed?(snapshot.one_au.completion.valid() || snapshot.homecoming.completion.valid()?8U:std::uint32_t{snapshot.missionCompletion.state}):std::uint32_t{snapshot.lifetime};
    bool encoded = writer.write(lifetime + 1, 4) && writer.write(completed?2U:1U, 3)
                   && writer.write(0, kPresenceWidth) && writer.write(kSignedZero, 32)
                   && writer.write(0, 32) && writer.write(kSignedZero+ordinal, 32)
                   && writer.write(snapshot.nativeRound.controlled
                       ? 1U + forest_switch_count(snapshot)
                       : state::activity::beyond_infinity::forest::selected(snapshot.beyond_infinity)
                           ? 3U : 1U + forest_switch_count(snapshot), 6)
                   && writer.write(kWaitingSwitchKey, 32) && writer.write(1, kPresenceWidth)
                   && writer.write(kWaitingSwitchClass, 32) && writer.write(kSignedZero, 32);
    if (snapshot.nativeRound.controlled) {
        encoded = encoded && forest_switches::write(writer, snapshot.nativeForestSwitches);
    } else if (snapshot.strike_bond.enabled) {
        for (const auto& row : state::activity::strike_bond::kForestHashSwitches) {
            encoded = encoded && writer.write(row.key,32) && writer.write(1,1)
                && writer.write(encounters::kHashClass,32) && writer.write(row.value,32);
        }
    } else if (snapshot.strike_pact.services) {
        for (const auto& row : state::activity::strike_pact::kForestHashSwitches) {
            encoded = encoded && writer.write(row.key,32) && writer.write(1,1)
                && writer.write(encounters::kHashClass,32) && writer.write(row.value,32);
        }
    } else if (state::activity::beyond_infinity::forest::selected(snapshot.beyond_infinity)) {
        encoded = encoded && state::activity::beyond_infinity::forest::write(writer,snapshot.beyond_infinity.forestPass);
    } else if (snapshot.omegaForestVexEncounters) {
        // This value is a raw typed hash, not a biased integer or boolean.
        encoded = encoded && writer.write(encounters::kVexKey, 32)
                  && writer.write(1, kPresenceWidth) && writer.write(encounters::kHashClass, 32)
                  && writer.write(encounters::kActiveValue, 32);
    }
    encoded = encoded && writer.write(kSignedZero, 32);
    for (std::size_t index = 0; encoded && index < kSpawnOverrideCount; ++index) {
        const std::uint32_t slice =
            snapshot.hasSpawnOverride ? snapshot.spawnSliceSet + kSpawnOverrideIndexBias : 0U;
        const std::uint32_t hash =
            snapshot.hasSpawnOverride ? snapshot.spawnSetHash : kAbsentSpawnSetHash;
        encoded = writer.write(slice, kSpawnOverrideIndexWidth) && writer.write(hash, 32);
    }
    // Struct `+1256` is the out-of-bounds `activity_quarantine` selector. The reader arms the
    // quarantine at or below 0x3F unsigned, so minus one leaves it clear and teleports nobody.
    return encoded && writer.write(0, kPresenceWidth) && writer.write(0, 32)
           && writer.write(kSignedMinusOne, 32) && writer.write(0, 32)
           && writer.write(kSlotTypeBias, kSlotTypeWidth)
           && writer.write(kSlotIndexBias, kSlotIndexWidth) && writer.write(0, 32)
           && writer.write(0, 3);
}

/**
 * Writes the spawn-key body, which maps the 32 ordinals to themselves.
 * @param writer Body writer.
 * @return True when the body fits.
 */
[[nodiscard]] bool write_spawn_keys(bits::Writer& writer) noexcept {
    bool encoded = true;
    for (std::size_t index = 0; encoded && index < kSpawnKeyCount; ++index) {
        encoded = writer.write(kSignedZero + index, 32);
    }
    return encoded && writer.write(0, kPresenceWidth) && writer.write(kSignedMinusOne, 32);
}

/** Writes schema 0x808099C4's constructed state shared by slots 18 and 35. */
[[nodiscard]] bool
write_shared_mission_state(bits::Writer& writer, bool active) noexcept {
    return writer.write(active ? 1U : 0U, kPresenceWidth)
           && legacy_pad_bits(writer, 5 * 64 + 32);
}

/** Writes the neutral mission-director state that makes its authority datum persistent. */
[[nodiscard]] bool write_mission_director(bits::Writer& writer,
                                          const Snapshot& snapshot,bool missionOwned,
                                          bool nativeRoundOwned=false) noexcept {
    if(missionOwned) {
        // Constructed 808099BF state: no countdown, with an explicit on/off
        // level. The lifetime filter uses each mission's scenario bubble ordinal.
        const bool restricted = nativeRoundOwned
            ? snapshot.nativeRound.restricted
            : (snapshot.one_au.enabled ? snapshot.one_au.restricted
                : snapshot.homecoming.enabled ? snapshot.homecoming.restricted
                : snapshot.strike_bond.enabled ? snapshot.strike_bond.restricted
                : snapshot.hijacked.enabled ? snapshot.hijacked.restricted
                : snapshot.deep_storage.enabled ? snapshot.deep_storage.restricted
                : snapshot.omegaMission.restriction);
        return writer.write(restricted ? 1U : 0U,1) && writer.write(0,1)
            && writer.write(1,2) && writer.write(0,2) && writer.write(0,1)
            && writer.write(0,64) && writer.write(0x134F00C00000ULL,64)
            && writer.write(0,64) && writer.write(0,64) && writer.write(UINT64_MAX,64)
            && writer.write(0x3F800000U,32);
    }
    // The two enum fields carry bias 1. Wire zero would decode below their declared range.
    return writer.write(snapshot.missionDirectorVariant & 1U, kPresenceWidth)
           && writer.write((snapshot.missionDirectorVariant >> 1U) & 1U, kPresenceWidth)
           && writer.write(1, 2) && writer.write(1, 2)
           && write_shared_mission_state(writer, snapshot.missionDirectorActive);
}

/** Writes the neutral activity-script state that creates its authority datum. */
[[nodiscard]] bool write_activity_script(bits::Writer& writer,
                                         const Snapshot& snapshot) noexcept {
    // The shared-state head is set in the client storage object. That materializes the datum but
    // does not supply the activity-host executor that advances the authored mission graph.
    const bool timeState = snapshot.archiveOmega
        ? native::omega_activity_script::time_state(writer)
        : writer.write(1, kPresenceWidth) && legacy_pad_bits(writer, 5 * 64 + 32);
    return timeState
           && writer.write(!snapshot.archiveOmega && snapshot.activityScriptFlag ? 1U : 0U, kPresenceWidth)
           && writer.write(kSignedZero
                               + static_cast<std::uint32_t>(snapshot.activityScriptState),
                           32);
}

[[nodiscard]] bool active_omega_portal_component(const Snapshot& snapshot,
                                                 std::uint32_t key,
                                                 std::uint8_t slotType,
                                                 std::uint16_t slotIndex) noexcept {
    if ((snapshot.omegaSceneAuthority && snapshot.seedAuthoredSensors)
        || !snapshot.omegaPortalMutation
        || (snapshot.omegaOpeningStage != kOmegaOpeningStageTriggered
         && snapshot.omegaOpeningStage != kOmegaOpeningStagePortal)
        || slotType != kOmegaPortalVisualSlotType) {
        return false;
    }
    const bool teleport = key == kOmegaTeleportRegistry
                          && slotIndex == kOmegaTeleportSlotIndex;
    const bool visual = key == kOmegaOpeningRegistry
                        && slotIndex >= kOmegaPortalVisualFirstIndex
                        && slotIndex <= kOmegaPortalVisualLastIndex;
    return teleport || visual;
}

/**
 * The barrier WALL itself: nine authored gateway-push presentation components whose creation
 * the frozen run never performed (the always-open symptom). Activating them with the same
 * neutral-transform type-4 body used by the portal visuals makes the Vex block wall exist;
 * the dissolve later deactivates them at the beam moment.
 */
[[nodiscard]] bool active_omega_gateway_push(const Snapshot& snapshot,
                                             std::uint32_t key,
                                             std::uint8_t slotType,
                                             std::uint16_t slotIndex) noexcept {
    return snapshot.omegaGateAuthority && kOmegaGateAuthorityBodyReady
           && snapshot.omegaDialogueArm && key == kOmegaOpeningRegistry
           && slotType == kOmegaPortalVisualSlotType
           && slotIndex >= kOmegaGatewayPushFirstIndex
           && slotIndex <= kOmegaGatewayPushLastIndex;
}

[[nodiscard]] bool active_omega_portal_gate(const Snapshot& snapshot,
                                            std::uint32_t key,
                                            std::uint8_t slotType,
                                            std::uint16_t slotIndex) noexcept {
    // Armed on the in-world latch (like the dialogue/directive) rather than the synthetic
    // Triggered/Portal stages, which normal play parks before. The gate_authority experiment
    // remains the operator's switch.
    if (!snapshot.omegaGateAuthority || !kOmegaGateAuthorityBodyReady
        || !snapshot.omegaDialogueArm) {
        return false;
    }
    const bool interior = key == kOmegaTeleportRegistry && slotType == 23 && slotIndex == 1;
    const bool missionController = key == kOmegaOpeningRegistry && slotType == 23
                                   && slotIndex == kOmegaGateControllerSlotIndex;
    return interior || missionController;
}

/**
 * Activates one authored type-4 portal component without inventing an actor reference. This body
 * is shared by the Lighthouse teleporter and the three portal-beam components.
 * The canonical absent reference deliberately selects the component's authored local transform.
 */
[[nodiscard]] bool write_active_omega_portal_component(bits::Writer& writer) noexcept {
    return native::placement::write_active(writer);
}

/**
 * Schema 80804F48 CLOSED state: three ordered {float32 desired, biased-int16 revision,
 * bool snap} channel records committing the authored closed tuple {0, 1, 0} with positive
 * revisions. Retail SPAWNS the thin Vex barrier by establishing this closed state at mission
 * start; Ikora's beam moment later flips it open. Without any committed state the barrier
 * never exists, which is the current always-open symptom.
 */
[[nodiscard]] bool write_active_omega_portal_gate(bits::Writer& writer) noexcept {
    // Channel 0 (device position): closed 0.0, revision 1, snap.
    if (!writer.write(0U, 32U) || !writer.write(0x8001U, 16U) || !writer.write(1U, 1U)) {
        return false;
    }
    // Channel 1 (barrier presence): 1.0, revision 1, snap.
    if (!writer.write(0x3F800000U, 32U) || !writer.write(0x8001U, 16U)
        || !writer.write(1U, 1U)) {
        return false;
    }
    // Channel 2: 0.0, revision 1, snap.
    return writer.write(0U, 32U) && writer.write(0x8001U, 16U) && writer.write(1U, 1U);
}

[[nodiscard]] bool omega_dialogue(std::uint32_t key, std::uint8_t slotType,
                                  std::uint16_t slotIndex) noexcept {
    return key == kOmegaDialogueRegistry && slotType == kOmegaDialogueSlotType
           && slotIndex == kOmegaDialogueSlotIndex;
}

/** 8080992F: signed generation/index/scalar use their reflected +2^31 bias.
 * Native +9F1B45 tests byte +8 to create the authored candidate; +9F2200 selects
 * index zero. No actor reference or transform override is supplied. */
[[nodiscard]] bool write_portal_entry(bits::Writer& writer) noexcept {
    return writer.write(kSignedZero + 1U, 32) && writer.write(kSignedZero, 32)
        && writer.write(1, 1) && writer.write(0, 1) && writer.write(kSignedZero, 32)
        && writer.write(kAbsentSpawnSetHash, 32) && writer.write(0, kSlotTypeWidth)
        && writer.write(kSlotIndexBias - 1U, kSlotIndexWidth)
        && writer.write(0, 32) && writer.write(0, 32) && writer.write(0, 32)
        && writer.write(0, 1) && writer.write(0, 2);
}

[[nodiscard]] bool authored_dialogue(const Snapshot& snapshot,
                                     std::uint32_t key,
                                     std::uint8_t slotType,
                                     std::uint16_t slotIndex) noexcept {
    return snapshot.publishAuthoredCueTransition && key == snapshot.authoredCueRegistry
           && slotType == kOmegaDialogueSlotType && slotIndex == kOmegaDialogueSlotIndex;
}

[[nodiscard]] bool authored_scene_selector(const Snapshot& snapshot,
                                            std::uint32_t key,
                                            std::uint8_t slotType,
                                            std::uint16_t slotIndex) noexcept {
    return snapshot.publishAuthoredSceneSelector
           && key == snapshot.authoredSceneRegistry
           && slotType == snapshot.authoredSceneType
           && slotIndex == snapshot.authoredSceneIndex;
}

/**
 * Publishes one package-authored Scene entry. The selector chooses the native Scene definition;
 * its recovered type-2 anchor lets that definition resolve its own wall animation and cast.
 */
[[nodiscard]] bool write_authored_scene_authority(bits::Writer& writer,
                                                  const Snapshot& snapshot) noexcept {
    // The selector is a signed i32 on the wire. Like the other reflected signed fields, it uses
    // a +2^31 bias: writing the definition value directly caused 0x80B82771 to decode as
    // 0x00B82771. Adding the bias modulo 2^32 emits 0x00B82771 and restores the authored value.
    const std::uint32_t wireSelector = snapshot.authoredSceneSelector + kSignedZero;
    return writer.write(wireSelector, 32)
           && writer.write(1U, kSceneEntryCountBits)
           && writer.write(snapshot.authoredSceneEntryRegistry, 32U)
           // Reflected reference fields store type with +1 and index with +0x8000 biases.
           && writer.write(static_cast<std::uint32_t>(snapshot.authoredSceneEntryType) + 1U, 7U)
           && writer.write(static_cast<std::uint32_t>(snapshot.authoredSceneEntryIndex) + 0x8000U,
                           16U)
           && writer.write(0U, kSceneWordCountBits)
           && writer.write(kSignedZero, 32U);
}

/**
 * One 0x80809C42 reference in its authored-absent form {0x811C9DC5, type -1, slot -1}. The
 * scan's reference gate (+0x4E2990) treats type/slot of -1 as "no target" and dispatches
 * directly; an all-zero reference reads as a SET target, fails to resolve, and skips the
 * dispatch. Wire: u32 bias 0; i8 bias 1 (stored 0 = -1); i16 bias 0x8000 (stored 0x7FFF = -1).
 */
[[nodiscard]] bool write_dialogue_ref_absent(bits::Writer& writer) noexcept {
    return writer.write(kAbsentSpawnSetHash, 32) && writer.write(0, 7)
           && writer.write(0x7FFF, 16);
}

/**
 * Ghost pre-roll dialogue body. Emits the tag-reflection 0x80804F77 struct: a zero root reference
 * then 128 records. Record 0 is armed (generation 1 vs the memset-0 processed array, mode 2) so the
 * client scan (+0x100A180) dispatches bank row 0 = AD60F465; the rest carry generation 0 and are
 * skipped. Non-selecting fields are written as their bias so they decode to zero; the per-record
 * optional u64 time is omitted. */
/** One 0x8080500B record (5,275 bits), all fields decoding to zero, tile count zero.
 * Wire conventions calibrated against the LIVE-PROVEN directive schema (walk 4,787 + 15
 * type-2 bools = the observed 4,802): bools are 1 wire bit even though the reflection
 * gives them width 0, and inline arrays are FIXED-length with no count prefix â€” so the
 * 100-element 0x8080500C tile array is always fully serialized (4,800 bits); the 7-bit
 * count field only says how many entries are meaningful. */
[[nodiscard]] bool write_generator_500b(bits::Writer& writer) noexcept {
    // +0x00 u32=0; +0x04 u8 bias0x80 ->0; then 0x8080500F (196 bits).
    if (!writer.write(0, 32) || !writer.write(0x80, 8)) {
        return false;
    }
    // 0x8080500F: four groups of {u8 bias0x80, u8 bias0x80, f32, bool} = 196 bits.
    for (std::size_t group = 0; group < 4; ++group) {
        if (!writer.write(0x80, 8) || !writer.write(0x80, 8) || !writer.write(0, 32)
            || !writer.write(0, 1)) {
            return false;
        }
    }
    // +0x2C u7 ->0; +0x2D bool; +0x30/+0x34 two f32; +0x38..+0x48 five i32 bias INT32_MIN.
    if (!writer.write(0, 7) || !writer.write(0, 1) || !writer.write(0, 32)
        || !writer.write(0, 32)) {
        return false;
    }
    for (std::size_t value = 0; value < 5; ++value) {
        if (!writer.write(0x80000000ULL, 32)) {
            return false;
        }
    }
    // +0x4C 0x8080500D: 7-bit tile count = 0, then ALL 100 fixed tiles (3 x i16 bias
    // 0x8000 each, written bias-neutral so they decode to zero; unread while count = 0).
    if (!writer.write(0, 7)) {
        return false;
    }
    for (std::size_t tile = 0; tile < 100; ++tile) {
        if (!writer.write(0x8000, 16) || !writer.write(0x8000, 16)
            || !writer.write(0x8000, 16)) {
            return false;
        }
    }
    return true;
}

/** Neutral map-generator authority body (0x80805007 -> 0x80805008). */
[[nodiscard]] bool write_active_omega_forest_generator(bits::Writer& writer) noexcept {
    // 0x80805008: two 0x8080500B records, then one 0x80805009.
    if (!write_generator_500b(writer) || !write_generator_500b(writer)) {
        return false;
    }
    // 0x80805009: u32=0, a 32-byte array (256 bits) and a 64-byte array (512 bits), all zero.
    if (!writer.write(0, 32)) {
        return false;
    }
    for (std::size_t byte = 0; byte < 32 + 64; ++byte) {
        if (!writer.write(0, 8)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool omega_forest_generator(const Snapshot& snapshot, std::uint32_t key,
                                          std::uint8_t slotType, std::uint16_t slotIndex) noexcept {
    return kOmegaForestGeneratorBodyReady && snapshot.omegaForestGenerator
           && key == kOmegaForestGeneratorRegistry
           && slotType == kOmegaForestGeneratorSlotType
           && slotIndex == kOmegaForestGeneratorSlotIndex;
}

[[nodiscard]] bool write_active_omega_dialogue(bits::Writer& writer,
                                               const Snapshot& snapshot) noexcept {
    if (!write_dialogue_ref_absent(writer)) {
        return false;
    }
    for (std::size_t record = 0U; record < kOmegaDialogueRecords; ++record) {
        // Record 6 arms once the player is inside the gate tunnel; its generation starts at 0,
        // so the client's processed-generation mirror dispatches it exactly once when armed.
        const bool active = snapshot.publishAuthoredCueTransition
                                ? record == snapshot.authoredDialogueRecord
                                : (record == 0U
                                   || (record == kOmegaTunnelDialogueRecord
                                       && snapshot.omegaTunnelDialogue)
                                   || (record == 7U && snapshot.omegaVistaDialogue)
                                   || (record == 9U && snapshot.omegaExitDialogue)
                                   || ((record >= 12U && record < 34U)
                                       && snapshot.omegaLairDialoguePendingRow == record
                                       && (snapshot.omegaLairDialogueRequestedMask & (UINT64_C(1) << record)) != 0));
        const bool requested = active || (!snapshot.publishAuthoredCueTransition
            && (record >= 12U && record < 34U)
            && (snapshot.omegaLairDialogueRequestedMask & (UINT64_C(1) << record)) != 0);
        // field0 0x80809B3F: the mandatory u64 defaults to -1 (observed in the default-filled
        // component), then the optional u64 time. The record validity check is `time != 0`, so
        // the armed record carries a nonzero time; inert records omit it (their generation is 0).
        if (!writer.write(0xFFFFFFFFFFFFFFFFULL, 64)
            || !writer.write(active ? 1U : 0U, kPresenceWidth)
            || (active && !writer.write(1, 64))) {
            return false;
        }
        if (!write_dialogue_ref_absent(writer)) {  // field1 0x80809C42, decodes to zero
            return false;
        }
        // generation (i32 @+0x18, bias INT32_MIN): 1 arms record 0, 0 leaves the rest inert.
        if (!writer.write((requested ? 1ULL : 0ULL) + 0x80000000ULL, 32)) {
            return false;
        }
        // mode (i8 @+0x1C, 2 bits, bias 1): 2 is the play mode for record 0.
        if (!writer.write((active ? 2ULL : 0ULL) + 1ULL, 2)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool omega_directive(std::uint32_t key, std::uint8_t slotType,
                                   std::uint16_t slotIndex) noexcept {
    return key == kOmegaDialogueRegistry && slotType == kOmegaDirectiveSlotType
           && slotIndex == kOmegaDirectiveSlotIndex;
}

[[nodiscard]] bool authored_directive(const Snapshot& snapshot,
                                      std::uint32_t key,
                                      std::uint8_t slotType,
                                      std::uint16_t slotIndex) noexcept {
    return snapshot.publishAuthoredCueTransition && key == snapshot.authoredCueRegistry
           && slotType == kOmegaDirectiveSlotType && slotIndex == kOmegaDirectiveSlotIndex;
}

/** One 1,563-bit directive record; biased numerics are written to decode as zero. */
[[nodiscard]] bool write_directive_record(bits::Writer& writer,
                                          bool active,
                                          std::uint32_t activeEvent,
                                          std::uint32_t waypointRegistry,
                                          std::uint16_t waypointIndex,
                                          std::uint32_t destination) noexcept {
    // Event key, then the biased discriminator (decoded 0 -> HUD identity 32678D66).
    if (!writer.write(active ? activeEvent : kAbsentSpawnSetHash, 32)
        || !writer.write(0x80000000ULL, 32)) {
        return false;
    }
    // Lifecycle (2 bits, bias 1): 0 = active/installable, -1 = absent entry.
    if (!writer.write(active ? 1U : 0U, 2)) {
        return false;
    }
    // 0x808099C4 time/progress: the u64 time values use the engine's -1 "no value" form; a
    // zero end-time renders as a 0:00 HUD countdown that retail's opening does not show.
    if (!writer.write(0, kPresenceWidth)) {
        return false;
    }
    for (std::size_t value = 0; value < 5; ++value) {
        if (!writer.write(0xFFFFFFFFFFFFFFFFULL, 64)) {
            return false;
        }
    }
    if (!writer.write(0, 32)) {
        return false;
    }
    // Four biased i32 values, decoded -1 (the engine's no-value form).
    for (std::size_t value = 0; value < 4; ++value) {
        if (!writer.write(0x7FFFFFFFULL, 32)) {
            return false;
        }
    }
    // enum (2 bits, bias 1) decoded 0; absent reference; enum (3 bits, bias 1) decoded 0.
    if (!writer.write(1, 2) || !write_dialogue_ref_absent(writer) || !writer.write(1, 3)) {
        return false;
    }
    for (std::size_t target = 0; target < 4; ++target) {
        // Native type-68 tick (+100A8BF) consumes reference_a as the beacon target.
        // Type and index are biased, exactly as the type-43 structured reference is.
        const bool selected = active && target == 0 && waypointRegistry != 0;
        const bool referenceWritten = selected
            ? writer.write(waypointRegistry, 32) && writer.write(48U, 7)
                && writer.write(static_cast<std::uint32_t>(waypointIndex) + 0x8000U, 16)
            : write_dialogue_ref_absent(writer);
        if (!referenceWritten || !write_dialogue_ref_absent(writer)) {
            return false;
        }
        namespace omega = state::activity::omega;
        const auto point = omega::waypoint(waypointRegistry == omega::kHandoffGroup
            ? omega::Route::forestExit : (waypointRegistry == omega::kCrownGroup
                ? omega::Route::crownEntrance : omega::Route::forestEntrance));
        const bool located = selected && destination != kAbsentSpawnSetHash && destination != 0;
        const std::array<std::uint32_t, 4> locator{
            destination, point.bubbleName, waypointRegistry, point.pointName};
        for (const auto hash : locator) {
            if (!writer.write(located ? hash : kAbsentSpawnSetHash, 32)) {
                return false;
            }
        }
        if (!writer.write(0, kPresenceWidth)) {
            return false;
        }
    }
    return true;
}

/** The opening objective body: record 0 active on C252E306, ring entry 0. */
[[nodiscard]] bool write_active_omega_directive(bits::Writer& writer,
                                                const Snapshot& snapshot) noexcept {
    // The forest banner replaces the opening tracker in place: same body shape, next event key.
    const bool lairObjective = snapshot.omegaLairObjectiveEvent == 0x3517D4D5U
        || snapshot.omegaLairObjectiveEvent == 0x31A51CEBU;
    const std::uint32_t activeEvent = snapshot.publishAuthoredCueTransition
                                          ? snapshot.authoredDirectiveEvent
                                          : lairObjective ? snapshot.omegaLairObjectiveEvent
                                          : (snapshot.omegaForestBanner
                                                 ? kOmegaForestObjectiveEvent
                                                 : kOmegaOpeningObjectiveEvent);
    if (!write_dialogue_ref_absent(writer) || !write_dialogue_ref_absent(writer)) {
        return false;
    }
    for (std::size_t record = 0; record < 3; ++record) {
        if (!write_directive_record(writer, record == 0, activeEvent,
                                   snapshot.publishAuthoredCueTransition || lairObjective
                                       ? 0U : snapshot.omegaWaypointRegistry,
                                   snapshot.omegaWaypointIndex, snapshot.omegaWaypointDestination)) {
            return false;
        }
    }
    // Ring selector (3 bits, bias 1): decoded 0 selects the active entry.
    return writer.write(1, 3);
}

/** One mission component's exact neutral auth-body shape. */
struct MissionBody final {
    std::uint32_t registryKey;
    std::uint16_t slotIndex;
    std::uint8_t slotType;
    std::uint32_t authClass;
    std::uint16_t width;
    const std::uint16_t* ones;
    std::uint16_t oneCount;
};

// Earlier schema tooling started the codec descriptor array 0x18 bytes too early. Every body that
// was generated from that view was structurally short or semantically biased (including Omega's
// type-30 monitors and type-70 engagement state). Keep the registry empty until each body is
// recovered from the corrected reflection walker; registration/sense delivery does not require an
// invented auth body.
constexpr std::array<MissionBody, 0> kMissionBodies{};

[[nodiscard]] const MissionBody* find_mission_body(std::uint32_t key,
                                                   std::uint8_t slotType,
                                                   std::uint16_t slotIndex) noexcept {
    for (const MissionBody& body : kMissionBodies) {
        if (body.registryKey == key && body.slotType == slotType && body.slotIndex == slotIndex) {
            return &body;
        }
    }
    return nullptr;
}

[[nodiscard]] bool active_omega_boundary_body(const Snapshot& snapshot,
                                              std::uint32_t key,
                                              std::uint8_t slotType,
                                              std::uint16_t slotIndex) noexcept {
    return (snapshot.omegaOpeningStage == kOmegaOpeningStageTriggered
            || snapshot.omegaOpeningStage == kOmegaOpeningStagePortal)
           && find_mission_body(key, slotType, slotIndex) != nullptr;
}

[[nodiscard]] bool write_mission_body(bits::Writer& writer, const MissionBody& body) noexcept {
    std::size_t next = 0;
    for (std::uint16_t bit = 0; bit < body.width; ++bit) {
        std::uint32_t value = 0;
        if (next < body.oneCount && body.ones[next] == bit) {
            value = 1;
            ++next;
        }
        if (!writer.write(value, 1)) {
            return false;
        }
    }
    return true;
}

} // namespace

/** Reports how many bits of auth body one slot carries. */
std::size_t
legacy_auth_body_bits(const Snapshot& snapshot,
               std::uint32_t key,
               std::uint8_t slotType,
               std::uint16_t slotIndex,
               bool carriesPlayerKey) noexcept {
    if (round::timer(snapshot.nativeRound,key,slotType,slotIndex)) return round::kBodyBits;
    if (round::lifetime(snapshot.nativeRound,key,slotType,slotIndex)) {
        if (snapshot.nativeRound.controlled
            && !forest_switches::valid(snapshot.nativeForestSwitches)) return 0;
        return kLifetimeBits + forest_switch_count(snapshot) * encounters::kSwitchBits;
    }
    if (round::director(snapshot.nativeRound,key,slotType,slotIndex)) return kMissionDirectorBits;
    if (const auto* request = status_effect::find(snapshot.statusEffects,key,slotType,slotIndex);
        request != nullptr && status_effect::valid(snapshot.statusEffects,snapshot.roster,snapshot.region)) {
        return status_effect::bits(*request);
    }
    if(const auto* request=player_trigger::find(snapshot.playerTriggers,key,slotIndex);
        request && slotType==31 && player_trigger::valid(snapshot.playerTriggers,snapshot.roster,snapshot.region))
        return player_trigger::kAuthBits;
    if(const auto count=state::activity::deadly_trial::body_bits(snapshot.deadly_trial,key,slotType,slotIndex)) { return count; }
    if(const auto count=state::activity::gateway::body_bits(snapshot.gateway,key,slotType,slotIndex)) { return count; }
    if(const auto count=state::activity::beyond_infinity::body_bits(snapshot.beyond_infinity,key,slotType,slotIndex)) { return count; }
    if(const auto count=state::activity::vanilla::one_au::body_bits(snapshot.one_au,key,slotType,slotIndex)) {return count;}
    if(const auto count=state::activity::vanilla::homecoming::body_bits(snapshot.homecoming,key,slotType,slotIndex)) {return count;}
    if(const auto count=state::activity::deep_storage::body_bits(snapshot.deep_storage,key,slotType,slotIndex)) { return count; }
    if(const auto count=state::activity::strike_bond::body_bits(snapshot.strike_bond,key,slotType,slotIndex)) { return count; }
    if(const auto count=state::activity::strike_pact::body_bits(snapshot.strike_pact,key,slotType,slotIndex)) { return count; }
    if(const auto count=native::lost_sector_shield::body_bits(snapshot.lostSectorShields,key,slotType,slotIndex))return count;
    if(const auto count=state::activity::vendors::presentation::body_bits(snapshot.vendorPresentation,key,slotType,slotIndex)) {return count;}
    if(const auto count=state::activity::hijacked::body_bits(snapshot.hijacked,key,slotType,slotIndex)) { return count; }
    if(const auto count=state::activity::newlight::launchpad::welcome::body_bits(snapshot.newlightWelcome,key,slotType,slotIndex)) {return count;}
    if(state::activity::newlight::launchpad::tower::matches(snapshot.launchpadTower,key,slotType,slotIndex)) {return 263;}
    if(state::activity::gateway_intro::matches(snapshot.gatewayIntro,key,slotType,slotIndex)) {return 263;}
    if(const auto count=state::activity::newlight::launchpad::body_bits(snapshot.launchpad,key,slotType,slotIndex)) {return count;}
    if(const auto* request=native::engagement::find(snapshot.engagements,key,slotType,slotIndex)) return native::engagement::body_bits(*request);
    if(const auto* request=native::world_device::find(snapshot.devices,key,slotType,slotIndex)) return native::world_device::valid(request->state)?native::world_device::kPayloadBits:0;
    if(const auto* request=native::forest_generator::find(snapshot.generators,key,slotType,slotIndex)) return native::forest_generator::body_bits(request->state);
    if(native::npc_animation::find(snapshot.animations,key,slotType,slotIndex)) return native::npc_animation::kBodyBits;
    if(native::cue::find(snapshot.cues,key,slotType,slotIndex)) return native::cue::kBits;
    if(const auto* request=native::dialogue::find(snapshot.dialogues,key,slotType,slotIndex)) return native::dialogue::body_bits(*request);
    if(const auto* request=native::world_sequence::find(snapshot.sequences,key,slotType,slotIndex)) return native::world_sequence::valid(*request)?native::world_sequence::kPayloadBits:0;
    if(const auto* request=native::event_participant::find(snapshot.eventParticipants,key,slotType,slotIndex)) return native::event_participant::body_bits(*request);
    if(const auto* request=native::music::find(snapshot.music,key,slotType,slotIndex)) return native::music::valid(*request)?native::music::kBits:0;
    if(const auto* request=native::placement::find(snapshot.placements,key,slotType,slotIndex)) return native::placement::body_bits(*request);
    if(const auto* request=native::population::find(snapshot.populations,key,slotType,slotIndex))
        return native::population::bits(*request);
    if(const auto* request=native::population::find_member(snapshot.populations,key,slotType,slotIndex))
        return native::population::member_bits(request->source.retireOwned);
    if(snapshot.omegaEndingSelected && state::activity::omega::ending::slot(key,slotType,slotIndex)) return 263;
    if (snapshot.omegaBossAuthority && boss::parent_slot(key, slotType, slotIndex)) return boss::kParentBits;
    if (snapshot.omegaBossAuthority && boss::member_slot(key, slotType, slotIndex))
        return boss::member_bits(snapshot.omegaBossGeneration!=0,snapshot.omegaBossGeneration,snapshot.omegaMission.arm,true);
    if (snapshot.omegaMission.generation) {
        if(const auto* route=transit::find(key,slotType,slotIndex))
            return slotType==4?(route->role==transit::Role::sink?375:252):147;
        if(const auto count=rescue::bits(snapshot.omegaMission,key,slotType,slotIndex)) return count;
        if(mission_devices::cannon(key,slotType,slotIndex)<4) return slotType==4?252:147;
        if (const auto* source=mission_auth::find(key,slotType,slotIndex)) return mission_auth::bits(*source);
        if (mission_auth::member(key,slotType,slotIndex)) return boss::kMemberBits;
    }
    if (snapshot.omegaLairAuthority && lair::find(key, slotType, slotIndex)) return lair::kSourceBits;
    if (snapshot.omegaSceneAuthority && snapshot.seedAuthoredSensors && kOmegaSceneAuthorityBodyReady) {
        if (ikora::source_slot(key, slotType, slotIndex)) return ikora::kSourceBits;
        if (ikora::scene_slot(key, slotType, slotIndex))
            return ikora::kSceneBits + (snapshot.omegaIkoraPortalRequested ? 32U : 0U);
        if (ikora::gate_slot(key, slotType, slotIndex)) return ikora::kGateBits;
    }
    if (snapshot.omegaPortalEntry && state::activity::omega::portal_entry::slot(key, slotType, slotIndex))
        return kOmegaPortalEntryBits;
    if (active_omega_portal_component(snapshot, key, slotType, slotIndex)) {
        return kOmegaPortalComponentBits;
    }
    if (active_omega_portal_gate(snapshot, key, slotType, slotIndex)) {
        return kOmegaPortalGateBits;
    }
    if (active_omega_gateway_push(snapshot, key, slotType, slotIndex)) {
        return kOmegaPortalComponentBits;
    }
    if (active_omega_boundary_body(snapshot, key, slotType, slotIndex)) {
        return find_mission_body(key, slotType, slotIndex)->width;
    }
    if (authored_scene_selector(snapshot, key, slotType, slotIndex)) {
        return kAuthoredActiveSceneBits;
    }
    if (omega_forest_generator(snapshot, key, slotType, slotIndex)) {
        return kOmegaForestGeneratorBits;
    }
    if (kOmegaDialogueBodyReady
        && ((snapshot.omegaSceneAuthority && snapshot.omegaDialogueArm
             && omega_dialogue(key, slotType, slotIndex))
            || authored_dialogue(snapshot, key, slotType, slotIndex))) {
        // The armed tunnel record carries its optional 64-bit time field, like record 0.
        return kOmegaDialogueBits
               + (snapshot.publishAuthoredCueTransition ? 0U
                   : 64U * (static_cast<unsigned>(snapshot.omegaTunnelDialogue)
                            + static_cast<unsigned>(snapshot.omegaVistaDialogue)
                            + static_cast<unsigned>(snapshot.omegaExitDialogue)
                            + static_cast<unsigned>((snapshot.omegaLairDialoguePendingRow >= 12U
                                && snapshot.omegaLairDialoguePendingRow < 34U)
                                && (snapshot.omegaLairDialogueRequestedMask
                                    & (UINT64_C(1) << snapshot.omegaLairDialoguePendingRow)) != 0)));
    }
    if (kOmegaDirectiveBodyReady
        && ((snapshot.omegaSceneAuthority && snapshot.omegaDialogueArm
             && omega_directive(key, slotType, slotIndex))
            || authored_directive(snapshot, key, slotType, slotIndex))) {
        return kOmegaDirectiveBits;
    }
    if(slotType==13 && !native::player_predicates::compose(snapshot.playerPredicates,snapshot.omegaPortalPlayerHash,
        state::activity::omega::portal_entry::kRequiredPlayerHash))return 0;
    if (slotType == kSlotTypeParticipation) {
        return carriesPlayerKey
                   ? kParticipationBits + (snapshot.hasRegion ? kParticipationRegionBits : 0)
                         + (snapshot.one_au.enabled || snapshot.homecoming.enabled || snapshot.nativeRespawnRestricted ? 16U : 0U)
                         + 32U*native::player_predicates::compose(snapshot.playerPredicates,snapshot.omegaPortalPlayerHash,
                             state::activity::omega::portal_entry::kRequiredPlayerHash).value().count
                   : 0;
    }
    if (slotType == kSlotTypeLifetime) {
        if(snapshot.strike_pact.services || snapshot.strike_bond.enabled) { return kLifetimeBits + forest_switch_count(snapshot) * encounters::kSwitchBits; }
        return state::activity::beyond_infinity::forest::selected(snapshot.beyond_infinity)
            ? kLifetimeBits+2*state::activity::beyond_infinity::forest::kSwitchBits
            : snapshot.omegaForestVexEncounters ? kOmegaLifetimeBits : kLifetimeBits;
    }
    if (slotType == kSlotTypeActivityScript && kInitializeActivityScript
        && (snapshot.initializeMissionAuthorityRuntime
            || snapshot.publishOmegaOpeningTransition
            || snapshot.publishAuthoredCueTransition)) {
        return kActivityScriptBits;
    }
    if (slotType == kSlotTypeMissionDirector && kInitializeMissionDirector
        && (snapshot.initializeMissionAuthorityRuntime
            || ((snapshot.one_au.enabled || snapshot.homecoming.enabled || snapshot.omegaMission.generation || snapshot.deep_storage.enabled || snapshot.hijacked.enabled || snapshot.strike_bond.enabled || snapshot.launchpad.enabled || snapshot.launchpadTower.enabled || snapshot.gatewayIntro.enabled) && key==0x4786C0E0U && slotIndex==1)
            || snapshot.publishOmegaOpeningTransition
            || snapshot.publishAuthoredCueTransition)) {
        return kMissionDirectorBits;
    }
    if (slotType == kSlotTypeConfiguration) {
        return kConfigurationBits;
    }
    if (slotType == kSlotTypePackage) {
        return kPackageBits;
    }
    if (slotType == kSlotTypeQueues) {
        return kQueueBits;
    }
    if (slotType == kSlotTypeSpawnKeys) {
        return kSpawnKeyBits;
    }
    if (snapshot.seedAuthoredSensors && key == 0x30A025E8U) {
        const MissionBody* const body = find_mission_body(key, slotType, slotIndex);
        if (body != nullptr) {
            return body->width;
        }
    }
    return 0;
}

/** Writes one slot's auth body. */
bool legacy_write_auth_body(bits::Writer& writer,
                     const Snapshot& snapshot,
                     std::uint32_t key,
                     std::uint8_t slotType,
                     std::uint16_t slotIndex,
                     bool carriesPlayerKey) noexcept {
    if (round::timer(snapshot.nativeRound,key,slotType,slotIndex)) {
        return state::activity::coo::native_clock::countdown(writer,
            snapshot.nativeRound.completion.valid(),
            snapshot.nativeRound.endEpoch);
    }
    if (round::lifetime(snapshot.nativeRound,key,slotType,slotIndex)) {
        return write_lifetime(writer,snapshot,
            snapshot.nativeRound.restricted ? snapshot.nativeRound.bubble : 0U,
            snapshot.lifetimeScenarioOrdinal);
    }
    if (round::director(snapshot.nativeRound,key,slotType,slotIndex)) {
        return write_mission_director(writer,snapshot,true,true);
    }
    if (const auto* request = status_effect::find(snapshot.statusEffects,key,slotType,slotIndex);
        request != nullptr && status_effect::valid(snapshot.statusEffects,snapshot.roster,snapshot.region)) {
        return status_effect::write_authority(writer,*request);
    }
    if(const auto* request=player_trigger::find(snapshot.playerTriggers,key,slotIndex);
        request && slotType==31 && player_trigger::valid(snapshot.playerTriggers,snapshot.roster,snapshot.region)) {
        return player_trigger::arm(writer,request->generation);
    }
    if(state::activity::deadly_trial::body_bits(snapshot.deadly_trial,key,slotType,slotIndex)) {
        return state::activity::deadly_trial::write_body(writer,snapshot.deadly_trial,key,slotType,slotIndex);
    }
    if(state::activity::beyond_infinity::body_bits(snapshot.beyond_infinity,key,slotType,slotIndex)) {
        return state::activity::beyond_infinity::write_body(writer,snapshot.beyond_infinity,key,slotType,slotIndex);
    }
    if(state::activity::vanilla::one_au::body_bits(snapshot.one_au,key,slotType,slotIndex)) {
        return state::activity::vanilla::one_au::write_body(writer,snapshot.one_au,key,slotType,slotIndex);
    }
    if(state::activity::vanilla::homecoming::body_bits(snapshot.homecoming,key,slotType,slotIndex)) {
        return state::activity::vanilla::homecoming::write_body(writer,snapshot.homecoming,key,slotType,slotIndex);
    }
    if(state::activity::deep_storage::body_bits(snapshot.deep_storage,key,slotType,slotIndex)) {
        return state::activity::deep_storage::write_body(writer,snapshot.deep_storage,key,slotType,slotIndex);
    }
    if(native::lost_sector_shield::body_bits(snapshot.lostSectorShields,key,slotType,slotIndex))
        return native::lost_sector_shield::write_body(writer,snapshot.lostSectorShields,key,slotType,slotIndex);
    if(state::activity::vendors::presentation::body_bits(snapshot.vendorPresentation,key,slotType,slotIndex)) {
        return state::activity::vendors::presentation::write(writer,snapshot.vendorPresentation,key,slotType,slotIndex);
    }
    if(state::activity::newlight::launchpad::welcome::body_bits(snapshot.newlightWelcome,key,slotType,slotIndex)) {
        return state::activity::newlight::launchpad::welcome::write(writer,snapshot.newlightWelcome,key,slotType,slotIndex);
    }
    if(state::activity::newlight::launchpad::tower::matches(snapshot.launchpadTower,key,slotType,slotIndex)) {
        return state::activity::newlight::launchpad::tower::write(writer,snapshot.launchpadTower);
    }
    if(state::activity::gateway_intro::matches(snapshot.gatewayIntro,key,slotType,slotIndex)) {
        return state::activity::gateway_intro::write(writer,snapshot.gatewayIntro);
    }
    if(state::activity::newlight::launchpad::body_bits(snapshot.launchpad,key,slotType,slotIndex)) {
        return state::activity::newlight::launchpad::write_body(writer,snapshot.launchpad,key,slotType,slotIndex);
    }
    if(state::activity::hijacked::body_bits(snapshot.hijacked,key,slotType,slotIndex)) {
        return state::activity::hijacked::write_body(writer,snapshot.hijacked,key,slotType,slotIndex);
    }
    if(state::activity::gateway::body_bits(snapshot.gateway,key,slotType,slotIndex)) {
        return state::activity::gateway::write_body(writer,snapshot.gateway,key,slotType,slotIndex);
    }
    if(state::activity::strike_bond::body_bits(snapshot.strike_bond,key,slotType,slotIndex)) {
        return state::activity::strike_bond::write_body(writer,snapshot.strike_bond,key,slotType,slotIndex);
    }
    if(state::activity::strike_pact::body_bits(snapshot.strike_pact,key,slotType,slotIndex)) {
        return state::activity::strike_pact::write_body(writer,snapshot.strike_pact,key,slotType,slotIndex);
    }
    if(const auto* request=native::engagement::find(snapshot.engagements,key,slotType,slotIndex)) return native::engagement::write(writer,*request);
    if(const auto* request=native::world_device::find(snapshot.devices,key,slotType,slotIndex)) return native::world_device::write_payload(writer,request->state);
    if(const auto* request=native::forest_generator::find(snapshot.generators,key,slotType,slotIndex)) return native::forest_generator::write_payload(writer,request->state);
    if(const auto* request=native::npc_animation::find(snapshot.animations,key,slotType,slotIndex))
        return native::npc_animation::write(writer,request->control);
    if(const auto* request=native::cue::find(snapshot.cues,key,slotType,slotIndex)) return native::cue::write(writer,*request);
    if(const auto* request=native::dialogue::find(snapshot.dialogues,key,slotType,slotIndex)) return native::dialogue::write(writer,*request);
    if(const auto* request=native::world_sequence::find(snapshot.sequences,key,slotType,slotIndex)) return native::world_sequence::write(writer,*request);
    if(const auto* request=native::event_participant::find(snapshot.eventParticipants,key,slotType,slotIndex)) return native::event_participant::write(writer,*request);
    if(const auto* request=native::music::find(snapshot.music,key,slotType,slotIndex)) return native::music::write(writer,*request);
    if(const auto* request=native::placement::find(snapshot.placements,key,slotType,slotIndex)) return native::placement::write(writer,*request);
    if(const auto* request=native::population::find(snapshot.populations,key,slotType,slotIndex))
        return native::combatant_source::write_source(writer,request->source);
    if(const auto* request=native::population::find_member(snapshot.populations,key,slotType,slotIndex))
        return native::population::write_member(writer,request->source.generation,
            request->source.retireOwned);
    const std::size_t start = writer.bit_count();
    const std::size_t expected =
        legacy_auth_body_bits(snapshot, key, slotType, slotIndex, carriesPlayerKey);
    bool encoded = true;
    if (snapshot.omegaBossAuthority && boss::parent_slot(key, slotType, slotIndex)) {
        encoded = boss::write_parent(writer, snapshot.omegaBossGeneration != 0, snapshot.omegaBossGeneration);
    } else if (snapshot.omegaBossAuthority && boss::member_slot(key, slotType, slotIndex)) {
        encoded = boss::write_member(writer, snapshot.omegaBossGeneration != 0, snapshot.omegaBossGeneration,snapshot.omegaMission.arm,true);
    } else if(snapshot.omegaEndingSelected && state::activity::omega::ending::slot(key,slotType,slotIndex)) {
        encoded=state::activity::omega::ending::write(writer,snapshot.omegaEndingRevision,snapshot.omegaEndingPlay);
    } else if(snapshot.omegaMission.generation && transit::find(key,slotType,slotIndex)) {
        encoded=transit::write(writer,snapshot.omegaMission,key,slotType,slotIndex);
    } else if(rescue::bits(snapshot.omegaMission,key,slotType,slotIndex)) {
        encoded=rescue::write(writer,snapshot.omegaMission,key,slotType,slotIndex);
    } else if(snapshot.omegaMission.generation && mission_devices::cannon(key,slotType,slotIndex)<4) {
        encoded=mission_devices::write(writer,snapshot.omegaMission,key,slotType,slotIndex);
    } else if (snapshot.omegaMission.generation && mission_auth::find(key,slotType,slotIndex)) {
        const auto& source=*mission_auth::find(key,slotType,slotIndex);
        encoded=mission_auth::write_source(writer,source,snapshot.omegaMission.generation,
            snapshot.omegaMission.requested[mission::source_index(key,slotIndex)]);
    } else if (snapshot.omegaMission.generation && mission_auth::member(key,slotType,slotIndex)) {
        const auto& source=*mission_auth::member(key,slotType,slotIndex);
        const auto index=mission::source_index(key,source.slot);
        encoded=boss::write_member(writer,snapshot.omegaMission.requested[index][0]!=0,snapshot.omegaMission.generation);
    } else if (snapshot.omegaLairAuthority && lair::find(key, slotType, slotIndex)) {
        encoded = lair::write_source(writer, *lair::find(key, slotType, slotIndex),
                                    snapshot.omegaLairGeneration, snapshot.omegaLairLeftStarted);
    } else if (snapshot.omegaSceneAuthority && snapshot.seedAuthoredSensors && kOmegaSceneAuthorityBodyReady
               && ikora::source_slot(key, slotType, slotIndex)) {
        encoded = ikora::write_source(writer);
    } else if (snapshot.omegaSceneAuthority && snapshot.seedAuthoredSensors && kOmegaSceneAuthorityBodyReady
               && ikora::scene_slot(key, slotType, slotIndex)) {
        encoded = ikora::write_scene(writer, snapshot.omegaIkoraPortalRequested);
    } else if (snapshot.omegaSceneAuthority && snapshot.seedAuthoredSensors && kOmegaSceneAuthorityBodyReady
               && ikora::gate_slot(key, slotType, slotIndex)) {
        encoded = ikora::write_gate(writer, snapshot.omegaIkoraLatticeReleased);
    } else if (snapshot.omegaPortalEntry && state::activity::omega::portal_entry::slot(key, slotType, slotIndex)) {
        encoded = write_portal_entry(writer);
    } else if (active_omega_portal_component(snapshot, key, slotType, slotIndex)) {
        encoded = write_active_omega_portal_component(writer);
    } else if (active_omega_portal_gate(snapshot, key, slotType, slotIndex)) {
        encoded = write_active_omega_portal_gate(writer);
    } else if (active_omega_gateway_push(snapshot, key, slotType, slotIndex)) {
        encoded = write_active_omega_portal_component(writer);
    } else if (active_omega_boundary_body(snapshot, key, slotType, slotIndex)) {
        encoded = write_mission_body(writer, *find_mission_body(key, slotType, slotIndex));
    } else if (authored_scene_selector(snapshot, key, slotType, slotIndex)) {
        encoded = write_authored_scene_authority(writer, snapshot);
    } else if (omega_forest_generator(snapshot, key, slotType, slotIndex)) {
        encoded = write_active_omega_forest_generator(writer);
    } else if (kOmegaDialogueBodyReady
               && ((snapshot.omegaSceneAuthority && snapshot.omegaDialogueArm
                    && omega_dialogue(key, slotType, slotIndex))
                   || authored_dialogue(snapshot, key, slotType, slotIndex))) {
        encoded = write_active_omega_dialogue(writer, snapshot);
    } else if (kOmegaDirectiveBodyReady
               && ((snapshot.omegaSceneAuthority && snapshot.omegaDialogueArm
                    && omega_directive(key, slotType, slotIndex))
                   || authored_directive(snapshot, key, slotType, slotIndex))) {
        encoded = write_active_omega_directive(writer, snapshot);
    } else if (slotType == kSlotTypeParticipation && carriesPlayerKey) {
        encoded = write_participation(writer, snapshot);
    } else if (slotType == kSlotTypeLifetime) {
        const bool sharedLifetime=key==0x4786C0E0U && slotIndex==3;
        encoded = write_lifetime(writer, snapshot,sharedLifetime
            ?(snapshot.one_au.enabled && snapshot.one_au.restricted?snapshot.one_au.bubble:snapshot.homecoming.enabled && snapshot.homecoming.restricted?snapshot.homecoming.bubble:snapshot.strike_bond.enabled && snapshot.strike_bond.restricted?17U:snapshot.hijacked.enabled && snapshot.hijacked.restricted?40U:snapshot.deep_storage.enabled && snapshot.deep_storage.restricted?19U:
              snapshot.omegaMission.generation && snapshot.omegaMission.restriction?14U:0U):0U,
            sharedLifetime ? snapshot.lifetimeScenarioOrdinal : std::nullopt);
    } else if (slotType == kSlotTypeActivityScript && kInitializeActivityScript
               && (snapshot.initializeMissionAuthorityRuntime
                   || snapshot.publishOmegaOpeningTransition
                   || snapshot.publishAuthoredCueTransition)) {
        encoded = write_activity_script(writer, snapshot);
    } else if (slotType == kSlotTypeMissionDirector && kInitializeMissionDirector
               && (snapshot.initializeMissionAuthorityRuntime
                   || ((snapshot.one_au.enabled || snapshot.homecoming.enabled || snapshot.omegaMission.generation || snapshot.deep_storage.enabled || snapshot.hijacked.enabled || snapshot.strike_bond.enabled || snapshot.launchpad.enabled || snapshot.launchpadTower.enabled || snapshot.gatewayIntro.enabled) && key==0x4786C0E0U && slotIndex==1)
                   || snapshot.publishOmegaOpeningTransition
                   || snapshot.publishAuthoredCueTransition)) {
        encoded = write_mission_director(writer, snapshot,(snapshot.one_au.enabled || snapshot.homecoming.enabled || snapshot.omegaMission.generation || snapshot.deep_storage.enabled || snapshot.hijacked.enabled || snapshot.strike_bond.enabled || snapshot.launchpad.enabled || snapshot.launchpadTower.enabled || snapshot.gatewayIntro.enabled)
            && key==0x4786C0E0U && slotIndex==1);
    } else if (slotType == kSlotTypeConfiguration) {
        // Both optional arrays absent and the terminal tag clear is the constructed state.
        encoded = writer.write(0, kPresenceWidth) && writer.write(0, kPresenceWidth)
                  && writer.write(0, kPresenceWidth) && writer.write(0, 32);
    } else if (slotType == kSlotTypePackage) {
        // 7 absent top-level fields keep the package-owned configuration.
        encoded = legacy_pad_bits(writer, kPackageBits);
    } else if (slotType == kSlotTypeQueues) {
        encoded = writer.write(0, 7) && writer.write(0, 5);
    } else if (slotType == kSlotTypeSpawnKeys) {
        encoded = write_spawn_keys(writer);
    } else if (snapshot.seedAuthoredSensors && key == 0x30A025E8U) {
        const MissionBody* const body = find_mission_body(key, slotType, slotIndex);
        if (body != nullptr) {
            encoded = write_mission_body(writer, *body);
        }
    }
    return encoded && writer.bit_count() == start + expected;
}

} // namespace dawn::middleware::bap::activity_message::sensor_auth_update
