#pragma once

#include <Windows.h>
#include <intrin.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>

#include "../../../core/logging/log.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "one_au_entrance.h"
#include "homecoming_entrance.h"
#include "../../../state/activity/vanilla/homecoming/runtime.h"
#include "../../../state/activity/vanilla/homecoming/ship_barrier.h"
#include "../../../state/activity/Newlight/launchpad/runtime.h"

namespace dawn::client::hooks::bootflow::omega_vex_lattice_probe {
namespace detail {
// Package 80C7097C runtime header 90: definition 80803910 at A78.
// Unlike the old distant vex_wall hypothesis, this named device is the gate at
// Lighthouse placement 80F5009E/26, entity 80F4AF92, model 80F4AF50.
inline constexpr std::array<std::uint32_t,4> kIdentity{0x80C7097C,0x80803910,0xA78,0};
inline constexpr std::array<std::byte,16> kPositionPrefix{
    std::byte{0x48},std::byte{0x8B},std::byte{0xC4},std::byte{0x48},
    std::byte{0x89},std::byte{0x58},std::byte{0x10},std::byte{0x48},
    std::byte{0x89},std::byte{0x68},std::byte{0x18},std::byte{0x48},
    std::byte{0x89},std::byte{0x70},std::byte{0x20},std::byte{0x57}};
inline constexpr std::array<std::byte,16> kTickPrefix{
    std::byte{0x48},std::byte{0x89},std::byte{0x5C},std::byte{0x24},
    std::byte{0x18},std::byte{0x55},std::byte{0x56},std::byte{0x57},
    std::byte{0x41},std::byte{0x56},std::byte{0x41},std::byte{0x57},
    std::byte{0x48},std::byte{0x83},std::byte{0xEC},std::byte{0x60}};
using PositionFn=std::uint64_t(__fastcall*)(std::byte*,float,char);
using TickFn=std::uint8_t(__fastcall*)(std::byte*,const void*);
inline std::array<hooking::detour::Handle,2> handles{};
inline std::atomic<PositionFn> positionOriginal{};
inline std::atomic<TickFn> tickOriginal{};
inline hooking::CallGate callGate{};
inline std::uintptr_t image{};
inline SRWLOCK receiptLock=SRWLOCK_INIT;
inline unsigned lines{};

// Reads only the object currently passed by the native call; no remembered
// pointer is dereferenced later, no world traversal and no game-side calls.
inline bool copy_bytes(const void* source,void* destination,std::size_t bytes) noexcept {
    if(source==nullptr) { return false; }
    __try { std::memcpy(destination,source,bytes);return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<typename T> inline T value(const std::byte* data,std::size_t offset) noexcept {
    T result{};std::memcpy(&result,data+offset,sizeof(result));return result;
}
struct State final {
    std::uintptr_t address{};
    std::uint32_t self{},owner{},group{},authority{0xEE};
    std::uint8_t initialized{};
    std::array<float,6> channels{};
    std::array<std::int32_t,6> revisions{};
};
inline bool snapshot(std::byte* device,State& out) noexcept {
    std::array<std::uint32_t,4> identity{};
    if(!copy_bytes(device,identity.data(),sizeof(identity)) || identity!=kIdentity) { return false; }
    std::array<std::byte,0x968> raw{};
    if(!copy_bytes(device,raw.data(),raw.size())
        || std::memcmp(raw.data()+0x30,"dg_if_gate",11)!=0
        || std::memcmp(raw.data(),kIdentity.data(),sizeof(kIdentity))!=0) { return false; }
    out.address=reinterpret_cast<std::uintptr_t>(device);
    out.self=value<std::uint32_t>(raw.data(),0x24);
    out.owner=value<std::uint32_t>(raw.data(),0x2C);
    out.group=value<std::uint32_t>(raw.data(),0x70);
    out.initialized=value<std::uint8_t>(raw.data(),0x74);
    constexpr std::array<std::size_t,6> offsets{0x370,0x37C,0xA0,0xAC,0x6A0,0x6AC};
    for(std::size_t i=0;i<6;++i) {
        out.channels[i]=value<float>(raw.data(),offsets[i]);
        out.revisions[i]=value<std::int32_t>(raw.data(),0x950+i*4);
    }
    std::uint32_t word{};
    if(image && copy_bytes(reinterpret_cast<void*>(image+0x26BE0E0
        +((out.owner&0x1FFFU)>>5U)*4U),&word,sizeof(word))) {
        out.authority=(word>>(out.owner&31U))&1U;
    }
    return true;
}
template<typename... Args> inline void report(const char* format,Args... args) noexcept {
    if(lines>=1024) { return; }
    std::array<char,768> line{};
    const int size=std::snprintf(line.data(),line.size(),format,args...);
    if(size<=0 || static_cast<std::size_t>(size)>=line.size()) { return; }
    ++lines;
    core::log::write(core::log::Channel::client,core::log::Level::info,
                    {line.data(),static_cast<std::size_t>(size)});
}
struct Previous final { State state{};std::uint64_t when{}; };
inline std::array<Previous,8> previous{};
inline unsigned replacement{};

inline void ancestry(std::byte* device) noexcept {
    auto address=reinterpret_cast<std::uintptr_t>(device);
    for(unsigned depth=0;depth<8;++depth) {
        std::array<std::byte,0x30> raw{};
        if(address<0x10000 || !copy_bytes(reinterpret_cast<void*>(address),raw.data(),raw.size())) { break; }
        const auto delta=value<std::int64_t>(raw.data(),0x10);
        report("ev=omega_lattice stage=ancestry device=%p depth=%u address=%p "
               "prefix=%08X,%08X,%08X,%08X parent_relative=%lld self=%08X mutation=observe_only",
               static_cast<void*>(device),depth,reinterpret_cast<void*>(address),
               value<std::uint32_t>(raw.data(),0),value<std::uint32_t>(raw.data(),4),
               value<std::uint32_t>(raw.data(),8),value<std::uint32_t>(raw.data(),12),
               static_cast<long long>(delta),value<std::uint32_t>(raw.data(),0x24));
        // Package ancestry is relative at +10. Stop on implausible deltas;
        // prefix words are recorded as evidence, not guessed as actor identity.
        if(delta==0 || delta < -0x1000000 || delta > 0x1000000) { break; }
        address=static_cast<std::uintptr_t>(static_cast<std::int64_t>(address)+0x10+delta);
    }
}
inline void receipt(const char* stage,std::byte* device,const State& state,
                    const State* before,std::uintptr_t caller,float requested,char snap,
                    bool force) noexcept {
    if(!TryAcquireSRWLockExclusive(&receiptLock)) { return; }
    Previous* slot=nullptr;
    for(auto& candidate:previous) {
        if(candidate.state.address==state.address && candidate.state.self==state.self) {
            slot=&candidate;break;
        }
    }
    const bool fresh=slot==nullptr;
    if(fresh) { slot=&previous[replacement++%previous.size()];*slot={}; }
    const auto now=GetTickCount64();
    const bool change=std::memcmp(slot->state.channels.data(),state.channels.data(),sizeof(state.channels))!=0
        || slot->state.revisions!=state.revisions || slot->state.initialized!=state.initialized;
    const bool target=slot->state.channels[1]!=state.channels[1]
        || slot->state.channels[3]!=state.channels[3] || slot->state.channels[5]!=state.channels[5];
    const bool endpoint=state.channels[0]==state.channels[1];
    if(fresh || force || (change && (target || endpoint || now-slot->when>=500))) {
        report("ev=omega_lattice stage=%s device=%p self=%08X owner=%08X authority=%u "
               "group=%08X initialized=%u position=%.6f/%.6f power=%.6f/%.6f lock=%.6f/%.6f "
               "revisions=%d,%d,%d,%d,%d,%d caller_rva=%llX requested=%.6f snap=%d "
               "before_position=%.6f/%.6f mutation=observe_only",
               stage,static_cast<void*>(device),state.self,state.owner,state.authority,state.group,
               static_cast<unsigned>(state.initialized),state.channels[0],state.channels[1],
               state.channels[2],state.channels[3],state.channels[4],state.channels[5],
               state.revisions[0],state.revisions[1],state.revisions[2],state.revisions[3],
               state.revisions[4],state.revisions[5],
               static_cast<unsigned long long>(caller>=image && caller-image<0x8000000 ? caller-image:0),
               requested,static_cast<int>(snap),before ? before->channels[0]:state.channels[0],
               before ? before->channels[1]:state.channels[1]);
        slot->when=now;
        if(fresh) { ancestry(device); }
    }
    slot->state=state;
    ReleaseSRWLockExclusive(&receiptLock);
}
__declspec(noinline) inline std::uint64_t __fastcall position(
    std::byte* device,float requested,char snap) noexcept {
    const hooking::CallGate::Scope call(callGate);
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    State before{},after{};
    const bool matched=call.accepts_side_effects() && snapshot(device,before);
    const auto result=hooking::await_original(positionOriginal)(device,requested,snap);
    std::array<std::uint32_t,4> identity{};
    if(call.accepts_side_effects() && requested==1.F && snap==1
        && copy_bytes(device,identity.data(),sizeof identity)
        && identity==std::array<std::uint32_t,4>{0x80C7069BU,0x80803910U,0xA78U,0}) {
        state::activity::newlight::launchpad::apply_native_lighting_switch(device,requested,snap);
    }
    if(call.accepts_side_effects() && matched && snapshot(device,after) && before.self==after.self) {
        receipt("position_worker",device,after,&before,caller,requested,snap,true);
    }
    return result;
}
__declspec(noinline) inline std::uint8_t __fastcall tick(std::byte* device,const void* context) noexcept {
    const hooking::CallGate::Scope call(callGate);
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    State before{},after{};
    // 1AU-fix patches the first snapshot call in this DF7FF0 callback. Run the
    // repair before the native device tick, regardless of the lattice identity.
    // The Ghost sensor's E4A590 callback is only for console ownership.
    // Homecoming runs first so the 1AU diagnostic line stays the last report of this tick.
    if(call.accepts_side_effects()) { homecoming_entrance::update(image,device);one_au_entrance::update(image,device); }
    const bool matched=call.accepts_side_effects() && snapshot(device,before);
    if(matched && !before.initialized) {
        receipt("first_tick_before",device,before,nullptr,caller,0,0,true);
    }
    const auto result=hooking::await_original(tickOriginal)(device,context);
    const bool barrierPending=call.accepts_side_effects()
        && state::activity::vanilla::homecoming::update_ship_barrier(device);
    // Observe AFTER native initialization. A dormant device's own tick is not
    // a command-delivery mechanism; retain a validated weak binding instead.
    std::array<std::uint32_t,4> identity{};
    if(call.accepts_side_effects() && copy_bytes(device,identity.data(),sizeof identity)
        && identity==std::array<std::uint32_t,4>{0x80FA2F0AU,0x80803910U,0xA78U,0}) {
        state::activity::newlight::launchpad::observe_native_lighting_scene(device);
    }
    if(call.accepts_side_effects() && matched && snapshot(device,after) && before.self==after.self) {
        receipt("tick_after",device,after,&before,caller,0,0,!before.initialized);
    }
    // DF7FF0 returning zero retires its scheduler callback. A ship barrier's
    // device movement ends before its graph fade/receipt, so retain only that
    // authenticated pending device until the guarded native release returns.
    return state::activity::vanilla::homecoming::ship_barrier::tick_result(result,barrierPending);
}
inline bool idle() noexcept { return callGate.idle(); }
} // namespace detail

// Optional diagnostic installation: failure leaves normal mission behavior intact.
// Register before gameplay, alongside other startup probes; do not reinstall live.
inline bool install() noexcept {
    using namespace detail;
    if(handles[0].attached || handles[1].attached) {
        return handles[0].attached && handles[1].attached && callGate.accepting();
    }
    callGate.quiesce();
    image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::array<std::byte,16> positionBytes{},tickBytes{};
    if(!image || !copy_bytes(reinterpret_cast<void*>(image+0xDF6C70),positionBytes.data(),16)
        || !copy_bytes(reinterpret_cast<void*>(image+0xDF7FF0),tickBytes.data(),16)
        || positionBytes!=kPositionPrefix || tickBytes!=kTickPrefix) { return false; }
    const std::array<hooking::detour::Spec,2> specs{{
        {reinterpret_cast<void*>(image+0xDF6C70),reinterpret_cast<void*>(&position)},
        {reinterpret_cast<void*>(image+0xDF7FF0),reinterpret_cast<void*>(&tick)}}};
    if(!hooking::detour::install(specs,handles)) { return false; }
    hooking::publish_original(positionOriginal,reinterpret_cast<PositionFn>(handles[0].original));
    hooking::publish_original(tickOriginal,reinterpret_cast<TickFn>(handles[1].original));
    callGate.accept();
    core::log::write(core::log::Channel::client,core::log::Level::info,
        "ev=omega_lattice stage=install result=ok device=80C7097C name=dg_if_gate mutation=observe_only");
    return true;
}
inline void quiesce() noexcept { detail::callGate.quiesce(); }
// False means a suspended invocation remains active: retain the module and retry
// through the existing teardown policy. Never unload this header's owner on false.
inline bool uninstall() noexcept {
    using namespace detail;
    quiesce();
    if(!handles[0].attached && !handles[1].attached) { return true; }
    const std::array<hooking::detour::ProtectedCodeEntry,9> entries{{
        {reinterpret_cast<void*>(&position)},{reinterpret_cast<void*>(&tick)},
        {reinterpret_cast<void*>(&one_au_entrance::update)},
        {reinterpret_cast<void*>(&homecoming_entrance::update)},
        {reinterpret_cast<void*>(&state::activity::vanilla::homecoming::update_ship_barrier)},
        {reinterpret_cast<void*>(&state::activity::newlight::launchpad::apply_native_lighting_switch)},
        {reinterpret_cast<void*>(&state::activity::newlight::launchpad::observe_native_lighting_scene)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}}};
    if(hooking::detour::uninstall(handles,entries,&idle)!=hooking::detour::UninstallResult::removed) {
        return false;
    }
    positionOriginal.store(nullptr,std::memory_order_release);
    tickOriginal.store(nullptr,std::memory_order_release);
    return true;
}
} // namespace dawn::client::hooks::bootflow::omega_vex_lattice_probe
