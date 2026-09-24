#include "gateway_cannon.h"
#include "gateway_cannon_native.h"
#include "gateway_native_read.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../state/activity/gateway/runtime.h"
#include "../../../core/logging/log.h"
#include <atomic>
#include <cstdio>

namespace dawn::client::hooks::bootflow::gateway_cannon {
namespace {
namespace g=state::activity::gateway;
using Apply=void(__fastcall*)(void*,void*,std::int32_t,float) noexcept;
constexpr std::uintptr_t kApply=0xD3B550;
constexpr std::array<unsigned char,16> kPrefix{
    0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xAC,0x24};
hooking::CallGate gate;
std::array<hooking::detour::Handle,1> hooks{};
std::atomic<Apply> original{};
std::atomic<std::uint64_t> logged{UINT64_MAX};
std::uintptr_t image{};

bool final_launcher(void* raw) noexcept {
    gateway_native::Read read{image};return owns(read,reinterpret_cast<std::uintptr_t>(raw));
}

// D39B30 keeps overlap admission/removal native and zeroes the target's force,
// velocity and apply flags before invoking D3B550 for each region. Suppress only
// that accumulation: do not stop its tick, edit a shared asset, or retain a target.
__declspec(noinline) void __fastcall apply(void* component,void* target,std::int32_t region,float dt) noexcept {
    hooking::CallGate::Scope scope(gate);
    const auto fn=hooking::await_original(original);
    if(scope.accepts_side_effects() && final_launcher(component)) {
        const auto request=g::cannon_request();
        if(request.run) {
            const auto state=(request.run<<1)|(request.released?1ULL:0ULL);
            if(logged.exchange(state,std::memory_order_relaxed)!=state) {
                std::array<char,224> line{};
                std::snprintf(line.data(),line.size(),"ev=gateway stage=final_cannon run=%llu released=%u gate=end_clear scope=launch_force",
                    static_cast<unsigned long long>(request.run),request.released?1U:0U);
                core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
            }
            if(g::cannon::blocked(request)) return;
        }
    }
    fn(component,target,region,dt);
}
bool idle() noexcept { return gate.idle(); }
}
bool install() noexcept {
    if(original.load(std::memory_order_acquire)) return gate.accepting();
    gate.quiesce();image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    gateway_native::Read read{image};std::array<unsigned char,16> bytes{};
    if(!read.value(image+kApply,bytes) || bytes!=kPrefix) return false;
    const std::array specs{hooking::detour::Spec{reinterpret_cast<void*>(image+kApply),reinterpret_cast<void*>(&apply)}};
    if(!hooking::detour::install(specs,hooks)) return false;
    hooking::publish_original(original,reinterpret_cast<Apply>(hooks[0].original));
    logged.store(UINT64_MAX,std::memory_order_relaxed);gate.accept();
    core::log::write(core::log::Channel::client,core::log::Level::info,
        "ev=gateway stage=final_cannon_install result=ok boundary=D3B550 definition=80F46DB6 gate=end_clear");
    return true;
}
void quiesce() noexcept { gate.quiesce(); }
bool uninstall() noexcept {
    gate.quiesce();if(!original.load(std::memory_order_acquire)) return true;
    const std::array protect{
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&apply)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&final_launcher)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}};
    if(hooking::detour::uninstall(hooks,protect,&idle)!=hooking::detour::UninstallResult::removed) return false;
    original.store(nullptr,std::memory_order_release);image=0;return true;
}
}
