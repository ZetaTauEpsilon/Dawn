#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "activity_patch_epoch_parser.h"
#include "scene_sense.h"
#include "squad_sense.h"
#include "monitor_sense.h"
#include "combatant_sense.h"
#include "native_sense.h"

namespace dawn::middleware::bap::activity_message::sense_update {

/** The client reports sensor sense changes. It is the client's answer to the roster update. */
inline constexpr std::uint32_t kMessageType = 6;
/** Covers all 1,130 bits of the largest supported Scene delta, including revision. */
inline constexpr std::size_t kCapturedBodyBitCapacity = 1152;
/** Bounded decoded storage. Omega currently publishes six groups and at most 57 objects. */
inline constexpr std::size_t kRosterEntryCapacity = 128;
inline constexpr std::size_t kSenseGroupCapacity = 20;
// Crown/rescue add 239 sense-bearing descriptors. The complete roster has fewer
// than 400; leave headroom without putting a megabyte on nested parser stacks.
inline constexpr std::size_t kSenseObjectCapacity = 512;

/** The body opens with the same 128-bit patch epoch the roster update echoes. */
inline constexpr std::uint8_t kEpochFieldWidth = 64;
/** One literal zero bit follows the epoch. A set bit means the body is not this shape. */
inline constexpr std::uint8_t kLiteralZeroWidth = 1;
/** The client's outer destination bounds the whole body. */
inline constexpr std::size_t kOuterBitCapacity = 514'048;

/** One group acknowledged by the type-6 roster mirror. */
struct RosterEntry final {
    std::uint32_t registryKey{};
    /** -1 names the top-level list; 0..63 names a bubble-local subblock. */
    std::int16_t bubble{-1};
    std::uint8_t state{};
    bool active{};
};

/** One bounded phase-two group in a sensor report. */
struct SenseGroup final {
    std::uint32_t registryKey{};
    std::uint32_t bodyBits{};
    std::uint16_t firstObject{};
    std::uint16_t objectCount{};
};

/** One changed authored sensor object and its exact schema body. */
struct SenseObject final {
    std::uint32_t registryKey{};
    std::uint64_t bodyFirst{};
    std::uint64_t bodySecond{};
    std::uint64_t bodyThird{};
    std::uint64_t bodyFourth{};
    /** Remaining MSB-first chunks; the last partial chunk is right-aligned like the first three. */
    std::array<std::uint64_t, kCapturedBodyBitCapacity / 64 - 3> bodyTail{};
    /** Dawn's legacy FNV-like diagnostic hash over the meaningful width and body bits. */
    std::uint64_t bodyHash{};
    std::uint32_t bodyBits{};
    /** Raw native sensor revision, outside the reflected root delta. */
    std::uint32_t revision{};
    std::uint16_t slotIndex{};
    std::uint16_t bodySetBitCount{};
    std::uint16_t groupOrdinal{};
    std::uint16_t objectOrdinal{};
    std::uint8_t slotType{};
    /** Canonical native root/schema/revision. Group terminators are never body bits. */
    std::uint32_t nativeSchema{};
    std::uint32_t nativeRevision{};
    bool hasNativeSchema{};
    bool hasRootDelta{};
    native_sense::SourceDelta sourceDelta{};
    /** Native type-37 activation mirror, scoped by this object's registry and slot. */
    std::uint32_t forestActive32{};
    std::uint64_t forestActive64{};
    std::uint32_t forestRevision{};
    std::uint32_t forestSeed{};
    bool hasForestGeneratorState{};
    /** Typed native type37 facts; separate from the legacy Omega flag decoder. */
    native::forest_generator_sense::Progress generatorProgress{};
    native::engagement_sense::Output engagement{};
    bool hasGeneratorProgress{};
    /** Complete reflected Scene output, including events beyond the diagnostic 256-bit prefix. */
    scene_sense::Output sceneOutput{};
    bool hasSceneOutput{};
    /** Reflected squad delta for a slot-type-1 sensor, including the task-evaluator costs. */
    squad_sense::Output squadOutput{};
    bool hasSquadOutput{};
    monitor_sense::Output monitorOutput{};
    bool hasMonitorOutput{};
    combatant_sense::Output combatantOutput{};
    bool hasCombatantOutput{};
    ghost_sense::Output ghostOutput{};
    bool hasGhostOutput{};
    object_sense::Output objectOutput{};
    device_sense::Output deviceOutput{};
    native_sense::Passenger passengerOutput{};
    bool hasObjectOutput{},hasDeviceOutput{},hasPassengerOutput{};
    /** True when the group envelope, rather than a recovered schema constant, set bodyBits. */
    bool inferredBodyWidth{};
    bool hasDelta{};
};

/** Bounded decode of the type-6 forms recovered from the current Omega captures. */
struct SenseUpdate final {
    patch_epoch::PatchEpoch epoch{};
    std::array<RosterEntry, kRosterEntryCapacity> rosterEntries{};
    std::array<SenseGroup, kSenseGroupCapacity> groups{};
    std::array<SenseObject, kSenseObjectCapacity> objects{};
    std::uint16_t rosterEntryCount{};
    std::uint16_t topLevelRosterCount{};
    std::uint16_t groupCount{};
    std::uint16_t objectCount{};
    std::uint8_t bubbleBlockCount{};
    /** Zero padding after the two required terminal bits. */
    std::uint8_t paddingBits{};
    bool hasRosterAcknowledgement{};
};

/**
 * Parses the recovered Omega type-6 subset: its roster mirror and object schemas 1, 2, 23, 30, 43,
 * 37 and 70 using their reflected optional fields and bounded arrays. Body widths exclude
 * the group object-list terminator. Unknown schemas, truncation, nonzero terminal/padding
 * bits, invalid array counts, or capacity overflow fail the whole parse.
 */
[[nodiscard]] bool parse_sense_update(std::span<const std::byte> input,
                                      SenseUpdate& update,
                                      std::size_t& consumedBits) noexcept;

[[nodiscard]] bool parse_omega_sense_update(std::span<const std::byte> input,
                                      SenseUpdate& update,
                                      std::size_t& consumedBits) noexcept;


} // namespace dawn::middleware::bap::activity_message::sense_update
