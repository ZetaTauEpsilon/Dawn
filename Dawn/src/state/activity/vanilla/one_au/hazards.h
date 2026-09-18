#pragma once
#include "native_catalog.h"

namespace dawn::state::activity::vanilla::one_au::hazards {
// Archive cinder.lua and apex.lua, through native 8080956A/8080954B authority.
enum class Mode : std::uint8_t { off, climb, escape };
struct State {bool deck{},grinder{},burn{};Mode core{};std::uint32_t deckRevision{},coreRevision{},grinderRevision{},burnRevision{};};
inline constexpr auto kDeck=asset(0x8162BF78,26,1),kDeckFilter=asset(0x8162BF78,34,8);
// 80BEB26F places 80C1D9E0, whose native resources include thermal damage.
inline constexpr auto kSunDamage=asset(0x8162BF78,26,2);
inline constexpr auto kThermal=asset(0xDA326D5F,26,0),kPipes=asset(0xDA326D5F,34,2),kRails=asset(0xA3B76C64,34,112);
inline constexpr coo::Asset kPipeVolumes[]{volume(0xDA326D5F,3),volume(0xDA326D5F,4),
    volume(0xDA326D5F,5),volume(0xDA326D5F,6),volume(0xDA326D5F,8)};
inline constexpr coo::Asset kDeckVolume[]{volume(0x992B5554,118)},kRailVolume[]{volume(0xA3B76C64,414)};
inline constexpr auto kGrinder=asset(0x382608B7,26,14),kGrinderFilter=asset(0x382608B7,34,59);
inline constexpr auto kWeapons=asset(0x8162BF78,26,4),kAbilities=asset(0x8162BF78,26,5);
inline constexpr auto kInvincibility=asset(0x8162BF78,26,3);
inline constexpr auto kWeaponsFilter=asset(0x8162BF78,34,17),kAbilitiesFilter=asset(0x8162BF78,34,18);
inline constexpr coo::Asset kGrinderVolume[]{volume(0x382608B7,141)},kChuteVolume[]{volume(0x57627106,116)};
constexpr bool valid() noexcept {
    if(!kDeck.registry || !kThermal.registry || !kDeckFilter.registry || !kPipes.registry || !kRails.registry) {return false;}
    for(auto a:kPipeVolumes) {if(!a.registry) {return false;}}
    return kDeckVolume[0].registry!=0 && kRailVolume[0].registry!=0;
}
static_assert(valid());
constexpr std::size_t bits(const State& s,coo::Asset a) noexcept {
    if(a==kSunDamage) {return s.burnRevision?186:0;}
    if(a==kGrinder) {return s.grinderRevision?186:0;}
    if(a==kGrinderFilter) {return s.grinderRevision?130:0;}
    if(a==kWeapons || a==kAbilities || a==kInvincibility) {return s.deckRevision?186:0;}
    if(a==kWeaponsFilter || a==kAbilitiesFilter) {return s.deckRevision?130:0;}
    if(a==kDeck) {return s.deckRevision?186:0;}
    if(a==kThermal) {return s.coreRevision?186:0;}
    if(a==kDeckFilter) {return s.deckRevision?130:0;}
    if(a==kPipes) {return s.coreRevision?494:0;}
    if(a==kRails) {return s.coreRevision?130:0;}
    return 0;
}
template<class W> bool reference(W& w,coo::Asset a) noexcept {
    return w.write(a.registry,32) && w.write(a.type+1U,7) && w.write(32768U+a.slot,16);
}
template<class W> bool filter(W& w,std::span<const coo::Asset> volumes,bool any) noexcept {
    if(volumes.empty() || volumes.size()>5 || (!any && volumes.size()!=1) || !w.write(volumes.size()+1,4)) {return false;}
    const auto players=[&] {return w.write(1,1) && w.write(0x8080957DU,32) && w.write(any?2U:1U,2);};
    if(!any && !players()) {return false;}
    for(auto v:volumes) {
        if(!v.registry || v.type!=60 || !w.write(1,1) || !w.write(0x80809576U,32)
            || !w.write(any?1U:2U,2) || !w.write(any?1U:0U,1) || !reference(w,v)) {return false;}
    }
    return !any || players();
}
template<class W> bool effect(W& w,coo::Asset filter,bool enabled,std::uint32_t revision) noexcept {
    if(!revision || revision>=0x7FFFFFFFU || (enabled && filter.type!=34)) {return false;}
    if(!w.write(0,1) || !w.write(enabled?0U:1U,1)) {return false;}
    for(unsigned i=0;i<3;++i) {if(!w.write(0x80000000U,32)) {return false;}}
    if(!w.write(revision^0x80000000U,32)) {return false;}
    if(enabled) {if(!reference(w,filter)) {return false;}}
    else if(!w.write(0x811C9DC5U,32) || !w.write(0,7) || !w.write(0x7FFF,16)) {return false;}
    return w.write(0,1);
}
template<class W> bool write(W& w,const State& s,coo::Asset a) noexcept {
    if(a==kSunDamage) {return effect(w,kDeckFilter,s.burn,s.burnRevision);}
    if(a==kGrinder) {return effect(w,kGrinderFilter,s.grinder,s.grinderRevision);}
    if(a==kGrinderFilter) {return filter(w,kGrinderVolume,false);}
    // The authored invincibility attachment protects the same continuous chute
    // corridor as weapon/ability suppression, including its launch and exit.
    if(a==kInvincibility) {return effect(w,kWeaponsFilter,s.deck,s.deckRevision);}
    if(a==kWeapons || a==kAbilities) {return effect(w,a==kWeapons?kWeaponsFilter:kAbilitiesFilter,s.deck,s.deckRevision);}
    if(a==kWeaponsFilter || a==kAbilitiesFilter) {return filter(w,kChuteVolume,false);}
    if(a==kDeck) {return effect(w,kDeckFilter,s.deck,s.deckRevision);}
    if(a==kThermal) {return effect(w,s.core==Mode::escape?kRails:kPipes,s.core!=Mode::off,s.coreRevision);}
    if(a==kDeckFilter) {return filter(w,kDeckVolume,false);}
    if(a==kPipes) {return filter(w,kPipeVolumes,true);}
    if(a==kRails) {return filter(w,kRailVolume,false);}
    return false;
}
}
