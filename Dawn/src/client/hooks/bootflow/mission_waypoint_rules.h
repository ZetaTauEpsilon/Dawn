#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include "../../../state/activity/coo/objective_service.h"
namespace dawn::client::hooks::bootflow::mission_waypoints {
namespace coo=state::activity::coo;
template<class T> inline T read(std::span<const std::byte> b,std::size_t offset) noexcept {
    T value{};if(offset<=b.size() && sizeof value<=b.size()-offset)std::memcpy(&value,b.data()+offset,sizeof value);return value;
}
template<class T> inline void put(std::span<std::byte> b,std::size_t offset,T value) noexcept {std::memcpy(b.data()+offset,&value,sizeof value);}
struct Result {bool handled{};std::uint16_t publish{};};
inline bool source(std::span<const std::byte> b,std::uint32_t definition) noexcept {
    return b.size()>=0x58 && read<std::uint32_t>(b,0)==definition
        && read<std::uint32_t>(b,4)==0x80804F54 && read<std::int64_t>(b,8)==0xB88
        && read<std::uint32_t>(b,0x48)!=UINT32_MAX && read<std::uint32_t>(b,0x4C)==0x80804F53
        && read<std::int64_t>(b,0x50)==0;
}
inline auto locator(const coo::MarkerTarget& marker) noexcept {
    if(marker.locator[0]==0 || marker.locator[0]==0x811C9DC5)return std::array<std::uint32_t,4>{0x811C9DC5,0x811C9DC5,0x811C9DC5,0x811C9DC5};
    return marker.locator;
}
inline bool selected(std::span<const std::byte> b,const coo::ObjectiveState& objective,bool displayGuard=false) noexcept {
    if(b.size()<0xB10 || !objective.active || !objective.published)return false;
    const auto index=read<std::uint32_t>(b,0x478);if(index>=3)return false;
    const auto row=0x190+index*0xF8;const auto& marker=objective.marker;
    const auto key=marker.valid()?marker.asset.registry:0x811C9DC5;
    const auto packed=marker.valid()?std::uint32_t(marker.asset.type)|(std::uint32_t(marker.asset.slot)<<16):0xFFFF00FF;
    if(read<std::uint32_t>(b,row)!=objective.event || read<std::int8_t>(b,row+8)!=0
        || (displayGuard && read<std::uint8_t>(b,row+0x64)!=0)
        || read<std::uint32_t>(b,row+0x68)!=key || read<std::uint32_t>(b,row+0x6C)!=packed
        || read<std::array<std::uint32_t,4>>(b,row+0x78)!=locator(marker))return false;
    for(unsigned i=0;i<3;++i)if(i!=index && read<std::int8_t>(b,0x198+i*0xF8)==0)return false;
    return true;
}
struct Point {coo::Asset asset{};std::array<float,4> position{};std::uint8_t display{};};
inline Result points(std::span<std::byte> b,std::uint32_t context,const std::array<const Point*,4>& targets) noexcept {
    Result result{true,0};const auto index=read<std::uint32_t>(b,0x478);
    for(unsigned i=0;i<13;++i) {
        const auto p=0x480+i*0x80;const auto* target=i>=index*4 && i<index*4+4?targets[i-index*4]:nullptr;bool changed{};
        if(target) {
            const auto packed=std::uint32_t(target->asset.type)|(std::uint32_t(target->asset.slot)<<16);
            changed=read<std::uint8_t>(b,p+4)!=3 || read<std::uint8_t>(b,p+12)!=target->display
                || read<std::uint32_t>(b,p+24)!=context || read<std::array<float,4>>(b,p+32)!=target->position
                || read<std::uint32_t>(b,p+48)!=target->asset.registry || read<std::uint32_t>(b,p+52)!=packed;
            put<std::uint8_t>(b,p+4,3);put(b,p+12,target->display);put(b,p+24,context);
            put(b,p+32,target->position);put(b,p+48,target->asset.registry);put(b,p+52,packed);
        } else if(read<std::uint8_t>(b,p+4)!=0) {put<std::uint8_t>(b,p+4,0);changed=true;}
        if(changed)result.publish|=static_cast<std::uint16_t>(1U<<i);
    }
    return result;
}
}
