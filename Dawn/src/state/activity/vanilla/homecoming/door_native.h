#pragma once
#include "controller.h"

namespace dawn::state::activity::vanilla::homecoming {
// Audit V43: the bazaar door (7BA8F95D/23/9) animates open but its authored
// destruction state never changes, so its collision stays closed. The graph
// releases the door after the applied opening; the client then sets the physics
// component authored "state" to "destroyed" through the native named-state
// setter, once, on the verified live placement.
struct DoorRequest {
    coo::Generation owner{};
    std::uint32_t generation{};
    bool enabled() const noexcept { return owner.valid() && generation!=0; }
    friend bool operator==(const DoorRequest&,const DoorRequest&)=default;
};
inline DoorRequest door_request(coo::Generation owner,const Frame& frame) noexcept {
    const auto& door=frame.native[asset_index(kBazaarDoor)];
    if(!owner.valid() || !frame.enabled || frame.fault || frame.finished || !frame.doorReleased || frame.doorHandled
        || frame.section!=static_cast<std::uint8_t>(Section::boulevard)
        || !door.managed || !door.acknowledged || door.position!=1.F) { return {}; }
    return {owner,frame.spawnGeneration};
}
DoorRequest door_request() noexcept;
namespace door_native {
inline constexpr std::uintptr_t kEntities=0x1F93428,kEntityStride=0x1F93430,kSetter=0xA1FBB0;
inline constexpr std::size_t kRows=8192,kStride=0xE0;
inline constexpr std::array<unsigned char,16> kSetterPrefix{
    0x53,0x48,0x83,0xEC,0x20,0x8B,0x02,0x49,0x8B,0xD8,0x48,0x8D,0x54,0x24,0x38,0x89};
// FNV-1 32 of the lowercase names: "state", "destroyed", "undamaged".
inline constexpr std::uint32_t kStateName=0x5266EA90U,kDestroyed=0x51033A3AU,kUndamaged=0x3E7E5B25U;
// D_DOOR_BAZAAR placement: list 80B505B2 entry 15, entity asset 80B71B7C, physics
// component definition 80C4A498 (kind 80808A0C).
inline constexpr std::uint32_t kList=0x80B505B2U,kRecord=15,kComponentDefinition=0x80C4A498U,kComponentKind=0x80808A0CU;
inline constexpr std::uint64_t kAuthored=0x7B8AC80734FA725FULL;
struct Row {
    std::uint32_t flags{},entity{UINT32_MAX},bundle{UINT32_MAX},table{},record{};
    std::uint64_t authored{};
    bool live(std::size_t slot) const noexcept {
        return !(flags&5U) && entity!=UINT32_MAX && (entity&0x1FFFU)==slot;
    }
    friend bool operator==(const Row&,const Row&)=default;
};
inline bool placement(const Row& row) noexcept {return row.table==kList && row.record==kRecord && row.authored==kAuthored;}
}
}
