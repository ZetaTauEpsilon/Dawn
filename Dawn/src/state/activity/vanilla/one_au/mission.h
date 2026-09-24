#pragma once
#include "native_catalog.h"
#include <initializer_list>

namespace dawn::state::activity::vanilla::one_au {
inline constexpr std::uint32_t kLanding=0xD8CE8390U,kBridge=0xF6FFB59EU,kProcessing=0x382608B7U,
    kTunnel=0xEAA003F3U,kSun=0x992B5554U,kReady=0xE3932CF1U,kAscent=0x35BB48BCU,
    kFoundry=0x57627106U,kAccess=0xA36972D4U,kCore=0xA3B76C64U;
inline constexpr coo::Asset kModule{kRoot,kScenario,0,0};
enum class Section : std::uint8_t { landing,bridge,processing,grinder,tunnel,sunside,ready,ascent,foundry,access,exchangers,core,escape,count };
enum class Mechanic : std::uint32_t { restrict=1,allow,finishSection,arm };
enum class Event : std::uint32_t { started=1,completed,pickedUp,inserted,destroyed };
enum class Milestone : std::uint32_t { lightsEndArrival=0x300,processingDiscovery,readyOneReinforce,readyTwoReinforce,foundryMid,foundryFinal,meatReinforce };
// The archived discovery handler accepts any of these areas OR entry-enemy
// clearance. The carry-object strip is one route, never a mandatory prerequisite.
inline constexpr coo::Asset kProcessingDiscoveryAreas[]{
    volume(0xE3EC5485U,9),volume(0xE3EC5485U,31),volume(0xE3EC5485U,30),volume(kProcessing,146)
};
// Arrival is a route milestone, not the small apex dialogue trigger alone.
// Retain the authored landing area and downstream access/security areas so a
// missed dialogue-volume sample cannot strand the previous section.
inline constexpr coo::Asset kLightsEndArrivalAreas[]{
    volume(0x8162BF78U,30),volume(0x8162BF78U,39),
    volume(kAccess,86),volume(kAccess,87),volume(kAccess,88),volume(kAccess,89),volume(0xFC43D98EU,7)
};
enum class Interaction : std::uint8_t { bridge,processingCell,processingSink,coreCell,coreSink,east,west,coffin,count };
inline constexpr coo::Asset kInteractionAssets[]{
    asset(kBridge,65,60),asset(kProcessing,4,31),asset(kProcessing,4,32),asset(kCore,4,18),asset(kCore,4,19),
    asset(kCore,4,26),asset(kCore,4,34),asset(kCore,4,42)
};
inline constexpr auto kBridgeLink=asset(kBridge,65,60);
enum class Cohort : std::uint8_t { helipad,catwalk,catwalkMid,pipes,mercury,bridge,sunLanding,processingEntry,grinderDefense,
    grinderBoss,tumbler,readyOne,sunEast,sunBridge,sunWest,readyTwo,chamber,meatGrinder,ascent,
    foundryEntry,foundryMid,foundryBoss,accessOne,accessTwo,security,east,west,coffin,mercuryBonus,grinderReinforcements,electronControllers,
    readyOneReinforce,readyTwoReinforce,meatReinforce,sunSecret,eastReinforce,eastFinal,westReinforce,westFinal,count };
struct Member {std::uint32_t registry;std::uint16_t slot;};
struct CohortBinding {Cohort id;std::span<const Member> members;};
// Encounter selection is reconstruction policy. Native source definitions still
// own category/template selection, placement, actor creation and AI initialization.
template<std::uint32_t Registry,std::size_t N>
consteval auto cohort_members(const std::uint16_t (&slots)[N]) noexcept {
    std::array<Member,N> out{};
    for(std::size_t i=0;i<N;++i) {out[i]={Registry,slots[i]};}return out;
}
inline constexpr auto kHelipad=cohort_members<kLanding>({12,13,14});
inline constexpr auto kCatwalk=cohort_members<kLanding>({3,4,5,6,7});
inline constexpr auto kCatwalkMid=cohort_members<kLanding>({8,9,10,11});
// Source 58 is the stray pipe support enemy. Spawn and clearance share this list.
inline constexpr auto kPipes=cohort_members<kBridge>({59});
// 38/39 are bridge_vignette_*, authored to the bridge task group, not the
// landing fight. Holding them in Mercury spawned them across a bridge that has
// not extended yet, and made the console gate wait on squads over the gap.
inline constexpr auto kMercury=cohort_members<kBridge>({40,51,52,53,54,55});
inline constexpr auto kMercuryBonus=cohort_members<kBridge>({42,43,45,47,49});
// kBridge 36, kProcessing 40/42 and kCore 86/88/91/94/97/100 carry no authored
// placement or task group in native-evidence.json. Excluding them did not stop
// squads standing over the unextended bridge and only thinned the encounters,
// so they stay in and take a derived group from their authored neighbours.
inline constexpr auto kBridgeCrossing=cohort_members<kBridge>({33,34,35,36,37});
inline constexpr auto kSunLanding=cohort_members<kBridge>({56,57});
inline constexpr auto kProcessingEntry=cohort_members<kProcessing>({36,38,40,42});
inline constexpr auto kGrinderDefense=cohort_members<kProcessing>({44,45,48,54});
inline constexpr auto kGrinderReinforcements=cohort_members<kProcessing>({46,47,49,50,55});
inline constexpr auto kGrinderBoss=cohort_members<kProcessing>({52,51});
inline constexpr auto kTumbler=cohort_members<kTunnel>({5,7});
inline constexpr auto kReadyOne=cohort_members<kTunnel>({8,9});
inline constexpr auto kReadyOneReinforce=cohort_members<kTunnel>({10,11});
inline constexpr auto kSunEast=cohort_members<kSun>({12,14});
inline constexpr auto kSunBridge=cohort_members<kSun>({10,11});
inline constexpr auto kSunWest=cohort_members<kSun>({16,17,18,19});
inline constexpr auto kSunSecret=cohort_members<kSun>({20});
inline constexpr auto kReadyTwo=cohort_members<kReady>({2,5,6});
inline constexpr auto kReadyTwoReinforce=cohort_members<kReady>({1,3,4});
inline constexpr auto kChamber=cohort_members<kAscent>({12,13});
inline constexpr auto kMeatGrinder=cohort_members<kAscent>({14,16,22});
inline constexpr auto kMeatReinforce=cohort_members<kAscent>({15,19,23});
inline constexpr auto kAscentFight=cohort_members<kAscent>({24,25});
inline constexpr auto kFoundryEntry=cohort_members<kFoundry>({8,9,10,17});
inline constexpr auto kFoundryMid=cohort_members<kFoundry>({11,12,13,18});
inline constexpr auto kFoundryBoss=cohort_members<kFoundry>({14,15,16,19});
inline constexpr auto kAccessOne=cohort_members<kAccess>({8,9,10});
inline constexpr auto kAccessTwo=cohort_members<kAccess>({11,12,13});
inline constexpr auto kSecurity=cohort_members<kAccess>({14,16,18,19,21});
inline constexpr auto kElectronControllers=cohort_members<kAccess>({14,16});
inline constexpr auto kEast=cohort_members<kCore>({56,57,68});
inline constexpr auto kEastReinforce=cohort_members<kCore>({58,59,62,63,69});
inline constexpr auto kEastFinal=cohort_members<kCore>({54,60,61,64,65,66,67});
inline constexpr auto kWest=cohort_members<kCore>({72,73,78});
inline constexpr auto kWestReinforce=cohort_members<kCore>({74,75,79,80,81});
inline constexpr auto kWestFinal=cohort_members<kCore>({70,76,77,82,83,84,85});
inline constexpr auto kCoffin=cohort_members<kCore>({86,88,90,91,92,93,94,95,96,97,98,99,100,101,102});
inline constexpr CohortBinding kCohorts[]{
    {Cohort::helipad,kHelipad},{Cohort::catwalk,kCatwalk},{Cohort::catwalkMid,kCatwalkMid},{Cohort::pipes,kPipes},
    {Cohort::mercury,kMercury},{Cohort::bridge,kBridgeCrossing},{Cohort::sunLanding,kSunLanding},
    {Cohort::processingEntry,kProcessingEntry},{Cohort::grinderDefense,kGrinderDefense},{Cohort::grinderBoss,kGrinderBoss},
    {Cohort::tumbler,kTumbler},{Cohort::readyOne,kReadyOne},{Cohort::sunEast,kSunEast},{Cohort::sunBridge,kSunBridge},
    {Cohort::sunWest,kSunWest},{Cohort::readyTwo,kReadyTwo},{Cohort::chamber,kChamber},{Cohort::meatGrinder,kMeatGrinder},
    {Cohort::ascent,kAscentFight},{Cohort::foundryEntry,kFoundryEntry},{Cohort::foundryMid,kFoundryMid},{Cohort::foundryBoss,kFoundryBoss},
    {Cohort::accessOne,kAccessOne},{Cohort::accessTwo,kAccessTwo},{Cohort::security,kSecurity},
    {Cohort::east,kEast},{Cohort::west,kWest},{Cohort::coffin,kCoffin},{Cohort::mercuryBonus,kMercuryBonus},{Cohort::grinderReinforcements,kGrinderReinforcements},
    {Cohort::electronControllers,kElectronControllers},{Cohort::readyOneReinforce,kReadyOneReinforce},{Cohort::readyTwoReinforce,kReadyTwoReinforce},
    {Cohort::meatReinforce,kMeatReinforce},{Cohort::sunSecret,kSunSecret},{Cohort::eastReinforce,kEastReinforce},{Cohort::eastFinal,kEastFinal},
    {Cohort::westReinforce,kWestReinforce},{Cohort::westFinal,kWestFinal}
};
// The authored task group is the objective binding. The nine sources that ship
// without one take the group their authored neighbours in the same area use, so
// they still reach an objective instead of idling.
constexpr coo::native_combatant::TacticalGroup tactical(const Spawn& p) noexcept {
    if(p.sceneOwned || p.tactical.registry) {
        // Spatial ranking alone tied Mercury support C to the bridge objective
        // because those task areas overlap. Encounter identity resolves the tie.
        return p.registry==kBridge && p.source==53?coo::native_combatant::TacticalGroup{kBridge,16,4}:p.tactical;
    }
    if(p.registry==kBridge) {return {kBridge,15,-1};}
    if(p.registry==kProcessing) {return {kProcessing,33,-1};}
    if(p.registry==kCore) {return {kCore,15,-1};}
    return {};
}
// One fixed arena for all 15 immutable graphs, including both checkpoint
// variants. Storage never moves; each graph's steps and commands are contiguous.
// Bounds failures invalidate the mission instead of publishing partial graphs.
struct GraphStorage {
    std::array<coo::CommandSpec,384> commands{};
    std::array<coo::Step,192> steps{};
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
    Graph bridgeCheckpoint{},processingCheckpoint{};
    Mission() noexcept;
    Mission(const Mission&)=delete;
    Mission& operator=(const Mission&)=delete;
    bool valid() const noexcept;
};
const Mission& mission() noexcept;
} // namespace dawn::state::activity::vanilla::one_au
