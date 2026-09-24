#include <Windows.h>
#include "hijacked_presentation.h"
#include "gateway_native_read.h"
#include "mission_waypoint_native.h"
#include "../../../state/activity/hijacked/runtime.h"
namespace dawn::client::hooks::bootflow::hijacked_presentation {
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
void observe_directive(void* instance) noexcept {
    if(!instance)return;
    auto* component=static_cast<std::byte*>(instance);
    std::array<std::byte,0xB10> b{};
    if(!copy(component,b) || !source(b,false) || !owns(component,b))return;
    const auto state=mission::request();const auto& f=state.frame;
    const auto owner=state.owner;
    if(!owner.valid() || !f.enabled)return;
    const auto native=functions();
    const auto content=native.content?native.content(component):nullptr;
    const bool ready=content && native.ready && native.ready();
    std::uint32_t visible=UINT32_MAX;
    if(ready && f.presentation.active && f.presentation.published) {
        std::array<std::byte,0x1488> banners{};
        if(copy(reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr))+0x2FB6838,banners))visible=visible_row(b,banners,f);
    }
    mission::observe_objective_readiness(owner,read<std::uint32_t>(b,0x48),
        reinterpret_cast<std::uintptr_t>(component),reinterpret_cast<std::uintptr_t>(content),ready,f.presentation.revision,visible);
    const auto index=read<std::uint32_t>(b,0x478);
    if(ready && !f.finished && f.presentation.active && f.presentation.published && index<3
        && read<std::uint32_t>(b,0x190+index*0xF8)==f.presentation.event && read<std::uint8_t>(b,0x198+index*0xF8)==0) {
        const auto latest=mission::request();
        if(latest.owner==owner && latest.frame.enabled && !latest.frame.finished && latest.frame.presentation.active
            && latest.frame.presentation.event==f.presentation.event && latest.frame.presentation.revision==f.presentation.revision && owns(component,b))
            mission_waypoint_native::restore_meshes(mission_waypoint_native::hijackedMeshes);
    }
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
