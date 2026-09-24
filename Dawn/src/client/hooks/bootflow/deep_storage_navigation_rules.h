#pragma once
#include "mission_waypoint_rules.h"
#include "../../../state/activity/deep_storage/frame.h"
namespace dawn::client::hooks::bootflow::deep_storage_navigation {
using namespace mission_waypoints;
namespace mission=state::activity::deep_storage;
inline bool source(std::span<const std::byte> b) noexcept {return mission_waypoints::source(b,0x80B565DF);}
inline Point point(const mission::navigation::Target& t) noexcept {return {t.marker.asset,t.position,t.display};}
// Release RVAs 26C850 (route) and 26C250 (final platforms, box, conflux).
inline Result build(std::span<std::byte> b,std::uint32_t context,const coo::ObjectiveState& objective) noexcept {
    const auto* target=mission::navigation::route(objective.event,context);
    if(!target || !source(b) || objective.marker!=mission::marker(objective.event) || !selected(b,objective))return {};
    const auto p=point(*target);return points(b,context,{&p,nullptr,nullptr,nullptr});
}
inline Result build(std::span<std::byte> b,std::uint32_t context,const mission::Frame& frame) noexcept {
    const auto& objective=frame.presentation;
    if(objective.event!=0x025CC54F && objective.event!=0xCB573BE1)return build(b,context,objective);
    if(!source(b) || context!=19 || !frame.enabled || frame.finished || (frame.section!=5 && frame.section!=6)
        || objective.marker!=mission::marker(objective.event) || !selected(b,objective))return {};
    const std::array<Point,4> p{point(mission::navigation::finalTargets[0]),point(mission::navigation::finalTargets[1]),
        point(mission::navigation::finalTargets[2]),point(mission::navigation::interior[4])};
    std::array<const Point*,4> targets{};
    if(frame.section==5 && !frame.scanComplete[1]) {
        if(objective.event==0x025CC54F && !frame.lensDestroyed) {
            if(frame.plates[1].armed && !frame.plates[1].charged)targets[0]=&p[0];
            if(frame.plates[2].armed && !frame.plates[2].charged)targets[1]=&p[1];
            if(frame.plates[1].charged && frame.plates[2].charged && frame.lensExposed)targets[2]=&p[2];
        } else if(objective.event==0xCB573BE1 && frame.lensDestroyed)targets[3]=&p[3];
    }
    return points(b,context,targets);
}
}
