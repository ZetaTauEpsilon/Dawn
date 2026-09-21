#pragma once
#include "frame.h"
#include "traversal_catalog.h"
#include "ai_bindings.h"
#include "service_bindings.h"
#include "../coo/native_scene_authority.h"
#include "../coo/native_presentation_authority.h"
#include "../coo/native_combatant_authority.h"
#include "../coo/native_device_authority.h"
namespace dawn::state::activity::gateway {
// Authored 80EC0ABC events release the arrival idle, then the conversation hold.
inline constexpr std::array<std::uint32_t,2> kVanceEvents{0x3A5C256CU,0xC2656F80U};
[[nodiscard]] inline std::size_t vance_event_count(const Frame& frame) noexcept {
    return !frame.sceneGeneration?0U:frame.vanceConversation?2U:frame.vanceEntered?1U:0U;
}
[[nodiscard]] inline std::size_t body_bits(const Frame& frame,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!frame.enabled) { return 0; }
    if(key==0x986985D0U) {
        if(type==53 && slot==2) { return coo::native_presentation::dialogue_bits(frame.generations,frame.activeRow); }
        if(type==68 && slot==0 && (frame.objective!=0 || frame.presentation.published)) { return coo::native_presentation::kDirectiveBits; }
    }
    // The physical Forest lattice is closed throughout this mission, including arrival.
    if(key==kMainlandRegistry && type==23 && slot==2) { return 147; }
    if(frame.spawnGeneration==0) { return 0; }
    if(key==0xBA0B27A0U) {
        if(type==23 && slot<=1) { return 147; }
        if(type==1 && slot==4) { return 641; }
        if(type==43 && slot==5) { return (frame.sceneGeneration?129U:74U)+32U*vance_event_count(frame); }
    }
    if(key==kMainlandRegistry) {
        if(type==4 && ((slot>=26 && slot<=28) || slot==32 || slot==33)) { return 252; }
        if(type==23 && (slot==0 || slot==3 || slot==4)) { return 147; }
    }
    if(type==1) { const auto* s=spawn(key,slot);return s && (frame.cohorts&(1U<<s->cohort))?(s->categories==2?673U:641U):0; }
    if(key!=kTraversalRegistry) { return 0; }
    if(type==4 && (slot==3 || slot==4)) { return 252; }
    if(type==23 && slot<=2) { return 147; }
    // Prepublish shield selectors before any marcher source is requested.
    if(type==26 && slot>=25 && slot<=205 && (slot-25)%18==0) { return 186; }
    if(type==34 && slot>=268 && slot<=278) { return 364; }
    if(frame.marchers) {
        if(type==23 && slot>=8 && slot<206 && (slot-8)%18<8) { return 147; }
    }
    return 0;
}
template<class Writer> bool write_body(Writer& writer,const Frame& frame,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(body_bits(frame,key,type,slot)==0) { return false; }
    if(key==0x986985D0U) {
        if(type==53) { return coo::native_presentation::dialogue(writer,frame.generations,frame.activeRow); }
        if(frame.services) { return coo::native_presentation::waypoint_objective(writer,frame.presentation); }
        if(!frame.finished) { return coo::native_presentation::directive(writer,frame.objective); }
        // Retire the Vance objective after the validated ending timeline finishes.
        using namespace coo::native_presentation;
        return absent(writer) && absent(writer) && directive_record(writer,false,frame.objective)
            && directive_record(writer,false,frame.objective) && directive_record(writer,false,frame.objective) && writer.write(1,3);
    }
    if(key==0xBA0B27A0U) {
        if(type==23) {
            const bool raised=(frame.lighthouseChannels&(1U<<slot))!=0;
            return coo::native_device::position_only(writer,raised?0.F:1.F,
                static_cast<std::int16_t>(frame.spawnGeneration+(raised?1U:0U)),!raised);
        }
        return type==1?coo::native_scene::write_source(writer,frame.spawnGeneration,frame.sceneGeneration!=0)
            :coo::native_scene::scene(writer,key,4,frame.sceneGeneration,std::span{kVanceEvents}.first(vance_event_count(frame)));
    }
    if(frame.services && key==kMainlandRegistry) {
        for(std::size_t i=0;i<frame.objects.size();++i) {
            const auto& b=kEndingObjects[i];const auto& state=frame.objects[i];
            if(type==4 && slot==b.source.slot) { return coo::native_device::object(writer,state.generation,state.create); }
            if(type==23 && slot==b.device.slot) { return coo::native_device::position_only(writer,state.position,state.apply?state.revision:std::int16_t{-1},true); }
        }
    }
    if(key==kMainlandRegistry && type==4) {
        const auto index=slot<=28?slot-24:slot-27;
        const bool prepared=(frame.preparedMask&(1U<<index))!=0;
        const bool active=slot==27?!frame.lighthouseOpen:slot==32?!frame.moduleDestroyed
            :slot==33?!frame.moduleDestroyed:true;
        // Preparation can retire an entity without clearing the committed generation.
        // Request creation once with the next generation; exposure never respawns it.
        const auto generation=frame.spawnGeneration+((slot==27 || slot==32 || slot==33) && prepared?1U:0U);
        return coo::native_device::object(writer,generation,prepared && active);
    }
    if(key==kMainlandRegistry && type==23 && slot!=2) {
        // Native module presentation: 1 = shielded, .75 = exposed, 0 = retired.
        // The entrance barrier and beam remain on until real module destruction.
        const float position=slot==0?(frame.lighthouseOpen?0.F:1.F)
            :frame.moduleDestroyed?0.F:slot==4 && frame.moduleVulnerable?.75F:1.F;
        return coo::native_device::position_only(writer,position,frame.moduleDestroyed?3:frame.moduleVulnerable?2:1,true);
    }
    if(type==1) {
        const auto& s=*spawn(key,slot);
        coo::native_combatant::Source source{key,frame.spawnGeneration,s.rule,coo::population_size::first(s.count,s.categories),{},static_cast<std::uint8_t>(s.categories==2?1U:0U),s.categories==2,s.rule!=UINT16_MAX};
        source.tactical=tactical_group(s);
        return coo::native_combatant::write_source(writer,source);
    }
    if(key==kMainlandRegistry && type==23 && slot==2) { return coo::native_device::position_only(writer,1.F,1,true); }
    if(type==4) { return coo::native_device::object(writer,frame.spawnGeneration,frame.cannons); }
    if(type==23) {
        const bool active=slot==2?frame.finalCannon:slot<2?frame.cannons:frame.marchers;
        return coo::native_device::channels(writer,active?1.F:0.F,active?1U:0U);
    }
    if(type==26) { return coo::native_device::linked_effect(writer,key,static_cast<std::uint16_t>(268+(slot-25)/18),true); }
    if(type==34) {
        const auto base=static_cast<std::uint16_t>(16+(slot-268)*18);
        const std::array<std::uint16_t,4> sources{base,static_cast<std::uint16_t>(base+2),static_cast<std::uint16_t>(base+4),static_cast<std::uint16_t>(base+6)};
        return coo::native_device::collection(writer,key,sources);
    }
    return false;
}
}
