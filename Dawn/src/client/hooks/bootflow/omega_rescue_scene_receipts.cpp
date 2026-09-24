#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>

#include "omega_rescue_scene_receipts.h"
#include "gateway_vance_native_path.h"
#include "strike_bond_ending_scene_path.h"
#include "beyond_infinity_native_receipts.h"
#include "beyond_infinity_future_cast.h"
#include "../../../state/activity/beyond_infinity/runtime.h"
#include "../../../state/activity/vanilla/homecoming/runtime.h"
#include "../../../state/activity/vanilla/adieu/runtime.h"
#include "../../../state/activity/strike_bond/runtime.h"
#include "../../../state/activity/gateway/runtime.h"
#include "omega_enemy_native_reference.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../core/logging/log.h"
#include "../../../state/activity/omega_first_lair_runtime.h"
#include "../../../state/activity/omega_presentation.h"
#include "../../../state/activity/omega_rescue_scene_authority.h"

namespace dawn::client::hooks::bootflow {
namespace {
namespace npc=state::activity::omega_rescue_npc;
namespace fight=state::activity::omega_first_lair;
using Tick=void(__fastcall*)(void*) noexcept;
hooking::CallGate g_gate;
hooking::detour::Handle g_handle{};
std::atomic<Tick> g_original{};
std::uintptr_t g_image{};
constexpr std::array<unsigned char,23> kPrefix{
    0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,
    0x83,0xEC,0x20,0x48,0x8B,0xD9,0x48,0x8D,0x54,0x24,0x30};
struct Ref {std::uint32_t handle{},kind{};std::int64_t offset{};};
// Original 351C90 writes {serial,handle}; 352310 compares serial at +0 for handle at +4.
struct Weak {std::uint32_t serial{UINT32_MAX},handle{UINT32_MAX};};
static_assert(sizeof(Ref)==16 && sizeof(Weak)==8);

/** Why a component whose definition names a catalog Scene did not yield a receipt.
 * Logged once per (run, component, reason), bounded per run; observe-only. */
enum class Reject : std::uint8_t {
    none,
    definition,      // definition unresolvable or typed identity (registry/type/slot) mismatch
    request,         // encounter has not requested this Scene (dormant, stopped or invalid)
    group,           // 4E5C60 group handle unreadable or invalid
    sensor,          // 4E5D80 sense handle unreadable or invalid
    generation,      // component +254 differs from the requested generation
    weak,            // +2E8 unreadable, or 352310 reports the selector missing/recycled
    selector,        // selector header {graph,80806384,...,own handle} mismatch
    post_identity,   // group/sensor handle changed across the original call
    post_generation, // +254 changed across the original call
    post_status,     // +258/+264/+268 unreadable or outside the native range
    blocking_child   // entrance finished, but the 808062FE (runtime ref kind; definition class 808062FD) node/child is not qualified
};
constexpr const char* name(Reject reason) noexcept {
    switch(reason) {
    case Reject::definition: return "definition";
    case Reject::request: return "request";
    case Reject::group: return "group";
    case Reject::sensor: return "sensor";
    case Reject::generation: return "generation";
    case Reject::weak: return "weak";
    case Reject::selector: return "selector";
    case Reject::post_identity: return "post_identity";
    case Reject::post_generation: return "post_generation";
    case Reject::post_status: return "post_status";
    case Reject::blocking_child: return "blocking_child";
    default: return "none";
    }
}
struct Diag {
    Reject reason{};
    std::uint32_t generation{UINT32_MAX},expected{};
    Weak weak{};
    std::uint32_t tableSerial{UINT32_MAX};
    std::int32_t directoryStride{};
    std::uint16_t groupIndex{UINT16_MAX};
    std::uint32_t group{UINT32_MAX},sensor{UINT32_MAX};
    std::uintptr_t selectorAddress{};
    std::array<std::uint32_t,4> header{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX}; // +0,+4,+24,+2C
    std::uint8_t completed{UINT8_MAX};
    std::uint32_t count{UINT32_MAX};
    std::uintptr_t node{};
    std::uint8_t nodeState{UINT8_MAX};
    std::int32_t nodeStop{INT32_MIN};
    Weak child{};
    std::uint32_t childTag{};
};

template<class T> T at(const std::byte* bytes) noexcept {
    T value{};std::memcpy(&value,bytes,sizeof value);return value;
}
bool add(std::uintptr_t base,std::int64_t offset,std::uintptr_t& value) noexcept {
    if(offset>=0) {
        if(static_cast<std::uint64_t>(offset)>UINTPTR_MAX-base) {return false;}
        value=base+static_cast<std::uintptr_t>(offset);
    } else {
        const auto magnitude=static_cast<std::uint64_t>(-(offset+1))+1U;
        if(magnitude>base) {return false;}value=base-static_cast<std::uintptr_t>(magnitude);
    }
    return value>=0x10000;
}
struct Read {
    std::size_t copied{};
    bool copy(std::uintptr_t address,std::span<std::byte> output) noexcept {
        if(address<0x10000 || address>UINTPTR_MAX-output.size()
            || copied>16384 || output.size()>16384-copied) {return false;}
        copied+=output.size();SIZE_T size{};
        return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),
            output.data(),output.size(),&size) && size==output.size();
    }
    template<class T> bool value(std::uintptr_t address,T& output) noexcept {
        return copy(address,std::as_writable_bytes(std::span{&output,std::size_t{1}}));
    }
    bool resolve(const Ref& ref,std::uintptr_t& output) noexcept {
        if(ref.handle==UINT32_MAX) {return false;}
        std::uintptr_t directory{},registry{};
        if(!value(g_image+0x2439C70,directory) || !value(directory,registry)) {return false;}
        const auto shifted=static_cast<std::uint32_t>(static_cast<std::int32_t>(ref.handle)>>13);
        const auto index=((static_cast<std::uint64_t>(shifted)|0xFFC0000ULL)>>18)&(shifted&0xFFFFU);
        std::array<std::byte,0x38> row{};
        if(!copy(registry+index*0x40,row)) {return false;}
        const auto stride=at<std::int32_t>(row.data()+0x30);
        if(stride<=0 || stride>0x100000) {return false;}
        std::uintptr_t element{};
        if(!add(at<std::uintptr_t>(row.data()+8),
            static_cast<std::int64_t>(ref.handle&0x1FFFU)*stride,element)) {return false;}
        std::uint64_t relocation{};
        if(!value(element+8,relocation)) {return false;}
        const auto base=omega_enemy_native_reference::corrected_base(element,relocation,
            at<std::int32_t>(row.data()+0x34));
        return add(static_cast<std::uintptr_t>(base),ref.offset,output);
    }
    // Original 352310 (also 351C90): P = *(image+2439C70); rows = *P; row stride = i32 at
    // P+10 (NOT P+8); metadata = *(rows + idx*stride + 10); head = *metadata;
    // elements = *(metadata+8); count = u16 at head+1C; serial = u32 at
    // elements + u32(metadata+1C) + (handle&1FFF)*u32(metadata+20). idx =
    // (((handle>>31)&3C00)|3FF) & (handle>>13) & FFFF. Matching only the low actor
    // index or a readable pointer is insufficient; the serial must match.
    bool weak(const Weak& ref,std::uintptr_t& output,Diag& diag) noexcept {
        if(ref.handle==UINT32_MAX) {return false;}
        std::uintptr_t directory{},registry{},metadata{},head{},elements{};
        std::int32_t directoryStride{};
        if(!value(g_image+0x2439C70,directory) || !value(directory,registry)
            || !value(directory+0x10,directoryStride)) {return false;}
        diag.directoryStride=directoryStride;
        if(directoryStride<=0 || directoryStride>0x1000) {return false;}
        const auto index=((static_cast<std::int32_t>(ref.handle)>>31&0x3C00U)|0x3FFU)
            &(ref.handle>>13)&0xFFFFU;
        if(!value(registry+static_cast<std::uintptr_t>(index)*directoryStride+0x10,metadata)
            || metadata==0 || !value(metadata,head) || !value(metadata+8,elements)
            || elements==0) {return false;}
        std::uint16_t count{};std::uint32_t offset{},stride{},serial{};
        if(!value(head+0x1C,count) || (ref.handle&0x1FFFU)>=count
            || !value(metadata+0x1C,offset) || !value(metadata+0x20,stride)
            || stride==0 || stride>0x100000) {return false;}
        std::uintptr_t entry{};
        if(!add(elements,static_cast<std::int64_t>(offset)+
            static_cast<std::int64_t>(ref.handle&0x1FFFU)*stride,entry)
            || !value(entry,serial)) {return false;}
        diag.tableSerial=serial;
        if(serial!=ref.serial) {return false;}
        return resolve({ref.handle,0,0},output);
    }
};
struct Capture {
    const npc::Scene* scene{};
    fight::SceneRequest request{};
    std::uint32_t group{UINT32_MAX};
    std::uint16_t groupIndex{UINT16_MAX};
    std::uint32_t sensor{UINT32_MAX};
    std::uint32_t selector{UINT32_MAX};
    std::uintptr_t selectorAddress{};
    std::uint8_t completed{};
    std::uint32_t events{};
};
// Original 4E5C60: u16 at component+20, masked 1FFF, indexes rows at *(image+1F92108) with
// stride *(image+1F92110); returns the u32 at row+20. The accessor itself does not prove the
// returned salted handle shares the row's low bits, so that equality is not an admission
// guard; the handle is revalidated after the original call instead.
bool group_handle(Read& read,std::uintptr_t component,std::uint32_t& handle,std::uint16_t& index) noexcept {
    std::uintptr_t table{};std::int32_t stride{};
    if(!read.value(component+0x20,index) || !read.value(g_image+0x1F92108,table)
        || !read.value(g_image+0x1F92110,stride) || stride<0x24 || stride>0x100000
        || !read.value(table+static_cast<std::uintptr_t>(index&0x1FFFU)*stride+0x20,handle)) {return false;}
    return handle!=UINT32_MAX;
}
bool capture(Read& read,std::uintptr_t component,std::uint64_t run,Capture& out,Diag& diag) noexcept {
    std::array<std::byte,16> bytes{};
    if(!read.copy(component,bytes)) {return false;}
    const auto ref=at<Ref>(bytes.data());
    if(ref.kind!=0x80806266U || ref.offset<=0 || ref.offset>0x100000) {return false;}
    for(const auto& scene:npc::kScenes) {
        if(scene.definition==ref.handle) {out.scene=&scene;break;}
    }
    if(out.scene==nullptr) {return false;}
    std::uintptr_t definition{};std::array<std::byte,8> scope{};
    if(!read.resolve(ref,definition) || !read.copy(definition+0x30,scope)
        || at<std::uint32_t>(scope.data())!=npc::kRegistry
        || at<std::uint16_t>(scope.data()+4)!=43
        || at<std::uint16_t>(scope.data()+6)!=out.scene->slot) {diag.reason=Reject::definition;return false;}
    out.request=fight::scene_request(run,out.scene->slot);
    diag.expected=out.request.command.generation;
    if(!out.request.enabled || out.request.command.generation==0 || out.request.command.stop
        || !npc::valid(out.request.command)) {diag.reason=Reject::request;return false;}
    if(!group_handle(read,component,out.group,out.groupIndex)) {
        diag.groupIndex=out.groupIndex;diag.reason=Reject::group;return false;
    }
    diag.groupIndex=out.groupIndex;diag.group=out.group;
    if(!read.value(component+0x170,out.sensor) || out.sensor==UINT32_MAX) {diag.reason=Reject::sensor;return false;}
    diag.sensor=out.sensor;
    std::uint32_t generation{};Weak weak{};
    if(!read.value(component+0x254,generation)) {diag.reason=Reject::generation;return false;}
    diag.generation=generation;
    if(generation!=out.request.command.generation) {diag.reason=Reject::generation;return false;}
    if(!read.value(component+0x2E8,weak)) {diag.reason=Reject::weak;return false;}
    diag.weak=weak;
    if(!read.weak(weak,out.selectorAddress,diag)) {diag.reason=Reject::weak;return false;}
    diag.selectorAddress=out.selectorAddress;
    // B43220 stores the handle read at the created selector component's +24 (557470) and
    // B438B0/DADD10 read the selector's +0 definition reference {graph,80806384,offset};
    // +2C is the owning instance handle B438B0 destroys on completion.
    std::array<std::byte,0x80> selector{};
    if(!read.copy(out.selectorAddress,selector)) {diag.reason=Reject::selector;return false;}
    diag.header={at<std::uint32_t>(selector.data()),at<std::uint32_t>(selector.data()+4),
        at<std::uint32_t>(selector.data()+0x24),at<std::uint32_t>(selector.data()+0x2C)};
    if(diag.header[0]!=out.scene->selector-1U || diag.header[1]!=0x80806384U
        || diag.header[2]!=weak.handle) {diag.reason=Reject::selector;return false;}
    out.selector=weak.handle;
    return true;
}
// Original 1B6ACB0 keeps the 808062FD-class node (runtime Ref kind 808062FE, live-verified 2026-09-06 PID 33984) running while its instance is alive (DA9820),
// child weak +1B0 resolves (352310) and stop counter +1A0 <= 0; DAB8C0 keeps the node state
// byte at +98 (1 = started, 2 = completed).
bool blocking_child(Read& read,const Capture& captured,Diag& diag) noexcept {
    const auto slot=captured.scene->slot;
    const std::size_t nodeIndex=slot==9?26U:(slot==27 || slot==33)?2U:slot==46?4U:SIZE_MAX;
    if(nodeIndex==SIZE_MAX) {return false;}
    std::uint64_t count{};std::int64_t relative{};std::uintptr_t table{},node{};
    if(!read.value(captured.selectorAddress+0x38,count) || count>256 || nodeIndex>=count
        || !read.value(captured.selectorAddress+0x40,relative)
        || !add(captured.selectorAddress+0x40,relative+0x10,table)) {return false;}
    const auto pointer=table+nodeIndex*0x30+0x20;
    if(!read.value(pointer,relative) || !add(pointer,relative,node)) {return false;}
    diag.node=node;
    Ref definitionRef{};std::uintptr_t definition{},childAddress{};
    if(!read.value(node,definitionRef)
        || definitionRef.handle!=captured.scene->selector-1U || definitionRef.kind!=0x808062FEU
        || !read.resolve(definitionRef,definition) || !read.value(definition+0x58,diag.childTag)
        || diag.childTag!=0x80EC0E00U) {return false;}
    if(!read.value(node+0x98,diag.nodeState) || !read.value(node+0x1A0,diag.nodeStop)
        || !read.value(node+0x1B0,diag.child)) {return false;}
    return diag.nodeState==1 && diag.nodeStop<=0 && read.weak(diag.child,childAddress,diag);
}

constexpr std::size_t kRejectLimit=64;
struct RejectKey {std::uint64_t run{};std::uintptr_t component{};Reject reason{};};
SRWLOCK g_rejectLock=SRWLOCK_INIT;
std::array<RejectKey,kRejectLimit> g_rejects{};
std::size_t g_rejectCount{};
std::uint64_t g_rejectRun{};
bool g_rejectLimitLogged{};
bool admit_reject(std::uint64_t run,std::uintptr_t component,Reject reason) noexcept {
    bool admit=false,limit=false;
    AcquireSRWLockExclusive(&g_rejectLock);
    if(g_rejectRun!=run) {g_rejectRun=run;g_rejectCount=0;g_rejectLimitLogged=false;}
    if(g_rejectCount>=kRejectLimit) {
        if(!g_rejectLimitLogged) {g_rejectLimitLogged=true;limit=true;}
    } else {
        bool seen=false;
        for(std::size_t i=0;i<g_rejectCount;++i) {
            const auto& key=g_rejects[i];
            if(key.component==component && key.reason==reason) {seen=true;break;}
        }
        if(!seen) {g_rejects[g_rejectCount++]={run,component,reason};admit=true;}
    }
    ReleaseSRWLockExclusive(&g_rejectLock);
    if(limit) {
        std::array<char,128> text{};
        const int size=std::snprintf(text.data(),text.size(),
            "ev=omega_rescue_scene stage=reject_limit run=%llu limit=%u boundary=B438B0",
            static_cast<unsigned long long>(run),static_cast<unsigned>(kRejectLimit));
        if(size>0 && static_cast<std::size_t>(size)<text.size()) {
            core::log::write(core::log::Channel::client,core::log::Level::info,
                {text.data(),static_cast<std::size_t>(size)});
        }
    }
    return admit;
}
void log_reject(const Capture& captured,const Diag& diag,std::uintptr_t component,std::uint64_t run) noexcept {
    if(captured.scene==nullptr || diag.reason==Reject::none || !admit_reject(run,component,diag.reason)) {return;}
    std::array<char,640> text{};
    const int size=std::snprintf(text.data(),text.size(),
        "ev=omega_rescue_scene stage=reject reason=%s run=%llu slot=%u component=%016llX"
        " generation=%08X expected=%u weak=%08X/%08X table_serial=%08X dir_stride=%d"
        " group_index=%u group=%08X sensor=%08X selector=%016llX header=%08X,%08X,%08X,%08X"
        " complete=%u events=%u node=%016llX node_state=%u node_stop=%d child=%08X/%08X child_tag=%08X"
        " boundary=B438B0 mutation=observe_only",
        name(diag.reason),static_cast<unsigned long long>(run),captured.scene->slot,
        static_cast<unsigned long long>(component),diag.generation,diag.expected,
        diag.weak.serial,diag.weak.handle,diag.tableSerial,diag.directoryStride,
        diag.groupIndex,diag.group,diag.sensor,static_cast<unsigned long long>(diag.selectorAddress),
        diag.header[0],diag.header[1],diag.header[2],diag.header[3],
        diag.completed,diag.count,static_cast<unsigned long long>(diag.node),diag.nodeState,diag.nodeStop,
        diag.child.serial,diag.child.handle,diag.childTag);
    if(size>0 && static_cast<std::size_t>(size)<text.size()) {
        core::log::write(core::log::Channel::client,core::log::Level::info,
            {text.data(),static_cast<std::size_t>(size)});
    }
}
void report(const Capture& captured,fight::SceneMilestone milestone) noexcept {
    if(!fight::observe_scene(captured.request.token,captured.scene->slot,milestone)) {return;}
    if(milestone==fight::SceneMilestone::started) {
        if(captured.scene->slot==46) {
            state::activity::omega_presentation::note_encounter(captured.request.token.boss.run,
                state::activity::omega_presentation::Encounter::osirisHolds,3);
        } else {state::activity::omega_presentation::observe_scene(captured.scene->definition,true);}
    }
    std::array<char,384> text{};
    const int size=std::snprintf(text.data(),text.size(),
        "ev=omega_rescue_scene stage=receipt run=%llu slot=%u generation=%u selector=%08X"
        " group=%08X group_index=%u sensor=%08X complete=%u events=%u milestone=%u boundary=B438B0",
        static_cast<unsigned long long>(captured.request.token.boss.run),captured.scene->slot,
        captured.request.command.generation,captured.selector,captured.group,captured.groupIndex,
        captured.sensor,captured.completed,captured.events,static_cast<unsigned>(milestone));
    if(size>0 && static_cast<std::size_t>(size)<text.size()) {
        core::log::write(core::log::Channel::client,core::log::Level::info,
            {text.data(),static_cast<std::size_t>(size)});
    }
}
namespace gateway=state::activity::gateway;
struct GatewaySceneCapture {
    gateway::SceneReceipt receipt{};
    Weak weak{};
    std::uintptr_t address{};
    Reject rejected{};
};
bool capture_gateway_scene(Read& read,std::uintptr_t component,const gateway::EndingRequest& request,GatewaySceneCapture& out) noexcept {
    if(!request.enabled || !request.sceneGeneration) { return false; }
    Ref ref{};std::uintptr_t definition{};std::array<std::byte,8> scope{};
    if(!read.value(component,ref) || ref.handle!=0x80F46DE0U || ref.kind!=0x80806266U || ref.offset!=0x368) { return false; }
    out.rejected=Reject::definition;
    if(!read.resolve(ref,definition) || !read.copy(definition+0x30,scope)
        || at<std::uint32_t>(scope.data())!=0xBA0B27A0U || at<std::uint16_t>(scope.data()+4)!=43
        || at<std::uint16_t>(scope.data()+6)!=5) { return false; }
    std::uint16_t index{};std::uint32_t group{},sensor{},generation{};Weak weak{};Diag diag{};
    out.rejected=Reject::group;
    if(!group_handle(read,component,group,index) || !read.value(component+0x170,sensor) || sensor==UINT32_MAX) { return false; }
    out.rejected=Reject::generation;
    if(!read.value(component+0x254,generation) || generation!=request.sceneGeneration) { return false; }
    out.rejected=Reject::weak;
    if(!read.value(component+0x2E8,weak) || !read.weak(weak,out.address,diag)) { return false; }
    out.rejected=Reject::selector;
    Ref selector{};std::uint32_t self{},owner{};
    if(!read.value(out.address,selector) || selector.handle!=0x80EC0ABCU || selector.kind!=0x80806384U
        || !read.value(out.address+0x24,self) || self!=weak.handle
        || !read.value(out.address+0x2C,owner) || owner==UINT32_MAX) { return false; }
    out.weak=weak;out.receipt={request.run,generation,group,sensor,self};out.rejected=Reject::none;return true;
}
void complete_gateway_scene(Read& read,std::uintptr_t component,const GatewaySceneCapture& capture) noexcept {
    std::uint32_t group{},sensor{},generation{},count{};std::uint16_t index{};std::uint8_t complete{};
    if(!group_handle(read,component,group,index) || group!=capture.receipt.group
        || !read.value(component+0x170,sensor) || sensor!=capture.receipt.sensor
        || !read.value(component+0x254,generation) || generation!=capture.receipt.generation
        || !read.value(component+0x258,complete) || complete>1
        || !read.value(component+0x264,count) || count>32) { return; }
    Weak weak{};Diag diag{};std::uintptr_t address{};
    if(!read.value(component+0x2E8,weak) || weak.serial!=capture.weak.serial || weak.handle!=capture.weak.handle
        || !read.weak(weak,address,diag)) { return; }
    const auto stage=gateway_vance_native_path::probe(read,address,weak.handle,
        [&](const gateway_vance_native_path::Weak& child,std::uintptr_t& out) noexcept {
            return read.weak({child.serial,child.handle},out,diag);
        });
    Weak after{};
    if(!stage.valid || !read.value(component+0x2E8,after) || after.serial!=weak.serial || after.handle!=weak.handle) { return; }
    gateway::observe_scene(capture.receipt,false);
    if(stage.turned) { gateway::observe_vance(capture.receipt,gateway::VanceMilestone::turned); }
    if(stage.conversation) { gateway::observe_vance(capture.receipt,gateway::VanceMilestone::conversationStarted); }
    if(complete) { gateway::observe_scene(capture.receipt,true); }
}
#include "beyond_infinity_scene_receipts.inl"
#include "homecoming_scene_receipts.inl"
#include "adieu_scene_receipts.inl"
#include "strike_bond_ending_scene_receipts.inl"
__declspec(noinline) void __fastcall tick(void* raw) noexcept {
    hooking::CallGate::Scope gate(g_gate);
    const auto original=hooking::await_original(g_original);
    const auto nav=state::activity::omega_presentation::navigation();
    Read read;Capture captured;Diag diag;
    const auto component=reinterpret_cast<std::uintptr_t>(raw);
    const bool observing=gate.accepts_side_effects() && nav.enabled && nav.run!=0;
    const bool owned=observing && capture(read,component,nav.run,captured,diag);
    const auto gatewayRequest=gate.accepts_side_effects()?gateway::ending_request():gateway::EndingRequest{};
    GatewaySceneCapture gatewayCapture{};Read gatewayRead{};
    const bool gatewayOwned=capture_gateway_scene(gatewayRead,component,gatewayRequest,gatewayCapture);
    if(!gatewayOwned && gatewayCapture.rejected!=Reject::none
        && admit_reject(gatewayRequest.run,component,gatewayCapture.rejected)) {
        std::array<char,256> line{};
        const int size=std::snprintf(line.data(),line.size(),
            "ev=gateway stage=vance_scene_wait run=%llu generation=%u reason=%s component=%016llX boundary=B438B0 mutation=observe_only",
            static_cast<unsigned long long>(gatewayRequest.run),gatewayRequest.sceneGeneration,name(gatewayCapture.rejected),static_cast<unsigned long long>(component));
        if(size>0 && static_cast<std::size_t>(size)<line.size()) { core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(size)}); }
    }
    beyond_native::SceneSample beyondBefore{};Read beyondRead{};
    state::activity::coo::Generation beyondOwner{};bool beyondOwned{};
    if(gate.accepts_side_effects() && read_beyond_scene(beyondRead,component,beyondBefore)) {
        const auto request=beyond::request();beyond::SceneReceipt receipt{};
        beyondOwned=beyond_native::scene_sample(beyondBefore,request.frame,request.owner,receipt);beyondOwner=request.owner;
        if(beyondOwned) {
            prepare_beyond_scene(beyondRead,component,beyondBefore,request);
            beyond::observe_scene(receipt,false);observe_beyond_speech(beyondRead,component,beyondBefore,receipt);
        }
    }
    if(gate.accepts_side_effects()) {observe_homecoming_scene(component);}
    original(raw);
    if(gate.accepts_side_effects()) {observe_homecoming_scene(component);}
    if(gate.accepts_side_effects()) {observe_adieu_scene(component);}
    if(gate.accepts_side_effects()) observe_garden_ending_speech(component);
    if(beyondOwned && gate.accepts_side_effects()) { finish_beyond_scene(beyondRead,component,beyondBefore,beyondOwner); }
    if(gatewayOwned && gate.accepts_side_effects()) { complete_gateway_scene(gatewayRead,component,gatewayCapture); }
    if(!observing || !gate.accepts_side_effects()) {return;}
    const auto now=state::activity::omega_presentation::navigation();
    if(!now.enabled || now.run!=nav.run) {return;}
    if(!owned) {log_reject(captured,diag,component,nav.run);return;}
    std::uint32_t sensor{},group{},generation{},count{};std::uint16_t index{};std::uint8_t completed{};
    if(!group_handle(read,component,group,index) || group!=captured.group
        || !read.value(component+0x170,sensor) || sensor!=captured.sensor) {
        diag.reason=Reject::post_identity;diag.group=group;diag.groupIndex=index;diag.sensor=sensor;
        log_reject(captured,diag,component,nav.run);return;
    }
    if(!read.value(component+0x254,generation) || generation!=captured.request.command.generation) {
        diag.reason=Reject::post_generation;diag.generation=generation;
        log_reject(captured,diag,component,nav.run);return;
    }
    // DADD10 output: +258 complete (selector state byte +7C == 4), +264 count (<= 32), +268 hashes.
    if(!read.value(component+0x258,completed) || completed>1
        || !read.value(component+0x264,count) || count>32) {
        diag.reason=Reject::post_status;diag.completed=completed;diag.count=count;
        log_reject(captured,diag,component,nav.run);return;
    }
    std::array<std::uint32_t,32> events{};
    if(count!=0 && !read.copy(component+0x268,
        std::as_writable_bytes(std::span(events).first(count)))) {
        diag.reason=Reject::post_status;diag.completed=completed;diag.count=count;
        log_reject(captured,diag,component,nav.run);return;
    }
    captured.completed=completed;captured.events=count;
    diag.completed=completed;diag.count=count;
    report(captured,fight::SceneMilestone::started);
    bool entranceFinished=captured.scene->slot==27 || captured.scene->slot==33;
    for(std::size_t i=0;i<count;++i) {
        entranceFinished=entranceFinished || events[i]==npc::kIntroBlockingReady;
    }
    if(!completed && entranceFinished) {
        if(blocking_child(read,captured,diag)) {report(captured,fight::SceneMilestone::rescueReady);}
        else {diag.reason=Reject::blocking_child;log_reject(captured,diag,component,nav.run);}
    }
    if(completed) {report(captured,fight::SceneMilestone::completed);}
}
bool idle() noexcept {return g_gate.idle();}
}

bool install_omega_rescue_scene_receipts() noexcept {
    if(g_original.load(std::memory_order_acquire)!=nullptr) {return g_gate.accepting();}
    g_gate.quiesce();g_image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(g_image==0) {return false;}
    auto* target=reinterpret_cast<unsigned char*>(g_image+0xB438B0);
    if(!std::equal(kPrefix.begin(),kPrefix.end(),target)) {return false;}
    if(!hooking::detour::install({reinterpret_cast<std::byte*>(target),reinterpret_cast<void*>(&tick)},g_handle)) {return false;}
    hooking::publish_original(g_original,reinterpret_cast<Tick>(g_handle.original));
    g_gate.accept();
    core::log::write(core::log::Channel::client,core::log::Level::info,
        "ev=omega_rescue_scene stage=install result=ok target=B438B0 mutation=observe_only");
    return true;
}
void quiesce_omega_rescue_scene_receipts() noexcept {g_gate.quiesce();}
bool uninstall_omega_rescue_scene_receipts() noexcept {
    g_gate.quiesce();
    if(g_original.load(std::memory_order_acquire)==nullptr) {return true;}
    const std::array protectedEntries{
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&tick)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&observe_homecoming_scene)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&observe_adieu_scene)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&read_beyond_scene)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&finish_beyond_scene)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&prepare_beyond_scene)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&recover_beyond_future_cast)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&create_beyond_future_actor_safe)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&observe_beyond_speech)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&clear_beyond_callback)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&capture_gateway_scene)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&complete_gateway_scene)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}};
    if(hooking::detour::uninstall(g_handle,protectedEntries,&idle)!=hooking::detour::UninstallResult::removed) {return false;}
    g_original.store(nullptr,std::memory_order_release);g_image=0;return true;
}
} // namespace dawn::client::hooks::bootflow
