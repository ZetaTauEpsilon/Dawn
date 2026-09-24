#pragma once
#include <array>
#include <cstring>
#include <span>
#include "../../../state/activity/deadly_trial/frame.h"
#include "../../../state/activity/deadly_trial/navigation.h"
#include "mission_waypoint_rules.h"
namespace dawn::client::hooks::bootflow::deadly_trial_presentation {
namespace trial=state::activity::deadly_trial;
template<class T> inline T read(std::span<const std::byte> b,std::size_t o) noexcept { T v{};if(o<=b.size() && sizeof v<=b.size()-o) { std::memcpy(&v,b.data()+o,sizeof v); }return v; }
template<class T> inline void put(std::span<std::byte> b,std::size_t o,T v) noexcept { std::memcpy(b.data()+o,&v,sizeof v); }
inline bool source(std::span<const std::byte> b,bool dialogue) noexcept {
    return b.size()>=0x58 && read<std::uint32_t>(b,0)==(dialogue?0x80B2E709U:0x80B2E706U)
        && read<std::uint32_t>(b,4)==(dialogue?0x80804F4CU:0x80804F54U)
        && read<std::int64_t>(b,8)==(dialogue?0x1408:0xB88)
        && read<std::uint32_t>(b,0x48)!=UINT32_MAX
        && read<std::uint32_t>(b,0x4C)==(dialogue?0x80804F4BU:0x80804F53U)
        && read<std::int64_t>(b,0x50)==0;
}
inline bool route_point(std::span<std::byte> b,const trial::Frame& f,std::uint32_t index=0) noexcept {
    if(b.size()<0xB00 || index>=3 || !source(b,false) || !f.enabled || index!=read<std::uint32_t>(b,0x478)) { return false; }
    const auto nav=trial::navigation::goal(f.presentation.event);
    if(!nav.target.valid())return false;
    const auto p=0x480+index*0x200,row=0x190+index*0xF8;
    if(!f.presentation.active) {
        if(read<std::uint32_t>(b,p+48)!=nav.target.asset.registry || read<std::uint8_t>(b,p+52)!=nav.target.asset.type
            || read<std::uint16_t>(b,p+54)!=nav.target.asset.slot)return false;
        put<std::uint8_t>(b,p+4,0);return true;
    }
    if(!f.presentation.published || f.presentation.marker!=nav.target || read<std::uint32_t>(b,row)!=f.presentation.event
        || read<std::uint8_t>(b,row+8)!=0 || read<std::uint8_t>(b,row+0x64)!=0
        || read<std::uint32_t>(b,row+0x68)!=nav.target.asset.registry || read<std::uint8_t>(b,row+0x6C)!=nav.target.asset.type
        || read<std::uint16_t>(b,row+0x6E)!=nav.target.asset.slot
        || read<std::array<std::uint32_t,4>>(b,row+0x78)!=nav.target.locator)return false;
    put<std::uint8_t>(b,p+4,3);put<std::uint8_t>(b,p+12,2);put(b,p+24,nav.bubble);
    put(b,p+32,std::array<float,4>{nav.position.x,nav.position.y,nav.position.z,1.F});
    put(b,p+48,read<std::uint64_t>(b,row+0x68));
    return true;
}
inline mission_waypoints::Result build(std::span<std::byte> b,const trial::Frame& f) noexcept {
    if(b.size()<0xB10 || !f.enabled || f.finished || !f.presentation.active || !f.presentation.published)return {};
    const auto index=read<std::uint32_t>(b,0x478);if(index>=3)return {};
    for(unsigned i=0;i<3;++i)if(i!=index && read<std::uint8_t>(b,0x198+i*0xF8)==0)return {};
    const auto point=0x480+index*0x200;const auto before=read<std::array<std::byte,0x38>>(b,point);
    if(!route_point(b,f,index))return {};
    mission_waypoints::Result result{true,0};
    if(before!=read<std::array<std::byte,0x38>>(b,point))result.publish=static_cast<std::uint16_t>(1U<<(index*4));
    for(unsigned i=0;i<13;++i)if(i!=index*4 && read<std::uint8_t>(b,0x484+i*0x80)!=0) {
        put<std::uint8_t>(b,0x484+i*0x80,0);result.publish|=static_cast<std::uint16_t>(1U<<i);
    }
    return result;
}
using Register=void(__fastcall*)(const void*) noexcept;
void observe_directive(void*,Register) noexcept;
bool build_directive(void*,Register,std::uint32_t(__fastcall*)() noexcept) noexcept;
}
