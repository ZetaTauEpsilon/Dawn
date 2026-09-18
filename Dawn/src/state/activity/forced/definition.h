#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../destination/definition.h"

namespace dawn::state::activity::forced {

/** The bubble number picks one of the 64 wire slots. */
inline constexpr std::uint8_t kMaximumBubble = 63;
/** The slice-set index is a 10-bit bias-1 wire field, so this is the largest it can hold. */
inline constexpr std::uint16_t kMaximumSliceSet = 1'022;
/** The set normal arrivals use. */
inline constexpr std::uint32_t kDefaultSpawnSetHash = 0x2EA8FB98U;
/**
 * The hash that names no set, so the Client searches the loaded world itself.
 * A set belongs to the map, not the bubble. Send one the bubble lacks and nothing spawns.
 */
inline constexpr std::uint32_t kAbsentSpawnSetHash = 0x811C9DC5U;

/**
 * One operator-chosen destination that replaces what the client asked for.
 * Nothing here is saved. The process starts with no selection and the switch off.
 */
struct ForcedDestination {
    /** Destination package name, without its `:scenario_client` suffix. */
    std::array<char, destination::kPackageNameCapacity> packageName{};
    std::uint8_t packageNameLength{};
    std::uint8_t bubble{};
    std::uint16_t sliceSet{};
    /** Spawn-set name hash, used only when one was chosen. */
    std::uint32_t spawnSetHash{};
    bool hasBubble{};
    bool hasSliceSet{};
    bool hasSpawnSetHash{};
    /** True only for an authored cinematic destination whose scenario declares no world bubbles. */
    bool bubbleless{};
    /** The global switch. Off means the client's own selection stands. */
    bool enabled{};
};

/**
 * Tests whether a forced destination names enough to replace a client selection.
 * The spawn set is the one optional part. Without it the Client picks its own point.
 * @param value Candidate selection.
 * @return True when the switch is on and the destination, bubble, and slice set are all named.
 */
[[nodiscard]] constexpr bool active(const ForcedDestination& value) noexcept {
    const bool world = value.hasBubble && value.bubble <= kMaximumBubble && value.hasSliceSet
                       && value.sliceSet <= kMaximumSliceSet;
    const bool cinematic = value.bubbleless && !value.hasBubble && !value.hasSliceSet;
    return value.enabled && value.packageNameLength != 0
           && value.packageNameLength <= value.packageName.size() && (world || cinematic);
}

namespace profiles {

// Launchpad begins outside the Wall. Its default spawn is inside the Breach.
constexpr ForcedDestination launchpad_opening() noexcept {
    ForcedDestination v{};constexpr char name[]="mission_launchpad";
    for(std::size_t i=0;i<sizeof(name)-1;++i) {v.packageName[i]=name[i];}
    v.packageNameLength=sizeof(name)-1;v.bubble=3;v.sliceSet=24;v.spawnSetHash=0xEAEC2335U;
    v.hasBubble=v.hasSliceSet=v.hasSpawnSetHash=v.enabled=true;return v;
}
inline constexpr ForcedDestination kLaunchpadOpening=launchpad_opening();
static_assert(active(kLaunchpadOpening));

/** Measured Homecoming opening from mission_towerfall's installed scenario definition. */
inline constexpr char kTowerfallPackageName[] = "mission_towerfall";
/** Measured Underwatch opening used by the archived activity: bubble ordinal 9, region 72. */
inline constexpr std::uint8_t kTowerfallOpeningBubble = 9;
inline constexpr std::uint16_t kTowerfallOpeningSlice = 72;

/**
 * Builds the isolated Towerfall mission profile. The spawn remains absent deliberately: the
 * retained authored Chosen route owns the actual arrival point inside the opening bubble.
 */
[[nodiscard]] constexpr ForcedDestination towerfall_opening() noexcept {
    ForcedDestination value{};
    constexpr std::size_t packageLength = sizeof kTowerfallPackageName - 1;
    for (std::size_t index = 0; index < packageLength; ++index) {
        value.packageName[index] = kTowerfallPackageName[index];
    }
    value.packageNameLength = static_cast<std::uint8_t>(packageLength);
    value.bubble = kTowerfallOpeningBubble;
    value.sliceSet = kTowerfallOpeningSlice;
    value.hasBubble = true;
    value.hasSliceSet = true;
    value.enabled = true;
    return value;
}

inline constexpr ForcedDestination kTowerfallOpening = towerfall_opening();

/** Gateway's Lighthouse opening: bubble 15, region 120, and the package-derived
 * landing-zone spawn set confirmed in game. */
inline constexpr char kGatewayPackageName[] = "mission_abs";
inline constexpr std::uint8_t kGatewayOpeningBubble = 15;
inline constexpr std::uint16_t kGatewayOpeningSlice = 120;
inline constexpr std::uint32_t kGatewayOpeningSpawn = 0x69F52B3EU;
[[nodiscard]] constexpr ForcedDestination gateway_opening() noexcept {
    ForcedDestination value{};
    for (std::size_t index = 0; index < sizeof kGatewayPackageName - 1; ++index) {
        value.packageName[index] = kGatewayPackageName[index];
    }
    value.packageNameLength = sizeof kGatewayPackageName - 1;
    value.bubble = kGatewayOpeningBubble;
    value.sliceSet = kGatewayOpeningSlice;
    value.spawnSetHash = kGatewayOpeningSpawn;
    value.hasBubble = true;
    value.hasSliceSet = true;
    value.hasSpawnSetHash = true;
    value.enabled = true;
    return value;
}
inline constexpr ForcedDestination kGatewayOpening = gateway_opening();

/** Haunted Forest's furnished opening: bubble 13, slice set 104, and its authored spawn set. */
inline constexpr char kHauntedForestPackageName[] = "infinite_abyss";
inline constexpr std::uint8_t kHauntedForestOpeningBubble = 13;
inline constexpr std::uint16_t kHauntedForestOpeningSlice = 104;
inline constexpr std::uint32_t kHauntedForestOpeningSpawn = 0x79E3AB1FU;
[[nodiscard]] constexpr ForcedDestination haunted_forest_opening() noexcept {
    ForcedDestination value{};
    for (std::size_t index = 0; index < sizeof kHauntedForestPackageName - 1; ++index) {
        value.packageName[index] = kHauntedForestPackageName[index];
    }
    value.packageNameLength = sizeof kHauntedForestPackageName - 1;
    value.bubble = kHauntedForestOpeningBubble;
    value.sliceSet = kHauntedForestOpeningSlice;
    value.spawnSetHash = kHauntedForestOpeningSpawn;
    value.hasBubble = true;
    value.hasSliceSet = true;
    value.hasSpawnSetHash = true;
    value.enabled = true;
    return value;
}
inline constexpr ForcedDestination kHauntedForestOpening = haunted_forest_opening();


// A Deadly Trial native town opening. Spawn is the recovered point set inside
// the opening dialogue filter; final orientation requires a live run.
constexpr ForcedDestination deadly_trial_opening() noexcept {
    ForcedDestination v{};constexpr char name[]="adventure_ginger";
    for(std::size_t i=0;i<sizeof(name)-1;++i) { v.packageName[i]=name[i]; }
    v.packageNameLength=sizeof(name)-1;v.bubble=51;v.sliceSet=408;v.spawnSetHash=0x43954D08U;
    v.hasBubble=v.hasSliceSet=v.hasSpawnSetHash=v.enabled=true;return v;
}
inline constexpr ForcedDestination kDeadlyTrialOpening=deadly_trial_opening();
static_assert(active(kDeadlyTrialOpening));
// Recovered opening spawn; orientation and landing require live acceptance.
constexpr ForcedDestination beyond_infinity_opening() noexcept {
    ForcedDestination v{};constexpr char name[]="adventure_vod";
    for(std::size_t i=0;i<sizeof(name)-1;++i) { v.packageName[i]=name[i]; }
    v.packageNameLength=sizeof(name)-1;v.bubble=15;v.sliceSet=120;v.spawnSetHash=0x26B11B02U;
    v.hasBubble=v.hasSliceSet=v.hasSpawnSetHash=v.enabled=true;return v;
}
inline constexpr ForcedDestination kBeyondInfinityOpening=beyond_infinity_opening();
static_assert(active(kBeyondInfinityOpening));
// PACKAGE: three Io map spawn rows inside the mission's Rupture opening volume.
constexpr ForcedDestination deep_storage_opening() noexcept {
    ForcedDestination v{};constexpr char name[]="adventure_whisk";
    for(std::size_t i=0;i<sizeof(name)-1;++i) {v.packageName[i]=name[i];}
    v.packageNameLength=sizeof(name)-1;v.bubble=4;v.sliceSet=32;v.spawnSetHash=0x3AE5AC33U;
    v.hasBubble=v.hasSliceSet=v.hasSpawnSetHash=v.enabled=true;return v;
}
inline constexpr ForcedDestination kDeepStorageOpening=deep_storage_opening();
static_assert(active(kDeepStorageOpening));

// Tree of Probabilities: the Lighthouse opening of strike_pact, bubble 15, region 120, with
// the spawn set the accepted Dawn host build launched from.
constexpr ForcedDestination strike_pact_opening() noexcept {
    ForcedDestination v{};constexpr char name[]="strike_pact";
    for(std::size_t i=0;i<sizeof(name)-1;++i) { v.packageName[i]=name[i]; }
    v.packageNameLength=sizeof(name)-1;v.bubble=15;v.sliceSet=120;v.spawnSetHash=0x0E1523FEU;
    v.hasBubble=v.hasSliceSet=v.hasSpawnSetHash=v.enabled=true;return v;
}
inline constexpr ForcedDestination kStrikePactOpening=strike_pact_opening();
// Authored three-player Lighthouse south-side start, map 32, facing northeast.
// Spatial match to the supplied Garden opening view; live framing needs acceptance.
constexpr ForcedDestination strike_bond_opening() noexcept {
    ForcedDestination v{};constexpr char name[]="strike_bond";
    for(std::size_t i=0;i<sizeof(name)-1;++i) {v.packageName[i]=name[i];}
    v.packageNameLength=sizeof(name)-1;v.bubble=15;v.sliceSet=120;v.spawnSetHash=0xB09FB979U;
    v.hasBubble=v.hasSliceSet=v.hasSpawnSetHash=v.enabled=true;return v;
}
inline constexpr ForcedDestination kStrikeBondOpening=strike_bond_opening();
constexpr ForcedDestination campaign_opening(ForcedDestination v, const char* name, std::uint8_t size) noexcept {
    v.packageName.fill(0);
    for (std::size_t i=0;i<size;++i) { v.packageName[i]=name[i]; }
    v.packageNameLength=size;return v;
}
inline constexpr auto kMissionPactOpening=campaign_opening(kStrikePactOpening,"mission_pact",12);
inline constexpr auto kMissionBondOpening=campaign_opening(kStrikeBondOpening,"mission_bond",12);
static_assert(active(kStrikeBondOpening));
static_assert(active(kStrikePactOpening));
// Spatial reconstruction: the three native map points in this set lie inside
// Hijacked's Artifacts Edge briefing volume. The retail launch selection is unknown.
constexpr ForcedDestination hijacked_opening() noexcept {
    ForcedDestination v{};constexpr char name[]="adventure_rumba";
    for(std::size_t i=0;i<sizeof(name)-1;++i) {v.packageName[i]=name[i];}
    v.packageNameLength=sizeof(name)-1;v.bubble=13;v.sliceSet=104;v.spawnSetHash=0x1BD69720U;
    v.hasBubble=v.hasSliceSet=v.hasSpawnSetHash=v.enabled=true;return v;
}
inline constexpr ForcedDestination kHijackedOpening=hijacked_opening();
static_assert(active(kHijackedOpening));

// Package-derived Starboard Landing: bubble 8, region 64; all three spawn
// points in set 2EA8FB98 lie inside the native helipad arrival volume.
constexpr ForcedDestination one_au_opening() noexcept {
    ForcedDestination v{};constexpr char name[]="mission_ember";
    for(std::size_t i=0;i<sizeof(name)-1;++i) {v.packageName[i]=name[i];}
    v.packageNameLength=sizeof(name)-1;v.bubble=8;v.sliceSet=64;v.spawnSetHash=0x2EA8FB98U;
    v.hasBubble=v.hasSliceSet=v.hasSpawnSetHash=v.enabled=true;return v;
}
inline constexpr ForcedDestination kOneAuOpening=one_au_opening();
static_assert(active(kOneAuOpening));
} // namespace profiles

static_assert(active(profiles::kTowerfallOpening));
static_assert(active(profiles::kGatewayOpening));
static_assert(active(profiles::kHauntedForestOpening));

/**
 * Tests whether a candidate can be stored, complete or not.
 * A partial selection is kept so the interface can set one field at a time.
 * @param value Candidate selection.
 * @return True when every named field is inside its wire range.
 */
[[nodiscard]] constexpr bool storable(const ForcedDestination& value) noexcept {
    return value.packageNameLength <= value.packageName.size()
           && (!value.hasBubble || value.bubble <= kMaximumBubble)
           && (!value.hasSliceSet || value.sliceSet <= kMaximumSliceSet);
}

} // namespace dawn::state::activity::forced
