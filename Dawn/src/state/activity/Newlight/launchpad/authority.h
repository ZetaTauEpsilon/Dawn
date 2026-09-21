#pragma once
#include "controller.h"
#include "performances.h"
#include "lighting.h"
#include "../../coo/native_presentation_authority.h"
#include "../../coo/native_device_authority.h"

namespace dawn::state::activity::newlight::launchpad {
struct PlayerEffect {std::uint32_t registry;std::uint16_t effect,filter;};
inline constexpr PlayerEffect kPlayerEffects[]{
    {0x3DDD9E09U,0,3},{0x3DDD9E09U,1,4},
    {kBreach,69,98},{kBreach,70,99},{kBreach,71,100},
    {kBreach,72,101},{kBreach,73,102},{kBreach,74,103},{kHangar,14,16}};
inline bool effect_active(const Frame& f,std::size_t i) noexcept {
    if(!f.enabled || f.cinematic.phase!=cinematics::Phase::gameplay) {return false;}
    switch(i) {
    case 0:case 1:return f.section==0;
    case 2:return f.section==1 && f.ghost.phase==ghost::Phase::dormant;
    case 3:return f.section<3;
    case 4:return f.section==1 && !f.pickups[0].granted;
    case 7:return f.pickups[1].granted && f.section==2;
    case 8:return f.pickups[2].granted && f.section==4;
    default:return false;
    }
}
constexpr coo::Asset actor_source(coo::Asset actor) noexcept {
    const auto* member=find(actor.registry,actor.type,actor.slot);
    if(!member || actor.type!=2) {return {};}
    for(const auto& candidate:kAssets) {
        if(candidate.asset.registry==actor.registry && candidate.asset.type==1
            && member->name.starts_with(candidate.name)
            && member->name.substr(candidate.name.size()).starts_with("__")) {return candidate.asset;}
    }
    return {};
}
inline std::size_t body_bits(const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(f.cinematic.owner.valid() && cinematics::index(key,type,slot)>=0) {return 263;}
    // Retired groups must not keep receiving actor/scene/device requests during
    // slice destruction. The persistent root still clears music and objectives.
    if(f.cinematic.ending() && key!=kRoot) {return 0;}
    if(!f.enabled || !f.spawnGeneration) {return 0;}const auto* a=find(key,type,slot);if(!a) {return 0;}
    if(type==6 && key==kKetch && slot==0) {return 263;}
    if(a->asset==asset(kRoot,11,1)) {return 7223;}
    if(a->asset==kDialogueAsset) {return coo::native_presentation::dialogue_bits(f.generations,f.activeRow);}
    if(a->asset==kDirectiveAsset) {return f.presentation.published?coo::native_presentation::kDirectiveBits:0;}
    if(a->asset==transport::kPassenger) {return f.skiff.phase==transport::Phase::idle?0:86;}
    if(a->asset==transport::kPilot) {return f.skiff.publication.bits();}
    if(a->asset==transport::kCarrier && f.skiff.phase!=transport::Phase::idle) {return coo::native_combatant::kSourceBits;}
    if(a->asset==ghost::kActor) {return f.ghost.publication.bits();}
    if(a->asset==ghost::kControl) {return ghost::animation::body_bits(f.ghost.events().size());}
    if(a->asset==ghost::kSource && f.ghost.phase!=ghost::Phase::dormant) {return coo::native_combatant::kSourceBits;}
    if(const auto* n=performances::nest(a->asset)) {
        return f.native[asset_index(asset(kBreach,1,n->source))].managed?ghost::animation::body_bits(f.light?1:0):0;
    }
    if(type==2) {const auto source=actor_source(a->asset);return source.registry && f.native[asset_index(source)].managed
        ?performances::staged_shank(a->asset)?performances::kStagedShankBits
            :performances::entrance(a->asset)?performances::kEntranceBits:coo::native_combatant::kBindBits:0;}
    if(type==3 && a->authority==0x80807F0CU) {return 225;}
    for(const auto& p:kPlayerEffects) if(key==p.registry) {
        if(type==26 && slot==p.effect) {return 186;}
        if(type==34 && slot==p.filter) {return 39;}
    }
    const auto& s=f.native[asset_index(a->asset)];
    if(a->asset==lighting::kSource && s.prepared && s.active && f.lightRequested) {return lighting::kAuthorityBits;}
    // Both mission and ambient Breach placements contain the light gate. Own
    // their initial inactive state so an unused copy cannot obstruct the route.
    if(type==4 && !s.managed && ((key==kBreach && slot<=5) || (key==0x68D82EA6U && slot<=1))) {return 252;}
    if(!s.managed) {return 0;}
    if(type==1) {
        const auto i=spawn_index(a->asset);if(i==kSpawns.size()) {return 0;}
        return performances::staged_source(a->asset)?coo::native_combatant::kSourceBits+32U*(kSpawns[i].categories-1U)
            :coo::native_combatant::authored_source_bits(kSpawns[i].categories,kSpawns[i].tactical.registry!=0);
    }
    if(type==4 && a->authority==0x8080992FU) {return s.prepared && a->asset==kRifle?375:252;}
    if(type==23 && a->authority==0x80804F48U) {return 147;}
    if(type==5 && a->authority==0x80804F04U) {return 7359;}
    return 0;
}
template<class W> bool interactable(W& w,std::uint32_t generation,bool active) noexcept {
    const auto absent=[&] {return w.write(0x811C9DC5U,32) && w.write(0,7) && w.write(0x7FFF,16);};
    return w.write(generation^0x80000000U,32) && w.write(0x80000000U,32)
        && w.write(active?1U:0U,1) && w.write(0,1) && w.write(0x80000000U,32)
        && absent() && w.write(0,32) && w.write(0,32) && w.write(0,32)
        && w.write(0,1) && w.write(1,2) && w.write(1,1) && w.write(0x80804FB8U,32)
        && w.write(0,2) && absent() && w.write(0x80000000U,32) && w.write(0,1);
}
template<class W> bool write_body(W& w,const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!body_bits(f,key,type,slot)) {return false;}
    if(const auto i=cinematics::index(key,type,slot);i>=0) {return cinematics::write(w,f.cinematic,static_cast<std::size_t>(i));}
    if(type==6 && key==kKetch && slot==0) {
        auto state=f.cinematic;state.movie=0;state.play=f.ketch;state.revisions[0]=f.ketchRevision?f.ketchRevision:f.spawnGeneration;
        return cinematics::write(w,state,0);
    }
    const auto a=find(key,type,slot)->asset;
    if(a==transport::kPassenger) {return f.skiff.passenger(w);}
    if(a==transport::kPilot) {return f.skiff.publication.write(w);}
    if(a==transport::kCarrier && f.skiff.phase!=transport::Phase::idle) {
        coo::native_combatant::Source source{key,f.skiff.publication.program.generation,0,0,{}};
        source.hasRule=false;source.memberOwned=true;return coo::native_combatant::write_source(w,source);
    }
    if(a==ghost::kActor) {return f.ghost.publication.write(w);}
    if(a==ghost::kControl) {return ghost::animation::write(w,f.ghost.control(),f.ghost.events());}
    if(a==ghost::kSource && f.ghost.phase!=ghost::Phase::dormant) {
        coo::native_combatant::Source source{key,f.ghost.publication.program.generation,0,0,{}};
        source.hasRule=false;source.memberOwned=true;return coo::native_combatant::write_source(w,source);
    }
    if(const auto* n=performances::nest(a)) {
        const auto& s=f.native[asset_index(asset(kBreach,1,n->source))];
        const ghost::animation::Event event{n->sequence,0xC9B0910DU};
        return ghost::animation::write(w,{n->sequence,ghost::animation::kEmptyHash,s.generation},
            f.light?std::span{&event,std::size_t{1}}:std::span<const ghost::animation::Event>{});
    }
    if(type==2) {
        const auto source=actor_source(a);if(!source.registry) {return false;}
        const auto generation=f.native[asset_index(source)].generation;
        // The named Walker is the only tank. Type-39 attaches it to the Skiff;
        // a simultaneous manifest request would create another loose Walker.
        if(a==transport::kTankMember) {return coo::native_combatant::write_spawn(w,generation);}
        if(performances::staged_shank(a)) {return performances::write_staged_shank(w,generation);}
        if(performances::staged_source(source)) {return coo::native_combatant::write_spawn(w,generation);}
        const auto sequence=performances::entrance(a);
        return sequence?performances::write_entrance(w,generation,sequence):coo::native_combatant::write_bind(w,generation);
    }
    if(a==kDialogueAsset) {return coo::native_presentation::dialogue(w,f.generations,f.activeRow);}
    // The native receiver replaces HUD targets through its three-record ring.
    // Reusing record zero with marker display disabled leaves the old waypoint.
    if(a==kDirectiveAsset) {return coo::native_presentation::waypoint_objective(w,f.presentation,{},false,
        f.presentation.event==kObjectives[5]?static_cast<std::int32_t>(f.assaultDefeated):-1,kAssaultTarget);}
    if(a==asset(kRoot,11,1)) {
        constexpr std::uint8_t sections[]{0,1,2,7,10};
        const bool playing=f.cinematic.phase==cinematics::Phase::gameplay && !f.finished && !f.ketch;
        if(!w.write(playing?1U<<(f.section==3?(f.shipFound?8U:f.assault?7U:6U):sections[f.section]):0U,32) || !w.write(0,32) || !w.write(0,32) || !w.write(0,32)) {return false;}
        for(unsigned i=0;i<129;++i) {if(!coo::native_presentation::absent(w)) {return false;}}return true;
    }
    if(type==3) {
        if(!w.write(1,1)) {return false;}
        for(unsigned i=0;i<24;++i) {if(!w.write(1,1) || !w.write(0,7)) {return false;}}
        return w.write(1,1) && w.write(f.spawnGeneration,31);
    }
    for(std::size_t i=0;i<std::size(kPlayerEffects);++i) {
        const auto p=kPlayerEffects[i];if(key!=p.registry) {continue;}
        if(type==34 && slot==p.filter) {return w.write(1,4) && w.write(1,1) && w.write(0x8080957DU,32) && w.write(1,2);}
        if(type==26 && slot==p.effect) {
            const bool on=effect_active(f,i);
            if(!w.write(0,1) || !w.write(on?0U:1U,1)) {return false;}
            for(unsigned n=0;n<3;++n) {if(!w.write(0x80000000U,32)) {return false;}}
            return w.write((f.spawnGeneration+16U*f.section+(f.ghost.phase!=ghost::Phase::dormant?128U:0U)+(f.light?1U:0U)+(f.pickups[0].granted?2U:0U)+(f.pickups[1].granted?4U:0U)+(f.pickups[2].granted?8U:0U))^0x80000000U,32)
                && (on?(w.write(key,32) && w.write(35,7) && w.write(32768U+p.filter,16)):coo::native_presentation::absent(w)) && w.write(0,1);
        }
    }
    const auto& s=f.native[asset_index(a)];
    if(a==lighting::kSource && s.prepared && s.active && f.lightRequested) {return lighting::write(w,s.generation);}
    if(type==1) {
        const auto i=spawn_index(a);const auto& p=kSpawns[i];auto task=p.tactical;
        if(task.registry) {task.revision=s.generation;if(f.tactics[i].revision==s.generation) {task.row=f.tactics[i].group;}}
        std::array<std::uint8_t,15> counts{};
        if(performances::staged_source(a)) {
            // Type-42 addresses named members. Let those members create their
            // authored actors; a loose request creates unbound copies instead.
            coo::native_combatant::Source source{key,s.generation,0,0,{}};
            source.hasRule=false;source.memberOwned=true;source.categories=std::span(counts).first(p.categories);
            return coo::native_combatant::write_source(w,source);
        }
        if(s.active && !s.sourceCleared && a!=transport::kTank && !performances::entrance_source(a)) {
            for(unsigned n=0;n<p.categories;++n) {counts[n]=1;}
            if(p.categories==1) {counts[0]=p.count;}
        }
        coo::native_combatant::Source source{key,s.generation,0,0,task};
        source.hasRule=false;source.categories=std::span(counts).first(p.categories);
        source.memberOwned=a==transport::kTank || performances::entrance_source(a);
        return coo::native_combatant::write_authored_source(w,source,{0,0,0,0});
    }
    if(type==4) {return s.prepared && a==kRifle?interactable(w,s.generation,s.active):coo::native_device::object(w,s.managed?s.generation:f.spawnGeneration,s.active);}
    if(type==23) {return coo::native_device::position_only(w,s.position,static_cast<std::int16_t>(s.generation),s.snap);}
    if(type==5) {
        // F9EDB0 seeks to activityClock - startTicks. Zero fast-forwards late
        // ambush cues past their authored audio instead of starting them now.
        if(!w.write(s.sequenceStartTicks,64) || !w.write(UINT64_MAX,64) || !w.write(s.active?s.sequenceRevision:255,8)) {return false;}
        for(unsigned i=0;i<4;++i) {if(!w.write(0,32)) {return false;}}
        for(unsigned i=0;i<129;++i) {if(!coo::native_presentation::absent(w)) {return false;}}return true;
    }
    return false;
}
}
