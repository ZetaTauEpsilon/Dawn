#pragma once
#include "frame.h"
#include "../coo/native_mission_forest_authority.h"
#include "shield_authority.h"
#include "ai_bindings.h"
#include "../coo/native_presentation_authority.h"
#include "../coo/native_device_authority.h"
#include "../coo/native_combatant_authority.h"
#include "../coo/native_scene_authority.h"
#include "../coo/native_scene_cast_authority.h"

namespace dawn::state::activity::beyond_infinity {
inline const SourceBinding* source(coo::Asset asset) noexcept {
    for(const auto& value:kSources) { if(value.asset==asset) { return &value; } }return nullptr;
}
inline std::size_t body_bits(const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!f.enabled || !f.spawnGeneration) { return 0; }
    const auto* binding=find(key,type,slot);if(!binding) { return 0; }
    if(type==37 && key==0x8E70632BU && slot==3 && binding->authority==0x80805007U)return coo::native_generator::kActivationBits;
    if(binding->asset==kDialogueAsset) { return coo::native_presentation::dialogue_bits(f.generations,f.activeRow); }
    if(key==kRoot && type==68 && slot==0 && f.presentation.published) { return coo::native_presentation::kDirectiveBits; }
    const auto& state=f.native[asset_index(binding->asset)];
    if(type==43) {
        const auto i=scene_index(binding->asset);
        return i<std::size(kScenes)?coo::native_scene::cast_bits(state.active?kScenes[i].cast.size():0,state.active?f.sceneRequests[i].count:0):0;
    }
    if(type==1) {
        const auto* s=source(binding->asset);
        return s && (s->sceneOwned || state.managed)?(s->sceneOwned?641U:s->categories==2?673U:641U):0;
    }
    if(!state.managed) { return 0; }
    if(const auto shieldBits=shields::bits(binding->asset,state)) { return shieldBits; }
    if(type==4 && binding->authority==0x8080992FU) { return coo::native_device::object_bits(state.active,binding->asset==kPlate,coo::native_device::interaction::Mode::unchanged,binding->asset==kPlate); }
    if(type==23 && binding->authority==0x80804F48U) { return 147; }
    return 0;
}
template<class Writer>
bool write_body(Writer& w,const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!body_bits(f,key,type,slot)) { return false; }
    const auto asset=find(key,type,slot)->asset;
    if(type==37)return coo::native_generator::write_activation(w,coo::native_generator::beyond_request(f.forestSeed,f.forestPass,f.forestReady && !f.finished));
    if(asset==kDialogueAsset) { return coo::native_presentation::dialogue(w,f.generations,f.activeRow); }
    if(type==68) { return coo::native_presentation::waypoint_objective(w,f.presentation); }
    const auto& state=f.native[asset_index(asset)];
    if(type==43) {
        const auto& scene=kScenes[scene_index(asset)];
        return coo::native_scene::cast_scene(w,state.active?state.generation:0U,state.active?scene.cast:std::span<const coo::Asset>{},state.active?f.sceneRequests[scene_index(asset)].inputs():std::span<const std::uint32_t>{});
    }
    if(type==1) {
        const auto& s=*source(asset);
        // Cast placement and multiplicity stay with the native Scene.
        if(s.sceneOwned) { return coo::native_scene::write_source(w,f.spawnGeneration,state.active); }
        return coo::native_combatant::write_source(w,{key,state.generation,s.rule,
            static_cast<std::uint8_t>(state.active?1:0),tactical_group(asset),static_cast<std::uint8_t>(state.active && s.categories==2?1:0),s.categories==2,s.hasRule});
    }
    if(type==26 || type==34) { return shields::write(w,asset,state); }
    if(type==4) { return coo::native_device::object(w,state.generation,state.active,asset==kPlate?&f.plateCapture.state:nullptr,coo::native_device::interaction::Mode::unchanged,asset==kPlate?&f.plateCapture.pose.state:nullptr); }
    const bool lens=key==0x233E7149U && slot==41;
    // Past C1 and the two static Future frames resolve to entity80F3DB0A.
    // Graph80F3DB09's activate range is [0.0901,0.1001]; position1 matches none.
    // Future43/46 select these frames in80F46016, not their transport cores.
    const bool portalFrame=asset==coo::Asset{0xC7FB7155U,0x80F461C7U,23,3}
        || asset==coo::Asset{0x0FF26BCCU,0x80F46043U,23,1}
        || asset==coo::Asset{0x0FF26BCCU,0x80F46046U,23,2};
    const float position=lens?lens_position(f,state.desired):(state.active?(portalFrame?.1F:1.F):0.F);
    return coo::native_device::position_only(w,position,
        static_cast<std::int16_t>(state.generation),lens);
}
}
