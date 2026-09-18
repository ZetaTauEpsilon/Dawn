#pragma once
#include "controller.h"
#include "device_authority.h"
#include "interactions.h"
#include "music.h"
#include "native_presentation_authority.h"
#include "native_combatant_authority.h"
#include "../../coo/native_device_authority.h"
#include "../../coo/native_scene_cast_authority.h"
#include "../../coo/native_scene_authority.h"
namespace dawn::state::activity::vanilla::one_au {
// 8080626B's reference array is a source-retirement list, not the full cast.
// B3F620 passes every entry to the enemy-source consumer at 4E83B0. A type-48
// marker resolves successfully but faults there when interpreted as a source.
// The native Scene definition retains the complete, ordered graph parameters.
struct SceneSources {
    std::array<coo::Asset,15> values{};std::size_t count{};
    std::span<const coo::Asset> span() const noexcept {return std::span(values).first(count);}
};
constexpr SceneSources scene_sources(const Scene& scene) noexcept {
    SceneSources out{};
    for(const auto& member:scene.cast) {if(member.type==1) {out.values[out.count++]=member;}}
    return out;
}
static_assert([] {for(const auto& scene:kScenes) {if(scene.cast.size()>15) {return false;}}return true;}());
inline std::size_t body_bits(const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(f.cinematic.owner.valid() && cinematics::index(key,type,slot)>=0) {return 263;}
    if(!f.enabled || !f.spawnGeneration) {return 0;}const auto* a=find(key,type,slot);if(!a) {return 0;}
    if(const auto i=transport::member(key,type,slot);i>=0) {return transport::bits(f.transport.ships[static_cast<std::size_t>(i)].phase,static_cast<std::size_t>(i));}
    if(const auto n=hazards::bits(f.hazards,a->asset)) {return n;}
    if(a->asset==asset(0x4786C0E0,35,1)) {return 359;}
    if(a->asset==music::kSensor) {return music::kBits;}
    if(a->asset==kDialogueAsset) {return native_presentation::kDialogueBits+(f.activeRow==coo::kNoDialogue?0U:64U);}
    if(a->asset==kDirectiveAsset && f.presentation.published) {return native_presentation::kDirectiveBits;}
    if(a->asset==kBridgeLink) {return 65;}
    if(type==3 && a->authority==0x80807F0CU) {return 225;}
    if(type==1) {const auto i=spawn_index(a->asset);if(i<kSpawns.size() && kSpawns[i].sceneOwned) {return 641;}}
    const auto& s=f.native[asset_index(a->asset)];if(!s.managed) {return 0;}
    if(type==1) {const auto i=spawn_index(a->asset);return i<kSpawns.size()?641U+32U*(kSpawns[i].categories-1U):0;}
    if(type==5 && a->authority==0x80804F04U) {return 7359;}
    if(type==4 && a->authority==0x8080992FU) {return s.prepared && use_subscription(a->asset)?375:252;}
    if(type==23 && a->authority==0x80804F48U) {return 147;}
    if(type==43 && a->authority==0x8080626BU) {for(const auto& scene:kScenes) {if(scene.asset==a->asset) {return s.active?coo::native_scene::cast_bits(scene_sources(scene).count,slot==20?f.explosionCount:0):74;}}}
    return 0;
}
template<class Writer> bool write_body(Writer& w,const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!body_bits(f,key,type,slot)) {return false;}
    if(const auto i=cinematics::index(key,type,slot);i>=0) {return cinematics::write(w,f.cinematic,static_cast<std::size_t>(i));}
    if(const auto i=transport::member(key,type,slot);i>=0) {return transport::write(w,f.transport.ships[static_cast<std::size_t>(i)],static_cast<std::size_t>(i),f.transport.authorityGeneration);}
    const auto a=find(key,type,slot)->asset;
    if(hazards::bits(f.hazards,a)) {return hazards::write(w,f.hazards,a);}
    if(a==music::kSensor) {return music::write(w,f);}
    if(a==kDialogueAsset) {return native_presentation::dialogue(w,f.generations,f.activeRow);}
    if(a==kDirectiveAsset) {
        const auto elapsed=coo::native_activity_ticks((std::min)(f.escapeElapsed,std::uint64_t{60000}));
        const auto remaining=coo::native_activity_ticks(60000)-elapsed;
        // 808099C4: min, max, sampled elapsed, sampled remaining, sample clock.
        // 4C8FF0 subtracts time since that clock. A zero sample time spends the
        // entire mission age immediately; equal min/max also prevents interpolation.
        const native_presentation::TimedState time{{0,coo::native_activity_ticks(60000),elapsed,remaining,f.gameplayClockTicks},true,0x3F800000U};
        return native_presentation::objective(w,f.presentation,f.escapeClock?&time:nullptr,true);
    }
    if(a==asset(0x4786C0E0,35,1)) {
        const bool wipe=f.recovery.phase==recovery::Phase::countdown;
        const auto remaining=coo::native_activity_ticks((std::min)(f.wipeRemaining,recovery::kCountdownMs));
        const auto elapsed=coo::native_activity_ticks(recovery::kCountdownMs)-remaining;
        return w.write(f.restricted?1U:0U,1) && w.write(0,1) && w.write(1,2) && w.write(wipe?1U:0U,2)
            && w.write(wipe?1U:0U,1) && w.write(0,64) && w.write(wipe?coo::native_activity_ticks(recovery::kCountdownMs):0x134F00C00000ULL,64)
            && w.write(wipe?elapsed:0,64) && w.write(wipe?remaining:0,64) && w.write(wipe?f.gameplayClockTicks:UINT64_MAX,64) && w.write(0x3F800000U,32);
    }
    if(a==kBridgeLink) {return w.write(0x80000000U+f.spawnGeneration+1U,32)
        && w.write(f.interactions[0].armed && !f.interactions[0].completed?1U:0U,1) && w.write(0x811C9DC5U,32);}
    const auto& s=f.native[asset_index(a)];
    if(type==3) {
        if(!w.write(1,1)) {return false;}
        for(unsigned i=0;i<24;++i) {if(!w.write(1,1) || !w.write(0,7)) {return false;}}
        return w.write(1,1) && w.write(f.spawnGeneration,31);
    }
    if(type==1) {
        const auto& p=kSpawns[spawn_index(a)];
        if(p.sceneOwned) {return coo::native_scene::write_source(w,s.generation?s.generation:f.spawnGeneration,s.active && !s.sourceCleared);}
        const auto n=static_cast<std::uint8_t>(s.active && !s.sourceCleared && !transport::parent(a)?1:0);
        auto task=transport::parent(a)?coo::native_combatant::TacticalGroup{}:tactical(p);
        if(task.registry && f.tactics[spawn_index(a)].revision==s.generation) {task.row=f.tactics[spawn_index(a)].group;}
        const auto* entrance=refinery::entrance(p);
        return native_combatant::write_source(w,{key,s.generation,entrance?entrance->rule:p.rule,n,task,
            static_cast<std::uint8_t>(p.categories>=2?n:0),p.categories>=2,p.hasRule,transport::parent(a),
            static_cast<std::uint8_t>(p.categories==3?n:0),p.categories==3,transport::cargo(a),
            task.registry?s.generation:0U,!transport::parent(a),entrance!=nullptr});
    }
    if(type==5) {
        if((s.active && (!s.sequenceRevision || s.sequenceRevision==255)) || !w.write(0,64) || !w.write(0,64)
            || !w.write(s.active?s.sequenceRevision:255U,8)) {return false;}
        for(unsigned i=0;i<4;++i) {if(!w.write(0,32)) {return false;}}
        for(unsigned i=0;i<129;++i) {if(!native_presentation::absent(w)) {return false;}}
        return true;
    }
    if(type==4) {return s.prepared && use_subscription(a)?interactable(w,s.generation,s.active):coo::native_device::object(w,s.generation,s.active);}
    if(type==23) {return device_authority(w,s.position,s.power,s.lock,s.generation,s.snap);}
    if(type==43 && !s.active) {return coo::native_scene::cast_scene(w,0,{});}
    if(type==43) {for(const auto& scene:kScenes) {if(scene.asset==a) {return coo::native_scene::cast_scene(w,s.generation,scene_sources(scene).span(),slot==20?std::span(f.explosionInputs).first(f.explosionCount):std::span<const std::uint32_t>{});}}}
    return false;
}
}
