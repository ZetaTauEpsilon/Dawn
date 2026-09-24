#pragma once
#include <Windows.h>
#include "mission_waypoint_rules.h"
#include "gateway_native_read.h"
#include "../../../state/activity/coo/waypoint_delivery.h"
#include "../../../state/activity/runtime.h"
namespace dawn::client::hooks::bootflow::mission_waypoint_native {
using namespace mission_waypoints;
inline bool copy(const void* p,std::span<std::byte> b) noexcept {
    SIZE_T n{};return ReadProcessMemory(GetCurrentProcess(),p,b.data(),b.size(),&n) && n==b.size();
}
inline bool owns(void* component,std::span<const std::byte> b) noexcept {
    gateway_native::Read memory{reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))};std::uintptr_t resolved{};
    return memory.resolve(read<std::uint32_t>(b,0x48),resolved) && resolved==reinterpret_cast<std::uintptr_t>(component);
}
template<std::size_t N> inline void* target(std::uintptr_t rva,const std::array<std::uint8_t,N>& prefix) noexcept {
    auto* p=reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr))+rva;std::array<std::uint8_t,N> actual{};
    return copy(p,std::as_writable_bytes(std::span(actual))) && actual==prefix?p:nullptr;
}
struct Functions {
    using Content=void*(__fastcall*)(void*) noexcept;using Ready=bool(__fastcall*)() noexcept;
    using Meshes=void*(__fastcall*)() noexcept;using Refresh=void(__fastcall*)(void*) noexcept;
    Content content{};Ready ready{};Meshes meshes{};Refresh refresh{};
};
inline Functions functions() noexcept {
    static const auto result=[] {
        Functions f{};
        f.content=reinterpret_cast<Functions::Content>(target(0x1009430,std::array<std::uint8_t,16>{0x44,0x8B,0x09,0x4C,0x8B,0xD1,0x48,0x8B,0x05,0x33,0x08,0x43,0x01,0x41,0x8B,0xD1}));
        f.ready=reinterpret_cast<Functions::Ready>(target(0x137E1D0,std::array<std::uint8_t,16>{0x80,0x3D,0xF1,0x9A,0xC3,0x01,0,0x74,0x12,0x48,0x8D,0x05,0x5F,0x86,0xC3,0x01}));
        if(!f.content || !f.ready)return Functions{};
        f.meshes=reinterpret_cast<Functions::Meshes>(target(0xA23C50,std::array<std::uint8_t,16>{0x48,0x83,0xEC,0x28,0x48,0x8B,0x05,0x0D,0x43,0xD3,0x01,0x48,0x85,0xC0,0x0F,0x84}));
        f.refresh=reinterpret_cast<Functions::Refresh>(target(0xA26580,std::array<std::uint8_t,16>{0x40,0x55,0x48,0x8D,0xAC,0x24,0x50,0xAC,0xFF,0xFF,0xB8,0xB0,0x54,0,0,0xE8}));
        return f;
    }();return result;
}
inline void observe(void* component) noexcept {
    namespace activity=state::activity;namespace delivery=activity::coo::waypoint_delivery;
    if(!component || !activity::mission_seed_armed() || activity::world_phase()!=activity::WorldPhase::arrived)return;
    std::array<std::byte,0x58> b{};if(!copy(component,b) || !source(b,read<std::uint32_t>(b,0)))return;
    const auto run=activity::mission_run_generation();const auto ticket=delivery::lookup(run,read<std::uint32_t>(b,0));
    if(!ticket.valid() || !owns(component,b))return;
    const auto native=functions();if(!native.content || !native.ready)return;
    const auto content=native.content(component);const bool ready=content && native.ready();
    if(activity::mission_run_generation()==run)delivery::observe(ticket,read<std::uint32_t>(b,0x48),
        reinterpret_cast<std::uintptr_t>(component),reinterpret_cast<std::uintptr_t>(content),ready);
}
struct Mesh {std::uint32_t descriptor,resource;std::uint64_t bytes,nodes,links;};
inline constexpr std::array<Mesh,2> trialMeshes{{{0x80BE0E85,0x80BE0E80,111120,1060,2335},{0x80B2ED22,0x80B2ED21,1584,16,34}}};
inline constexpr std::array<Mesh,1> hijackedMeshes{{{0x80C4C37D,0x80C4C37C,68240,708,1574}}};
inline bool mesh_valid(const Mesh& m,std::span<const std::byte> d,std::span<const std::byte> r) noexcept {
    if(d.size()<0xE0 || r.size()<0x80 || read<std::uint64_t>(d,0)!=0xE0 || read<std::uint32_t>(d,0xC4)!=0x80809B48
        || read<std::uint32_t>(d,0xD8)!=m.resource || read<std::uint64_t>(r,0)!=m.bytes
        || read<std::uint64_t>(r,0x28)!=m.nodes || read<std::uint64_t>(r,0x38)!=m.links)return false;
    const auto nodes=read<std::uint64_t>(r,0x30),links=read<std::uint64_t>(r,0x40);
    return nodes<=m.bytes-0x30 && m.nodes*0x50<=m.bytes-0x30-nodes
        && links<=m.bytes-0x40 && m.links*2<=m.bytes-0x40-links;
}
struct MeshList {std::uint32_t count{};std::array<std::uint32_t,4> ids{};friend bool operator==(MeshList,MeshList)=default;};
inline bool merge_meshes(MeshList& result,std::span<const Mesh> required) noexcept {
    if(result.count>4)return false;
    for(unsigned i=0;i<result.count;++i) {
        if(result.ids[i]==UINT32_MAX)return false;
        for(unsigned j=0;j<i;++j)if(result.ids[i]==result.ids[j])return false;
    }
    auto next=result;
    for(const auto& m:required) {
        bool present=false;for(unsigned i=0;i<next.count;++i)present|=next.ids[i]==m.resource;
        if(!present) {if(next.count==4)return false;next.ids[next.count++]=m.resource;}
    }
    result=next;return true;
}
// Release 2723F0/273260: restore only these authored meshes, after validating
// both allocations and the unchanged native list. Never replace another mesh.
inline void restore_meshes(std::span<const Mesh> required) noexcept {
    const auto native=functions();if(!native.meshes || !native.refresh)return;
    auto* address=native.meshes();if(!address)return;MeshList before{};
    if(!copy(address,std::as_writable_bytes(std::span{&before,1})))return;
    auto after=before;if(!merge_meshes(after,required) || after==before)return;
    gateway_native::Read memory{reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))};
    for(const auto& mesh:required) {
        std::uintptr_t descriptor{},resource{};std::array<std::byte,0xE0> d{};std::array<std::byte,0x80> r{};
        if(!memory.resolve(mesh.descriptor,descriptor) || !memory.resolve(mesh.resource,resource)
            || !memory.copy(descriptor,d) || !memory.copy(resource,r) || !mesh_valid(mesh,d,r))return;
    }
    MeshList latest{};if(!copy(address,std::as_writable_bytes(std::span{&latest,1})) || latest!=before)return;
    std::memcpy(address,&after,sizeof after);native.refresh(address);
}
}
