#pragma once
#include "frame.h"
#include "../coo/native_presentation_authority.h"
#include "../coo/native_device_authority.h"
#include "../coo/native_combatant_authority.h"
namespace dawn::state::activity::deep_storage {
inline std::size_t body_bits(const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!f.enabled || !f.spawnGeneration) {return 0;}const auto* a=find(key,type,slot);if(!a) {return 0;}
    if(a->asset==kDialogueAsset) {return coo::native_presentation::dialogue_bits(f.generations,f.activeRow);}
    if(key==kRoot && type==68 && slot==0 && f.presentation.published) {return coo::native_presentation::kDirectiveBits;}
    for(const auto& scan:kScans) {if(a->asset==scan.link) {return 65;}}
    const auto& state=f.native[asset_index(a->asset)];if(!state.managed) {return 0;}
    if(type==1) {const auto i=spawn_index(a->asset);return i<std::size(kSpawns)?(kSpawns[i].categories==2?673U:641U):0;}
    if(type==4 && a->authority==0x8080992FU) {
        for(const auto& plate:kPlates) {if(a->asset==plate.source) {return coo::native_device::object_bits(state.active,true,coo::native_device::interaction::Mode::unchanged,true);}}
        return 252;
    }
    if(type==23 && a->authority==0x80804F48U) {return 147;}return 0;
}
template<class Writer> bool write_body(Writer& w,const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!body_bits(f,key,type,slot)) {return false;}const auto a=find(key,type,slot)->asset;
    if(a==kDialogueAsset) {return coo::native_presentation::dialogue(w,f.generations,f.activeRow);}
    if(type==68) {return coo::native_presentation::waypoint_objective(w,f.presentation);}
    if(type==65) {
        for(std::size_t i=0;i<2;++i) {if(a==kScans[i].link) {
            const bool active=f.scanArmed[i] && !f.scanComplete[i] && !f.finished;
            const auto gen=f.spawnGeneration+1U;
            return w.write(0x80000000U+gen,32) && w.write(active?1U:0U,1) && w.write(0x811C9DC5U,32);
        }}return false;
    }
    const auto& s=f.native[asset_index(a)];
    if(type==1) {const auto& p=kSpawns[spawn_index(a)];return coo::native_combatant::write_source(w,{key,s.generation,p.rule,
        static_cast<std::uint8_t>(s.active?1:0),p.tactical,static_cast<std::uint8_t>(s.active && p.categories==2?1:0),p.categories==2,true});}
    if(type==4) {
        for(std::size_t i=0;i<std::size(kPlates);++i) {if(a==kPlates[i].source) {return coo::native_device::object(w,s.generation,s.active,&f.plateCaptures[i].state,coo::native_device::interaction::Mode::unchanged,&f.plateCaptures[i].pose.state);}}
        return coo::native_device::object(w,s.generation,s.active);
    }
    return coo::native_device::position_only(w,device_position(f,a),static_cast<std::int16_t>(s.generation),false);
}
}
