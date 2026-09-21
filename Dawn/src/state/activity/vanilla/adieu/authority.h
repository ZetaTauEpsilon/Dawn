#pragma once
#include "controller.h"
#include "../../coo/native_combatant_authority.h"
#include "../../coo/native_device_authority.h"
#include "../../coo/native_presentation_authority.h"
#include "../../../../middleware/bap/activity_message/native/world_sequence_authority.h"

namespace dawn::state::activity::vanilla::adieu {
inline bool scene_member(const Frame& f,coo::Asset a) noexcept {
    for(const auto& scene:kScenes) {
        const auto* state=f.native.scene(scene.asset);
        if(!state || !state->requested || state->stopped) continue;
        for(const auto member:scene.cast) if(member==a) return true;
    }
    return false;
}
inline std::size_t body_bits(const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!f.enabled || !f.spawnGeneration) return 0;
    if(f.cinematic.owner.valid() && cinematics::index(key,type,slot)>=0) return 263;
    const auto* binding=find(key,type,slot);if(!binding) return 0;const auto a=binding->asset;
    if(const auto count=f.native.bits(a)) return count;
    if(a==kDialogueAsset) return coo::native_presentation::dialogue_bits(f.generations,f.activeRow);
    if(a==kObjectiveAsset && f.presentation.published) return coo::native_presentation::kDirectiveBits;
    if(type==3 && (slot==159 || slot==186) && key==kMain) return 225;
    if(type==4) {
        const auto i=object_index(a);if(i>=kObjects.size() || !f.managedObjects[i]) return 0;
        return a==kPickup && f.objects[i].create?375:252;
    }
    if(type==5 && key==kMain && slot>=128 && slot<=133 && f.sequences[slot-128]) return 7359;
    if(type==1) {
        for(std::size_t i=0;i<kCombat.size();++i) if(key==kCombat[i].registry && slot==kCombat[i].source && f.combat[i].active) return coo::native_combatant::kSourceBits;
        if(key==kMain && (slot==90 || slot==91) && f.ghostRetired) return coo::native_combatant::kSourceBits;
        for(const auto& source:kSources) if(source.asset==a && source.sceneOwned) return coo::native_combatant::authored_source_bits(source.categories,false);
    }
    // Scene cast members keep native names and do not request duplicate actors.
    if(type==2 && key==kMain) return coo::native_combatant::kBindBits;
    return 0;
}
template<class W> bool pickup_authority(W& w,std::uint32_t generation) noexcept {
    const auto absent=[&] {return coo::native_presentation::absent(w);};
    return w.write(generation^0x80000000U,32) && w.write(0x80000000U,32)
        && w.write(1,1) && w.write(0,1) && w.write(0x80000000U,32)
        && absent() && w.write(0,32) && w.write(0,32) && w.write(0,32)
        && w.write(0,1) && w.write(1,2) && w.write(1,1) && w.write(0x80804FB8U,32)
        && w.write(0,2) && absent() && w.write(0x80000000U,32) && w.write(0,1);
}
template<class W> bool write_body(W& w,const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!body_bits(f,key,type,slot)) return false;
    if(const auto i=cinematics::index(key,type,slot);i>=0) return cinematics::write(w,f.cinematic,static_cast<std::size_t>(i));
    const auto a=asset(key,type,slot);
    if(f.native.bits(a)) return f.native.write(w,a);
    if(a==kDialogueAsset) return coo::native_presentation::dialogue(w,f.generations,f.activeRow);
    if(a==kObjectiveAsset) return coo::native_presentation::objective(w,f.presentation,{},true);
    if(type==4) {
        const auto& state=f.objects[object_index(a)];
        // Both Ghost objects are deferred sources (definition+94 = 1).
        // Native 9F19F0 only calls 9EFAE0 to retire their existing entity when
        // the incoming generation increases. Inactive at the same generation
        // merely stops future creation. This extra generation is within the
        // run's 256-generation lease and stays constant on retransmission.
        const bool retireGhost=key==kMain && (slot==118 || slot==119) && state.phase==coo::ObjectPhase::retired;
        const auto generation=state.generation+(retireGhost?1U:0U);
        return a==kPickup && state.create?pickup_authority(w,generation):coo::native_device::object(w,generation,state.create);
    }
    if(type==3) {
        if(!w.write(1,1)) return false;
        for(unsigned i=0;i<24;++i) if(!w.write(1,1) || !w.write(0,7)) return false;
        return w.write(1,1) && w.write(f.spawnGeneration,31);
    }
    if(type==2) return coo::native_combatant::write_bind(w,f.spawnGeneration);
    if(type==5) {
        // F9EDB0 seeks to activityClock - startTicks. A late healing cue must
        // start now, with no end bound, rather than seek from mission time zero.
        return middleware::bap::activity_message::native::world_sequence::write(w,
            {key,slot,kBubble,static_cast<std::uint8_t>(slot==133 && f.ghostRetired?255:1),
                f.sequenceStartTicks[slot-128],UINT64_MAX});
    }
    if(type==1) {
        coo::native_combatant::Source source{};source.registry=key;source.generation=f.spawnGeneration;source.hasRule=false;
        if(key==kMain && (slot==90 || slot==91) && f.ghostRetired) {
            // A changed native source generation removes the old scene-owned
            // Ghost entities. Replacing the loose population with zero does not.
            ++source.generation;source.retireOwned=true;
            return coo::native_combatant::write_source(w,source);
        }
        for(std::size_t i=0;i<kCombat.size();++i) if(key==kCombat[i].registry && slot==kCombat[i].source) {
            const auto& state=f.combat[i];source.generation=state.generation;
            source.looseRequested=state.cleared?0:1;source.tactical={key,kCombat[i].tactical,state.row,state.generation};
            // Loose enemies need their explicit authored placement rule and
            // cumulative request mode. Scene reservation/replacement is separate.
            source.hasRule=true;source.ruleSlot=kCombat[i].rule;
            return coo::native_combatant::write_source(w,source);
        }
        source.sceneRequested=scene_member(f,a);
        for(const auto& binding:kSources) if(binding.asset==a) {
            std::array<std::uint8_t,4> counts{};source.categories=std::span(counts).first(binding.categories);
            return coo::native_combatant::write_authored_source(w,source,{0,0,0,0});
        }
    }
    return false;
}
}
