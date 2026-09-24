#pragma once
#include "mission.h"
#include "../../../../middleware/bap/activity_message/device_sense.h"

namespace dawn::state::activity::vanilla::one_au::refinery {
// Type-23 sense reads the actual interpolated position at native device +0x370,
// not the commanded target at +0x37C (10693B0 -> 15E8CA0; DF6C70/DF7FF0).
// Keep optional value/revision deltas together. A revision alone can confirm an
// unchanged measured pose, but cannot turn a partly open door into a closed one.
struct Pose {
    float position{};
    std::int32_t revision{-1};
    bool known{};
    bool observe(const middleware::bap::activity_message::device_sense::Output& delta,
                 std::uint32_t generation, float target) noexcept {
        if ((delta.present & 2U) && delta.revisions[0] < revision) { return false; }
        if (delta.present & 2U) { revision = delta.revisions[0]; }
        if (delta.present & 1U) { position = delta.values[0]; known = true; }
        return known && revision >= 0 && static_cast<std::uint32_t>(revision) == generation && position == target;
    }
};

struct Entrance { std::uint16_t source, rule; };
// Native 66/99 is ceiling_both, 100 is entry_door_medium, 102 is hatches 1/2,
// and 103 is hatches 3/4. Retain the authored sources, templates and task groups;
// select their entrances through native spawn-rule references. The boss keeps
// the medium entry door. Wave gates open the required hatches before placement.
inline constexpr Entrance kEntrances[]{
    {44,99}, {45,99}, {48,102}, {54,100},
    {46,99}, {47,99}, {49,102}, {50,103}, {55,99},
    {52,100}, {51,103},
};
constexpr const Entrance* entrance(const Spawn& spawn) noexcept {
    if (spawn.registry == kProcessing) {
        for (const auto& entry : kEntrances) { if (entry.source == spawn.source) { return &entry; } }
    }
    return nullptr;
}
static_assert([] {
    for (const auto& entry : kEntrances) {
        if (!find(kProcessing,1,entry.source) || !find(kProcessing,66,entry.rule)) { return false; }
    }
    return true;
}());
}
