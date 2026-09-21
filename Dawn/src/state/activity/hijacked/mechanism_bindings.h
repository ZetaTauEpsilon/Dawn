#pragma once
#include "native_catalog.h"
#include "ai_bindings.h"
#include "../coo/objective_service.h"
namespace dawn::state::activity::hijacked {
struct PlateBinding {coo::Asset source,volume;float chargeSeconds;};
// Same native altar/timer resource as Deep Storage; charge time is reconstructed.
inline constexpr PlateBinding kPlates[]{
    {{0xD997395EU,0x80B429A9U,4,19},{0xD997395EU,0x80B4238DU,60,114},5.F},
};
struct ScanBinding {coo::Asset source,link;std::uint32_t controllerDefinition;};
inline constexpr ScanBinding kScans[]{
    {{0xD997395EU,0x80B423B5U,4,23},{0x40A009B5U,0x80B42410U,65,0},0x8157E6B1U},
};
// Native hydra_objective rows, ordered from initial sighting toward the final arena.
inline constexpr coo::native_combatant::TacticalGroup kBossTactics[]{
    {0x153E22CDU,20,5},{0x153E22CDU,20,2},{0x153E22CDU,20,1},
};
// Route markers bind the recovered native point sources; no invented coordinates.
inline constexpr coo::Asset kObjectiveTargets[]{
    {0xF5737F85U,0x80B4270DU,47,6}, // Tangle approach
    {0xD8FA09CAU,0x80B4245EU,47,8}, // Mists cave
    {0x2D20FD66U,0x80B42423U,47,13}, // Pursuit route
    {0x153E22CDU,0x80B421FAU,1,21}, // Entangled Mind
    {0x3C7C8AE9U,0x80B423F4U,47,1}, // Cave exit
    {0x701F9CE5U,0x80B42763U,47,13}, // Well of Echoes approach
    {0xA12CA9FAU,0x80B42A7FU,47,5}, // Materializing platforms
    kScans[0].source,
};
inline constexpr coo::MarkerTarget marker(std::uint32_t event) noexcept {
    for(std::size_t i=0;i<std::size(kObjectives);++i) {
        if(event==kObjectives[i].event) {
            constexpr std::uint32_t areas[]{0x37A08717,0x849E9C59,0x849E9C59,0x849E9C59,0x849E9C59,0x37A08717,0x29C88401,0x29C88401};
            const auto a=kObjectiveTargets[i];
            return {a,a.type==47?std::array<std::uint32_t,4>{0xF995E43A,areas[i],a.registry,0x1811EF12}:std::array<std::uint32_t,4>{}};
        }
    }
    return {};
}
float device_position(coo::Asset,bool) noexcept;
}
