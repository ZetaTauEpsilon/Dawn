#pragma once
#include "catalog.h"
#include "../../coo/lifecycle_service.h"
#include "../../coo/native_scene_cast_authority.h"
#include "../../coo/native_music_authority.h"
#include "../../../../middleware/bap/activity_message/scene_sense.h"
#include "../../../../middleware/bap/activity_message/native/status_effect_authority.h"
#include <bitset>

namespace dawn::state::activity::vanilla::adieu {
namespace effect_wire=middleware::bap::activity_message::native::status_effect;
namespace scene_wire=middleware::bap::activity_message::scene_sense;

// Scene inputs and exported outputs use the same native event table. An echo of
// an input we published must never become evidence that its performance finished.
struct SceneState {
    std::uint32_t generation{},revision{};
    std::uint32_t selector{UINT32_MAX},serial{UINT32_MAX};
    std::array<std::uint32_t,32> inputs{};
    std::uint8_t count{};
    bool requested{},armed{},started{},stopped{},completed{};
    std::bitset<32> outputs{};
    std::bitset<32> speech{},speechFinished{};
    bool performanceFinished{};
};
struct PlaybackRequest {
    coo::Generation owner{};coo::Asset scene{};std::uint32_t generation{};
    std::uint32_t selector{UINT32_MAX},serial{UINT32_MAX};
};
struct PlaybackReceipt {
    PlaybackRequest request{};std::uint32_t selector{UINT32_MAX},serial{UINT32_MAX};
    std::bitset<32> speech{};bool performanceFinished{};std::bitset<32> outputs{};
    std::bitset<32> speechFinished{};bool completed{};
};
// Scene completion survives selector destruction. The caller verifies the
// component definition/scope and samples these native fields twice. Preserve
// the previously observed selector identity; never invent a new playback.
inline PlaybackReceipt completed_playback(const PlaybackRequest& request,std::uint32_t generation,
    std::uint8_t complete,std::uint32_t selector) noexcept {
    if(!request.owner.valid() || !generation || generation!=request.generation || complete!=1
        || selector!=UINT32_MAX || request.selector==UINT32_MAX || request.serial==UINT32_MAX) return {};
    PlaybackReceipt receipt{request,request.selector,request.serial};receipt.completed=true;return receipt;
}
class Presentation final {
public:
    bool begin(coo::Generation owner) noexcept {
        reset();if(!owner.valid() || owner.value>0x7FFFFFFEU) return false;
        owner_=owner;return true;
    }
    void reset() noexcept {owner_={};scenes_={};effects_={};effectManaged_.reset();music_=UINT8_MAX;}
    coo::Generation owner() const noexcept {return owner_;}
    static constexpr std::size_t scene_index(coo::Asset asset) noexcept {
        for(std::size_t i=0;i<std::size(kScenes);++i) if(kScenes[i].asset==asset) return i;
        return std::size(kScenes);
    }
    bool request_scene(coo::Generation owner,coo::Asset asset) noexcept {
        const auto i=scene_index(asset);
        if(owner!=owner_ || !owner.valid() || i>=scenes_.size()) return false;
        auto& state=scenes_[i];if(state.stopped) return false;
        if(!state.requested) {state.requested=true;state.generation=owner.value;state.revision=1;}
        return true;
    }
    bool input(coo::Generation owner,coo::Asset asset,std::uint32_t event) noexcept {
        const auto i=scene_index(asset);
        if(owner!=owner_ || !owner.valid() || i>=scenes_.size()) return false;
        auto& state=scenes_[i];const auto& binding=kScenes[i];
        if(!state.requested || state.stopped) return false;
        bool declared{};for(const auto allowed:binding.events) declared|=event==allowed;
        if(!declared) return false;
        for(const auto emitted:binding.emitted) if(event==emitted) return false;
        for(std::size_t n=0;n<state.count;++n) if(state.inputs[n]==event) return true;
        if(state.count>=state.inputs.size()) return false;
        state.inputs[state.count++]=event;if(state.started) ++state.revision;return true;
    }
    // Creating a selector before its object cast exists can bind an empty actor
    // cell for the whole performance. Publish only after native object receipts.
    bool arm(coo::Generation owner,coo::Asset asset) noexcept {
        const auto i=scene_index(asset);
        if(owner!=owner_ || !owner.valid() || i>=scenes_.size()) return false;
        auto& state=scenes_[i];
        if(!state.requested || state.stopped) return false;
        state.armed=true;return true;
    }
    bool stop_scene(coo::Generation owner,coo::Asset asset) noexcept {
        const auto i=scene_index(asset);
        if(owner!=owner_ || !owner.valid() || i>=scenes_.size() || !scenes_[i].requested) return false;
        auto& state=scenes_[i];if(!state.stopped) {state.stopped=true;++state.revision;}return true;
    }
    bool observe(coo::Generation owner,coo::Asset asset,const scene_wire::Output& receipt) noexcept {
        const auto i=scene_index(asset);
        if(owner!=owner_ || !owner.valid() || i>=scenes_.size() || !receipt.delta
            || receipt.eventCount>receipt.events.size() || receipt.state>3) return false;
        auto& state=scenes_[i];
        if(!state.requested || state.stopped || receipt.generationWire!=(state.generation^0x80000000U)
            || !receipt.hasSourceRevision || receipt.sourceRevision!=state.revision) return false;
        // Playback is established by the selector observer, never by this echo.
        if(!state.started) return false;
        if(receipt.completed) state.completed=true;
        for(std::size_t n=0;n<receipt.eventCount;++n) {
            const auto event=receipt.events[n];bool echo{};
            for(std::size_t p=0;p<state.count;++p) echo|=state.inputs[p]==event;
            if(echo) continue;
            for(std::size_t p=0;p<kScenes[i].events.size();++p)
                if(kScenes[i].events[p]==event) state.outputs.set(p);
        }
        return true;
    }
    bool playback(const PlaybackReceipt& receipt) noexcept {
        const auto i=scene_index(receipt.request.scene);
        if(receipt.request.owner!=owner_ || !owner_.valid() || i>=scenes_.size()
            || receipt.selector==UINT32_MAX || receipt.serial==UINT32_MAX) return false;
        auto& state=scenes_[i];
        if(!state.requested || !state.armed || state.stopped || (receipt.completed && !state.started)
            || receipt.request.generation!=state.generation
            || (state.selector!=UINT32_MAX && (state.selector!=receipt.selector || state.serial!=receipt.serial))) return false;
        if(!state.started && state.count) ++state.revision;
        state.selector=receipt.selector;state.serial=receipt.serial;state.started=true;
        state.completed|=receipt.completed;
        state.performanceFinished|=receipt.performanceFinished;
        state.speech|=receipt.speech|receipt.speechFinished;state.speechFinished|=receipt.speechFinished;
        for(std::size_t n=0;n<kScenes[i].events.size();++n) if(receipt.outputs[n]) {
            for(const auto emitted:kScenes[i].emitted) if(emitted==kScenes[i].events[n]) state.outputs.set(n);
        }
        return true;
    }
    bool output(coo::Asset asset,std::uint32_t event) const noexcept {
        const auto i=scene_index(asset);if(i>=scenes_.size() || !scenes_[i].started) return false;
        for(std::size_t n=0;n<kScenes[i].events.size();++n)
            if(kScenes[i].events[n]==event) return scenes_[i].outputs[n];
        return false;
    }
    const SceneState* scene(coo::Asset asset) const noexcept {
        const auto i=scene_index(asset);return i<scenes_.size()?&scenes_[i]:nullptr;
    }
    // Only player effects supported by this adapter. Ghost/light/environment
    // targets require authored collections and cannot use the all-player selector.
    static constexpr bool player_effect(std::uint16_t slot) noexcept {
        return slot<24 && slot!=10 && slot!=11 && slot!=13 && slot!=17 && slot!=20 && slot!=23;
    }
    bool effect(coo::Generation owner,std::uint16_t slot,bool enabled,bool once=false) noexcept {
        if(owner!=owner_ || !owner.valid() || !player_effect(slot)) return false;
        const auto* binding=find(kMain,26,slot);
        if(!binding || binding->component!=effect_wire::kComponentClass || binding->authority!=effect_wire::kSchema) return false;
        auto& request=effects_[slot];
        if(effectManaged_[slot] && request.enabled==enabled && request.once==once && !once) return true;
        if(request.selectionRevision==INT32_MAX) return false;
        const auto revision=effectManaged_[slot]?request.selectionRevision+1:static_cast<std::int32_t>(owner_.value);
        request={kMain,slot,kBubble,enabled,once,revision};effectManaged_.set(slot);return true;
    }
    static constexpr std::uint16_t ghost_collection(std::uint16_t slot) noexcept {
        return slot==17?246:slot==20?248:UINT16_MAX;
    }
    bool ghost_effect(coo::Generation owner,std::uint16_t slot,bool enabled) noexcept {
        if(owner!=owner_ || !owner.valid() || ghost_collection(slot)==UINT16_MAX) return false;
        auto& request=effects_[slot];
        // A persistent native output must not restart the one-shot every tick.
        if(effectManaged_[slot] && request.enabled==enabled) return true;
        if(request.selectionRevision==INT32_MAX) return false;
        const auto revision=effectManaged_[slot]?request.selectionRevision+1:static_cast<std::int32_t>(owner.value);
        request={kMain,slot,kBubble,enabled,true,revision};effectManaged_.set(slot);return true;
    }
    bool music(coo::Generation owner,std::uint8_t section) noexcept {
        if(owner!=owner_ || !owner.valid() || section>=std::size(kMusic)) return false;
        music_=section;return true;
    }
    std::size_t bits(coo::Asset asset) const noexcept {
        if(!owner_.valid()) return 0;
        if(asset==kMusicAsset) return music_<std::size(kMusic)?128U+129U*55U:0;
        if(asset.registry==kMain && asset.type==26 && asset.slot<effects_.size() && effectManaged_[asset.slot])
            return ghost_collection(asset.slot)!=UINT16_MAX?186:effect_wire::bits(effects_[asset.slot]);
        if(asset.registry==kMain && asset.type==34)
            for(const auto slot:{17U,20U}) if(asset.slot==ghost_collection(static_cast<std::uint16_t>(slot)) && effectManaged_[slot]) return 94;
        const auto i=scene_index(asset);if(i>=scenes_.size() || !scenes_[i].requested || !scenes_[i].armed) return 0;
        const auto& state=scenes_[i];
        return coo::native_scene::cast_bits(state.stopped?0:source_cast(kScenes[i]).count,state.stopped || !state.started?0:state.count);
    }
    template<class Writer> bool write(Writer& writer,coo::Asset asset) const noexcept {
        if(!bits(asset)) return false;
        if(asset==kMusicAsset) return coo::native_music::select(writer,music_);
        if(asset.type==26) {
            const auto& request=effects_[asset.slot];const auto collection=ghost_collection(asset.slot);
            if(collection==UINT16_MAX) return effect_wire::write_authority(writer,request);
            if(!writer.write(1,1) || !writer.write(request.enabled?0U:1U,1)) return false;
            for(unsigned i=0;i<4;++i)
                if(!writer.write(0x80000000U+(i==3?static_cast<std::uint32_t>(request.selectionRevision):0U),32)) return false;
            return writer.write(kMain,32) && writer.write(35,7) && writer.write(32768U+collection,16) && writer.write(0,1);
        }
        if(asset.type==34) {
            // Native single-entity selector, scoped to o_ghost_finding. Never
            // substitute the all-player selector for these two Ghost effects.
            return writer.write(1,4) && writer.write(1,1) && writer.write(0x80809579U,32)
                && writer.write(1,2) && writer.write(kMain,32) && writer.write(5,7) && writer.write(32768U+118U,16);
        }
        const auto i=scene_index(asset);const auto& state=scenes_[i];
        if(state.stopped) return coo::native_scene::cast_scene(writer,state.generation,{}, {},state.revision,true);
        const auto cast=source_cast(kScenes[i]);
        return coo::native_scene::cast_scene(writer,state.generation,std::span(cast.values).first(cast.count),
            std::span(state.inputs).first(state.started?state.count:0),state.revision);
    }
    struct Cast {std::array<coo::Asset,15> values{};std::size_t count{};};
    static constexpr Cast source_cast(const Scene& scene) noexcept {
        Cast result{};
        for(const auto& member:scene.cast) if(member.type==1 || member.type==4) {
            if(result.count==result.values.size()) return {};
            result.values[result.count++]=member;
        }
        return result;
    }
private:
    coo::Generation owner_{};
    std::array<SceneState,std::size(kScenes)> scenes_{};
    std::array<effect_wire::Request,24> effects_{};
    std::bitset<24> effectManaged_{};
    std::uint8_t music_{UINT8_MAX};
};
static_assert([] {
    for(const auto& scene:kScenes) {
        std::size_t count{};for(const auto& member:scene.cast) if(member.type==1 || member.type==4) ++count;
        if(count>15 || scene.events.size()>32 || Presentation::source_cast(scene).count!=count) return false;
    }
    return true;
}());
}
