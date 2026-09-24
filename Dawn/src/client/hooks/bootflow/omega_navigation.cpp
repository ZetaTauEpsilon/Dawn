#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <utility>

#include "omega_forest_recipe.h"
#include "deadly_trial_presentation.h"
#include "deep_storage_navigation_rules.h"
#include "mission_navigation_hooks.h"
#include "forest_strike_waypoints.h"
#include "gateway_native_read.h"
#include "../../../state/activity/deep_storage/runtime.h"
#include "hijacked_presentation.h"
#include "omega_navigation_rules.h"
#include "internal.h"
#include "../../hooking/detour.h"
#include "../../player/player_position.h"
#include "../../../core/logging/log.h"
#include "../../../state/activity/omega_presentation.h"

namespace dawn::client::hooks::bootflow {
namespace {
namespace presentation = state::activity::omega_presentation;
using RoutePoint = std::uint32_t(__fastcall*)(void*, void*, void*) noexcept;
using HudPoints = void(__fastcall*)(void*) noexcept;
using DirectiveTick = void(__fastcall*)(void*) noexcept;
using DirectiveBuild = void(__fastcall*)(void*, std::uint32_t) noexcept;
using RegisterPoint = void(__fastcall*)(const omega_navigation::Reference*) noexcept;
std::array<hooking::detour::Handle, 4> g_handles{};
std::atomic<RoutePoint> g_routeOriginal{};
std::atomic<HudPoints> g_hudOriginal{};
std::atomic<DirectiveTick> g_directiveTickOriginal{};
std::atomic<DirectiveBuild> g_directiveBuildOriginal{};
std::atomic<RegisterPoint> g_registerPoint{};
using ContextRevision=std::uint32_t(__fastcall*)() noexcept;
std::atomic<ContextRevision> g_contextRevision{};
std::atomic_bool g_enabled{};
std::atomic_uint32_t g_calls{};
std::atomic_uint64_t g_retiredRun{UINT64_MAX};
std::atomic_uint64_t g_terminalGateRun{UINT64_MAX};
SRWLOCK g_traceLock = SRWLOCK_INIT;
std::uint64_t g_traceRun{UINT64_MAX}, g_traceHash{};
unsigned g_traceCount{};
std::uint64_t g_directiveTraceRun{UINT64_MAX};
presentation::NavigationGoal g_directiveTraceGoal{};

struct Call final {
    Call() noexcept { g_calls.fetch_add(1, std::memory_order_acq_rel); }
    ~Call() { g_calls.fetch_sub(1, std::memory_order_release); }
};
bool idle() noexcept { return g_calls.load(std::memory_order_acquire) == 0; }

bool copy(const void* from, std::span<std::byte> to) noexcept {
    SIZE_T copied{};
    return ReadProcessMemory(GetCurrentProcess(), from, to.data(), to.size(), &copied)
           && copied == to.size();
}
template <typename T>
T read(const std::byte* from) noexcept {
    T value{};
    std::memcpy(&value, from, sizeof value);
    return value;
}
template <std::size_t N>
void log(const std::array<char, N>& line, int size) noexcept {
    if (size > 0 && static_cast<std::size_t>(size) < N) {
        core::log::write(core::log::Channel::client, core::log::Level::info,
                        {line.data(), static_cast<std::size_t>(size)});
    }
}

/** FFB850 returns 0=remove, 1=publish, 2=retain. Caller 1007C80 owns registration
 * and removal of this worker's route point; returning 0 uses its normal unregister path.
 * Its verified final-open branch hands off to the fixed exit route immediately.
 * Door interaction markers and the generated route are not modified here. */
__declspec(noinline) std::uint32_t __fastcall route_point_hook(
    void* worker, void* currentNode, void* point) noexcept {
    const Call call;
    const auto original = g_routeOriginal.load(std::memory_order_acquire);
    const auto result = original != nullptr ? original(worker, currentNode, point) : 0U;
    if (!g_enabled.load(std::memory_order_acquire)) { return result; }
    mission_navigation_hooks::forest_terminal(worker,currentNode,result);
    const auto nav = presentation::navigation();
    if (!nav.enabled) { return result; }
    std::array<std::byte, omega_forest::kWorkerPrefixSize> bytes{};
    if (!copy(worker, bytes) || !omega_forest::matches(bytes, "mission_scot")) { return result; }
    if (!nav.forestComplete && g_terminalGateRun.load(std::memory_order_acquire)!=nav.run
        && nav.goal==presentation::NavigationGoal::forestGates) {
        std::array<std::byte,0x38> node{};
        std::uint8_t gatewayIndex{};
        std::uintptr_t gatewayAddress{};
        const auto workerAddress=reinterpret_cast<std::uintptr_t>(worker);
        if (omega_navigation::owns_node(bytes,workerAddress,
                                       reinterpret_cast<std::uintptr_t>(currentNode))
            && copy(currentNode,node)
            && omega_navigation::terminal_gateway_index(bytes,node,result,gatewayIndex)
            && omega_navigation::relative_address(workerAddress,
                read<std::int64_t>(bytes.data()+0x890),
                0x8A0+gatewayIndex*omega_navigation::kGatewaySize,gatewayAddress)) {
            std::array<std::byte,omega_navigation::kGatewaySize> gateway{};
            if (copy(reinterpret_cast<const void*>(gatewayAddress),gateway)
                && omega_navigation::gateway_open(gateway)) {
                g_terminalGateRun.store(nav.run,std::memory_order_release);
                std::array<char,240> line{};
                const int size=std::snprintf(line.data(),line.size(),
                    "ev=omega_navigation stage=terminal_gate run=%llu worker=%p node=%p "
                    "gateway=%u progress=1.000 target_bubble=14",
                    static_cast<unsigned long long>(nav.run),worker,currentNode,
                    static_cast<unsigned>(gatewayIndex));
                log(line,size);
            }
        }
    }
    if (!nav.forestComplete && g_terminalGateRun.load(std::memory_order_acquire)!=nav.run) {
        return result;
    }
    if (g_retiredRun.exchange(nav.run, std::memory_order_acq_rel) != nav.run) {
        std::array<char, 240> line{};
        const int size = std::snprintf(line.data(), line.size(),
            "ev=omega_navigation stage=forest_retired run=%llu worker=%p native=%u "
            "current=%u next=%u goal=%u goal_progress=%.3f",
            static_cast<unsigned long long>(nav.run), worker, result,
            static_cast<unsigned>(read<std::uint8_t>(bytes.data()+0x899)),
            static_cast<unsigned>(read<std::uint8_t>(bytes.data()+0x89A)),
            static_cast<unsigned>(read<std::uint8_t>(bytes.data()+0x89B)),
            read<float>(bytes.data()+0x89C));
        log(line, size);
    }
    return 0U;
}

/** The objective text and its destination have separate native lifecycles. Omega's
 * synthetic type68 body has no target refs, so +100A6E0 creates +A80 as a destination
 * fallback even after the opening is complete. Repair that exact owned point after
 * a rebuild AND on ticks, so crossing a landmark need not wait for another auth body. */
__declspec(noinline) void update_directive_navigation(void* component) noexcept {
    if (!g_enabled.load(std::memory_order_acquire)) { return; }
    const auto nav = presentation::navigation();
    const auto publish = g_registerPoint.load(std::memory_order_acquire);
    if (!nav.enabled || publish == nullptr) { return; }
    std::array<std::byte,omega_navigation::kComponentSize> bytes{};
    omega_navigation::Reference reference{};
    if (!copy(component,bytes) || !omega_navigation::fallback_reference(bytes,reference)) { return; }
    const auto oldPoint=read<std::array<float,4>>(bytes.data()+0xAA0);
    const auto goal=omega_navigation::forward_goal(nav.goal,
        g_terminalGateRun.load(std::memory_order_acquire)==nav.run);
    if (!omega_navigation::update_fallback(bytes,goal)) { return; }
    // The native call owns this live component for the duration of the hook. Only fields
    // whose original bytes were just validated above are copied back to that component.
    auto* destination=static_cast<std::byte*>(component);
    std::memcpy(destination+0xA84,bytes.data()+0xA84,1);
    std::memcpy(destination+0xA8C,bytes.data()+0xA8C,1);
    std::memcpy(destination+0xA98,bytes.data()+0xA98,4);
    std::memcpy(destination+0xAA0,bytes.data()+0xAA0,16);
    publish(&reference); // kind0 follows native C747C0 removal; kind3 refreshes the point.
    AcquireSRWLockExclusive(&g_traceLock);
    const bool emit=g_directiveTraceRun!=nav.run || g_directiveTraceGoal!=goal;
    g_directiveTraceRun=nav.run;
    g_directiveTraceGoal=goal;
    ReleaseSRWLockExclusive(&g_traceLock);
    if (emit) {
        const auto point=read<std::array<float,4>>(bytes.data()+0xAA0);
        std::array<char,360> line{};
        const int size=std::snprintf(line.data(),line.size(),
            "ev=omega_navigation stage=directive run=%llu goal=%u owner=%08X/+%llX "
            "kind=%u target_bubble=%u previous=%.3f,%.3f,%.3f target=%.3f,%.3f,%.3f",
            static_cast<unsigned long long>(nav.run),static_cast<unsigned>(goal),
            reference.handle,static_cast<unsigned long long>(reference.offset),
            static_cast<unsigned>(read<std::uint8_t>(bytes.data()+0xA84)),
            read<std::uint32_t>(bytes.data()+0xA98),
            oldPoint[0],oldPoint[1],oldPoint[2],point[0],point[1],point[2]);
        log(line,size);
    }
}

__declspec(noinline) void __fastcall directive_tick_hook(void* component) noexcept {
    const Call call;
    const auto original=g_directiveTickOriginal.load(std::memory_order_acquire);
    if (original != nullptr) { original(component); }
    if(g_enabled.load(std::memory_order_acquire)) {
        mission_waypoint_native::observe(component);
        hijacked_presentation::observe_directive(component);
        deadly_trial_presentation::observe_directive(component,
            reinterpret_cast<deadly_trial_presentation::Register>(g_registerPoint.load(std::memory_order_acquire)));
    }
    update_directive_navigation(component);
    if(g_enabled.load(std::memory_order_acquire))forest_strike_waypoints::retire(component,g_registerPoint.load(std::memory_order_acquire));
    if(g_enabled.load(std::memory_order_acquire))mission_navigation_hooks::tick(component,
        g_registerPoint.load(std::memory_order_acquire),g_contextRevision.load(std::memory_order_acquire));
}


__declspec(noinline) void __fastcall directive_build_hook(void* component,
                                                        std::uint32_t context) noexcept {
    const Call call;
    if(g_enabled.load(std::memory_order_acquire)) {
        const auto publish=g_registerPoint.load(std::memory_order_acquire);const auto revision=g_contextRevision.load(std::memory_order_acquire);
        if(mission_navigation_hooks::build_beyond(component,context,publish,revision)
            || mission_navigation_hooks::build_deep(component,context,publish,revision)
            || deadly_trial_presentation::build_directive(component,reinterpret_cast<deadly_trial_presentation::Register>(publish),revision))return;
    }
    const auto original=g_directiveBuildOriginal.load(std::memory_order_acquire);
    if (original != nullptr) { original(component,context); }
    update_directive_navigation(component);
    if(g_enabled.load(std::memory_order_acquire))forest_strike_waypoints::retire(component,g_registerPoint.load(std::memory_order_acquire));
}

/** Read-only capture of the native world-space HUD marker list. This distinguishes a
 * remaining mission marker from the worker's route hint without hiding unrelated markers.
 * C73820 emits at most sixteen 0xA0 records after an eight-byte header. */
__declspec(noinline) void __fastcall hud_points_hook(void* list) noexcept {
    const Call call;
    const auto original = g_hudOriginal.load(std::memory_order_acquire);
    if (original != nullptr) { original(list); }
    if (!g_enabled.load(std::memory_order_acquire)) { return; }
    const auto nav = presentation::navigation();
    if (!nav.enabled) { return; }
    std::array<std::byte, 8+16*0xA0> bytes{};
    if (!copy(list, bytes)) { return; }
    const auto count = read<std::uint32_t>(bytes.data());
    if (count > 16) { return; }
    std::uint64_t hash = 14695981039346656037ULL;
    const auto mix = [&hash](std::uint32_t value) noexcept {
        hash = (hash ^ value) * 1099511628211ULL;
    };
    mix(count);
    mix(static_cast<std::uint32_t>(nav.landmark));
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto* record = bytes.data()+8+i*0xA0;
        for (auto offset : {0x20U,0x24U,0x28U,0x2CU,0x30U,0x78U,0x80U,0x84U}) {
            mix(read<std::uint32_t>(record+offset));
        }
    }
    AcquireSRWLockExclusive(&g_traceLock);
    if (g_traceRun != nav.run) { g_traceRun=nav.run; g_traceHash=0; g_traceCount=0; }
    const bool emit = hash != g_traceHash && g_traceCount < 256;
    g_traceHash = hash;
    if (emit) { ++g_traceCount; }
    ReleaseSRWLockExclusive(&g_traceLock);
    if (!emit) { return; }
    const auto player = player::position::snapshot();
    std::array<char, 320> line{};
    int size = std::snprintf(line.data(), line.size(),
        "ev=omega_navigation stage=hud run=%llu landmark=%u count=%u player=%.2f,%.2f,%.2f",
        static_cast<unsigned long long>(nav.run), static_cast<unsigned>(nav.landmark), count,
        player.position[0], player.position[1], player.position[2]);
    log(line, size);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto* record = bytes.data()+8+i*0xA0;
        size = std::snprintf(line.data(), line.size(),
            "ev=omega_navigation stage=marker run=%llu index=%u id=%08X kind=%u "
            "position=%.3f,%.3f,%.3f appearance=%08X/%08X/+%llX",
            static_cast<unsigned long long>(nav.run), i, read<std::uint32_t>(record+0x20),
            static_cast<unsigned>(read<std::uint8_t>(record+0x24)),
            read<float>(record+0x28), read<float>(record+0x2C), read<float>(record+0x30),
            read<std::uint32_t>(record+0x78), read<std::uint32_t>(record+0x7C),
            static_cast<unsigned long long>(read<std::uint64_t>(record+0x80)));
        log(line, size);
    }
}

template <std::size_t N>
void* target(std::uintptr_t rva, const std::array<std::byte, N>& prefix) noexcept {
    const auto* image = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) { return nullptr; }
    std::array<std::byte, N> actual{};
    return copy(image+rva, actual) && actual==prefix ? const_cast<std::byte*>(image+rva) : nullptr;
}
} // namespace

bool install_omega_navigation() noexcept {
    if (g_handles[0].attached) { return g_enabled.load(std::memory_order_acquire); }
    constexpr std::array routePrefix{
        std::byte{0x40},std::byte{0x53},std::byte{0x41},std::byte{0x56},
        std::byte{0x48},std::byte{0x81},std::byte{0xEC},std::byte{0x28},
        std::byte{0x03},std::byte{0x00},std::byte{0x00},std::byte{0x48},
        std::byte{0x8B},std::byte{0x05},std::byte{0x26},std::byte{0xE2}};
    constexpr std::array hudPrefix{
        std::byte{0x40},std::byte{0x56},std::byte{0x41},std::byte{0x56},
        std::byte{0x48},std::byte{0x81},std::byte{0xEC},std::byte{0x28},
        std::byte{0x01},std::byte{0x00},std::byte{0x00},std::byte{0x48},
        std::byte{0x8B},std::byte{0x05},std::byte{0x56},std::byte{0x62}};
    constexpr std::array tickPrefix{
        std::byte{0x48},std::byte{0x89},std::byte{0x5C},std::byte{0x24},
        std::byte{0x10},std::byte{0x48},std::byte{0x89},std::byte{0x6C},
        std::byte{0x24},std::byte{0x18},std::byte{0x56},std::byte{0x57},
        std::byte{0x41},std::byte{0x54},std::byte{0x41},std::byte{0x56}};
    constexpr std::array buildPrefix{
        std::byte{0x40},std::byte{0x55},std::byte{0x53},std::byte{0x56},
        std::byte{0x41},std::byte{0x55},std::byte{0x48},std::byte{0x8D},
        std::byte{0x6C},std::byte{0x24},std::byte{0xC1},std::byte{0x48},
        std::byte{0x81},std::byte{0xEC},std::byte{0xD8},std::byte{0x00}};
    constexpr std::array registerPrefix{
        std::byte{0x41},std::byte{0x56},std::byte{0x48},std::byte{0x83},
        std::byte{0xEC},std::byte{0x30},std::byte{0x44},std::byte{0x8B},
        std::byte{0x01},std::byte{0x4C},std::byte{0x8B},std::byte{0xF1},
        std::byte{0x48},std::byte{0x8B},std::byte{0x49},std::byte{0x08}};
    const auto publish=reinterpret_cast<RegisterPoint>(target(0xC763D0,registerPrefix));
    if (publish == nullptr) { return false; }
    const std::array<hooking::detour::Spec, 4> specs{{
        {target(0xFFB850,routePrefix), reinterpret_cast<void*>(&route_point_hook)},
        {target(0xC73820,hudPrefix), reinterpret_cast<void*>(&hud_points_hook)},
        {target(0x100A3A0,tickPrefix), reinterpret_cast<void*>(&directive_tick_hook)},
        {target(0x100A6E0,buildPrefix), reinterpret_cast<void*>(&directive_build_hook)}
    }};
    if (!hooking::detour::install(specs, g_handles)) {
        core::log::write(core::log::Channel::client,core::log::Level::warn,
                        "ev=omega_navigation stage=install result=fail");
        return false;
    }
    g_routeOriginal.store(reinterpret_cast<RoutePoint>(g_handles[0].original),std::memory_order_release);
    g_hudOriginal.store(reinterpret_cast<HudPoints>(g_handles[1].original),std::memory_order_release);
    g_directiveTickOriginal.store(reinterpret_cast<DirectiveTick>(g_handles[2].original),std::memory_order_release);
    g_directiveBuildOriginal.store(reinterpret_cast<DirectiveBuild>(g_handles[3].original),std::memory_order_release);
    g_registerPoint.store(publish,std::memory_order_release);
    constexpr std::array revisionPrefix{std::byte{0x48},std::byte{0x83},std::byte{0xEC},std::byte{0x28},
        std::byte{0xE8},std::byte{0x87},std::byte{0xFE},std::byte{0xFF},std::byte{0xFF},
        std::byte{0x48},std::byte{0x85},std::byte{0xC0},std::byte{0x74},std::byte{0x08}};
    g_contextRevision.store(reinterpret_cast<ContextRevision>(target(0x4FFB60,revisionPrefix)),std::memory_order_release);
    g_enabled.store(true,std::memory_order_release);
    core::log::write(core::log::Channel::client,core::log::Level::info,
                    "ev=omega_navigation stage=install result=ok targets=+FFB850,+C73820,+100A3A0,+100A6E0");
    return true;
}

void quiesce_omega_navigation() noexcept { g_enabled.store(false,std::memory_order_release); }

bool uninstall_omega_navigation() noexcept {
    quiesce_omega_navigation();
    if (!g_handles[0].attached) { return true; }
    const std::array<hooking::detour::ProtectedCodeEntry,5> protectedCode{{
        {reinterpret_cast<void*>(&route_point_hook)}, {reinterpret_cast<void*>(&hud_points_hook)},
        {reinterpret_cast<void*>(&directive_tick_hook)}, {reinterpret_cast<void*>(&directive_build_hook)},
        {reinterpret_cast<void*>(&update_directive_navigation)}
    }};
    if (hooking::detour::uninstall(g_handles,protectedCode,idle)
        != hooking::detour::UninstallResult::removed) { return false; }
    g_routeOriginal.store(nullptr,std::memory_order_release);
    g_hudOriginal.store(nullptr,std::memory_order_release);
    g_directiveTickOriginal.store(nullptr,std::memory_order_release);
    g_directiveBuildOriginal.store(nullptr,std::memory_order_release);
    g_registerPoint.store(nullptr,std::memory_order_release);
    g_contextRevision.store(nullptr,std::memory_order_release);
    g_retiredRun.store(UINT64_MAX,std::memory_order_release);
    g_terminalGateRun.store(UINT64_MAX,std::memory_order_release);
    return true;
}
} // namespace dawn::client::hooks::bootflow
