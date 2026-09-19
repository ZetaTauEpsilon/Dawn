#pragma once
#include "mission.h"
#include "cinematics.h"
#include "../../../../middleware/bap/activity_message/object_sense.h"
#include "../../../../middleware/bap/activity_message/device_sense.h"
#include "../../../../middleware/bap/activity_message/source_sense.h"
#include "../../../../middleware/bap/activity_message/ghost_sense.h"
#include "../../coo/lifecycle_service.h"
#include "../../coo/object_service.h"
#include "../../coo/population_service.h"
#include "../../coo/objective_service.h"
#include "../../coo/native_activity_clock.h"
#include "../../coo/stall_diagnostics.h"
#include <bitset>
namespace dawn::state::activity::vanilla::homecoming {
struct EnemyReceipt {
    std::uint64_t run{};std::uint32_t actor{UINT32_MAX},owner{UINT32_MAX},generation{};
    std::uint16_t source{};std::uint32_t registry{};
    bool valid() const noexcept {return run && generation && actor!=UINT32_MAX && owner!=UINT32_MAX && registry;}
    friend bool operator==(const EnemyReceipt&,const EnemyReceipt&)=default;
};
struct NativeState {
    std::uint32_t generation{},sourceOwner{UINT32_MAX};float position{},power{1.F},lock{};
    std::array<std::int32_t,3> deviceVersions{};
    // Measured type-23 pose (native +0x370) and the revision that produced it.
    float observedPosition{};std::int32_t observedRevision{-1};
    std::uint8_t sequenceRevision{},deviceSeen{};
    // Publication serializes fields explicitly; these flags have no wire layout.
    bool managed:1{},desired:1{},prepared:1{},active:1{},acknowledged:1{},snap:1{},deviceSynchronized:1{},sourceCleared:1{},bound:1{},retired:1{},observed:1{},poseKnown:1{};
};
struct SceneState {std::array<std::uint32_t,4> events{};std::uint8_t count{};};
struct TacticalState {
    std::array<std::uint8_t,24> costs{};std::uint32_t known{},revision{};
    std::int8_t group{-1};
};
struct Frame {
    bool enabled{},finished{},restricted{},fault{};std::uint8_t section{},bubble{9},activeRow{coo::kNoDialogue};
    // Selected authored music section of bank 80B5090F; none until the graph starts one.
    std::uint8_t musicSection{music_section::none};
    std::uint32_t spawnGeneration{},revision{},objective{};std::uint64_t gameplayClockTicks{};
    std::array<std::uint32_t,std::size(kDialogue)> generations{};
    std::array<NativeState,std::size(kAssets)> native{};
    std::array<TacticalState,kSpawns.size()> tactics{};
    std::array<SceneState,std::size(kScenes)> scenes{};
    coo::ObjectiveState presentation{};coo::CompletionPublication completion{};
    cinematics::State cinematic{};
    // Native receipts the graph observes: the console scan, Zavala's revival and the
    // bazaar door's authored destruction handoff.
    bool consoleArmed{},consoleStarted{},consoleScanned{},reviveArmed{},reviveUsed{},doorReleased{},doorHandled{};
    std::array<bool,3> generatorDown{};
};
struct Request {coo::Generation owner{};Frame frame{};};
struct MountedPosition {
    coo::Generation owner{};
    std::uint32_t player{UINT32_MAX},vehicle{UINT32_MAX},seat{UINT32_MAX};
    Point position{};
};
constexpr std::size_t asset_index(coo::Asset a) noexcept {
    const auto i=catalog_index(a.registry,a.type,a.slot);
    return i<std::size(kAssets) && kAssets[i].asset==a?i:std::size(kAssets);
}
inline constexpr auto kSpawnSlots=[] {
    std::array<std::uint16_t,std::size(kAssets)> slots{};slots.fill(static_cast<std::uint16_t>(kSpawns.size()));
    for(std::size_t i=0;i<kSpawns.size();++i) {
        const auto& spawn=kSpawns[i];const auto index=catalog_index(spawn.registry,1,spawn.source);
        if(index<slots.size()) {slots[index]=static_cast<std::uint16_t>(i);}
    }
    return slots;
}();
constexpr std::size_t spawn_index(coo::Asset a) noexcept {
    const auto i=catalog_index(a.registry,a.type,a.slot);return i<kSpawnSlots.size()?kSpawnSlots[i]:kSpawns.size();
}
inline constexpr auto kObjects=[] {
    std::array<coo::ObjectBinding,[] {std::size_t n{};for(const auto& a:kAssets) {if(a.asset.type==4) {++n;}}return n;}()> out{};
    std::size_t n{};for(const auto& a:kAssets) {if(a.asset.type==4) {out[n++]={a.asset,{},0.F,true};}}return out;
}();
inline constexpr auto kObjectSlots=[] {
    std::array<std::uint16_t,std::size(kAssets)> slots{};slots.fill(static_cast<std::uint16_t>(kObjects.size()));
    for(std::size_t i=0;i<kObjects.size();++i) {slots[asset_index(kObjects[i].source)]=static_cast<std::uint16_t>(i);}
    return slots;
}();
constexpr std::size_t object_index(coo::Asset a) noexcept {
    const auto i=asset_index(a);return i<kObjectSlots.size()?kObjectSlots[i]:kObjects.size();
}
constexpr std::size_t scene_index(coo::Asset a) noexcept {
    for(std::size_t i=0;i<std::size(kScenes);++i) {if(kScenes[i].asset==a) {return i;}}return std::size(kScenes);
}
inline constexpr std::uint64_t kSettleMs=3000;
bool contains(const Volume&,Point) noexcept;
bool crosses(const Volume&,Point,Point) noexcept;
class Controller final : private coo::Services {
public:
    void reset() noexcept;
    bool select(std::uint64_t,std::uint64_t now=0) noexcept;
    bool fly_in_complete(coo::Generation) noexcept;
    bool arrival(coo::Generation,std::uint8_t,std::uint64_t) noexcept;
    bool cinematic(coo::Generation,const cinematics::Incident&,std::uint64_t) noexcept;
    bool ghost(coo::Generation,coo::Asset,const middleware::bap::activity_message::ghost_sense::Output&) noexcept;
    // Advance without copying the publication frame into temporary adapters.
    bool advance(std::uint64_t,std::uint64_t,bool) noexcept;
    Frame update(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
        return advance(run,now,ready)?frame_:Frame{};
    }
    void position(std::uint64_t,Point) noexcept;
    bool mounted(const MountedPosition&) noexcept;
    bool prepared(coo::Generation,coo::Asset) noexcept;
    bool object(const coo::ObjectReceipt&) noexcept;
    bool device(coo::Generation,coo::Asset,const middleware::bap::activity_message::device_sense::Output&) noexcept;
    bool source(coo::Generation,coo::Asset,const middleware::bap::activity_message::source_sense::Output&) noexcept;
    bool use(coo::Generation,coo::Asset,const middleware::bap::activity_message::object_sense::Output&) noexcept;
    bool admitted(const EnemyReceipt&) noexcept;
    bool died(const EnemyReceipt&) noexcept;
    bool readiness(const EnemyReceipt& r,coo::EnemyReadiness v) noexcept {return population_.observe(r,v);}
    template<class V> void pending_enemies(V v) const noexcept {population_.pending(v);}
    bool submitted(std::uint64_t,std::uint32_t,std::uint8_t,std::uint32_t,std::uint64_t) noexcept;
    bool door_handled() noexcept;
    coo::Generation owner() const noexcept {return lifecycle_.owner();}
    const Frame& frame() const noexcept {return frame_;}
    const Graph& graph() const noexcept {return mission().phases[frame_.section];}
    coo::Diagnostics diagnostics() const noexcept {return executor_.diagnostics();}
    auto step_state(std::size_t i) const noexcept {return executor_.step_state(i);}
    coo::StallDetail missing(const coo::CommandSpec&) const noexcept;
private:
    bool publish(const coo::Command&) noexcept override;
    void cancel(const coo::Command&) noexcept override {}
    bool request(coo::Asset,bool,float=0.F,float power=1.F,float lock=0.F,bool snap=false) noexcept;
    bool observed(const coo::CommandSpec&) const noexcept;
    bool visited(coo::Asset) const noexcept;
    bool cohort(Cohort) noexcept;
    bool cleared(Cohort) const noexcept;
    bool settled(Cohort) const noexcept;
    bool scene_request(coo::Asset,std::uint32_t) noexcept;
    coo::MarkerTarget marker(std::uint32_t event) const noexcept;
    std::uint64_t run_{},now_{};bool arrived_{},started_{},phaseFinished_{};
    coo::Executor executor_{};coo::LifecycleService lifecycle_{};coo::NativeActivityClock clock_{};
    coo::ObjectiveService objectives_{};coo::DialogueService<std::size(kDialogue)> dialogue_{};
    coo::ObjectService<kObjects.size()> objects_{};coo::PopulationService<EnemyReceipt,kSpawns.size(),16> population_{};
    std::bitset<std::size(kVolumes)> seen_{},inside_{};
    Point previousPosition_{};bool hasPosition_{};
    std::bitset<std::size(kDialogue)> submitted_{};std::array<std::uint64_t,std::size(kDialogue)> voiceEnd_{};
    std::array<std::uint64_t,static_cast<std::size_t>(Cohort::count)> clearedSince_{};
    std::array<std::array<std::uint64_t,coo::Executor::kMaxCommands>,coo::Executor::kMaxSteps> requestedAt_{};
    cinematics::Sequence cinematics_{};
    Frame frame_{};
};
}
