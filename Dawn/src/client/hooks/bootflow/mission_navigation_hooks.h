#pragma once
#include "mission_waypoint_native.h"
#include "beyond_infinity_navigation_rules.h"
#include "deep_storage_navigation_rules.h"
#include "omega_navigation_rules.h"
#include "../../../state/activity/beyond_infinity/runtime.h"
#include "../../../state/activity/deep_storage/runtime.h"
#include "../../../state/activity/coo/native_mission_forest_authority.h"
namespace dawn::client::hooks::bootflow::mission_navigation_hooks {
namespace native=mission_waypoint_native;namespace rules=mission_waypoints;
using Publish=void(__fastcall*)(const omega_navigation::Reference*) noexcept;
using Revision=std::uint32_t(__fastcall*)() noexcept;
inline void commit(void* instance,std::span<const std::byte> b,rules::Result result,Publish publish,Revision revision) noexcept {
    auto* component=static_cast<std::byte*>(instance);
    for(unsigned i=0;i<13;++i)if(result.publish&(1U<<i)) {
        const auto p=0x480+i*0x80;
        for(const auto [offset,size]:std::array<std::pair<std::size_t,std::size_t>,5>{{{4,1},{12,1},{24,4},{32,16},{48,8}}})
            std::memcpy(component+p+offset,b.data()+p+offset,size);
        const omega_navigation::Reference ref{rules::read<std::uint32_t>(b,0x48),0x80804F55,p};publish(&ref);
    }
    const auto current=revision();std::memcpy(component+0xB00,&current,4);component[0xB04]=std::byte{1};
}
inline bool build_deep(void* instance,std::uint32_t context,Publish publish,Revision revision) noexcept {
    namespace mission=state::activity::deep_storage;
    if((context!=4 && context!=19) || !publish || !revision)return false;
    std::array<std::byte,0xB10> b{};
    if(!native::copy(instance,b) || !deep_storage_navigation::source(b))return false;
    const auto request=mission::request();const auto& f=request.frame;
    if(!request.owner.valid() || !f.enabled || f.finished || !native::owns(instance,b))return false;
    const auto result=deep_storage_navigation::build(b,context,f);if(!result.handled)return false;
    const auto latest=mission::request();const auto& n=latest.frame;
    if(latest.owner!=request.owner || !n.enabled || n.finished || n.presentation.revision!=f.presentation.revision)return false;
    if(f.presentation.event==0x025CC54F || f.presentation.event==0xCB573BE1) {
        if(n.section!=f.section || n.lensExposed!=f.lensExposed || n.lensDestroyed!=f.lensDestroyed || n.scanComplete[1]!=f.scanComplete[1])return false;
        for(unsigned i=1;i<3;++i)if(n.plates[i].armed!=f.plates[i].armed || n.plates[i].charged!=f.plates[i].charged)return false;
    }
    commit(instance,b,result,publish,revision);return true;
}
inline bool build_beyond(void* instance,std::uint32_t context,Publish publish,Revision revision) noexcept {
    namespace mission=state::activity::beyond_infinity;
    if(context>=20 || !(0xC8110U&(1U<<context)) || !publish || !revision)return false;
    std::array<std::byte,0xB10> b{};
    if(!native::copy(instance,b) || !rules::source(b,0x80F46225))return false;
    const auto request=mission::request();const auto& f=request.frame;
    if(!request.owner.valid() || !f.enabled || f.finished || !native::owns(instance,b))return false;
    const auto result=beyond_infinity_navigation::build(b,context,f);if(!result.handled)return false;
    const auto latest=mission::request();const auto& n=latest.frame;
    if(latest.owner!=request.owner || !n.enabled || n.finished || n.presentation.revision!=f.presentation.revision
        || n.presentation.event!=f.presentation.event || n.presentation.marker!=f.presentation.marker || !n.presentation.active || !n.presentation.published
        || n.section!=f.section || n.forestPass!=f.forestPass || n.navigation!=f.navigation || n.wellEntered!=f.wellEntered
        || n.lensDestroyed!=f.lensDestroyed || n.lensExposed!=f.lensExposed)return false;
    commit(instance,b,result,publish,revision);return true;
}
inline void tick(void* component,Publish publish,Revision revision) noexcept {
    using Context=void*(__fastcall*)() noexcept;using Bubble=std::uint32_t*(__fastcall*)(void*,std::uint32_t*) noexcept;
    static const auto context=reinterpret_cast<Context>(native::target(0x4294C0,std::array<std::uint8_t,8>{0x48,0x8D,0x05,0x39,0x48,0xB6,0x01,0xC3}));
    static const auto bubble=reinterpret_cast<Bubble>(native::target(0x429BA0,std::array<std::uint8_t,16>{0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x48,0x8B,0x59,0x08,0x48,0x8B}));
    if(!context || !bubble)return;
    std::array<std::byte,0x58> b{};if(!native::copy(component,b))return;
    const bool deep=deep_storage_navigation::source(b),beyond=rules::source(b,0x80F46225);
    if(!deep && !beyond)return;
    if(deep) {const auto request=state::activity::deep_storage::request();const auto event=request.frame.presentation.event;
        if(!request.owner.valid() || !request.frame.enabled || request.frame.finished || (event!=0x025CC54F && event!=0xCB573BE1))return;}
    std::uint32_t id=UINT32_MAX;auto* ctx=context();if(!ctx || bubble(ctx,&id)!=&id)return;
    if(deep && id==19)build_deep(component,id,publish,revision);
    if(beyond)build_beyond(component,id,publish,revision);
}
inline void forest_terminal(void* worker,void* node,std::uint32_t result) noexcept {
    namespace mission=state::activity::beyond_infinity;
    if(!worker || !node || result!=0)return;
    const auto request=mission::request();const auto& f=request.frame;
    if(!request.owner.valid() || request.owner.value!=f.spawnGeneration || !f.enabled || f.finished || !f.forestReady)return;
    if(f.forestPass==1) {if(f.section<2 || f.section>3 || f.navigation.forestPastComplete)return;}
    else if(f.forestPass==2) {if(f.section<4 || f.section>5 || f.navigation.forestFutureComplete)return;}else return;
    if(f.forestSeed!=state::activity::coo::native_generator::mission_seed(request.owner.run,request.owner.value,f.forestPass))return;
    std::array<std::byte,0x9BD> w{};std::array<std::byte,0x38> n{};
    if(!native::copy(worker,w) || !native::copy(node,n) || rules::read<std::uint32_t>(w,4)!=0x80804FEC
        || rules::read<std::uint32_t>(w,0x96C)!=0x80F4D0F1 || rules::read<std::uint32_t>(w,0x940)!=f.forestSeed)return;
    const auto address=reinterpret_cast<std::uintptr_t>(worker);std::uint8_t index{};std::uintptr_t gateway{},resolved{};
    if(!omega_navigation::owns_node(w,address,reinterpret_cast<std::uintptr_t>(node))
        || !omega_navigation::terminal_gateway_index(w,n,result,index)
        || !omega_navigation::relative_address(address,rules::read<std::int64_t>(w,0x890),0x8A0+index*0x360,gateway))return;
    gateway_native::Read memory{reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))};
    if(!memory.resolve(rules::read<std::uint32_t>(w,0x24),resolved) || resolved!=address)return;
    std::array<std::byte,0x360> g{};if(native::copy(reinterpret_cast<void*>(gateway),g) && omega_navigation::gateway_open(g))
        mission::observe_forest_terminal(request.owner,f.forestPass);
}
}
