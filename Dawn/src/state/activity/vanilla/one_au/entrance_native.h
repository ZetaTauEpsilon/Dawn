#pragma once
#include "bridge_scan.h"

namespace dawn::state::activity::vanilla::one_au {
struct EntranceRequest {
    coo::Generation owner{};
    std::uint32_t generation{};
    std::uint8_t section{};
    bool enabled() const noexcept { return owner.valid() && generation && section<=static_cast<std::uint8_t>(Section::processing); }
    friend bool operator==(const EntranceRequest&,const EntranceRequest&)=default;
};
inline EntranceRequest entrance_request(coo::Generation owner,const Frame& frame) noexcept {
    // Restricted only controls the Darkness Zone presentation. It must not
    // suppress either repair, including after entering the processing room.
    if(!frame.enabled || frame.finished || frame.fault || frame.recovery.holding()) return {};
    return {owner,frame.spawnGeneration,frame.section};
}
EntranceRequest entrance_request() noexcept;

namespace entrance_native {
inline constexpr std::size_t kRows=8192,kStride=0xE0;
inline constexpr std::uintptr_t kEntities=0x1F93428,kEntityStride=0x1F93430,kRetire=0x56A8F0;
inline constexpr std::array<unsigned char,16> kRetirePrefix{
    0x48,0x89,0x5C,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57,0x48,0x83,0xEC,0x40,0x48};
struct Row {
    std::uint32_t flags{},entity{UINT32_MAX},bundle{UINT32_MAX},table{},record{};
    std::uint64_t authored{};
    bool live(std::size_t slot) const noexcept {
        return !(flags&5U) && entity!=UINT32_MAX && (entity&0x1FFFU)==slot;
    }
    friend bool operator==(const Row&,const Row&)=default;
};
inline bool door(const Row& row) noexcept {
    return row.table==0x80C32CEEU && ((row.record==0 && row.authored==0x30DB525724EDDB9FULL)
        || (row.record==1 && row.authored==0xAB48F4FA0B44B151ULL));
}
inline bool bridge(const Row& row) noexcept {
    if(row.table!=0x80F0C055U) return false;
    switch(row.record) {case 91:case 92:case 93:case 102:case 108:case 124:return true;default:return false;}
}
inline bool same_placement(const Row& a,const Row& b) noexcept {
    return a.table==b.table && a.record==b.record && a.authored==b.authored;
}
template<class World> bool unchanged(World& world,const Row& row) noexcept {
    Row fresh{};
    return world.stable() && world.row(row.entity&0x1FFFU,fresh) && fresh==row;
}
struct Result {
    unsigned doors{};std::uint32_t retired{UINT32_MAX};
    unsigned rows{},ownedBridges{},unownedBridges{},incomplete{},callbackSelf{},matches{};
};
// Invoked within the existing device callback's lifetime, never on a timer.
// World supplies checked native reads and engine calls; tests use synthetic rows.
template<class World,class Current>
Result repair(World& world,std::uint32_t callbackEntity,const EntranceRequest& wanted,Current&& current) noexcept {
    Result result{};
    if(!wanted.enabled()) return result;
    for(std::size_t slot=0;slot<kRows;++slot) {
        Row row{};bool owned{};
        if(!world.row(slot,row)) continue;
        ++result.rows;
        if(!row.live(slot) || (!door(row) && !bridge(row)) || !world.owned(row.entity,owned)) continue;
        if(bridge(row)) { if(owned) ++result.ownedBridges;else ++result.unownedBridges; }
        if(owned) continue;
        if(door(row)) {
            if(wanted.section==static_cast<std::uint8_t>(Section::landing)) continue;
            if(!unchanged(world,row) || !world.owned(row.entity,owned) || owned || current()!=wanted) continue;
            // Do not write device position, locks, power, proximity or animation.
            world.grant(row.entity);++result.doors;continue;
        }
        // The reference checks the bundle only on the entity passed to retirement.
        // Doors and the owned matching placement do not dereference their bundles.
        if(row.bundle==UINT32_MAX) {++result.incomplete;continue;}
        if(row.entity==callbackEntity) {++result.callbackSelf;continue;}
        for(std::size_t other=0;other<kRows;++other) {
            Row owner{};bool ownerLocal{};
            if(other==slot || !world.row(other,owner) || !owner.live(other)
                || !same_placement(row,owner) || !world.owned(owner.entity,ownerLocal) || !ownerLocal) continue;
            ++result.matches;
            // 56A8F0 dereferences the candidate's bundle. Revalidate both salted
            // rows, ownership and the mission request immediately before it.
            if(!unchanged(world,row) || !unchanged(world,owner)
                || !world.owned(owner.entity,ownerLocal) || !ownerLocal
                || !world.owned(row.entity,owned) || owned || current()!=wanted) break;
            world.retire(row.entity);result.retired=row.entity;
            return result; // At most one retirement per callback, including reentry.
        }
    }
    return result;
}
}
}
