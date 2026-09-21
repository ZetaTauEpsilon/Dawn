#pragma once
#include "controller.h"
#include "device_authority.h"
#include "music.h"
#include "native_presentation_authority.h"
#include "native_combatant_authority.h"
#include "../../coo/native_combatant_authority.h"
#include "../../coo/native_device_authority.h"
#include "../../coo/native_scene_cast_authority.h"
#include "../../coo/native_scene_authority.h"
namespace dawn::state::activity::vanilla::homecoming {
// 8080626B's reference array is consumed by the enemy-source consumer at 4E83B0.
// Sources and objects (the Cayde scene's two doors) resolve there; a type-48
// marker faults, so the authored marker references stay out of the array. The
// native Scene definition retains the complete, ordered graph parameters.
struct SceneSources {
    std::array<coo::Asset,15> values{};std::size_t count{};
    std::span<const coo::Asset> span() const noexcept {return std::span(values).first(count);}
};
constexpr SceneSources scene_sources(const Scene& scene) noexcept {
    SceneSources out{};
    for(const auto& member:scene.cast) {
        if((member.type==1 || member.type==4) && out.count<out.values.size()) {out.values[out.count++]=member;}
    }
    return out;
}
static_assert([] {for(const auto& scene:kScenes) {std::size_t n{};for(const auto& m:scene.cast) {if(m.type==1 || m.type==4) {++n;}}if(n>15) {return false;}}return true;}());
// Native 8080992F with a 80804FB8 subscription: the dead Zavala revive prompt. The
// native controller owns the hold duration and accepted-use reply.
template<class W> bool interactable(W& w,std::uint32_t generation,bool active) noexcept {
    const auto absent=[&] {return w.write(0x811C9DC5U,32) && w.write(0,7) && w.write(0x7FFF,16);};
    return w.write(generation^0x80000000U,32) && w.write(0x80000000U,32)
        && w.write(active?1U:0U,1) && w.write(0,1) && w.write(0x80000000U,32)
        && absent() && w.write(0,32) && w.write(0,32) && w.write(0,32)
        && w.write(0,1) && w.write(1,2) && w.write(1,1) && w.write(0x80804FB8U,32)
        && w.write(0,2) && absent() && w.write(0x80000000U,32) && w.write(0,1);
}
inline std::size_t body_bits(const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(f.cinematic.owner.valid() && cinematics::index(key,type,slot)>=0) {return 263;}
    if(!f.enabled || !f.spawnGeneration) {return 0;}const auto* a=find(key,type,slot);if(!a) {return 0;}
    if(a->asset==asset(kRuntime,35,1)) {return 359;}
    if(a->asset==music::kSensor) {return music::kBits;}
    if(a->asset==kDialogueAsset) {return native_presentation::kDialogueBits+(f.activeRow==coo::kNoDialogue?0U:64U);}
    if(a->asset==kDirectiveAsset && f.presentation.published) {return native_presentation::kDirectiveBits;}
    if(a->asset==kConsoleLink) {return 65;}
    if(type==3 && a->authority==0x80807F0CU) {return 225;}
    if(type==1) {const auto i=spawn_index(a->asset);if(i<kSpawns.size() && kSpawns[i].sceneOwned) {return coo::native_combatant::authored_source_bits(kSpawns[i].categories,tactical(kSpawns[i]).registry!=0);}}
    const auto& s=f.native[asset_index(a->asset)];if(!s.managed) {return 0;}
    if(type==1) {const auto i=spawn_index(a->asset);return i<kSpawns.size()?native_combatant::source_bits(kSpawns[i].categories):0;}
    if(type==2) {return coo::native_combatant::kBindBits;}
    if(type==5 && a->authority==0x80804F04U) {return 7359;}
    if(type==4 && a->authority==0x8080992FU) {return s.prepared && use_subscription(a->asset)?375:252;}
    if(type==23 && a->authority==0x80804F48U) {return 147;}
    if(type==43 && a->authority==0x8080626BU) {
        const auto i=scene_index(a->asset);
        if(i<std::size(kScenes)) {return s.active?coo::native_scene::cast_bits(scene_sources(kScenes[i]).count,f.scenes[i].count):74;}
    }
    return 0;
}
template<class Writer> bool write_body(Writer& w,const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!body_bits(f,key,type,slot)) {return false;}
    if(const auto i=cinematics::index(key,type,slot);i>=0) {return cinematics::write(w,f.cinematic,static_cast<std::size_t>(i));}
    const auto a=find(key,type,slot)->asset;
    if(a==music::kSensor) {return music::write(w,f);}
    if(a==kDialogueAsset) {return native_presentation::dialogue(w,f.generations,f.activeRow);}
    if(a==kDirectiveAsset) {
        const auto progress=f.presentation.event==kObjectives[7]?f.assaultsRepelled:
            f.presentation.event==kObjectives[11]?std::count(f.generatorDown.begin(),f.generatorDown.end(),true):0;
        const bool counted=f.presentation.event==kObjectives[7] || f.presentation.event==kObjectives[11];
        // 80B508FE/F0D48F30 value 0 is description-only; value 1 carries
        // "Exhaust turbines destroyed" and the native counter flag.
        const auto variant=f.presentation.event==kObjectives[11]?1U:0U;
        return native_presentation::objective(w,f.presentation,nullptr,true,static_cast<std::uint32_t>(progress),counted?3U:0U,variant);
    }
    if(a==asset(kRuntime,35,1)) {
        // 808099C4 shared director: Darkness Zone presentation only; no wipe countdown.
        return w.write(f.restricted?1U:0U,1) && w.write(0,1) && w.write(1,2) && w.write(0,2)
            && w.write(0,1) && w.write(0,64) && w.write(0x134F00C00000ULL,64)
            && w.write(0,64) && w.write(0,64) && w.write(UINT64_MAX,64) && w.write(0x3F800000U,32);
    }
    if(a==kConsoleLink) {return w.write(0x80000000U+f.spawnGeneration+1U,32)
        && w.write(f.consoleArmed && !f.consoleScanned?1U:0U,1) && w.write(0x811C9DC5U,32);}
    const auto& s=f.native[asset_index(a)];
    if(type==3) {
        if(!w.write(1,1)) {return false;}
        for(unsigned i=0;i<24;++i) {if(!w.write(1,1) || !w.write(0,7)) {return false;}}
        return w.write(1,1) && w.write(f.spawnGeneration,31);
    }
    if(type==1) {
        const auto& p=kSpawns[spawn_index(a)];
        const bool live=s.active && !s.sourceCleared;
        auto task=tactical(p);
        if(task.registry && f.tactics[spawn_index(a)].revision==s.generation) {task.row=f.tactics[spawn_index(a)].group;}
        if(p.sceneOwned) {
            const auto counts=live?requests(p):std::array<std::uint8_t,4>{};
            coo::native_combatant::Source source{};
            source.registry=key;source.generation=s.generation?s.generation:f.spawnGeneration;
            source.categories=std::span(counts).first(p.categories);source.hasRule=false;
            // Scene/named-member creation is the sole actor owner. Ordinary
            // replacement mode also creates a loose actor (observed for the
            // Centurion source 20 + member 21), overflowing the kill ledger.
            // Native flag-removal actions change the existing actor, NOT this
            // population mode; never turn their receipt into a second spawn.
            source.sceneRequested=true;
            source.tactical=task;if(task.registry) {source.tactical.revision=source.generation;}
            return coo::native_combatant::write_authored_source(w,source,{0,0,0,0});
        }
        const auto* override=rule_override(p);
        native_combatant::Source source{};
        source.registry=key;source.generation=s.generation;source.ruleSlot=override?override->rule:p.rule;
        source.categories=p.categories;source.requested=live?requests(p):std::array<std::uint8_t,4>{};
        source.tactical=task;source.hasRule=p.hasRule || override!=nullptr;source.memberOwned=false;source.reserve=false;
        source.objectiveRevision=task.registry?s.generation:0U;source.authoredPlacement=true;source.overrideRule=override!=nullptr;
        return native_combatant::write_source(w,source);
    }
    if(type==2) {
        // Named members: bind on the run generation, retire on the member's own new revision.
        if(s.retired) {return coo::native_combatant::write_retire_member(w,s.generation);}
        return coo::native_combatant::write_bind(w,s.generation?s.generation:f.spawnGeneration);
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
    if(type==43 && !s.active) {return coo::native_scene::cast_scene(w,s.generation,{}, {},f.scenes[scene_index(a)].revision,true);}
    if(type==43) {
        const auto i=scene_index(a);if(i>=std::size(kScenes)) {return false;}
        return coo::native_scene::cast_scene(w,s.generation,scene_sources(kScenes[i]).span(),std::span(f.scenes[i].events).first(f.scenes[i].count),f.scenes[i].revision);
    }
    return false;
}
}
