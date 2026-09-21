#pragma once
#include <cmath>
#include "mission_waypoint_native.h"
#include "omega_navigation_rules.h"
namespace dawn::client::hooks::bootflow::forest_strike_waypoints {
using namespace mission_waypoints;
// Recovered 26BA50/26BE70. The authored Forest worker keeps its nearby marker;
// only a duplicate root fallback to bubble zero qualifies for retirement.
inline int duplicate(std::span<const std::byte> b) noexcept {
    const bool tree=source(b,0x80F474C0) || source(b,0x80F54AC8),garden=source(b,0x80F47501);
    if(b.size()<0xB10 || (!tree && !garden))return 0;
    const auto index=read<std::uint32_t>(b,0x478);if(index>=3)return 0;
    const auto row=0x190+index*0xF8;
    if(read<std::uint32_t>(b,0x180)!=(tree?0x2763EC90U:0x2763EC91U) || read<std::uint8_t>(b,0x184)!=70
        || read<std::uint16_t>(b,0x186)!=(tree?14:28) || read<std::uint32_t>(b,row)!=(tree?0xF150883CU:0x489890F3U)
        || read<std::uint8_t>(b,row+8)!=0 || read<std::uint32_t>(b,row+0x68)!=(tree?0x45B69C3CU:0xA9350228U)
        || read<std::uint8_t>(b,row+0x6C)!=47 || read<std::uint16_t>(b,row+0x6E)!=(tree?2:5))return 0;
    for(unsigned i=0;i<12;++i)if(read<std::uint8_t>(b,0x484+i*0x80)!=0)return 0;
    return read<std::uint8_t>(b,0xA84)==3 && read<std::uint32_t>(b,0xA98)==0?(tree?1:2):0;
}
inline bool nearby(int mission,std::span<const std::byte> worker) noexcept {
    if(worker.size()<0x970 || read<std::uint32_t>(worker,0)!=0x80F59867 || read<std::uint32_t>(worker,4)!=0x80804FEC
        || read<std::uint32_t>(worker,0x96C)!=(mission==1?0x80F4E01EU:0x80F4D83FU)
        || read<std::uint8_t>(worker,0x8A4)!=3 || read<std::uint32_t>(worker,0x8B8)!=(mission==1?10U:9U))return false;
    const auto position=read<std::array<float,4>>(worker,0x8C0);
    for(const auto v:position)if(!std::isfinite(v))return false;
    return position[3]==1.F;
}
inline void retire(void* component,void(__fastcall* publish)(const omega_navigation::Reference*) noexcept) noexcept {
    namespace activity=state::activity;namespace native=mission_waypoint_native;namespace delivery=activity::coo::waypoint_delivery;
    if(!publish || !activity::mission_seed_armed() || activity::world_phase()!=activity::WorldPhase::arrived)return;
    std::array<std::byte,0xB10> b{};if(!native::copy(component,b))return;
    const auto mission=duplicate(b);if(!mission)return;
    const auto run=activity::mission_run_generation();const auto ticket=delivery::lookup(run,read<std::uint32_t>(b,0));
    if(!ticket.valid() || !native::owns(component,b))return;
    using List=void*(__fastcall*)() noexcept;
    static const auto list=reinterpret_cast<List>(native::target(0xC748A0,std::array<std::uint8_t,16>{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0x1D,0x9B,0x0B,0xB8,0x01,0x48,0x85,0xDB}));
    if(!list)return;const auto address=reinterpret_cast<std::uintptr_t>(list());if(!address)return;
    gateway_native::Read memory{reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))};std::uint32_t count{};
    if(!memory.value(address,count) || count==0 || count>16)return;
    for(unsigned i=0;i<count;++i) {
        std::array<std::byte,0x24> marker{};
        if(!memory.copy(address+0x10+i*0x640,marker))return;
        const auto ref=read<gateway_native::Ref>(marker,8);const auto uses=read<std::uint32_t>(marker,0x20);
        if(ref.handle==UINT32_MAX || ref.kind!=0x80804F55 || ref.offset!=0x8A0 || uses<1 || uses>8)continue;
        std::uintptr_t worker{};std::array<std::byte,0x970> w{};
        if(!memory.resolve(ref.handle,worker) || !memory.copy(worker,w) || !nearby(mission,w))continue;
        if(activity::mission_run_generation()!=run || delivery::lookup(run,ticket.definition)!=ticket)return;
        static_cast<std::byte*>(component)[0xA84]=std::byte{0};
        const omega_navigation::Reference reference{read<std::uint32_t>(b,0x48),0x80804F55,0xA80};publish(&reference);return;
    }
}
}
