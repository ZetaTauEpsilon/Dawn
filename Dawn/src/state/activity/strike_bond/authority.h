#pragma once
#include "frame.h"
#include "../coo/native_player_trigger.h"
#include "../coo/native_presentation_authority.h"
#include "../coo/native_combatant_authority.h"
#include "../coo/native_device_authority.h"
#include "../coo/native_scene_cast_authority.h"
#include "../coo/native_scene_authority.h"
#include "../coo/native_clock_authority.h"
#include "../coo/native_music_authority.h"
namespace dawn::state::activity::strike_bond {
inline std::uint32_t encounter_key(const Frame& f,std::uint32_t key) noexcept {
    if(f.campaign && key==0x277205FBU) return kRoot;
    if(f.campaign && key==0x4786C0E0U) return 0xF29221F5U;
    return key;
}
// The wire list owns runtime participants. Static type-48 performance points
// remain in the authored 80806268 parameters, but must never be released as
// components by B3F620 -> 4E83B0 (roof crash at exe+4E8421).
struct SceneParticipants {
    std::array<coo::Asset,15> values{};std::size_t count{};
    std::span<const coo::Asset> view() const noexcept {return std::span(values).first(count);}
};
inline SceneParticipants participants(const Scene& scene) noexcept {
    SceneParticipants result{};
    for(const auto a:scene.cast) if(a.type==1 || a.type==4) result.values[result.count++]=a;
    return result;
}
inline coo::Asset audience(int region) noexcept {
    const std::uint32_t key=region/8==15?0x40BFB1C0U:region/8==10?0x2763EC90U:region/8==1?0xC95ECB1AU:region/8==17?0x2CB86C0FU:0;
    for(const auto& a:kAssets) if(a.asset.registry==key && a.asset.type==70) return a.asset;
    return {};
}
inline std::size_t body_bits(const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    key=encounter_key(f,key);
    if(!f.enabled || !f.spawnGeneration) return 0;
    if(f.campaign && key==0xC80A735BU && type==65 && slot==0) return 65;
    const auto* a=find(key,type,slot);if(!a) return 0;
    // t_kv_upper was omitted from the original catalogue, so its disable
    // request was never published. Its volume spans Z=207.662..228.662 across
    // the upper cannon path. Do not alter lower fall volumes or player health.
    if(type==32 && key==0x2CB86C0FU && slot==292) return f.region==136?57:0;
    if(type==31) return coo::native_player_trigger::kAuthBits;
    if(type==18) return 386;
    if(type==70) return 23;
    if(type==53) return coo::native_presentation::dialogue_bits(f.generations,f.activeRow);
    if(type==68) return f.presentation.published?coo::native_presentation::kDirectiveBits:0;
    if(type==11) return f.musicCandidate<128?7223:0;
    if(type==37) return f.generatorSeed?coo::native_generator::kActivationBits:0;
    const auto& state=f.native[asset_index(a->asset)];
    if(type==2 && a->asset==kBossActor) {
        const auto* source=find(key,1,3);
        return source && (f.native[asset_index(source->asset)].active || f.bossDead)?coo::native_combatant::kBindBits:0;
    }
    if(const auto* g=golem(key,type,slot)) {
        const auto& lens=f.native[asset_index(kLenses[g->lens].source)];
        return lens.managed?(type==26?186:94):0;
    }
    if(!state.managed) return 0;
    // Retirement is an explicit publication, not the absence of a spawn body.
    if(type==1 && key==kBossActor.registry && slot==3) return state.active || f.bossDead?coo::native_combatant::kSourceBits:0;
    if(type==1 && key==0xC80A735BU) return f.campaign && state.active?641:0;
    if(type==1) {const auto i=spawn_index(a->asset);return state.active && i<std::size(kSpawns)?coo::native_combatant::authored_source_bits(kSpawns[i].categories,true):0;}
    if(type==4) return 252;
    if(type==23) return 147;
    if(type==43) {
        const auto i=scene_index(a->asset);if(i==std::size(kScenes) || !f.scenes[i].generation) return 0;
        // Source objects complete preparation and native creation before a
        // scene can resolve its non-actor cast. Reserved actors are scene-owned.
        // A stop must still reach an existing scene after its cast is retired.
        for(const auto target:kScenes[i].cast) if(target.type==4 && !f.scenes[i].stop) {
            const auto n=asset_index(target);
            if(n==std::size(kAssets) || !f.native[n].active || !f.native[n].acknowledged) return 0;
        }
        return coo::native_scene::cast_bits(participants(kScenes[i]).count,f.scenes[i].eventCount);
    }
    return 0;
}
template<class Writer> bool write_body(Writer& w,const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    key=encounter_key(f,key);
    if(!body_bits(f,key,type,slot)) return false;
    const auto* a=find(key,type,slot);const auto& s=f.native[asset_index(a->asset)];
    switch(type) {
    case 65:return f.campaign && w.write(0x80000000U+f.spawnGeneration+1U,32)
        && w.write(f.scan.armed && !f.scan.complete && !f.finished?1U:0U,1) && w.write(0x811C9DC5U,32);
    case 1: {
        if(key==0xC80A735BU) return coo::native_scene::write_source(w,s.generation,s.active);
        const auto i=spawn_index(a->asset);const auto& row=kSpawns[i];
        // looseRequested/secondRequested request one actor per authored category. A category
        // holds six weighted SELECTIONS, but those are alternatives for the one slot, not six
        // slots: requesting six per category spawned roughly ten times the intended population.
        // kSpawns.count is therefore the category count, and that is the authored actor count.
        coo::native_combatant::Source source{key,f.spawnGeneration,0,1,{},
            static_cast<std::uint8_t>(row.categories==2?1:0),row.categories==2,false};
        source.variant = f.enemyVariant==5 && grandmaster_substitution_source(key,slot) ? 5U : 0U;
        if(key==kBossActor.registry && slot==3) {
            // A normal loose request owns spawning and AI. A reserved scene
            // request can commit its generation without ever creating Dendron.
            source.hasRule=true;source.ruleSlot=row.rule;
            source.tactical={key,row.objective,10,1};
            if(f.bossDead && !s.active) {
                // Changed generation with BC=0 invokes native source retirement
                // (4E9550 -> 4EC1A0) with no replacement request. The detached
                // corpse's captured entity lease is cleaned at that boundary.
                source.generation=s.generation;source.looseRequested=0;source.retireOwned=true;
            }
            return coo::native_combatant::write_source(w,source);
        }
        source.sceneRequested=row.sceneOwned;
        source.tactical={key,row.objective,static_cast<std::int8_t>(static_cast<int>(f.taskPlusOne[i])-1),1};
        return coo::native_combatant::write_authored_source(w,source,{0,0,0,0});
    }
    case 2:
        if(f.bossDead && a->asset==kBossActor) {
            const auto source=find(key,1,3)->asset;
            return coo::native_combatant::write_retire_member(w,f.native[asset_index(source)].generation);
        }
        return coo::native_combatant::write_bind(w,f.spawnGeneration);
    case 4:return coo::native_device::object(w,s.generation,s.active);
    case 11:return coo::native_music::select(w,f.musicCandidate);
    case 18:return coo::native_clock::countdown(w,f.completion.valid() && f.completion.state==6,f.endEpoch,f.campaign?10000U:30000U);
    case 23:return coo::native_device::position_only(w,device_position(f,a->asset),static_cast<std::int16_t>(s.generation),!animated_position(a->asset) || (key==kBossActor.registry && slot==173 && f.bossPlatformSnap));
    case 26: {
        const auto* g=golem(key,type,slot);if(!g) return false;
        const auto& lens=f.native[asset_index(kLenses[g->lens].source)];
        return coo::native_device::linked_effect(w,key,g->collection,lens.active && !f.lensDestroyed[g->lens] && !f.bossDead);
    }
    case 34: {
        const auto* g=golem(key,type,slot);if(!g) return false;
        return coo::native_device::collection(w,key,std::span(&g->source,1));
    }
    case 31:return coo::native_player_trigger::arm(w,f.spawnGeneration);
    case 32:
        // 8080955A: 55-bit scoped volume, then int8 enum [-1,1] in two bits.
        // Runtime false=0 is wire 1; wire 0 means the unset state (-1).
        return key==0x2CB86C0FU && slot==292 && w.write(key,32) && w.write(61,7)
            && w.write(406U+32768U,16) && w.write(1,2);
    case 37:return coo::native_generator::write_activation(w,forest_request(f.generatorSeed));
    case 43: {
        const auto i=scene_index(a->asset);const auto& scene=f.scenes[i];
        return coo::native_scene::cast_scene(w,scene.generation,participants(kScenes[i]).view(),std::span(scene.events).first(scene.eventCount),1,scene.stop);
    }
    case 53:return coo::native_presentation::dialogue(w,f.generations,f.activeRow);
    case 68:return coo::native_presentation::waypoint_objective(w,f.presentation,audience(f.region),true);
    case 70:return w.write(0,5) && w.write(0,1) && w.write(32769U,16) && w.write(0,1);
    default:return false;
    }
}
}
