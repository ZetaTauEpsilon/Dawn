#pragma once
#include "presentation.h"
#include "cinematics.h"
#include "../../coo/object_service.h"
#include "../../coo/population_service.h"
#include "../../coo/objective_service.h"
#include "../../coo/native_activity_clock.h"
#include "../../../../middleware/bap/activity_message/object_sense.h"
#include "../../../../middleware/bap/activity_message/source_sense.h"

namespace dawn::state::activity::vanilla::adieu {
enum class Stage : std::uint8_t { city,finding,escape,outskirts,campCombat,gap,bowlCombat,canyon,ending };
inline constexpr coo::Asset kModule{kRoot,kScenario,0,0};
inline constexpr auto kPickup=asset(kMain,4,126),kWeapon=asset(kMain,4,127),kFinding=asset(kMain,43,102);
// Not every reunion bank row runs: row 16 remains dormant in the captured
// native playthrough. Protect whichever lines actually start.
inline constexpr std::size_t kReunionFirstDialogueRow=14,kReunionDialogueRow=16;
inline constexpr std::uint32_t kReunionDepartureCue=0xB01280FEU;
// A reconstructed request of one actor per loose source. This is not a recovered
// retail quota. Scene-owned primer beasts are reserved separately.
struct Combat {std::uint32_t registry;std::uint16_t source,rule,tactical;std::uint8_t count;bool required;};
inline constexpr auto kCombat=[] {
    std::array<Combat,24> out{};
    for(std::size_t i=0;i<out.size();++i) {
        const auto slot=static_cast<std::uint16_t>(i<12?174+i:161+i-12);
        for(const auto& source:kSources) if(source.asset.registry==kMain && source.asset.slot==slot)
            out[i]={kMain,slot,source.rule,static_cast<std::uint16_t>(i<12?186:159),1,true};
    }
    return out;
}();
inline constexpr auto kObjects=[] {
    std::array<coo::ObjectBinding,[] {std::size_t n{};for(const auto& b:kBindings) if(b.asset.type==4) ++n;return n;}()> out{};
    std::size_t i{};for(const auto& b:kBindings) if(b.asset.type==4) out[i++]={b.asset,{},0.F,true};return out;
}();
constexpr std::size_t object_index(coo::Asset a) noexcept {
    for(std::size_t i=0;i<kObjects.size();++i) if(kObjects[i].source==a) return i;return kObjects.size();
}
struct EnemyReceipt {
    std::uint64_t run{};std::uint32_t actor{UINT32_MAX},owner{UINT32_MAX},generation{};
    std::uint16_t source{};std::uint32_t registry{};
    bool valid() const noexcept {return run && generation && registry && actor!=UINT32_MAX && owner!=UINT32_MAX;}
    friend bool operator==(const EnemyReceipt&,const EnemyReceipt&)=default;
};
struct GrantRequest {
    coo::ObjectReceipt binding{};
    bool valid() const noexcept {return binding.valid() && binding.source==kPickup;}
    friend bool operator==(const GrantRequest&,const GrantRequest&)=default;
};
struct CombatState {std::uint32_t generation{},sourceOwner{UINT32_MAX},known{};std::array<std::uint8_t,24> costs{};std::int8_t row{-1};bool active{},cleared{};};
struct Frame {
    bool enabled{},finished{},fault{},restricted{true},healed{},weaponUsed{},weaponGranted{};
    std::uint8_t bubble{kBubble},activeRow{coo::kNoDialogue};Stage stage{};
    std::uint32_t spawnGeneration{},revision{},objective{};std::uint64_t gameplayClockTicks{};
    std::array<std::uint32_t,std::size(kDialogue)> generations{};
    std::array<coo::ObjectState,kObjects.size()> objects{};
    std::bitset<kObjects.size()> managedObjects{};
    std::array<CombatState,kCombat.size()> combat{};
    std::bitset<6> sequences{};
    std::array<std::uint64_t,6> sequenceStartTicks{};
    bool ghostRetired{};
    Presentation native{};cinematics::State cinematic{};
    coo::ObjectiveState presentation{};coo::CompletionPublication completion{};
};
struct Request {coo::Generation owner{};Frame frame{};};
bool contains(const Volume&,Point) noexcept;
bool crosses(const Volume&,Point,Point) noexcept;
class Controller final {
public:
    void reset() noexcept;
    bool select(std::uint64_t run,std::uint64_t now=0) noexcept;
    bool advance(std::uint64_t run,std::uint64_t now,bool ready) noexcept;
    const Frame& frame() const noexcept {return frame_;}
    coo::Generation owner() const noexcept {return lifecycle_.owner();}
    bool arrival(coo::Generation,std::uint8_t route,std::uint64_t now) noexcept;
    bool cinematic(coo::Generation,const cinematics::Incident&,std::uint64_t now) noexcept;
    void position(coo::Generation,Point) noexcept;
    bool scene(coo::Generation,coo::Asset,const scene_wire::Output&) noexcept;
    bool playback(const PlaybackReceipt&,std::uint64_t now) noexcept;
    bool prepared(coo::Generation,coo::Asset) noexcept;
    bool object(const coo::ObjectReceipt&) noexcept;
    bool use(coo::Generation,coo::Asset,const middleware::bap::activity_message::object_sense::Output&) noexcept;
    GrantRequest grant_request() const noexcept;
    bool granted(const GrantRequest&,std::uint64_t instance) noexcept;
    bool admitted(const EnemyReceipt&) noexcept;
    bool died(const EnemyReceipt&) noexcept;
    bool source(coo::Generation,coo::Asset,const middleware::bap::activity_message::source_sense::Output&) noexcept;
    bool readiness(const EnemyReceipt& r,coo::EnemyReadiness state) noexcept {return population_.observe(r,state);}
    template<class Visit> void pending_enemies(Visit visit) const noexcept {population_.pending(visit);}
    bool submitted(coo::Generation,std::uint32_t bank,std::uint8_t row,std::uint32_t generation,std::uint64_t now) noexcept;
private:
    bool current(coo::Generation gen) const noexcept {return gen.valid() && gen==owner() && frame_.enabled && !frame_.fault && !frame_.finished;}
    bool gameplay() const noexcept {return cinematic_.state().phase==cinematics::Phase::gameplay;}
    bool seen(std::string_view name) const noexcept;
    bool once(std::string_view name) noexcept;
    void request_scene(std::uint16_t slot,std::uint32_t event=0) noexcept;
    void request_object(std::uint16_t slot) noexcept;
    void retire_object(std::uint16_t slot) noexcept;
    void retire_ghost() noexcept;
    void retire_section() noexcept;
    void objective(std::size_t index) noexcept;
    void speech(std::uint8_t row) noexcept;
    void music(std::uint8_t ordinal) noexcept;
    void effect(std::uint16_t slot,bool enabled,bool once=false) noexcept;
    void combat(std::size_t first) noexcept;
    bool cleared(std::size_t first) const noexcept;
    void cues() noexcept;
    void project() noexcept;
    coo::LifecycleService lifecycle_{};coo::NativeActivityClock clock_{};cinematics::Sequence cinematic_{};Frame frame_{};
    coo::ObjectService<kObjects.size()> objects_{};
    coo::PopulationService<EnemyReceipt,kCombat.size(),1> population_{};
    coo::DialogueService<std::size(kDialogue)> dialogue_{};coo::ObjectiveService objectives_{};
    std::bitset<std::size(kVolumes)> visited_{},handled_{};
    Point previous_{};bool hasPrevious_{},collapse_{},healing_{};std::uint64_t now_{},sceneVoiceUntil_{},reunionVoiceUntil_{};
    GrantRequest grant_{};
};
}
