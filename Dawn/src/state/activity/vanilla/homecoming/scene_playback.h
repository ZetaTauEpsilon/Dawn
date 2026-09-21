#pragma once
#include "mission.h"
#include "scene_actions.h"
#include "../../coo/lifecycle_service.h"
#include <bitset>
namespace dawn::state::activity::vanilla::homecoming {
// Offsets are relative to the installed selector prototype at 0x90. A stopped
// child is cancellation, never a natural performance-completion receipt.
struct Performance {coo::Asset scene;std::uint32_t graph,root,node,definition,kind;};
inline constexpr Performance kPerformances[]{
    {asset(kUnderwatch,43,17),0x80C3DEF5,0xEDF8,0x6C30,0x114E8,0x808062FE},
    {asset(kBoulevard,43,16),0x80C3DD7D,0xC878,0x63E0,0xED78,0x808062FE},
    {asset(kPlaza,43,2),0x80C3DEBD,0xB258,0x1CC0,0xBEB0,0x80806297},
    {asset(kPlaza,43,4),0x80BEB7A1,0x2868,0xA90,0x2D78,0x808062FE},
};
// Revival sends only B8C5C0A5, not the initial arrival's Cabal branch. Its
// animation 52 completes into idle 53 via input 56; action 59/10 never runs.
inline constexpr Performance kZavalaRevival{asset(kPlaza,43,2),0x80C3DEBD,0xB258,0x6670,0xD720,0x80806307};
inline constexpr std::uint32_t kRevivalAnimation=0x80F1FCB6,kRevivalIdleNode=0x68D0,
    kRevivalIdleDefinition=0xD7D8,kRevivalIdleAnimation=0x80BFA697;
// Zavala's arrival animation 80B8263B (action 59) emits input 63 on
// completion. Its downstream flag removal (action 10) is the handoff. The
// old 397A672B emitter at +1290 waits on a separate branch (input 55) that
// remained dormant in the captured plaza stall; it is not arrival completion.
// Shaxx graph 80BEB7EF child 26 (80B3A3F7) drives the exit DOOR, not his
// speech. It can remain active indefinitely. Never gate the gun rack or radio
// dialogue on it; observed speech rows 21-24 provide Shaxx's voice window.
// Its START is the door-opening beat. Keep this separate from kPerformances:
// the persistent door child must never reserve dialogue or gate on completion.
struct EntryPerformance {coo::Asset scene;std::uint32_t graph,root,node,definition,child,childRoot;};
inline constexpr EntryPerformance kShaxxDoorOpening{
    asset(kUnderwatch,43,14),0x80BEB7EF,0x52A8,0x3920,0x6718,0x80B3A863,0xC60};
constexpr bool door_opening_started(std::uint8_t state,std::int32_t stops) noexcept {return state==1 && stops==0;}
struct PlaybackRequest {coo::Generation owner{};coo::Asset scene{};std::uint32_t generation{};bool revival{};};
// Wait for the actual Cabal hold before submitting its external release;
// selector creation alone can precede acquisition and protection setup.
struct CombatHold {coo::Asset scene;std::uint32_t graph,root,node,definition,input;};
inline constexpr CombatHold kCombatHolds[]{
    {asset(kUnderwatch,43,111),0x80C3DF11,0x5528,0x24C0,0x6398,34},
    {asset(kMilitary,43,70),0x80C3DD7F,0x7388,0x4AF0,0x9048,46},
    {asset(kMilitary,43,72),0x80C3DD7F,0x7388,0x4AF0,0x9048,46},
    {asset(kMilitary,43,74),0x80C3DD7F,0x7388,0x4AF0,0x9048,46},
    {asset(kMilitary,43,77),0x80C3DD81,0x3E88,0x1760,0x4838,24},
};
constexpr const CombatHold* combat_hold_for(coo::Asset scene) noexcept {
    for(const auto& hold:kCombatHolds) if(hold.scene==scene) return &hold;
    return nullptr;
}
constexpr const Performance* performance_for(const PlaybackRequest& r) noexcept {
    if(r.revival) {return r.scene==kZavalaRevival.scene?&kZavalaRevival:nullptr;}
    for(const auto& p:kPerformances) {if(p.scene==r.scene) {return &p;}}return nullptr;
}
constexpr bool revival_handoff(std::uint8_t animation,std::uint8_t idle) noexcept {return animation==2 && idle==1;}
struct PlaybackReceipt {
    PlaybackRequest request{};std::uint32_t selector{UINT32_MAX},serial{UINT32_MAX};
    bool performanceFinished{},entryCue{};std::bitset<128> speech{};std::uint16_t combatReleased{},damageReleased{};
    bool combatHeld{};
};
constexpr bool natural_performance(std::uint8_t state,std::int32_t stops) noexcept {return state==2 && stops==0;}
// Receipts are monotonic within a scene generation. Once every consumed fact
// is latched, walking its live selector again cannot advance the mission. Do
// not stop the native scene or retire its actors: only retire our observer.
// SceneState resets on each new scene request, including Ward/revival re-entry.
template<class State> bool playback_pending(const PlaybackRequest& request,const State& state) noexcept {
    if(!request.owner.valid()) return false;
    if(!state.started) return true;
    if(performance_for(request) && !state.performanceFinished) return true;
    if((request.scene==kShaxxDoorOpening.scene || request.scene==asset(kBoulevard,43,16)) && !state.entryCue) return true;
    if(combat_hold_for(request.scene) && !state.combatHeld) return true;
    for(const auto& source:kSceneActionSources) {
        if(source.definition!=request.scene.definition) continue;
        const auto* plan=scene_action_plan(source.graph,source.root);
        if(!plan) return true; // incomplete inventory must never suppress observation
        for(const auto& action:plan->actions) {
            if(action.kind==SceneActionKind::speech) {
                if(!state.speech[action.value]) return true;
            } else {
                const auto bits=action.kind==SceneActionKind::combat?state.combatReleased:state.damageReleased;
                if(!(bits&(1U<<action.value))) return true;
            }
        }
    }
    return false;
}
// Only scenes that produce consumed receipts need native inspection. Derive
// explicit progression waits from the mission graph and union them with the
// complete package-derived speech/flag-action inventory. Decorative civilians
// keep running natively; no actions, actors or scene updates are suppressed.
inline const coo::Asset* playback_scene(std::uint32_t definition) noexcept {
    static const auto watched=[] {
        std::array<bool,std::size(kScenes)> result{};
        for(std::size_t i=0;i<std::size(kScenes);++i) {
            for(const auto tag:kSceneActionDefinitions) if(kScenes[i].asset.definition==tag) result[i]=true;
            for(const auto& phase:mission().phases) for(const auto& step:phase.definition.steps)
                for(const auto& command:step.commands) if(command.operation==coo::Operation::observation
                    && command.asset==kScenes[i].asset) result[i]=true;
        }
        return result;
    }();
    for(std::size_t i=0;i<std::size(kScenes);++i)
        if(watched[i] && kScenes[i].asset.definition==definition) return &kScenes[i].asset;
    return nullptr;
}
}
