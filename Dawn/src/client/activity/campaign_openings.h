#pragma once

#include <array>
#include <span>
#include <string_view>
#include "../../state/activity/forced/definition.h"
#include "../../state/activity/forced/prelaunch_profile.h"
#include "../../state/activity/strike_variants.h"
#include "../../state/build_data/activities/activity_catalog.h"

namespace dawn::client::activity::mission_launch {
namespace openings {
namespace forced = state::activity::forced;

struct Mission {
    const char* title;
    const char* location;
    const char* description;
    unsigned campaign;
    std::uint16_t activity;
    std::uint32_t investmentHash;
    forced::ForcedDestination destination;
};

// Measured successful Omega arrival: references/omega-native-success-20260827-155532.log.
inline constexpr auto kOmegaOpening = [] {
    forced::ForcedDestination value{};
    constexpr std::string_view name = "mission_scot";
    for (std::size_t i = 0; i < name.size(); ++i) { value.packageName[i] = name[i]; }
    value.packageNameLength = static_cast<std::uint8_t>(name.size());
    value.bubble = 15; value.sliceSet = 120; value.spawnSetHash = 0x4AB3287AU;
    value.hasBubble = value.hasSliceSet = value.hasSpawnSetHash = value.enabled = true;
    return value;
}();

inline constexpr std::array<Mission, 13> kMissions{{
    {"Homecoming", "THE LAST CITY", "Return to the Tower as the Red Legion attacks the Last City.", 0, 266, 0x62D85FB3U, forced::profiles::kTowerfallOpening},
    {"Gateway", "MERCURY", "Follow Ikora to Mercury and begin the search for Osiris.", 1, 292, 0x5A2E3FF4U, forced::profiles::kGatewayOpening},
    {"A Deadly Trial", "EUROPEAN DEAD ZONE", "Track a lead through the EDZ in search of a way into the Infinite Forest.", 1, 293, 0x87D9CA16U, forced::profiles::kDeadlyTrialOpening},
    {"Beyond Infinity", "MERCURY", "Enter the Infinite Forest and explore the Vex simulations.", 1, 294, 0x3E9433BDU, forced::profiles::kBeyondInfinityOpening},
    {"Deep Storage", "IO", "Search the Vex network on Io for the information you need.", 1, 295, 0x550500EEU, forced::profiles::kDeepStorageOpening},
    {"Tree of Probabilities", "MERCURY", "Follow the trail through the shifting paths of the Infinite Forest.", 1, 296, 0x3BBB85F8U, forced::profiles::kMissionPactOpening},
    {"Hijacked", "NESSUS", "Find a Vex mind on Nessus to help locate Panoptes.", 1, 297, 0x83211FEDU, forced::profiles::kHijackedOpening},
    {"A Garden World", "MERCURY", "Return to the Simulant Past and defeat Dendron, Root Mind. Scan the algorithm to locate Panoptes.", 1, 298, 0x4C36870FU, forced::profiles::kMissionBondOpening},
    {"Omega", "MERCURY", "Return to the Infinite Forest and confront Panoptes with Osiris.", 1, 299, 0x87AC2003U, kOmegaOpening},
    {"Tree of Probabilities", "MERCURY", "Pursue Valus Thuun through the Infinite Forest.", 2, 230, 0x9FFC7326U, forced::profiles::kStrikePactOpening},
    {"A Garden World", "MERCURY", "Climb the spire and defeat Dendron, Root Mind.", 2, 229, 0x99BDAB3DU, forced::profiles::kStrikeBondOpening},
    {"1AU", "THE ALMIGHTY", "Board the Almighty and disable its weapon before it destroys the Sun.", 0, 281, 0x38F926B2U, forced::profiles::kOneAuOpening},
    {"Exodus", "THE LAST CITY", "Find Ghost, escape the City, and follow the falcon through the mountains.", 0, 288, 0xB913ED3FU, forced::profiles::kAdieuOpening},
}};

// Display numbers are campaign positions, not implementation/route indices.
// Keep those indices stable for queued launches and saved UI selection.
[[nodiscard]] constexpr unsigned mission_number(std::size_t index) noexcept {
    if(index>=kMissions.size()) return 0;
    if(kMissions[index].activity==266) return 1;
    if(kMissions[index].activity==288) return 2;
    if(kMissions[index].activity==281) return 16;
    unsigned ordinal{};
    for(std::size_t i=0;i<=index;++i) if(kMissions[i].campaign==kMissions[index].campaign) ++ordinal;
    return ordinal;
}
inline constexpr auto kDisplayOrder=[] {
    std::array<std::size_t,kMissions.size()> order{};
    for(std::size_t i=0;i<order.size();++i) order[i]=i;
    for(std::size_t i=1;i<order.size();++i) {
        const auto value=order[i];auto j=i;
        while(j && (kMissions[order[j-1]].campaign>kMissions[value].campaign
            || (kMissions[order[j-1]].campaign==kMissions[value].campaign
                && mission_number(order[j-1])>mission_number(value)))) {
            order[j]=order[j-1];--j;
        }
        order[j]=value;
    }
    return order;
}();

// Keep existing route indices stable; only the implemented Red War missions are listed.
[[nodiscard]] constexpr bool listed(const Mission& mission) noexcept {
    return mission.campaign != 0 || mission.activity == forced::prelaunch::kOneAu.activity
        || mission.activity == forced::prelaunch::kTowerfall.activity || mission.activity==forced::prelaunch::kAdieu.activity;
}

struct Route {
    std::uint16_t transport{0xFFFF};
    forced::ForcedDestination destination{};
    [[nodiscard]] constexpr bool valid() const noexcept { return transport != 0xFFFF; }
};

// Select the exact installed public activity. Similar names include heroic and playlist variants.
// Keep the activity identity intact so Destiny derives its own mission/strike presentation.
[[nodiscard]] inline Route resolve(std::size_t mission,
    std::span<const state::build_data::activities::Definition> rows,
    state::activity::strikes::Difficulty difficulty = state::activity::strikes::Difficulty::standard) noexcept {
    if (mission >= kMissions.size()) { return {}; }
    const auto& selected = kMissions[mission];
    auto index = selected.activity;
    auto hash = selected.investmentHash;
    if (difficulty != state::activity::strikes::Difficulty::standard) {
        if (selected.campaign != 2) { return {}; }
        const auto* variant = state::activity::strikes::find(
            {selected.destination.packageName.data(), selected.destination.packageNameLength}, difficulty);
        if (!variant) { return {}; }
        index = variant->activity; hash = variant->hash;
    }
    if (rows.size() <= index || rows[index].index != index || rows[index].hash != hash
        || rows[index].name() != std::string_view(selected.destination.packageName.data(),
            selected.destination.packageNameLength)) { return {}; }
    return {index, selected.destination};
}
} // namespace openings
} // namespace dawn::client::activity::mission_launch
