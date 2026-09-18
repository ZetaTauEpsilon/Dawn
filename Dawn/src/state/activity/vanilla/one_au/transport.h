#pragma once
#include "mission.h"
#include "../../../../middleware/bap/activity_message/actor_sense.h"
#include <array>
namespace dawn::state::activity::vanilla::one_au::transport {
using Delta=middleware::bap::activity_message::actor_sense::Delta;
inline constexpr std::array<std::uint16_t,4> kParents{21,27,29,31},kMembers{22,28,30,32},kEntries{115,117,119,121};
inline constexpr std::array<std::uint16_t,4> kCargo{23,24,25,26};
// The two vignette squads are authored at the near end of the bridge deck, which
// is empty air until the console extends it. They ride down with the harvesters,
// so they belong on the manifest rather than being placed as a ground cohort.
inline constexpr std::array<std::uint16_t,2> kCarried{38,39};
constexpr int member(std::uint32_t registry,std::uint8_t type,std::uint16_t slot) noexcept {
    if(registry!=kBridge || type!=2) {return -1;}
    for(int i=0;i<4;++i) {if(kMembers[i]==slot) {return i;}}return -1;
}
constexpr bool parent(coo::Asset a) noexcept {if(a.registry!=kBridge || a.type!=1) {return false;}for(auto n:kParents) {if(a.slot==n) {return true;}}return false;}
constexpr bool cargo(coo::Asset a) noexcept {
    if(a.registry!=kBridge || a.type!=1) {return false;}
    if(a.slot>=23 && a.slot<=26) {return true;}
    for(auto n:kCarried) {if(a.slot==n) {return true;}}
    return false;
}
enum class Phase : std::uint8_t { dormant,entry,settle,delivery,hover,exit,action,retired };
struct Ship {
    Phase phase{};std::uint64_t deadline{};std::uint32_t generation{},programRevision{},programState{},deliveryRevision{};
    bool hasGeneration{},hasProgramRevision{},hasProgramState{},hasDeliveryRevision{},dead{};
    std::int8_t deliveryState{-1};
};
struct State {std::uint32_t authorityGeneration{};bool prepared{},started{};std::array<Ship,4> ships{};};
inline bool observe(Ship& s,const Delta& d,std::uint32_t generation,std::uint64_t now) noexcept {
    if(s.phase==Phase::dormant || s.phase==Phase::retired || (d.hasGeneration && d.generation!=generation)) {return false;}
    if(d.hasGeneration) {s.generation=d.generation;s.hasGeneration=true;}
    if(!s.hasGeneration || s.generation!=generation) {return false;}
    if(d.hasProgramRevision) {
        if(!s.hasProgramRevision || s.programRevision!=d.programRevision) {s.hasProgramState=false;}
        s.programRevision=d.programRevision;s.hasProgramRevision=true;
    }
    if(d.hasProgramState) {s.programState=d.programState;s.hasProgramState=true;}
    if(d.hasDeliveryRevision) {s.deliveryRevision=d.deliveryRevision;s.hasDeliveryRevision=true;}
    s.deliveryState=d.deliveryState;s.dead=d.dead;
    const auto old=s.phase;
    if(s.dead) {s.phase=Phase::retired;}
    else if(s.hasProgramRevision && s.hasProgramState && s.programState==1) {
        if(s.phase==Phase::entry && s.programRevision==1) {s.phase=Phase::settle;s.deadline=now+6000;}
        else if(s.phase==Phase::exit && s.programRevision==2) {s.phase=Phase::action;}
        else if(s.phase==Phase::action && s.programRevision==3) {s.phase=Phase::retired;}
    }
    if(s.phase==Phase::delivery && s.hasDeliveryRevision && s.deliveryRevision==1 && s.deliveryState==0) {s.phase=Phase::hover;s.deadline=now+4000;}
    return s.phase!=old;
}
inline bool advance(State& s,std::size_t i,bool cargoAdmitted,std::uint64_t now) noexcept {
    auto& ship=s.ships[i];const auto old=ship.phase;
    if(ship.phase==Phase::settle && now>=ship.deadline) {
        ship.phase=i<2?Phase::delivery:Phase::hover;ship.hasDeliveryRevision=false;ship.deadline=now+4000;
    }
    if(ship.phase==Phase::hover && cargoAdmitted && now>=ship.deadline) {ship.phase=Phase::exit;}
    return ship.phase!=old;
}
constexpr std::uint32_t program(Phase p) noexcept {return p==Phase::action?3:p==Phase::exit?2:1;}
/** Manifest: a four-bit count, one 55-bit reference per carried squad, and a generation. */
inline constexpr std::size_t kManifestReferences=3;
inline constexpr std::size_t kManifestBits=4+55*kManifestReferences+31;
constexpr std::size_t bits(Phase p,std::size_t i) noexcept {
    if(p==Phase::dormant) {return 0;}if(p==Phase::retired) {return 77;}
    return 156+202+(p==Phase::action?98:0)+(i<2 && p>=Phase::delivery?kManifestBits:0);
}
template<class W> bool write(W& w,const Ship& s,std::size_t i,std::uint32_t generation) noexcept {
    if(i>=4 || !generation || generation>=0x7FFFFFFF || s.phase==Phase::dormant) {return false;}
    const bool retired=s.phase==Phase::retired;
    if(!w.write(1,1) || !w.write(generation+(retired?1U:0U),31) || !w.write(1,2) || !w.write(1,3) || !w.write(retired?0U:1U,1) || !w.write(0,1)) {return false;}
    if(retired) {return w.write(0,1) && w.write(0,1) && w.write(1,1) && w.write(0,4) && w.write(generation+1,31);}
    const auto absent=[&] {return w.write(0x811C9DC5U,32) && w.write(0,7) && w.write(32767,16);};
    // Retain the native door channel with the movement program and manifest.
    // Only the two cargo-bearing ships have a manifest to drop, so the other
    // two hold their doors shut through the same hover phase.
    const bool open=i<2 && (s.phase==Phase::delivery || s.phase==Phase::hover);
    if(!w.write(1,1) || !w.write(s.phase>=Phase::exit?3:open?2:1,31) || !w.write(0,6) || !w.write(0,6)
        || !w.write(0,3) || !absent() || !w.write(0,32) || !w.write(1,5)
        || !w.write(0x80296344U,32) || !w.write(open?0x3F800000U:0U,32)
        || !w.write(1,1) || !w.write(program(s.phase),31) || !w.write(0,6) || !w.write(1,6)
        || !w.write(1,1) || !w.write(s.phase==Phase::action?10:4,4) || !w.write(1,2)) {return false;}
    if(s.phase==Phase::action) {
        if(!w.write(0x07EBF354U,32) || !w.write(0x7B0D3643U,32) || !w.write(0x811C9DC5U,32)
            || !absent() || !w.write(0,3) || !w.write(127,8)) {return false;}
    } else if(!w.write(kBridge,32) || !w.write(59,7) || !w.write(32768U+kEntries[i]+(s.phase==Phase::exit?1U:0U),16)
        || !w.write(1,8) || !w.write(1,1)) {return false;}
    const bool delivered=i<2 && s.phase>=Phase::delivery;
    if(!w.write(delivered?1U:0U,1)) {return false;}
    if(delivered) {
        if(!w.write(kManifestReferences,4)) {return false;}
        for(const auto slot:{kCargo[i],kCargo[i+2],kCarried[i]}) {if(!w.write(kBridge,32) || !w.write(2,7) || !w.write(32768U+slot,16)) {return false;}}
        if(!w.write(1,31)) {return false;}
    }
    return true;
}
}
