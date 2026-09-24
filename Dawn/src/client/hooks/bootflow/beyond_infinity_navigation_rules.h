#pragma once
#include "mission_waypoint_rules.h"
#include "../../../state/activity/beyond_infinity/navigation.h"
namespace dawn::client::hooks::bootflow::beyond_infinity_navigation {
using namespace mission_waypoints;
namespace mission=state::activity::beyond_infinity;
// Release RVA 726EE8, 14 consecutive 32-byte route points.
inline constexpr Point targets[]{
    {{0x91AF0A4E,0x80F460DB,47,10},{255.32737731933594F,747.4946899414062F,313.24560546875F,1.F},0},
    {{0x2823987A,0x80F461A7,47,3},{-545.9273681640625F,1864.2083740234375F,170.16819763183594F,1.F},0},
    {{0xC05EAA69,0x80F46209,47,4},{-783.8103637695312F,1073.582763671875F,-71.99919891357422F,1.F},0},
    {{0xBB66F71D,0x80F46214,47,11},{748.93359375F,710.0294189453125F,3.9194626808166504F,1.F},0},
    {{0xA908C5F7,0x80F4647E,47,4},{-559.347412109375F,1618.761962890625F,-73.F,1.F},2},
    {{0xB981A22D,0x80F4619C,47,3},{343.52313232421875F,249.94798278808594F,98.46297454833984F,1.F},0},
    {{0x0FF26BCC,0x80F4604C,4,4},{262.0851745605469F,752.0819702148438F,312.56524658203125F,1.F},0},
    {{0xA908C5F7,0x80F4647E,47,3},{-552.4461669921875F,1659.84326171875F,-77.96265411376953F,1.F},0},
    {{0x6A921301,0x80F46171,47,8},{-993.1536865234375F,1072.941162109375F,-67.51959991455078F,1.F},0},
    {{0x199D7650,0x80F46160,47,5},{-198.03565979003906F,1072.832275390625F,-66.94371795654297F,1.F},0},
    {{0xBB66F71D,0x80F46214,47,10},{686.9090576171875F,691.260498046875F,3.8406102657318115F,1.F},0},
    {{0x45AFDE9B,0x80F460D0,47,6},{135.03753662109375F,1412.0638427734375F,-2.2376184463500977F,1.F},0},
    {{0xCFB5D3B3,0x80F4648F,47,4},{-562.0250244140625F,1426.3017578125F,-45.24237823486328F,1.F},0},
    {{0x91AF0A4E,0x80F460DB,47,9},{221.32020568847656F,756.4769287109375F,313.140380859375F,1.F},0},
};
inline const Point* target(const mission::Frame& f,std::uint32_t context) noexcept {
    const auto& p=f.navigation;
    switch(f.presentation.event) {
    case 0x29BFCE5A:return &targets[!f.wellEntered?5:context==19?7:1];
    case 0x2FDA4350:return f.lensDestroyed?nullptr:&targets[f.lensExposed?4:7];
    case 0xB1AD777D:return &targets[12];
    case 0x64B46F54:return &targets[p.futureReflection?0:13];
    case 0x5569523A:return &targets[p.futureReturn?11:6];
    case 0x5AD156F5:return &targets[11];
    case 0x9225AEF3:return &targets[context==18 && p.pastEncounter?10:8];
    case 0xD60FA7DF:return &targets[10];
    case 0xBE5F8E8B:return &targets[context==8 || p.pastReturn?2:3];
    case 0x142EC956:
        if(context==18 && p.pastReturn)return &targets[2];
        if(context==8 && !p.forestFutureComplete)return nullptr;
        return &targets[context==4 && p.futureEntered?13:9];
    default:return nullptr;
    }
}
inline Result build(std::span<std::byte> b,std::uint32_t context,const mission::Frame& f) noexcept {
    if(!source(b,0x80F46225) || !f.enabled || f.finished || !mission::navigation::admitted(f,context)
        || f.presentation.marker!=mission::navigation::marker(f.presentation.event,f)
        || ((f.presentation.event==0x29BFCE5A || f.presentation.event==0x2FDA4350) && !f.presentation.marker.valid())
        || !selected(b,f.presentation,true))return {};
    return points(b,context,{target(f,context),nullptr,nullptr,nullptr});
}
}
