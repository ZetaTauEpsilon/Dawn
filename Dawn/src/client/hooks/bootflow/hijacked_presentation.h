#pragma once
#include <array>
#include <cstring>
#include <span>
#include "../../../state/activity/hijacked/frame.h"
namespace dawn::client::hooks::bootflow::hijacked_presentation {
namespace mission=state::activity::hijacked;
template<class T> inline T read(std::span<const std::byte> b,std::size_t o) noexcept { T v{};if(o<=b.size() && sizeof v<=b.size()-o) { std::memcpy(&v,b.data()+o,sizeof v); }return v; }
template<class T> inline void put(std::span<std::byte> b,std::size_t o,T v) noexcept { std::memcpy(b.data()+o,&v,sizeof v); }
inline bool source(std::span<const std::byte> b,bool dialogue) noexcept {
    return b.size()>=0x58 && read<std::uint32_t>(b,0)==(dialogue?0x80B4241FU:0x80B4241CU)
        && read<std::uint32_t>(b,4)==(dialogue?0x80804F4CU:0x80804F54U)
        && read<std::int64_t>(b,8)==(dialogue?0x1408:0xB88)
        && read<std::uint32_t>(b,0x48)!=UINT32_MAX
        && read<std::uint32_t>(b,0x4C)==(dialogue?0x80804F4BU:0x80804F53U)
        && read<std::int64_t>(b,0x50)==0;
}
void observe_directive(void*) noexcept;
// Release 272AE0: acknowledge only one exact visible native banner.
inline std::uint32_t visible_row(std::span<const std::byte> b,std::span<const std::byte> banners,const mission::Frame& f) noexcept {
    if(!source(b,false) || b.size()<0x480 || banners.size()<0x1488 || !f.enabled || f.finished
        || !f.presentation.active || !f.presentation.published)return UINT32_MAX;
    const auto index=read<std::uint32_t>(b,0x478);if(index>=3)return UINT32_MAX;
    const auto row=0x190+index*0xF8;
    if(read<std::uint32_t>(b,row)!=f.presentation.event || read<std::uint32_t>(b,row+4)!=0 || read<std::uint8_t>(b,row+8)!=0)return UINT32_MAX;
    const auto count=read<std::uint64_t>(banners,0x1480);if(count>16)return UINT32_MAX;unsigned matches{};
    for(std::size_t i=0;i<count;++i) {
        const auto p=i*0x148;
        if(read<std::uint8_t>(banners,p)!=2 || read<std::uint32_t>(banners,p+4)!=f.presentation.event*0x502C3F11U)continue;
        if(read<std::uint32_t>(banners,p+8)!=0xF995E43A || read<std::uint32_t>(banners,p+16)!=0x77852DB9
            || read<std::uint64_t>(banners,p+0x110)!=1 || read<std::int32_t>(banners,p+0x70)<0 || read<std::int32_t>(banners,p+0x70)>1)return UINT32_MAX;
        ++matches;
    }
    return matches==1?index:UINT32_MAX;
}
}
