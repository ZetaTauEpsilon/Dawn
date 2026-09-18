#include "deadly_trial_revival.h"
#include "../../../state/activity/strike_pact/runtime.h"
#include "../../../state/activity/strike_bond/runtime.h"
#include "../../../state/activity/deep_storage/runtime.h"
#include "../../../state/activity/hijacked/runtime.h"
#include "../../../state/activity/hijacked/scan_playback.h"
#include "../../../state/activity/vanilla/one_au/bridge_native.h"
#include "../../../state/activity/deep_storage/scan_playback.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../state/activity/deadly_trial/runtime.h"
#include "../../../state/activity/deadly_trial/ending_audio.h"
#include "../../../core/logging/log.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
namespace dawn::client::hooks::bootflow::deadly_trial_revival {
namespace {
namespace trial=state::activity::deadly_trial;
using gateway_native::Ref;using gateway_native::Weak;using gateway_native::Read;
using Tick=std::uintptr_t(__fastcall*)(void*,void*) noexcept;
hooking::CallGate gate;
hooking::detour::Handle hook{};
std::atomic<Tick> original{};
std::uintptr_t image{};
SRWLOCK stateLock=SRWLOCK_INIT;
struct State {
    state::activity::coo::Generation run{};
    trial::SceneReceipt receipt{};
    Weak owner{},ghost{};
    bool summoned{},bound{},started{},finished{},audio{};
} state;
struct Capture { std::uintptr_t owner{};Weak weak{};std::uint32_t group{},sensor{}; };
void log(const char* stage,std::uint64_t run,std::uint32_t handle) noexcept {
    std::array<char,192> line{};
    const int n=std::snprintf(line.data(),line.size(),"ev=deadly_trial stage=%s run=%llu handle=%08X boundary=E4A590",
        stage,static_cast<unsigned long long>(run),handle);
    if(n>0 && static_cast<std::size_t>(n)<line.size()) {
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});
    }
}
bool capture(Read& read,std::uintptr_t component,Capture& out) noexcept {
    Ref ref{},ownerRef{};std::uintptr_t definition{},table{};std::uint32_t stride{},self{},entity{};
    std::uint16_t index{};std::array<std::uint32_t,2> scope{};float duration{};
    if(!read.value(component,ref) || ref.handle!=0x80B2E6D7U || ref.kind!=0x80804D32U || ref.offset!=0x258
        || !read.resolve(ref.handle,definition) || !read.value(definition+0x288,scope)
        || scope[0]!=0x27660927U || scope[1]!=65U
        || !read.value(component+0x20,index) || !read.value(image+0x1F92108,table)
        || !read.value(image+0x1F92110,stride) || stride<0x24 || stride>0x100000
        || !read.value(table+static_cast<std::uintptr_t>(index&0x1FFFU)*stride+0x20,out.group) || out.group==UINT32_MAX
        || !read.value(component+0x170,out.sensor) || out.sensor==UINT32_MAX
        || !read.value(component+0x1D8,out.weak) || !read.weak(out.weak) || !read.resolve(out.weak.handle,out.owner)
        || !read.value(out.owner,ownerRef) || ownerRef.handle!=0x80FBDA45U || ownerRef.kind!=0x80804D3AU || ownerRef.offset!=0x358
        || !read.value(out.owner+0x24,self) || self!=out.weak.handle
        || !read.value(out.owner+0x2C,entity) || entity==UINT32_MAX
        || !read.value(out.owner+0x290,duration) || duration!=37.75F) { return false; }
    return true;
}
bool spawner(Read& read,std::uintptr_t& out) noexcept {
    std::uint32_t player{UINT32_MAX};
    reinterpret_cast<void(__fastcall*)(std::uint32_t*)>(image+0x4B2260)(&player);
    std::uintptr_t table{},row{},base{};std::uint32_t stride{},actual{},flags{},self{};
    if(player==UINT32_MAX || !read.value(image+0x1F93428,table) || !read.value(image+0x1F93430,stride)
        || stride<0x50 || stride>0x100000) { return false; }
    row=table+static_cast<std::uintptr_t>(player&0x1FFFU)*stride;
    if(!read.value(row+12,actual) || actual!=player || !read.value(row+4,flags) || (flags&4U)) { return false; }
    alignas(16) std::array<std::byte,0x40> result{};
    if(!reinterpret_cast<bool(__fastcall*)(std::uintptr_t,std::uint32_t,void*,std::uint32_t)>(image+0x557470)(row,0x808061CFU,result.data(),0)) { return false; }
    const auto handle=gateway_native::at<std::uint32_t>(result.data()+0x18);
    const auto offset=gateway_native::at<std::int64_t>(result.data()+0x20);
    if(offset<0 || offset>0x100000 || !read.resolve(handle,base)) { return false; }
    out=base+static_cast<std::uintptr_t>(offset);Ref ref{};std::uintptr_t resolved{};
    return read.value(out,ref) && ref.handle==0x80B9E438U && ref.kind==0x8080699AU && ref.offset==0x460
        && read.value(out+0x2C,actual) && actual==player && read.value(out+0x24,self)
        && read.resolve(self,resolved) && resolved==out;
}
bool ghost(Read& read,std::uintptr_t sp,std::uintptr_t& out,Weak& weak,std::uint8_t& mode) noexcept {
    Weak entity{};std::uint32_t handle{},self{},owner{};std::uintptr_t row{};Ref ref{};
    if(!read.value(sp+0x15C,entity) || !read.entity_row(entity,row)
        || !read.value(row+12,owner) || owner!=entity.handle
        || !read.value(sp+0x164,handle) || !read.resolve(handle,out)
        || !read.value(out,ref) || ref.kind!=0x80803F45U || ref.offset<=0 || ref.offset>0x100000
        || !read.value(out+0x24,self) || self!=handle || !read.value(out+0x2C,owner) || owner!=entity.handle
        || !read.value(out+0x2C8,mode)) { return false; }
    // 351C90 constructs the salted component weak reference used by the device.
    reinterpret_cast<void(__fastcall*)(Weak*,std::uint32_t)>(image+0x351C90)(&weak,handle);
    return read.weak(weak);
}
void update(std::uintptr_t component) noexcept {
    const auto request=trial::request();
    if(!request.enabled || !request.preparing || !request.owner.valid() || !request.interaction.valid()) { return; }
    Read read{image};Capture native{};
    if(!capture(read,component,native) || !TryAcquireSRWLockExclusive(&stateLock)) { return; }
    struct Unlock { ~Unlock() { ReleaseSRWLockExclusive(&stateLock); } } unlock;
    if(state.run!=request.owner) { state={};state.run=request.owner;state.owner=native.weak; }
    if(state.owner!=native.weak || state.finished) { return; }
    if(!state.bound) {
        std::uintptr_t sp{},actor{};Weak actorWeak{};std::uint8_t mode{};
        if(!spawner(read,sp)) { return; }
        if(!ghost(read,sp,actor,actorWeak,mode)) {
            Weak existing{};
            if(state.summoned || !read.value(sp+0x15C,existing) || existing.handle!=UINT32_MAX) { return; }
            std::uint32_t spawned{UINT32_MAX},selector{};state.summoned=true;
            // The native primitive uses the default zero selector, not the Tab UI.
            reinterpret_cast<void(__fastcall*)(std::uintptr_t,std::uint32_t*,const std::uint32_t*)>(image+0xB72E40)(sp,&spawned,&selector);
            log("revival_ghost_summoned",request.owner.run,spawned);return;
        }
        if(!request.sceneGeneration || mode!=1) { return; }
        const Ref link{native.weak.handle,0x80804D56U,0x30};
        // Retail 10A5370 releases the held-Ghost weapon slot BEFORE binding.
        // Omitting B737D0 leaves the Guardian's hand on screen during the scene.
        reinterpret_cast<void(__fastcall*)(std::uintptr_t)>(image+0xB737D0)(sp);
        reinterpret_cast<void(__fastcall*)(std::uintptr_t,const Ref*)>(image+0xBD7DF0)(actor,&link);
        Weak target{};std::int64_t offset{};
        if(!read.value(actor+0x1C8,target) || target!=native.weak || !read.value(actor+0x1D0,offset) || offset!=0x30) { return; }
        state.receipt={request.owner.run,request.sceneGeneration,native.group,native.sensor,native.weak.handle};
        state.ghost=actorWeak;state.bound=true;
        if(!trial::observe_scene_binding(state.receipt)) { state.finished=true; }
        return;
    }
    if(request.sceneGeneration!=state.receipt.generation || native.group!=state.receipt.group || native.sensor!=state.receipt.sensor) { return; }
    Playback playback{};
    if(!read.value(native.owner+0x294,playback.revision) || !read.value(native.owner+0x298,playback.mode)
        || !read.value(native.owner+0x299,playback.active) || !read.value(native.owner+0x29C,playback.elapsed)
        || !read.value(native.owner+0x290,playback.duration)) { return; }
    std::array<Weak,6> members{};bool participant{};
    if(read.value(native.owner+0x80,members) && read.weak(state.ghost)) {
        participant=std::find(members.begin(),members.end(),state.ghost)!=members.end();
    }
    if(!state.started && playback.started(request.sceneGeneration,participant)) {
        state.started=true;trial::observe_scene(state.receipt,false);
    }
    // Read-only observation of the authored row-10 event crossing. Native
    // animation dispatches the audio; the host only retains its remaining tail.
    if(state.started && !state.audio && playback.valid(request.sceneGeneration)
        && playback.active && participant && trial::valid_revival_audio_cue(playback.elapsed)) {
        state.audio=trial::observe_scene_audio(state.receipt,playback.elapsed);
    }
    if(playback.finished(request.sceneGeneration,state.started)) {
        state.finished=true;trial::observe_scene(state.receipt,true);
    }
}
#include "deep_storage_scan_receipts.inl"
#include "hijacked_scan_receipts.inl"
#include "campaign_scan_receipts.inl"
#include "one_au_bridge_scan.inl"
__declspec(noinline) std::uintptr_t __fastcall tick(void* component,void* output) noexcept {
    hooking::CallGate::Scope scope(gate);
    const auto fn=hooking::await_original(original);
    const auto result=fn(component,output);
    if(scope.accepts_side_effects()) {
        const auto sensor=reinterpret_cast<std::uintptr_t>(component);
        update(sensor);deep_scan::update(sensor);hijacked_scan::update(sensor);campaign_scan::update(sensor);one_au_scan::update(sensor);
    }
    return result;
}
bool idle() noexcept { return gate.idle(); }
struct Entry { std::uintptr_t rva;std::array<unsigned char,16> prefix; };
// Checked against the unpacked 2017 client and the accepted live call sequence.
constexpr Entry entries[]{
    {0xE4A590,{0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xfa,0x48,0x8b,0xd9}},
    {0x4B2260,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0xc7,0x01,0xff,0xff,0xff,0xff,0x48}},
    {0x557470,{0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x81,0xec,0x60,0x0c,0x00,0x00,0x48,0x8b,0x05}},
    {0xB72E40,{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20,0x55}},
    {0xB737D0,{0x40,0x57,0x48,0x83,0xec,0x50,0x0f,0xb7,0x41,0x2c,0x48,0x8b,0xf9,0x25,0xff,0x1f}},
    {0xBD7DF0,{0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x80,0xb9,0xc8,0x02,0x00,0x00}}
};
}
bool install() noexcept {
    if(original.load(std::memory_order_acquire)) { return gate.accepting(); }
    gate.quiesce();image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));Read read{image};
    for(const auto& entry:entries) {
        std::array<unsigned char,16> bytes{};
        if(!read.value(image+entry.rva,bytes) || bytes!=entry.prefix) { log("revival_install_mismatch",0,static_cast<std::uint32_t>(entry.rva));return false; }
    }
    if(!hooking::detour::install({reinterpret_cast<void*>(image+0xE4A590),reinterpret_cast<void*>(&tick)},hook)) { return false; }
    state={};hooking::publish_original(original,reinterpret_cast<Tick>(hook.original));gate.accept();log("revival_install",0,0);return true;
}
void quiesce() noexcept { gate.quiesce(); }
bool uninstall() noexcept {
    gate.quiesce();if(!original.load(std::memory_order_acquire)) { return true; }
    const std::array protectedEntries{
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&tick)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&update)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&deep_scan::update)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hijacked_scan::update)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&campaign_scan::update)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&one_au_scan::update)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}};
    if(hooking::detour::uninstall(hook,protectedEntries,&idle)!=hooking::detour::UninstallResult::removed) { return false; }
    original.store(nullptr,std::memory_order_release);state={};image=0;return true;
}
}
