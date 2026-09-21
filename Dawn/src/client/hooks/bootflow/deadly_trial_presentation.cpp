#include <Windows.h>
#include "deadly_trial_presentation.h"
#include "gateway_native_read.h"
#include "mission_waypoint_native.h"
#include "../../../state/activity/deadly_trial/runtime.h"
namespace dawn::client::hooks::bootflow::deadly_trial_presentation {
namespace {
using Content=void*(__fastcall*)(void*) noexcept;
using Ready=bool(__fastcall*)() noexcept;
struct Functions { Content content{};Ready ready{}; };
Functions functions() noexcept;
bool owns(std::byte* component,std::span<const std::byte> bytes) noexcept {
    gateway_native::Read memory{reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))};
    std::uintptr_t resolved{};
    return memory.resolve(read<std::uint32_t>(bytes,0x48),resolved) && resolved==reinterpret_cast<std::uintptr_t>(component);
}
bool copy(const void* p,std::span<std::byte> b) noexcept {
    SIZE_T n{};return ReadProcessMemory(GetCurrentProcess(),p,b.data(),b.size(),&n) && n==b.size();
}
}
void observe_directive(void* instance,Register publish) noexcept {
    if(!instance)return;
    auto* component=static_cast<std::byte*>(instance);
    std::array<std::byte,0xB10> b{};
    if(!copy(component,b) || !source(b,false) || !owns(component,b))return;
    const auto state=trial::presentation();const auto& f=state.frame;
    const state::activity::coo::Generation owner{state.run,f.spawnGeneration};
    if(!owner.valid() || !f.enabled)return;
    const auto native=functions();
    const auto content=native.content?native.content(component):nullptr;
    const bool ready=content && native.ready && native.ready();
    trial::observe_objective_readiness(owner,read<std::uint32_t>(b,0x48),
        reinterpret_cast<std::uintptr_t>(component),reinterpret_cast<std::uintptr_t>(content),ready);
    // The event and marker reference now arrive through 80804F67. The authored
    // Trial waypoint height still needs its separately scoped position correction.
    const auto index=read<std::uint32_t>(b,0x478);
    const auto point=static_cast<std::size_t>(0x480+index*0x200);
    const auto before=read<std::array<std::byte,0x38>>(b,point);
    if(!ready || !publish || index>=3 || !f.presentation.published
        || (f.presentation.active && read<std::uint32_t>(b,0x190+index*0xF8)!=f.presentation.event)
        || (read<std::int8_t>(b,0x198+index*0xF8)==0)!=f.presentation.active || !route_point(b,f,index)
        || before==read<std::array<std::byte,0x38>>(b,point))return;
    const auto latest=trial::presentation();
    if(latest.run!=state.run || latest.frame.spawnGeneration!=f.spawnGeneration
        || latest.frame.presentation.revision!=f.presentation.revision || !owns(component,b))return;
    std::memcpy(component+point+4,b.data()+point+4,1);std::memcpy(component+point+12,b.data()+point+12,1);
    std::memcpy(component+point+24,b.data()+point+24,4);std::memcpy(component+point+32,b.data()+point+32,16);
    std::memcpy(component+point+48,b.data()+point+48,8);
    gateway_native::Ref reference{read<std::uint32_t>(b,0x48),0x80804F55U,static_cast<std::int64_t>(point)};publish(&reference);
}
bool build_directive(void* instance,Register publish,std::uint32_t(__fastcall* revision)() noexcept) noexcept {
    if(!instance || !publish || !revision)return false;
    auto* component=static_cast<std::byte*>(instance);std::array<std::byte,0xB10> b{};
    if(!copy(component,b) || !source(b,false) || !owns(component,b))return false;
    const auto state=trial::presentation();const auto& f=state.frame;
    if(!state.run || !f.spawnGeneration || !f.enabled || f.finished)return false;
    const auto native=functions();if(!native.content || !native.ready || !native.content(component) || !native.ready())return false;
    const auto result=build(b,f);if(!result.handled)return false;
    const auto latest=trial::presentation();
    if(latest.run!=state.run || latest.frame.spawnGeneration!=f.spawnGeneration || !latest.frame.enabled || latest.frame.finished
        || latest.frame.presentation.event!=f.presentation.event || latest.frame.presentation.revision!=f.presentation.revision
        || latest.frame.presentation.marker!=f.presentation.marker || !latest.frame.presentation.active || !latest.frame.presentation.published
        || !owns(component,b))return false;
    mission_waypoint_native::restore_meshes(mission_waypoint_native::trialMeshes);
    for(unsigned i=0;i<13;++i)if(result.publish&(1U<<i)) {
        const auto p=0x480+i*0x80;
        for(const auto [offset,size]:std::array<std::pair<std::size_t,std::size_t>,5>{{{4,1},{12,1},{24,4},{32,16},{48,8}}})
            std::memcpy(component+p+offset,b.data()+p+offset,size);
        const gateway_native::Ref ref{read<std::uint32_t>(b,0x48),0x80804F55,p};publish(&ref);
    }
    const auto current=revision();std::memcpy(component+0xB00,&current,4);component[0xB04]=std::byte{1};return true;
}
namespace {
Functions functions() noexcept {
    static const Functions result=[]() noexcept {
        Functions f{};const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        std::array<std::byte,16> actual{};
        constexpr std::array contentPrefix{std::byte{0x44},std::byte{0x8B},std::byte{0x09},std::byte{0x4C},std::byte{0x8B},std::byte{0xD1},std::byte{0x48},std::byte{0x8B},std::byte{0x05},std::byte{0x33},std::byte{0x08},std::byte{0x43},std::byte{0x01},std::byte{0x41},std::byte{0x8B},std::byte{0xD1}};
        if(!copy(reinterpret_cast<void*>(image+0x1009430),actual) || actual!=contentPrefix) { return Functions{}; }
        f.content=reinterpret_cast<Content>(image+0x1009430);
        constexpr std::array readyPrefix{std::byte{0x80},std::byte{0x3D},std::byte{0xF1},std::byte{0x9A},std::byte{0xC3},std::byte{0x01},std::byte{0x00},std::byte{0x74},std::byte{0x12},std::byte{0x48},std::byte{0x8D},std::byte{0x05},std::byte{0x5F},std::byte{0x86},std::byte{0xC3},std::byte{0x01}};
        if(!copy(reinterpret_cast<void*>(image+0x137E1D0),actual) || actual!=readyPrefix) { return Functions{}; }
        f.ready=reinterpret_cast<Ready>(image+0x137E1D0);
        return f;
    }();return result;
}
}
}
