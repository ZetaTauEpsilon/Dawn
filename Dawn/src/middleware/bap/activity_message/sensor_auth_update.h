#include "../../../state/activity/vanilla/one_au/controller.h"
#include "../../../state/activity/vanilla/homecoming/controller.h"
#include "../../../state/activity/vanilla/adieu/controller.h"
#pragma once
#include "../../../state/activity/gateway_intro.h"
#include "native/lost_sector_shield_authority.h"
#include "native/forest_generator_authority.h"
#include "native/world_device_authority.h"
#include "native/world_sequence_authority.h"
#include "native/public_event_engagement_authority.h"
#include "native/public_event_participant_authority.h"
#include "native/music_authority.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "../../encoding/bit_writer.h"
#include "activity_patch_epoch_parser.h"
#include "../../../state/activity/gateway/frame.h"
#include "../../../state/activity/beyond_infinity/frame.h"
#include "../../../state/activity/deep_storage/frame.h"
#include "../../../state/activity/hijacked/frame.h"
#include "../../../state/activity/vendors/presentation.h"
#include "../../../state/activity/Newlight/launchpad/controller.h"
#include "../../../state/activity/Newlight/launchpad/tower.h"
#include "../../../state/activity/Newlight/launchpad/welcome.h"
#include "../../../state/activity/deadly_trial/frame.h"
#include "../../../state/activity/strike_pact/frame.h"
#include "native/population_authority.h"
#include "native/round_authority.h"
#include "native/forest_switches.h"
#include "native/status_effect_authority.h"
#include "native/placement_authority.h"
#include "../../../state/activity/coo/native_player_trigger.h"
#include "native/native_npc_animation_authority.h"
#include "native/adventure_cue_authority.h"
#include "native/adventure_dialogue_authority.h"
#include "native/adventure_player_predicates.h"
#include "../../../state/activity/strike_bond/frame.h"
#include "../../../state/activity/omega/omega_mission_state.h"
#include "../../../state/activity/omega_crown_respawn_authority.h"
#include "../../../state/activity/omega_rescue_scene_authority.h"
#include "../../../state/activity/omega_music_authority.h"

namespace dawn::middleware::bap::activity_message::sensor_auth_update {

/** The return-to-Lighthouse pool needs defaults only while the ending is offered. */
[[nodiscard]] constexpr bool ending_runtime_seed_required(bool bookendState, bool arrived,
    bool play, bool started, bool failed) noexcept {
    return bookendState && arrived && play && !started && !failed;
}

/** The client roster and its bubble grants both use activity message type 5. */
inline constexpr std::uint32_t kMessageType = 5;
/** The authority table is 64 usable bubbles plus one fallback slot. */
inline constexpr std::size_t kAuthoritySlotCount = 65;
/** Bubble 64 is in the table but the world controller cannot enter it, so it is never granted. */
inline constexpr std::uint8_t kMaximumGrantBubble = 63;
/** Ordered, delivery-committed states for Omega's first authored opening edge. */
inline constexpr std::uint8_t kOmegaOpeningStageNone = 0;
inline constexpr std::uint8_t kOmegaOpeningStageBaseline = 1;
inline constexpr std::uint8_t kOmegaOpeningStageScene = 2;
inline constexpr std::uint8_t kOmegaOpeningStageReady = 3;
inline constexpr std::uint8_t kOmegaOpeningStageTriggered = 4;
/** Retired type-1 spawner stage, retained only so an in-flight older connection can skip it. */
inline constexpr std::uint8_t kOmegaOpeningStageSpawner = 5;
/** Post-retirement portal components, followed by script state 3, then a quiescent stage. */
inline constexpr std::uint8_t kOmegaOpeningStagePortal = 6;
inline constexpr std::uint8_t kOmegaOpeningStageCompleted = 7;
inline constexpr std::uint8_t kOmegaOpeningStageSettled = 8;
/** Exact forest-entrance edge installs persistent script state 4. */
inline constexpr std::uint8_t kOmegaForestStageTransition = 9;
/** Reserved successor for the next observed authored edge. */
inline constexpr std::uint8_t kOmegaForestStageSettled = 10;
/** Complete 8080626B opening authority, including its authorized Ikora actor source. */
inline constexpr bool kOmegaSceneAuthorityBodyReady = true;
/**
 * Gate authority is PARKED: driving channel 0 to 0 turned off the starry portal in a live
 * test. The lattice is part of dg_if_gate's shared model 80F4AF50 (texture 80F4B5DA), whose
 * presentation graph reads device_position. The colocated gate variants supply that value
 * differently; the correct opening transition still needs a native device-state receipt.
 * The separately placed vex_wall is not evidence for the Ikora lattice.
 */
inline constexpr bool kOmegaGateAuthorityBodyReady = false;

/** Ghost pre-roll dialogue (type 53, schema 0x80804F77) is a recovered tag-reflection body. */
inline constexpr bool kOmegaDialogueBodyReady = true;
/** Opening objective tracker (type 68, schema 0x80804F67): proven layout, prescribed content. */
inline constexpr bool kOmegaDirectiveBodyReady = true;
/** Slot 35 encodes its startup flags as two single bits, so this is the widest value. */
inline constexpr std::uint8_t kMaximumMissionDirectorVariant = 3;
/** A grant token of zero equals the client's cleared mirror, so it grants nothing. */
inline constexpr std::uint16_t kMinimumGrantToken = 1;
/** Host storage for top-level and bubble-local groups together. Mercury needs
 * patrol groups alongside player roots, public events and adventure overlays.
 * Group bodies use presence-terminated records; this is not a wire count width. */
inline constexpr std::size_t kGroupCapacity = 256;
/** The three lifetime states spawn gate G4's unbounded jump table accepts. */
inline constexpr std::array<std::uint8_t, 3> kLifetimeStates = {3, 6, 10};
/** Slot flag bit for a block that carries a sense reset bit. */
inline constexpr std::uint8_t kSlotSenseFlag = 1;
/** Slot flag bit for a block that carries an auth reset bit and its delta root. */
inline constexpr std::uint8_t kSlotAuthFlag = 2;
/** The widest slice-set index the type-17 spawn override's bias-1 field accepts. */
inline constexpr std::uint32_t kMaximumSpawnSliceSet = 0x1FF;
/** The unset spawn-set hash. An override carrying it disables the override it was meant to arm. */
inline constexpr std::uint32_t kAbsentSpawnSetHash = 0x811C9DC5;

/** One bubble handed to this client, as a change against its own per-bubble mirror. */
struct Grant final {
    std::uint8_t bubble{};
    std::uint16_t token{};
};

/**
 * One roster group and its slots, in slot-index order.
 * A descriptor's real slot index is not necessarily its ordinal in these arrays.
 */
struct Group final {
    std::uint32_t key{};
    std::span<const std::uint8_t> slotTypes{};
    std::span<const std::uint8_t> slotFlags{};
    std::span<const std::uint16_t> slotIndices{};
};

inline constexpr std::size_t kBubbleSubBlockCapacity = 64;
inline constexpr std::size_t kBubbleKeyCapacity = 96;
inline constexpr std::uint32_t kMaximumSubBlockBubble = 63;

/** One field-1 roster sub-block, active only for the named bubble. */
struct BubbleSubBlock final {
    std::uint32_t bubble{};
    std::span<const std::uint32_t> keys{};
    /** Empty preserves the default all-present roster. Otherwise one 0/1 per key.
     * Native removal requires the old key at the same index with presence zero;
     * omitting the key or sending count zero does not unregister it. */
    std::span<const std::uint8_t> presence{};
    /** Optional full wire state bytes per ordinal; empty uses Snapshot::stateSequence. */
    std::span<const std::uint8_t> states{};
};

/** Which groups one destination publishes and which of them binds the player. */
struct Roster final {
    std::array<Group, kGroupCapacity> groups{};
    std::size_t groupCount{};
    /** Leading groups carried by the top-level list; remaining groups are bubble-local. */
    std::size_t topLevelGroupCount{};
    /** Group whose first type-13 block carries the player key. It must be one that registers. */
    std::uint32_t playerKeyGroup{};
    std::span<const BubbleSubBlock> bubbleSubBlocks{};
    /** Optional retained wire ordinals, independent of the active authority-body groups.
     * Removed entries retain their key with presence zero until this binding ends. */
    std::span<const std::uint32_t> topLevelKeys{};
    std::span<const std::uint8_t> topLevelPresence{};
    std::span<const std::uint8_t> topLevelStates{};
};

[[nodiscard]] inline std::size_t top_level_key_count(const Roster& roster) noexcept {
    return roster.topLevelKeys.empty() ? roster.topLevelGroupCount : roster.topLevelKeys.size();
}

/** Everything one `sensor_auth_update` carries. */
struct Snapshot final {
    /** Native source authority prepared by a server population service. */
    native::population::Batch populations{};
    native::placement::Batch placements{};
    native::lost_sector_shield::Batch lostSectorShields{};
    native::forest_generator::Batch generators{};
    native::world_device::Batch devices{};
    native::engagement::Batch engagements{};
    native::npc_animation::Batch animations{};
    /** Target-free native objectives prepared by an owning server service. */
    native::cue::Batch cues{};
    native::dialogue::Batch dialogues{};
    native::world_sequence::Batch sequences{};
    native::event_participant::Batch eventParticipants{};
    native::music::Batch music{};
    native::status_effect::Batch statusEffects{};
    state::activity::coo::native_player_trigger::Batch playerTriggers{};
    native::player_predicates::Set playerPredicates{};
    /** Exact, server-owned generic round authority profile; inactive by default. */
    native::round_authority::State nativeRound{};
    /** Optional typed Forest lifetime switches owned by the active generic round. */
    native::forest_switches::Batch nativeForestSwitches{};
    /** Selects the archive protocol only for mission_scot. */
    bool archiveOmega{};
    state::activity::gateway::Frame gateway{};
    state::activity::beyond_infinity::Frame beyond_infinity{};
    state::activity::deep_storage::Frame deep_storage{};
    state::activity::hijacked::Frame hijacked{};
    state::activity::vendors::presentation::Frame vendorPresentation{};
    state::activity::newlight::launchpad::Frame launchpad{};
    state::activity::newlight::launchpad::tower::Frame launchpadTower{};
    state::activity::gateway_intro::Frame gatewayIntro{};
    state::activity::newlight::launchpad::welcome::Frame newlightWelcome{};
    state::activity::vanilla::one_au::Frame one_au{};
    state::activity::vanilla::homecoming::Frame homecoming{};
    state::activity::vanilla::adieu::Frame adieu{};
    state::activity::deadly_trial::Frame deadly_trial{};
    state::activity::strike_pact::Frame strike_pact{};
    state::activity::strike_bond::Frame strike_bond{};
    state::activity::coo::CompletionPublication missionCompletion{};
    /** Terminal native phase8, with no success result, after a qualified Nightfall failure. */
    bool nightfallFailed{};
    /** Optional native type2 configuration, serialized before this type5 snapshot. */
    std::optional<native::activity_clock::Configuration> activityClock{};
    /** Original3C9FC0 consumes this64-bit header as native673200-unit elapsed time. */
    std::uint64_t activityElapsedTicks{};
    /** Message 52's payload, echoed exactly. A wrong epoch skips phase 2 and reports nothing. */
    patch_epoch::PatchEpoch patchEpoch{};
    Roster roster{};
    Grant grant{};
    /** Native gameplay clock base in 673200 ticks/second; zero preserves existing callers.
     * 3CA310 reads eight raw little-endian bytes after the grant through 351070;
     * 3CA34C publishes the resulting qword to clock+50. */
    std::uint64_t gameplayClockTicks{};
    /** Message 12's member record key. Zero leaves every type-13 block inert. */
    std::uint64_t playerKey{};
    /** Per-entry state byte. A change tears down and rebuilds every roster-owned object. */
    std::uint8_t stateSequence{};
    /** The participation record's region index. Its `+8` latch needs it. */
    std::uint32_t region{};
    std::uint32_t spawnSetHash{};
    std::uint32_t spawnSliceSet{};
    std::uint8_t lifetime{};
    /** Admitted lifetime authority+C scenario ordinal (0..63). */
    std::optional<std::uint32_t> lifetimeScenarioOrdinal{};
    /** Diagnostic slot-35 startup flags. Only the low two bits are encoded. */
    std::uint8_t missionDirectorVariant{};
    /** Shared schema-0x808099C4 activation latch carried by slot 35. */
    bool missionDirectorActive{};
    /** Initialize slot-18/35 mission-state storage once; this does not start a mission executor. */
    bool initializeMissionAuthorityRuntime{};
    /** Omit phase 2 afterward so keepalives cannot reset the stored authority state. */
    bool preserveMissionAuthorityState{};
    /** Publish only Omega's slot-18/35 opening transition, leaving every scene object untouched. */
    bool publishOmegaOpeningTransition{};
    /** Current targeted opening packet; baseline is committed session state, not a wire value. */
    std::uint8_t omegaOpeningStage{};
    /** Seed the forced authored mission component bodies after arrival. */
    bool seedAuthoredSensors{};
    /** Allow a complete, validated Omega Scene body once its codec is recovered. */
    bool omegaSceneAuthority{};
    /** Retained Ikora approach policy: deliver native C7ECAA77 without restarting the Scene. */
    bool omegaIkoraPortalRequested{};
    /** Native Scene output 792AAA50 has released D00142CF/23/16 for this binding. */
    bool omegaIkoraLatticeReleased{};
    /** Initial keyed-player data required by the authored portal contact predicate. */
    bool omegaPortalPlayerHash{};
    /** Retained exact lighthouse_teleport carrier after the native lattice release. */
    bool omegaPortalEntry{};
    /** Omega's authored Forest encounter selector, published through lifetime globals. */
    bool omegaForestVexEncounters{};
    /**
     * Arm the one-shot Ghost dialogue only after the client acknowledged the roster (in-world).
     * A type-53 generation submitted mid-load is consumed, deadline-dropped during the fade,
     * and never retried, so publishing before this point silently burns the line.
     */
    bool omegaDialogueArm{};
    /** Arms dialogue record 6 (bank AE2495AC): the Ghost line inside the gate tunnel. */
    bool omegaTunnelDialogue{};
    /** Forest vista (bank row 7) and exit (row 9), armed by run-local checkpoints. */
    bool omegaVistaDialogue{};
    bool omegaExitDialogue{};
    std::uint64_t omegaLairDialogueRequestedMask{};
    std::uint8_t omegaLairDialoguePendingRow{255};
    std::uint32_t omegaLairObjectiveEvent{};
    /** Optional authored type-47 target for the active Omega directive. */
    std::uint32_t omegaWaypointRegistry{};
    std::uint16_t omegaWaypointIndex{};
    std::uint32_t omegaWaypointDestination{0x811C9DC5U};
    /** Switches the active directive to the forest objective once the region is a forest one. */
    bool omegaForestBanner{};
    /** Current native presentation, with historical generations but only one eligible row. */
    std::array<std::uint32_t, 34> omegaDialogueGenerations{};
    std::uint32_t omegaObjectiveEvent{0xC252E306U};
    std::uint32_t omegaIntroRevision{};
    bool omegaIntroPlay{};
    std::uint32_t omegaBossGeneration{};
    // Active archive encounter control; independent of the unused legacy mission snapshot.
    state::activity::omega::boss_authority::ArmControl omegaArchiveArm{};
    state::activity::omega::boss_authority::IntroProgram omegaArchiveIntro{};
    /** Native first-approach sources. Requests remain zero until a verified arm lift. */
    std::uint32_t omegaFirstLairGeneration{};
    std::array<std::uint8_t,21> omegaFirstLairLoose{};
    bool omegaFirstLairAnchor{},omegaFirstCannonActive{},omegaFinalCannonActive{};
    std::uint8_t omegaActiveDialogueRow{0xFFU};
    /** Publishes a neutral map-generator authority body so the forest generator activates. */
    bool omegaForestGenerator{};
    std::uint32_t omegaForestSeed{};
    /** Stable Omega-only typed Vex selection, published before Forest generation. */
    /** Exact sq_boss authority. Dormant until the doorway; stable for its native member. */
    bool omegaBossAuthority{};
    /** First Lair sources stay dormant until an owned native left-arm receipt. */
    state::activity::omega::mission::Snapshot omegaMission{};
    std::uint32_t omegaEndingRevision{};
    bool omegaEndingSelected{};
    bool omegaLairAuthority{};
    std::uint32_t omegaLairGeneration{};
    bool omegaLairLeftStarted{};
    /** Allow a validated 80804F45 gate command once revision/polarity are recovered. */
    bool omegaGateAuthority{};
    /** Allow the experimental Lighthouse portal visual/transport body. */
    bool omegaPortalMutation{};
    /** Exact Lighthouse contact-effect carrier; independent of the synthetic scene stages. */
    /** Seed the exact contact predicate's hash in this mission's keyed player record.
     * Independent of wall activation: native-owned deltas must not resend participation. */
    /** Diagnostic only: this body changes the connection's published slot-35 latch. */
    bool missionDirectorTransition{};
    /** Schema-0x80809919 activity-script flag at aligned runtime +0x38. */
    bool activityScriptFlag{};
    /** Schema-0x80809919 biased signed scalar at aligned runtime +0x3C. */
    std::int32_t activityScriptState{};
    /** Publish one mission-independent root-cue/runtime transition. */
    bool publishAuthoredCueTransition{};
    /** Root registry containing the directive and dialogue components selected by the manifest. */
    std::uint32_t authoredCueRegistry{};
    /** Directive event key recovered from the mission's package cue table. */
    std::uint32_t authoredDirectiveEvent{};
    /** One record in the shared type-53 playback component. */
    std::uint8_t authoredDialogueRecord{};
    /** Delivery-correlated manifest stage; zero means this body carries no authored beat. */
    std::uint8_t authoredCueStage{};
    /** Publish the Scene selector paired with this authored cue transition. */
    bool publishAuthoredSceneSelector{};
    /** Registry and slot identity of the mission-local Scene controller. */
    std::uint32_t authoredSceneRegistry{};
    std::uint8_t authoredSceneType{};
    std::uint16_t authoredSceneIndex{};
    /** Package-authored selector consumed by the native Scene controller. */
    std::uint32_t authoredSceneSelector{};
    /** One package-recovered 55-bit active-entry reference for the selected Scene. */
    std::uint32_t authoredSceneEntryRegistry{};
    std::uint8_t authoredSceneEntryType{};
    std::uint16_t authoredSceneEntryIndex{};
    bool hasGrant{};
    bool hasRegion{};
    bool hasSpawnOverride{};
    bool preferSpawnHistory{};
    bool nativeRespawnRestricted{};
    /** Hold the client's spawn while it loads by emitting `awaiting_client_sync`. */
    bool awaitClientSync{};
    /** Register the groups and seed no object. Separates no components from no auth state. */
    bool phaseOneOnly{};
    /**
     * Fill the participation body on every type-13 slot, not only the group's first.
     * The gate reads the record of the object the player datum names. Only one type-13 slot
     * gets the body, so filling the first slot alone can miss that object.
     */
    bool keyOnEveryParticipationSlot{};
    /** Run-latched Crown arrival; native restriction applies only in Omega's Lair slice. */
    bool omegaCrownRestricted{};
    /** First Crown arena sources; native summon receipts release cumulative requests. */
    std::uint32_t omegaCrownGeneration{};
    std::array<std::array<std::uint8_t,2>,16> omegaCrownLoose{};
    bool omegaCrownAnchor{};
    /** Later native Crown cohorts share the run's source generation. */
    std::array<std::array<std::uint8_t,2>,21> omegaHiveLoose{};
    bool omegaHiveAnchor{};
    std::array<std::array<std::uint8_t,2>,19> omegaVexLoose{};
    bool omegaVexAnchor{};
    std::array<std::array<std::uint8_t,2>,12> omegaCabalLoose{};
    bool omegaCabalAnchor{};
    std::uint8_t omegaCrownCycle{};
    bool omegaCrownChargeEnabled{},omegaCrownChargeDunked{};
    bool omegaCrownEyeStatusActive{};
    /** Native DPS recovery launcher, retained until the next rescue begins. */
    bool omegaCrownReturnLaunch{};
    bool omegaCrownTransitLaunches{},omegaCrownTransitBridge{},omegaCrownTransitTarget{};
    std::uint64_t omegaCrownTransitCreated{};
    bool omegaCrownFinalTraversal{};
    state::activity::omega_crown_respawn::Restriction omegaCrownRestriction{};
    /** Retained native Osiris and Echo Scene commands; dormant commands request no actors. */
    std::uint32_t omegaRescueSourcesGeneration{};
    state::activity::omega_rescue_npc::Commands omegaRescueScenes{};
    std::uint16_t omegaRescueMarkerReadyMask{};
    /** The ending is only eligible after its authored alternate state is loaded. */
    bool omegaEndingPlay{};
    std::uint8_t omegaEndingState{};
    /** Explicit native roster removals precede the bookend transport. */
    bool omegaEndingRetire{};
    /** Seed the newly reset native runtime pool until the actual cinematic-start receipt. */
    bool omegaEndingSeedRuntime{};
    /** Explicit host music selection; absent leaves native music authority untouched. */
    bool omegaMusicPresent{};
    state::activity::omega_music::Authority omegaMusic{};
};

/**
 * Encodes one `sensor_auth_update` body.
 * Phase 2 has no resync point, so a one-bit slip corrupts every later block in silence. Every
 * writer checks its own end position and the encode fails rather than shipping a slipped body.
 * @param snapshot Patch epoch, optional bubble grant, and the destination's roster.
 * @param output Caller storage, left unchanged when validation fails or it is too small.
 * @param written Receives the encoded size on success or zero on failure.
 * @return True when the whole zero-padded body fits.
 */
[[nodiscard]] bool encode_sensor_auth_update(const Snapshot& snapshot,
                                             std::span<std::byte> output,
                                             std::size_t& written) noexcept;

/** Bits before the enable latch with no bubble block: 8 hardwipe, 128 epoch, 1 present, 64 clock ticks.
 */
inline constexpr std::size_t kLatchBitWithoutGrant = 201;
/** A bubble block adds the 65-bit authority mask, two head bits, three per element, and one token.
 */
inline constexpr std::size_t kBubbleBlockBits =
    kAuthoritySlotCount + 2 + 3 * kAuthoritySlotCount + 16;
/** Each patch-epoch element is an unsigned 64-bit wire value. */
inline constexpr std::uint8_t kEpochWidth = 64;
/** The unchecked hardwipe token is one byte, before the patch epoch. */
inline constexpr std::uint8_t kHardwipeWidth = 8;
/** Eight raw little-endian gameplay-clock bytes follow the bubble block; retained width name. */
inline constexpr std::uint8_t kActivityTokenWidth = 64;
/** Changed authority tokens use the schema's unsigned 16-bit field. */
inline constexpr std::uint8_t kGrantTokenWidth = 16;
/** Every presence bit and every loop continuation bit is one bit wide. */
inline constexpr std::uint8_t kPresenceWidth = 1;
/** Both roster delta counts use the same 9-bit field. */
inline constexpr std::uint8_t kDeltaCountWidth = 9;
/** The delta's key array starts here, measured from the delta's own root bit. */
inline constexpr std::size_t kDeltaKeysBit = 12;
/** The presence mask is eight words wide whatever the key count. */
inline constexpr std::size_t kDeltaMaskWords = 8;
/** A registry key and the per-object block's length field are both 32 bits. */
inline constexpr std::uint8_t kKeyWidth = 32;
/** The object reference is a bias-1 slot type and a bias-32768 slot index. */
inline constexpr std::uint8_t kSlotTypeWidth = 7;
inline constexpr std::uint8_t kSlotIndexWidth = 16;
inline constexpr std::uint32_t kSlotTypeBias = 1;
inline constexpr std::uint32_t kSlotIndexBias = 32768;
/** The widest index that remains in range after applying the signed-field bias. */
inline constexpr std::uint16_t kMaximumSlotIndex = 32767;
/** The per-entry state byte is stored biased, so the wire value never goes negative. */
inline constexpr std::uint32_t kStateByteBias = 0x80;

/** @param keyCount Published group count. @return Bit position of the delta's presence mask. */
[[nodiscard]] constexpr std::size_t delta_mask_bit(std::size_t keyCount) noexcept {
    return kDeltaKeysBit + 32 * keyCount + 1;
}

/** @param keyCount Published group count. @return Bit position of the delta's state count. */
[[nodiscard]] constexpr std::size_t delta_state_count_bit(std::size_t keyCount) noexcept {
    return delta_mask_bit(keyCount) + 32 * kDeltaMaskWords + 1;
}

inline constexpr std::uint8_t kBubbleCountWidth = 7;
inline constexpr std::uint32_t kBubbleKeyBias = 0x80000000;
inline constexpr std::size_t kBubbleMaskWords = 3;
inline constexpr std::size_t kBubbleSubBlockFixedBits =
    kPresenceWidth + kKeyWidth + kPresenceWidth + kPresenceWidth + kBubbleCountWidth
    + kPresenceWidth + 32 * kBubbleMaskWords + kPresenceWidth + kBubbleCountWidth;
inline constexpr std::size_t kBubbleSubBlockKeyBits = kKeyWidth + 8;

[[nodiscard]] constexpr std::size_t
bubble_bits(std::span<const BubbleSubBlock> subBlocks) noexcept {
    std::size_t bits = kBubbleCountWidth;
    for (const BubbleSubBlock& block : subBlocks) {
        bits += kBubbleSubBlockFixedBits + kBubbleSubBlockKeyBits * block.keys.size();
    }
    return bits;
}

/** Total delta size, including the optional bubble-local field-1 half. */
[[nodiscard]] constexpr std::size_t delta_bits(
    std::size_t keyCount, std::span<const BubbleSubBlock> subBlocks) noexcept {
    return delta_state_count_bit(keyCount) + kDeltaCountWidth + 8 * keyCount + 1
           + (subBlocks.empty() ? 0 : bubble_bits(subBlocks));
}

/**
 * Writes zero bits in chunks the writer accepts.
 * @param writer Body writer.
 * @param count Bits to write.
 * @return True when every bit fits.
 */
[[nodiscard]] bool pad_bits(encoding::bits::Writer& writer, std::size_t count) noexcept;

/**
 * Writes the bubble authority block.
 * @param writer Body writer positioned after the block's present bit.
 * @param grant The one bubble to hand over.
 * @return True when the whole block fits.
 */
[[nodiscard]] bool write_bubble_block(encoding::bits::Writer& writer, const Grant& grant) noexcept;

/**
 * Writes the phase-1 roster delta, which registers the group keys.
 * @param writer Body writer positioned after the enable latch.
 * @param roster Groups to register, in publish order.
 * @param stateSequence Biased per-entry state byte.
 * @return True when the whole delta fits and lands on its own end bit.
 */
[[nodiscard]] bool write_roster_delta(encoding::bits::Writer& writer,
                                      const Roster& roster,
                                      std::uint8_t stateSequence) noexcept;

/**
 * Reports how many bits of auth body one slot carries.
 * @param snapshot Message input, which decides the type-13 body width.
 * @param key Registry key of the owning group.
 * @param slotType Slot type from the group's slot array.
 * @param slotIndex Authored slot ordinal in the owning group.
 * @param carriesPlayerKey True for the one type-13 block that binds the player.
 * @return Body bits, or zero for a seed-only block.
 */
[[nodiscard]] std::size_t
auth_body_bits(const Snapshot& snapshot,
               std::uint32_t key,
               std::uint8_t slotType,
               std::uint16_t slotIndex,
               bool carriesPlayerKey) noexcept;

/**
 * Writes one slot's auth body.
 * @param writer Body writer positioned after the auth delta's root bit.
 * @param snapshot Message input.
 * @param key Registry key of the owning group.
 * @param slotType Slot type from the group's slot array.
 * @param slotIndex Authored slot ordinal in the owning group.
 * @param carriesPlayerKey True for the one type-13 block that binds the player.
 * @return True when the body fits and matches its declared width.
 */
[[nodiscard]] bool write_auth_body(encoding::bits::Writer& writer,
                                   const Snapshot& snapshot,
                                   std::uint32_t key,
                                   std::uint8_t slotType,
                                   std::uint16_t slotIndex,
                                   bool carriesPlayerKey) noexcept;

/**
 * Writes one per-object state block.
 * The 32-bit length counts the remainder, which includes the reset bit, the auth root bit and the
 * sense bit. Leaving the reset bit out of it desyncs by 3 bits with nothing reported.
 * @param writer Body writer positioned at the block's continuation bit.
 * @param snapshot Message input.
 * @param key Registry key of the owning group.
 * @param slotType Slot type from the group's slot array.
 * @param slotIndex Slot ordinal, which is also the slot's index.
 * @param flags Sense and auth emit bits for that slot type.
 * @param carriesPlayerKey True for the one type-13 block that binds the player.
 * @return True when the block fits and lands on its declared end bit.
 */
[[nodiscard]] bool write_object_block(encoding::bits::Writer& writer,
                                      const Snapshot& snapshot,
                                      std::uint32_t key,
                                      std::uint8_t slotType,
                                      std::uint16_t slotIndex,
                                      std::uint8_t flags,
                                      bool carriesPlayerKey) noexcept;


// Existing codecs remain isolated from Omega archive serialization.
[[nodiscard]] bool legacy_pad_bits(encoding::bits::Writer& writer, std::size_t count) noexcept;

/**
 * Writes the bubble authority block.
 * @param writer Body writer positioned after the block's present bit.
 * @param grant The one bubble to hand over.
 * @return True when the whole block fits.
 */
[[nodiscard]] bool legacy_write_bubble_block(encoding::bits::Writer& writer, const Grant& grant) noexcept;

/**
 * Writes the phase-1 roster delta, which registers the group keys.
 * @param writer Body writer positioned after the enable latch.
 * @param roster Groups to register, in publish order.
 * @param stateSequence Biased per-entry state byte.
 * @return True when the whole delta fits and lands on its own end bit.
 */
[[nodiscard]] bool legacy_write_roster_delta(encoding::bits::Writer& writer,
                                      const Roster& roster,
                                      std::uint8_t stateSequence) noexcept;

/**
 * Reports how many bits of auth body one slot carries.
 * @param snapshot Message input, which decides the type-13 body width.
 * @param key Registry key of the owning group.
 * @param slotType Slot type from the group's slot array.
 * @param slotIndex Authored slot ordinal in the owning group.
 * @param carriesPlayerKey True for the one type-13 block that binds the player.
 * @return Body bits, or zero for a seed-only block.
 */
[[nodiscard]] std::size_t
legacy_auth_body_bits(const Snapshot& snapshot,
               std::uint32_t key,
               std::uint8_t slotType,
               std::uint16_t slotIndex,
               bool carriesPlayerKey) noexcept;

/**
 * Writes one slot's auth body.
 * @param writer Body writer positioned after the auth delta's root bit.
 * @param snapshot Message input.
 * @param key Registry key of the owning group.
 * @param slotType Slot type from the group's slot array.
 * @param slotIndex Authored slot ordinal in the owning group.
 * @param carriesPlayerKey True for the one type-13 block that binds the player.
 * @return True when the body fits and matches its declared width.
 */
[[nodiscard]] bool legacy_write_auth_body(encoding::bits::Writer& writer,
                                   const Snapshot& snapshot,
                                   std::uint32_t key,
                                   std::uint8_t slotType,
                                   std::uint16_t slotIndex,
                                   bool carriesPlayerKey) noexcept;

/**
 * Writes one per-object state block.
 * The 32-bit length counts the remainder, which includes the reset bit, the auth root bit and the
 * sense bit. Leaving the reset bit out of it desyncs by 3 bits with nothing reported.
 * @param writer Body writer positioned at the block's continuation bit.
 * @param snapshot Message input.
 * @param key Registry key of the owning group.
 * @param slotType Slot type from the group's slot array.
 * @param slotIndex Slot ordinal, which is also the slot's index.
 * @param flags Sense and auth emit bits for that slot type.
 * @param carriesPlayerKey True for the one type-13 block that binds the player.
 * @return True when the block fits and lands on its declared end bit.
 */
[[nodiscard]] bool legacy_write_object_block(encoding::bits::Writer& writer,
                                      const Snapshot& snapshot,
                                      std::uint32_t key,
                                      std::uint8_t slotType,
                                      std::uint16_t slotIndex,
                                      std::uint8_t flags,
                                      bool carriesPlayerKey) noexcept;


[[nodiscard]] bool legacy_encode_sensor_auth_update(const Snapshot& snapshot,
                                             std::span<std::byte> output,
                                             std::size_t& written) noexcept;

} // namespace dawn::middleware::bap::activity_message::sensor_auth_update
