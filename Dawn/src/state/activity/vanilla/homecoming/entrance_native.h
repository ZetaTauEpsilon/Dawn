#pragma once
#include "controller.h"

namespace dawn::state::activity::vanilla::homecoming {
struct EntranceRequest {
    coo::Generation owner{};
    std::uint32_t generation{};
    std::uint8_t section{};
    bool enabled() const noexcept { return owner.valid() && generation!=0; }
    friend bool operator==(const EntranceRequest&,const EntranceRequest&)=default;
};
inline EntranceRequest entrance_request(coo::Generation owner,const Frame& frame) noexcept {
    if(!frame.enabled || frame.finished || frame.fault || frame.cinematic.phase!=cinematics::Phase::gameplay) return {};
    return {owner,frame.spawnGeneration,frame.section};
}
EntranceRequest entrance_request() noexcept;

// Audit V30/V31: the hangar gates, the pod doors, the command ship enter/exit
// doors, the contact doors and the Cabal console entities spawn without local
// authority in the client-hosted world, so the native device controllers ignore
// the published position channel. Grant the verified engine setter once per
// live placement, exactly like the 1AU entrance repair. No positions are written.
namespace entrance_native {
inline constexpr std::size_t kRows=8192,kStride=0xE0;
inline constexpr std::uintptr_t kEntities=0x1F93428,kEntityStride=0x1F93430;
struct Row {
    std::uint32_t flags{},entity{UINT32_MAX},bundle{UINT32_MAX},table{},record{};
    std::uint64_t authored{};
    bool live(std::size_t slot) const noexcept {
        return !(flags&5U) && entity!=UINT32_MAX && (entity&0x1FFFU)==slot;
    }
    friend bool operator==(const Row&,const Row&)=default;
};
struct Placement {std::uint32_t table,record;std::uint64_t authored;std::uint8_t section;};
// Entity placement lists by section: military gates (80F10442 entries 1/0) and the
// command ship list 80C3B4B7 (pod doors 25/26, enter 18, exit 19, contact door 22
// and the console 101).
inline constexpr Placement kPlacements[]{
    {0x80F10442U,1,0x125D2DA09F611701ULL,static_cast<std::uint8_t>(Section::military)},
    {0x80F10442U,0,0x690A3243C7E62499ULL,static_cast<std::uint8_t>(Section::military)},
    {0x80C3B4B7U,25,0xD8FFC2F1F8979C65ULL,static_cast<std::uint8_t>(Section::ship)},
    {0x80C3B4B7U,26,0x1EB7FCA5F866CD3AULL,static_cast<std::uint8_t>(Section::ship)},
    {0x80C3B4B7U,18,0x58951F29F7F93914ULL,static_cast<std::uint8_t>(Section::ship)},
    {0x80C3B4B7U,19,0x6D5ADA9032ADC4DAULL,static_cast<std::uint8_t>(Section::ship)},
    {0x80C3B4B7U,101,0x943C4D1493431230ULL,static_cast<std::uint8_t>(Section::ship)},
    {0x80C3B4B7U,22,0x82C228E55EC5F950ULL,static_cast<std::uint8_t>(Section::ship)},
};
// Contact doors 21/23/24/28 carry no recorded authored GUID; match them by list entry.
inline constexpr std::uint32_t kContactRecords[]{21,23,24,28};
inline constexpr std::uint32_t kShipList=0x80C3B4B7U;
inline bool door(const Row& row,std::uint8_t section) noexcept {
    for(const auto& p:kPlacements) {
        if(row.table==p.table && row.record==p.record && row.authored==p.authored) {
            return section>=p.section;
        }
    }
    if(row.table==kShipList && section>=static_cast<std::uint8_t>(Section::ship)) {
        for(const auto record:kContactRecords) {if(row.record==record) {return true;}}
    }
    return false;
}
template<class World> bool unchanged(World& world,const Row& row) noexcept {
    Row fresh{};
    return world.stable() && world.row(row.entity&0x1FFFU,fresh) && fresh==row;
}
struct Result {unsigned doors{},rows{},matches{};};
inline bool eligible(const EntranceRequest& wanted) noexcept {
    return wanted.enabled() && wanted.section>=static_cast<std::uint8_t>(Section::military);
}
// Callback-local repair is immediate. The fallback discovers streaming devices
// that have not ticked yet, at most twice per second for an unchanged world.
// This schedules discovery only; it never advances a mission or a door animation.
struct SweepGate {
    static constexpr std::uint64_t kIntervalMs=500;
    EntranceRequest request{};
    std::uintptr_t table{};
    std::uint64_t last{};
    bool primed{};
    void reset() noexcept {*this={};}
    bool due(const EntranceRequest& wanted,std::uintptr_t currentTable,std::uint64_t now) noexcept {
        if(!eligible(wanted) || !currentTable) {reset();return false;}
        if(primed && request==wanted && table==currentTable && now>=last && now-last<kIntervalMs) return false;
        request=wanted;table=currentTable;last=now;primed=true;return true;
    }
};
template<class World,class Current>
void repair_row(World& world,const EntranceRequest& wanted,const Row& row,Current&& current,Result& result) noexcept {
    bool owned{};
    if(!row.live(row.entity&0x1FFFU) || !door(row,wanted.section) || !world.owned(row.entity,owned)) return;
    ++result.matches;
    if(owned || !unchanged(world,row) || !world.owned(row.entity,owned) || owned || current()!=wanted) return;
    // Do not write device position, locks, power, proximity or animation.
    world.grant(row.entity);++result.doors;
}
template<class World,class Current>
Result repair_callback(World& world,const EntranceRequest& wanted,std::uint32_t entity,Current&& current) noexcept {
    Result result{};Row row{};
    if(!eligible(wanted) || entity==UINT32_MAX || !world.row(entity&0x1FFFU,row)) return result;
    ++result.rows;
    // A recycled slot must not redirect a callback to a different live entity.
    if(row.entity==entity) repair_row(world,wanted,row,current,result);
    return result;
}
// Invoked within the existing device callback lifetime, never on a timer.
// World supplies checked native reads and engine calls; tests use synthetic rows.
template<class World,class Current>
Result repair(World& world,const EntranceRequest& wanted,Current&& current) noexcept {
    Result result{};
    if(!eligible(wanted)) return result;
    for(std::size_t slot=0;slot<kRows;++slot) {
        Row row{};
        if(!world.row(slot,row)) continue;
        ++result.rows;
        if(row.live(slot)) repair_row(world,wanted,row,current,result);
    }
    return result;
}
}
}
