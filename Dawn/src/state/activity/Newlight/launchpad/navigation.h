#pragma once
#include "native_catalog.h"
#include "../../coo/objective_service.h"
namespace dawn::state::activity::newlight::launchpad::navigation {
// Recovered release RVA 5CCEB0, including the indoor Hangar doorway.
inline coo::MarkerTarget marker(std::uint32_t event) noexcept {
    const auto target=[](std::uint32_t key,std::uint16_t slot) noexcept {
        for(const auto& n:kNavigation)if(n.asset.registry==key && n.asset.type==47 && n.asset.slot==slot) {
            std::uint32_t hash=0x811C9DC5;for(const unsigned char c:n.name)hash=(hash*0x1000193)^c;
            const auto area=key==kExterior?0xD6AD210EU:key==kBreachRoute?0x441515C2U:key==kDivideRoute?0x3B2C8A5CU:0x82AE25B4U;
            return coo::MarkerTarget{n.asset,{0x4A26AD57,area,key,hash}};
        }
        return coo::MarkerTarget{};
    };
    if(event==kObjectives[1])return target(kExterior,7);
    if(event==kObjectives[2])return target(kBreachRoute,17);
    if(event==kObjectives[3])return target(kBreachRoute,21);
    if(event==kObjectives[4])return target(kBreachRoute,22);
    if(event==kObjectives[5])return target(kDivideRoute,8);
    if(event==kObjectives[6])return target(kDivideRoute,7);
    if(event==kObjectives[0] || event==kObjectives[7] || event==kObjectives[8])return target(kHangarRoute,8);
    return {};
}
}
