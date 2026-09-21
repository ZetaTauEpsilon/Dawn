#pragma once

#include <string_view>
#include "definition.h"

namespace dawn::state::activity::forced::prelaunch {

// Pinned investment rows, independently joined to the installed package table.
// A profile permits selection-time derivation, not mission authority publication.
struct Profile final {
    std::string_view package;
    std::int16_t activity;
    std::uint32_t investmentHash;
    std::uint32_t packageHash;
    std::uint32_t activityTag;
    std::uint32_t launchTag;
    const char* event;
};
inline constexpr std::int16_t kDonorActivity = 282;
inline constexpr Profile kTowerfall{"mission_towerfall", 266, 0x62D85FB3U,
    0x9ACCB518U, 0x80B500ACU, 0x80FDB97FU, "towerfall_direct"};
inline constexpr Profile kAdieu{"mission_journey",288,0xB913ED3FU,
    0xD8A28814U,0x80B5E014U,0x80C10457U,"adieu_direct"};
inline constexpr Profile kGateway{"mission_abs", 292, 0x5A2E3FF4U,
    0x986985D0U, 0x80F46D99U, 0x80F9FDD2U, "gateway_direct"};
inline constexpr Profile kHauntedForest{"infinite_abyss", 78, 0x56B7B6A5U,
    0x6DA20650U, 0x81550000U, 0x80F9FDD2U, "haunted_forest_direct"};

inline constexpr Profile kDeadlyTrial{"adventure_ginger",293,0x87D9CA16U,
    0xC9BC773AU,0x80B2E004U,0x80FDB97FU,"deadly_trial_direct"};

inline constexpr Profile kBeyondInfinity{"adventure_vod",294,0x3E9433BDU,
    0x03632571U,0x80F46000U,0x80F9FDD2U,"beyond_infinity_direct"};

inline constexpr Profile kDeepStorage{"adventure_whisk",295,0x550500EEU,
    0xE6E910D2U,0x80B56019U,0x80F9F35EU,"deep_storage_direct"};

inline constexpr Profile kHijacked{"adventure_rumba",297,0x83211FEDU,
    0x77852DB9U,0x80B4200FU,0x80FB5018U,"hijacked_direct"};

// Activity8153C013 joins scenario8153C01B and launch descriptor80FDB97F.
inline constexpr Profile kLaunchpad{"mission_launchpad",1,0xED5A458AU,
    0x71EA80ACU,0x8153C013U,0x80FDB97FU,"launchpad_direct"};
// Installed 81327CF0 row 281, independently decoded for the Red War opening.
inline constexpr Profile kOneAu{"mission_ember",281,0x38F926B2U,
    0x1A6AF329U,0x80B3C07DU,0x80FDB97FU,"one_au_direct"};

[[nodiscard]] constexpr const Profile* find(std::string_view package) noexcept {
    if(package==kAdieu.package) return &kAdieu;
    if(package==kOneAu.package) {return &kOneAu;}
    if (package == kTowerfall.package) { return &kTowerfall; }
    if (package == kGateway.package) { return &kGateway; }
    if (package == kHauntedForest.package) { return &kHauntedForest; }
    if (package == kDeadlyTrial.package) { return &kDeadlyTrial; }
    if (package == kBeyondInfinity.package) { return &kBeyondInfinity; }
    if(package==kDeepStorage.package) {return &kDeepStorage;}
    if(package==kHijacked.package) {return &kHijacked;}
    if(package==kLaunchpad.package) {return &kLaunchpad;}
    return nullptr;
}
[[nodiscard]] constexpr const Profile* configured(const ForcedDestination& value) noexcept {
    return active(value)
        ? find({value.packageName.data(), value.packageNameLength}) : nullptr;
}
[[nodiscard]] constexpr bool donor(std::int16_t source, std::int16_t destination) noexcept {
    return source == kDonorActivity && destination == kDonorActivity;
}
[[nodiscard]] constexpr bool native_haunted_forest(std::int16_t source,
    std::int16_t destination,std::string_view package) noexcept {
    return source==kHauntedForest.activity && destination==kHauntedForest.activity
        && package==kHauntedForest.package;
}
[[nodiscard]] constexpr bool matches(const Profile& profile, std::int16_t source,
    std::int16_t destination, std::string_view package) noexcept {
    return source == profile.activity && destination == profile.activity && package == profile.package;
}

} // namespace dawn::state::activity::forced::prelaunch
