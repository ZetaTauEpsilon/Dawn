#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include "client/activity/mission_launch.h"
#include "client/activity/campaign_openings.h"
#include "client/activity/campaign_dialogue.h"
#include "client/activity/mission_launch_options.h"
#include "core/logging/log.h"
#include "middleware/content/packages/tables/activity_table.h"

namespace {
namespace launch = dawn::client::activity::mission_launch;
namespace forced = dawn::state::activity::forced;
namespace build = dawn::state::build_data;
namespace tables = dawn::middleware::content::packages::tables;
unsigned g_checks{}, g_publishes{}, g_clears{}, g_dialogueSelections{};
build::scenarios::Definition g_layout{};
void check(bool value, const char* message) {
    ++g_checks; if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
std::vector<std::byte> load(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    check(file.good(), "installed fixture opens");
    const auto size = file.tellg(); check(size > 0, "installed fixture nonempty");
    std::vector<std::byte> result(static_cast<std::size_t>(size)); file.seekg(0);
    file.read(reinterpret_cast<char*>(result.data()), size); check(file.good(), "installed fixture read"); return result;
}
}
namespace dawn::state::build_data {
bool find_scenario_layout(std::string_view name, scenarios::Definition& output) noexcept {
    output = g_layout; return name == "infinite_abyss";
}
bool find_spawn_sets(std::string_view, std::span<spawn_sets::NameHash>, std::size_t& count) noexcept { count = 0; return false; }
}
namespace dawn::client::activity::campaign_dialogue {
bool select(std::int16_t) noexcept { ++g_dialogueSelections; return false; }
}
namespace dawn::client::hooks::bootflow {
bool prepare_mission_prelaunch(const dawn::state::activity::forced::ForcedDestination&) noexcept { return true; }
}
namespace dawn::state::activity::forced {
void clear() noexcept { ++g_clears; }
bool override_active() noexcept { return false; }
void snapshot(ForcedDestination& value) noexcept { value = {}; }
bool publish(const ForcedDestination&) noexcept { ++g_publishes; return true; }
bool publish_direct(const ForcedDestination&,std::int16_t) noexcept { ++g_publishes; return true; }
}
namespace dawn::state::activity {
std::uint64_t newest_joined_session() noexcept { return 0; }
std::uint64_t mission_run_generation() noexcept { return 0; }
}
namespace dawn::state::activity::destination {
bool snapshot(std::uint64_t, DestinationSelection&) noexcept { return false; }
}
namespace dawn::core::log {
void write(Channel, Level, std::string_view) noexcept {}
}
int main(int argc, char** argv) {
    check(argc == 3, "provide public activity table and installed Haunted scenario");
    static std::array<build::activities::Definition, build::activities::kCapacity> activities{};
    std::size_t count{};
    check(tables::activities::decode(load(argv[1]), activities, count) && count == 1170, "all exact native identities");
    check(build::activities::publish(std::span(activities).first(count)), "native catalog published");
    const auto data = load(argv[2]); tables::Array bubbles{};
    check(tables::scenario_bubbles(data, bubbles) && bubbles.count == 20, "actual Haunted scenario has twenty bubbles");
    constexpr std::string_view name = "infinite_abyss";
    std::copy(name.begin(), name.end(), g_layout.name.begin()); g_layout.nameLength = static_cast<std::uint8_t>(name.size());
    g_layout.tag = 0x81550015; g_layout.bubbleCount = 20;
    for (std::uint8_t i = 0; i < 20; ++i) {
        tables::Bubble bubble{}; tables::SliceState state{};
        check(tables::bubble_at(data, bubbles, i, bubble) && tables::slice_state_at(data, bubble, 0, state), "installed bubble/slice join");
        g_layout.bubbleStateCounts[i] = static_cast<std::uint8_t>(bubble.stateCount);
        g_layout.bubbleHashes[i] = bubble.nameHash;
        g_layout.bubbleMapIndices[i] = static_cast<std::uint16_t>(state.mapBubbleIndex);
    }
    forced::ForcedDestination value{};
    std::copy(name.begin(), name.end(), value.packageName.begin()); value.packageNameLength = static_cast<std::uint8_t>(name.size());
    value.enabled = true; value.hasBubble = true; value.hasSliceSet = true;
    unsigned valid{};
    for (std::uint8_t bubble = 0; bubble < 64; ++bubble) {
        for (std::uint16_t slice = 0; slice < 512; ++slice) {
            value.bubble = bubble; value.sliceSet = slice;
            const bool accepted = launch::validate_manual(value, g_layout, {}) == launch::ManualError::none;
            check(accepted == (bubble < 20 && slice == bubble * 8), "only this installed bubble's authored slice is allowed");
            valid += accepted ? 1U : 0U;
        }
    }
    check(valid == 20, "all twenty authored bubble/slice pairs retained");
    value.bubble = 13; value.sliceSet = 104;
    check(g_layout.bubbleMapIndices[13] == 30, "actual Forest Gate map bubble");
    build::spawn_sets::NameHash spawn{};
    spawn.value = 0x79E3AB1F; spawn.pointCount = 3; spawn.inMapPackage = 1; spawn.bubbleMask[30 / 8] = 1U << (30 % 8);
    value.hasSpawnSetHash = true; value.spawnSetHash = spawn.value;
    check(launch::validate_manual(value, g_layout, {&spawn, 1}) == launch::ManualError::none, "loaded bubble-bound spawn allowed");
    auto rejected = [&](const build::spawn_sets::NameHash& candidate) {
        check(launch::validate_manual(value, g_layout, {&candidate, 1}) == launch::ManualError::spawn,
            "unsupported spawn membership rejected");
    };
    auto bad = spawn; bad.bubbleMask = {}; rejected(bad);
    bad.unbound = 1; rejected(bad); // An unbound candidate is not proof of an arrival point.
    bad = spawn; bad.pointCount = 0; rejected(bad);
    bad = spawn; bad.inMapPackage = 0; bad.activityPackageCount = 1; bad.activityPackages[0] = 99; rejected(bad);
    bad = spawn; bad.activityPackageOverflow = 1; rejected(bad);
    bad = spawn; bad.activityPackageCount = 255; rejected(bad);
    value.hasSpawnSetHash = false;
    check(launch::validate_manual(value, g_layout, {}) == launch::ManualError::none, "client-selected spawn needs no guessed hash");
    auto other = g_layout; other.name[0] = 'x';
    check(launch::validate_manual(value, other, {}) == launch::ManualError::activity, "stale activity layout rejected");
    other = g_layout; other.truncated = 1;
    check(launch::validate_manual(value, other, {}) == launch::ManualError::activity, "truncated layouts rejected");
    other = g_layout; other.bubbleCount = 0;
    auto cinematic = value; cinematic.bubbleless = true; cinematic.hasBubble = cinematic.hasSliceSet = false;
    check(launch::validate_manual(cinematic, other, {}) == launch::ManualError::none, "authored bubbleless scenario needs no fabricated arrival");
    check(launch::validate_manual(cinematic, g_layout, {}) == launch::ManualError::bubble, "world scenario cannot claim cinematic bypass");
    auto homecoming = forced::profiles::kTowerfallOpening;
    check(launch::manual_transport_valid(282, homecoming, std::span(activities).first(count)), "Homecoming retains native Chosen282 transport");
    check(!launch::manual_transport_valid(266, homecoming, std::span(activities).first(count)), "Homecoming direct266 cannot bypass staged Chosen contract");
    check(!launch::manual_transport_valid(0, value, std::span(activities).first(count)), "nonlaunchable transport rejected");
    check(launch::request_manual(78, value), "manual request accepted into idle mailbox");
    value.bubble = 0; value.sliceSet = 0; value.packageName[0] = 'x';
    const auto queued = launch::snapshot();
    check(queued.manual && queued.busy && queued.index == 78 && queued.destination.bubble == 13
        && queued.destination.sliceSet == 104 && launch::destination_name(queued.destination) == name,
        "render-to-game request is an immutable copied value");
    check(g_publishes == 0 && !launch::request(299), "enqueue never publishes override and busy request cannot replace it");
    launch::poll(); // This isolated executable has no native Destiny signatures; fail closed.
    check(launch::snapshot().status == launch::Status::nativeUnavailable && !launch::snapshot().busy
        && g_publishes == 0, "unavailable native launch cannot publish the manual override");
    check(launch::request(648), "ordinary request remains available after failed manual request");
    const auto ordinary = launch::snapshot();
    check(!ordinary.manual && ordinary.index == 648 && ordinary.destination.packageNameLength == 0,
        "ordinary request clears prior manual payload and retains exact variant identity");
    launch::poll();
    namespace openings = launch::openings;
    constexpr std::array<const char*, 12> packages{"mission_towerfall", "mission_abs", "adventure_ginger",
        "adventure_vod", "adventure_whisk", "mission_pact", "adventure_rumba", "mission_bond", "mission_scot", "strike_pact", "strike_bond", "mission_ember"};
    constexpr std::array<unsigned, 12> bubblesExpected{9, 15, 51, 15, 4, 15, 13, 15, 15, 15, 15, 8};
    constexpr std::array<unsigned, 12> slicesExpected{72, 120, 408, 120, 32, 120, 104, 120, 120, 120, 120, 64};
    constexpr std::array<std::uint32_t, 12> spawnsExpected{0, 0x69F52B3E, 0x43954D08, 0x26B11B02,
        0x3AE5AC33, 0x0E1523FE, 0x1BD69720, 0xB09FB979, 0x4AB3287A, 0x0E1523FE, 0xB09FB979, 0x2EA8FB98};
    constexpr std::array<unsigned,12> nativeIds{266,292,293,294,295,296,297,298,299,230,229,281};
    for (std::size_t i = 0; i < packages.size(); ++i) {
        check(launch::request_opening(i), "opening queues its own installed activity");
        const auto opening = launch::snapshot();
        check(opening.opening && opening.manual && opening.busy && opening.index == nativeIds[i]
            && launch::destination_name(opening.destination) == packages[i]
            && opening.destination.bubble == bubblesExpected[i]
            && opening.destination.sliceSet == slicesExpected[i]
            && opening.destination.spawnSetHash == spawnsExpected[i]
            && opening.destination.hasSpawnSetHash == (i != 0),
            "every campaign and strike button retains its own identity and opening");
        check(!launch::request_opening((i + 1) % packages.size()),
            "another mission cannot overwrite an in-flight opening");
        check(g_clears == 0 && g_publishes == 0 && g_dialogueSelections == 0, "unavailable launch cannot change override or native dialogue policy");
        launch::poll();
        check(launch::snapshot().status == launch::Status::nativeUnavailable
            && g_clears == 0 && g_publishes == 0,
            "failed native validation preserves the previous override for every opening");
    }
    check(!launch::request_opening(packages.size()), "unknown opening cannot queue");
    namespace strikes = dawn::state::activity::strikes;
    for (const auto& variant : strikes::kVariants) {
        const auto mission = variant.package == "strike_pact" ? 9U : 10U;
        check(launch::request_variant(mission, variant.difficulty), "each installed strike variant queues");
        check(launch::snapshot().index == variant.activity && launch::snapshot().opening,
            "Nightfall keeps exact native difficulty index and opening policy");
        check(!launch::request_variant(mission, strikes::Difficulty::standard),
            "variant cannot overwrite a queued request");
        launch::poll();
        check(!launch::snapshot().busy && g_publishes == 0,
            "failed native variant validation does not mutate the live override");
        auto& native = activities[variant.activity];
        const auto saved = native;
        native.hash ^= 1;
        check(!openings::resolve(mission, std::span(activities).first(count), variant.difficulty).valid(),
            "same package with wrong difficulty hash fails closed");
        native = saved;
    }
    check(!launch::request_variant(5, strikes::Difficulty::grandmaster), "campaign cannot borrow strike Nightfall difficulty");
    check(!launch::request_variant(9, static_cast<strikes::Difficulty>(255)), "invalid difficulty is rejected");
    check(!openings::resolve(0, {}).valid(), "empty activity catalog fails closed");
    auto donor = activities[266];
    activities[282].package[0] = 'x';
    check(openings::resolve(0, std::span(activities).first(count)).valid(), "Chosen is not a launch dependency");
    activities[266].package[0] = 'x';
    check(!openings::resolve(0, std::span(activities).first(count)).valid(), "wrong native activity package fails closed");
    activities[266] = donor;
    activities[266].index = 299;
    check(!openings::resolve(0, std::span(activities).first(count)).valid(), "wrong native activity ordinal fails closed");
    std::cout << "PASS: " << g_checks << " installed-scenario/manual argument, dependent-membership, immutable request and native-unavailable checks\n";
}
