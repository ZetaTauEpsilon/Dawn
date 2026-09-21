#pragma once
#include "frame.h"
#include "skiff_authority.h"
#include "../coo/native_presentation_authority.h"
#include "../coo/native_device_authority.h"
namespace dawn::state::activity::deadly_trial {
inline std::size_t body_bits(const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!f.enabled || !f.spawnGeneration) { return 0; }
    if(key==kRoot && type==53 && slot==2) { return coo::native_presentation::dialogue_bits(f.generations,f.activeRow); }
    if(key==kRoot && type==68 && slot==0 && f.presentation.published) { return coo::native_presentation::kDirectiveBits; }
    if(key==kAlleysA && (f.cohorts&(1U<<4))) {
        if(type==1 && slot==34) { return 641; }
        if(type==2 && slot==35) { return skiff::kMemberBits; }
    }
    if(type==1) { const auto* s=spawn(key,slot);return s && (f.cohorts&(1U<<s->cohort))?(s->categories==2?673U:641U):0; }
    if(key==kAlleysA) {
        if(type==23 && slot==46) { return 147; }
        if(type==4 && ((slot>=4 && slot<=6)||(slot>=40 && slot<=45))) { return 252; }
    }
    if(key==kAlleysB) {
        if(type==23 && slot==14) { return 147; }
        if(type==4 && slot==59) { return coo::native_device::object_bits(f.reviveEnabled,false,coo::native_device::interaction::Mode::enabled); }
        if(type==4 && ((slot>=10 && slot<=12)||(slot>=21 && slot<=23))) { return 252; }

    }
    if(key==0x27660927U && type==65 && slot==0) { return 65; }
    return 0;
}
template<class Writer> bool write_body(Writer& w,const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!body_bits(f,key,type,slot)) { return false; }
    if(key==kRoot) { return type==53?coo::native_presentation::dialogue(w,f.generations,f.activeRow):coo::native_presentation::waypoint_objective(w,f.presentation); }
    if(key==kAlleysA && type==1 && slot==34) {
        // One authored member owns this Skiff; never issue a duplicate loose spawn.
        coo::native_combatant::Source ship{key,f.spawnGeneration,0,0,{},0,false,false};
        ship.memberOwned=true;return coo::native_combatant::write_source(w,ship);
    }
    if(key==kAlleysA && type==2 && slot==35) {
        return skiff::member(w,f.spawnGeneration);
    }
    if(type==1) { const auto& s=*spawn(key,slot);return coo::native_combatant::write_source(w,{key,f.spawnGeneration,s.rule,1,s.tactical,static_cast<std::uint8_t>(s.categories==2?1:0),s.categories==2,true}); }
    if(type==23) { const bool open=key==kAlleysA && f.barrierOpen;return coo::native_device::position_only(w,key==kAlleysA?(open?1.F:0.F):1.F,static_cast<std::int16_t>(f.spawnGeneration+(open?1U:0U)),true); }
    if(type==4) {
        const bool active=key==kAlleysB && slot==59?f.reviveEnabled:key==kAlleysA && slot<=6?f.pikes>=1:f.pikes>=2;
        return coo::native_device::object(w,f.spawnGeneration+(active?1U:0U),active,nullptr,
            key==kAlleysB && slot==59?coo::native_device::interaction::Mode::enabled:coo::native_device::interaction::Mode::unchanged);
    }
    // Native 80804D3F: biased i32 revision, active bit, selector hash.
    // Keep the Ghost-link dormant until the real Ghost is bound. The old
    // type-43 selector also plays row 9, so it must not own this revival.
    const auto gen=f.sceneBound?f.sceneGeneration:0U;
    return w.write(gen?0x80000000U+gen:0x7FFFFFFFU,32)
        && w.write(gen && !f.sceneComplete?1U:0U,1) && w.write(0x811C9DC5U,32);
}
}
