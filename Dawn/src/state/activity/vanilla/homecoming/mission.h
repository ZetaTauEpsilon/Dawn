#pragma once
#include "native_catalog.h"
#include <initializer_list>

namespace dawn::state::activity::vanilla::homecoming {
// Authored registries. Bubble 9 Underwatch, 4 Military, 6 Plaza, 0 Boulevard, 8 command ship.
inline constexpr std::uint32_t kRuntime=0x4786C0E0U,kUnderwatch=0x9D8076E4U,kUnderwatchRoute=0x9027B6A1U,
    kUnderwatchExit=0x026087A0U,kMilitary=0xAA9D42BEU,kMilitaryRoute=0xD3847A1FU,kPlaza=0x28A6B21FU,
    kPlazaProps=0xC0C5E876U,kPlazaRoute=0xBB7B62E0U,kPlazaAreas=0xF8F959CDU,kBoulevard=0x7BA8F95DU,
    kBoulevardRoute=0xA73F44A8U,kShip=0x2D322467U,kShipRoute=0x002D225EU;
inline constexpr coo::Asset kModule{kRoot,kScenario,0,0};
enum class Section : std::uint8_t { underwatch,armory,military,plaza,plazaWaves,boulevard,ship,generator,escape,count };
constexpr std::uint8_t bubble_of(Section s) noexcept {
    switch(s) {
    case Section::underwatch:case Section::armory:return 9;
    case Section::military:return 4;
    case Section::plaza:case Section::plazaWaves:return 6;
    case Section::boulevard:return 0;
    default:return 8;
    }
}
enum class Mechanic : std::uint32_t { restrict=1,allow,finishSection,arm,disarm,stopScene,retireMember,powerOff,pickup,releaseDoor };
// Observations the controller derives from several receipts rather than one asset.
enum class Milestone : std::uint32_t { caydeNear=0x300,shaxxNear,consoleScanned,generatorA,generatorB,generatorC };
// Type-4 observation arguments.
enum class ObjectEvent : std::uint32_t { present=1,used };
// The dead Zavala interactable is the only native use subscription in this mission.
inline constexpr auto kReviveInteract=asset(kPlaza,4,24),kConsoleLink=asset(kShip,65,166),kBazaarDoor=asset(kBoulevard,23,9);
constexpr bool use_subscription(coo::Asset a) noexcept {return a==kReviveInteract;}
enum class Cohort : std::uint8_t { underwatchCast,breachBackup,breachApproach,centurionRush,centurionBackup,heroMoment,postGun,
    hallwayDestruction,overlookStart,gateStartClear,friendlies,fakeFight,frames,corridor,gateHangarClear,hangarFloor,
    plazaInit,wave1,wave2,wave3,bazaarStart,bazaarMid,bazaarFar,bazaarEnd,bazaarAll,blasted,
    pods,damaged,hall,stairs,deckA,deckB,hardpoints,boss,airlock,matrix,engineUpper,engineLower,generatorGuards,escape,count };
struct Member {std::uint32_t registry;std::uint16_t slot;};
struct CohortBinding {Cohort id;std::span<const Member> members;};
template<std::uint32_t Registry,std::size_t N>
consteval auto cohort_members(const std::uint16_t (&slots)[N]) noexcept {
    std::array<Member,N> out{};
    for(std::size_t i=0;i<N;++i) {out[i]={Registry,slots[i]};}return out;
}
// Encounter membership is reconstruction policy (Sunrise v40 declarations). Native
// source definitions still own category selection, placement and actor creation.
inline constexpr auto kUnderwatchCast=cohort_members<kUnderwatch>({42,113,12,13,34,35,36,37,38,39,40,41,44,67});
inline constexpr auto kBreachBackup=cohort_members<kUnderwatch>({8,9});
// Clearance only: the breach and Centurion actors are Scene-owned and never placed loosely.
inline constexpr auto kBreachApproach=cohort_members<kUnderwatch>({6,8,9,20,22,23});
inline constexpr auto kCenturionRush=cohort_members<kUnderwatch>({22});
inline constexpr auto kCenturionBackup=cohort_members<kUnderwatch>({23});
inline constexpr auto kHeroMoment=cohort_members<kUnderwatch>({65,66});
inline constexpr auto kPostGun=cohort_members<kUnderwatch>({112,114,115});
inline constexpr auto kHallwayDestruction=cohort_members<kMilitary>({4});
inline constexpr auto kOverlookStart=cohort_members<kMilitary>({5,6,7,8,11});
// The overlook fight with the allied frames (source 7) does not gate the first door.
inline constexpr auto kGateStartClear=cohort_members<kMilitary>({4,5,6,8,11});
inline constexpr auto kFriendlies=cohort_members<kMilitary>({32,33});
inline constexpr auto kFakeFight=cohort_members<kMilitary>({66,67,68});
inline constexpr auto kFrames=cohort_members<kMilitary>({69,71,73,75,76});
// Source 17's authored point spawns outside the corridor walls; its three Legionaries are
// requested through source 13's point instead (count vector 1/3/2), plus the sniper.
inline constexpr auto kCorridor=cohort_members<kMilitary>({13,18});
inline constexpr auto kGateHangarClear=cohort_members<kMilitary>({13,18});
inline constexpr auto kHangarFloor=cohort_members<kMilitary>({19,24,26,30,34,36,38});
inline constexpr auto kPlazaInit=cohort_members<kPlaza>({12,13,27});
inline constexpr auto kWave1=cohort_members<kPlaza>({28,29,40,41});
inline constexpr auto kWave2=cohort_members<kPlaza>({31,33,34,35});
inline constexpr auto kWave3=cohort_members<kPlaza>({36,37,38,39});
inline constexpr auto kBazaarStart=cohort_members<kBoulevard>({6,11,8});
inline constexpr auto kBazaarMid=cohort_members<kBoulevard>({7,4});
inline constexpr auto kBazaarFar=cohort_members<kBoulevard>({3});
inline constexpr auto kBazaarEnd=cohort_members<kBoulevard>({5,2});
inline constexpr auto kBazaarAll=cohort_members<kBoulevard>({6,11,8,7,4,3,5,2});
inline constexpr auto kBlasted=cohort_members<kBoulevard>({19,20,21,22});
inline constexpr auto kPods=cohort_members<kShip>({4});
inline constexpr auto kDamaged=cohort_members<kShip>({5});
inline constexpr auto kHall=cohort_members<kShip>({6,7,8,9});
inline constexpr auto kStairs=cohort_members<kShip>({10,11});
inline constexpr auto kDeckA=cohort_members<kShip>({12,16,17});
inline constexpr auto kDeckB=cohort_members<kShip>({18,22,23,24});
inline constexpr auto kHardpoints=cohort_members<kShip>({29,30,31,32});
inline constexpr auto kBoss=cohort_members<kShip>({35,33});
inline constexpr auto kAirlock=cohort_members<kShip>({36,37,38,39,40});
inline constexpr auto kMatrix=cohort_members<kShip>({41,42,43,44});
inline constexpr auto kEngineUpper=cohort_members<kShip>({45,46,47});
inline constexpr auto kEngineLower=cohort_members<kShip>({48,49});
inline constexpr auto kGeneratorGuards=cohort_members<kShip>({50,51,52});
inline constexpr auto kEscape=cohort_members<kShip>({54,55});
inline constexpr CohortBinding kCohorts[]{
    {Cohort::underwatchCast,kUnderwatchCast},{Cohort::breachBackup,kBreachBackup},{Cohort::breachApproach,kBreachApproach},
    {Cohort::centurionRush,kCenturionRush},{Cohort::centurionBackup,kCenturionBackup},{Cohort::heroMoment,kHeroMoment},{Cohort::postGun,kPostGun},
    {Cohort::hallwayDestruction,kHallwayDestruction},{Cohort::overlookStart,kOverlookStart},{Cohort::gateStartClear,kGateStartClear},
    {Cohort::friendlies,kFriendlies},{Cohort::fakeFight,kFakeFight},{Cohort::frames,kFrames},{Cohort::corridor,kCorridor},
    {Cohort::gateHangarClear,kGateHangarClear},{Cohort::hangarFloor,kHangarFloor},
    {Cohort::plazaInit,kPlazaInit},{Cohort::wave1,kWave1},{Cohort::wave2,kWave2},{Cohort::wave3,kWave3},
    {Cohort::bazaarStart,kBazaarStart},{Cohort::bazaarMid,kBazaarMid},{Cohort::bazaarFar,kBazaarFar},{Cohort::bazaarEnd,kBazaarEnd},
    {Cohort::bazaarAll,kBazaarAll},{Cohort::blasted,kBlasted},
    {Cohort::pods,kPods},{Cohort::damaged,kDamaged},{Cohort::hall,kHall},{Cohort::stairs,kStairs},{Cohort::deckA,kDeckA},{Cohort::deckB,kDeckB},
    {Cohort::hardpoints,kHardpoints},{Cohort::boss,kBoss},{Cohort::airlock,kAirlock},{Cohort::matrix,kMatrix},
    {Cohort::engineUpper,kEngineUpper},{Cohort::engineLower,kEngineLower},{Cohort::generatorGuards,kGeneratorGuards},{Cohort::escape,kEscape}
};
static_assert(std::size(kCohorts)==static_cast<std::size_t>(Cohort::count));
// Per-category loose request counts. Defaults request every authored category once;
// the plaza waves and the relocated corridor request more of an authored category.
struct RequestOverride {std::uint32_t registry;std::uint16_t source;std::array<std::uint8_t,4> requests;};
inline constexpr RequestOverride kRequestOverrides[]{
    {kMilitary,13,{1,3,2,0}},
    {kPlaza,29,{3,0,0,0}},{kPlaza,40,{3,0,0,0}},{kPlaza,41,{3,0,0,0}},{kPlaza,31,{4,0,0,0}},
    {kPlaza,34,{3,0,0,0}},{kPlaza,35,{3,0,0,0}},{kPlaza,36,{3,0,0,0}},{kPlaza,37,{3,0,0,0}},
    {kPlaza,38,{3,0,0,0}},{kPlaza,39,{3,0,0,0}},
};
constexpr std::array<std::uint8_t,4> requests(const Spawn& p) noexcept {
    for(const auto& o:kRequestOverrides) {if(o.registry==p.registry && o.source==p.source) {return o.requests;}}
    std::array<std::uint8_t,4> out{};
    for(std::size_t i=0;i<p.categories && i<4;++i) {out[i]=1;}return out;
}
// Expected admitted actors for one source: requests times authored members per category.
constexpr std::uint8_t expected(const Spawn& p) noexcept {
    unsigned total{};const auto r=requests(p);
    for(std::size_t i=0;i<4;++i) {total+=static_cast<unsigned>(r[i])*p.members[i];}
    return static_cast<std::uint8_t>(total>63?63:total);
}
// Explicit native spawn rules. The hallway destruction Legionary's authored point is
// external to its source; its authored rule sr_military_hallway_destruction names it.
struct RuleOverride {std::uint32_t registry;std::uint16_t source,rule;};
inline constexpr RuleOverride kRuleOverrides[]{{kMilitary,4,172}};
constexpr const RuleOverride* rule_override(const Spawn& p) noexcept {
    for(const auto& o:kRuleOverrides) {if(o.registry==p.registry && o.source==p.source) {return &o;}}return nullptr;
}
// The authored task group is the objective binding; the native cost pass picks the row.
constexpr coo::native_combatant::TacticalGroup tactical(const Spawn& p) noexcept {return p.tactical;}
// One fixed arena for the nine immutable section graphs. Storage never moves; each
// graph's steps and commands are contiguous. Bounds failures invalidate the mission.
struct GraphStorage {
    std::array<coo::CommandSpec,640> commands{};
    std::array<coo::Step,256> steps{};
    std::size_t commandCount{},stepCount{};
};
struct Graph {
    coo::Definition definition{};
    void name(std::string_view n) noexcept;
    std::uint32_t add(std::string_view,std::uint32_t,std::initializer_list<coo::CommandSpec>) noexcept;
private:
    friend struct Mission;
    GraphStorage* storage_{};
    std::size_t first_{};
};
struct Mission {
    GraphStorage storage{};
    std::array<Graph,static_cast<std::size_t>(Section::count)> phases{};
    Mission() noexcept;
    Mission(const Mission&)=delete;
    Mission& operator=(const Mission&)=delete;
    bool valid() const noexcept;
};
const Mission& mission() noexcept;
// Dialogue command arguments carry the row in the low byte and a delay in milliseconds above it.
constexpr std::uint32_t dialogue_row(std::uint32_t argument) noexcept {return argument&0xFFU;}
constexpr std::uint32_t dialogue_delay(std::uint32_t argument) noexcept {return argument>>8;}
} // namespace dawn::state::activity::vanilla::homecoming
