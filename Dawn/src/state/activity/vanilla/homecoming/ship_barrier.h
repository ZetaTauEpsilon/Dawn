#pragma once
#include "controller.h"
#include "door_native.h"

namespace dawn::state::activity::vanilla::homecoming::ship_barrier {
// Only the two authored pod fields downstream of the accepted Ghost scan.
struct Placement {std::uint16_t slot;std::uint32_t record;std::uint64_t authored;};
inline constexpr Placement kPlacements[]{
    {56,25,0xD8FFC2F1F8979C65ULL},{57,26,0x1EB7FCA5F866CD3AULL}};
inline constexpr std::uint32_t kList=0x80C3B4B7U;
inline constexpr std::uint32_t kGraph=0x80F2769AU,kProvider=0x80B9F367U,kDevice=0x80C22C67U;
// 80F2769A opening action 4 sets this named state; action 5 animates the
// presentation variable to zero. Physics 80F27699 is enabled only in BD855C4F.
// Use the actual 80808870 state provider, NOT the 80808A0C physics component.
inline constexpr std::uint32_t kStateName=0x697C33ECU,kOpen=0xBD855C4CU,kBlocking=0xBD855C4FU;
inline constexpr std::uint32_t kPresentation=0xBBC7B08AU;
inline constexpr std::uintptr_t kSetState=0xA1FBB0,kSetVariable=0x576420,kAuthority=0x26BE0E0;
inline constexpr std::uintptr_t kAllocatorTlsIndex=0x20BBB30;
// Native 98D70 dispatches through TLS+50/vtable+18; physics removal also
// frees through TLS+58/vtable+20 (98F40). Camera callbacks lack these services.
// Never install/borrow another thread's allocator to make a mutation succeed.
template<class Read,class Executable>
bool allocator_ready(Read& read,std::uintptr_t tls,Executable&& executable) noexcept {
    if(tls<0x10000 || tls>UINTPTR_MAX-0x60) return false;
    for(const auto offset:{0x50U,0x58U}) {
        std::uintptr_t service{},vtable{};
        if(!read.value(tls+offset,service) || service<0x10000 || !read.value(service,vtable)
            || vtable<0x10000 || vtable>UINTPTR_MAX-0x28) return false;
        for(const auto methodOffset:{0x08U,0x10U,0x18U,0x20U}) {
            std::uintptr_t method{};
            if(!read.value(vtable+methodOffset,method) || !executable(method)) return false;
        }
    }
    return true;
}
// A1FBF0 asks 3F3C00 about the provider's bundle, NOT the entity bitset.
// 3F7A20 resolves that bundle's network record. 4DAD00 is the engine's
// ownership setter: updates +124, synchronizes the entity bit, clears the
// handoff counter and resets replication bookkeeping through 3FBB30.
inline constexpr std::uintptr_t kNetworkRecord=0x3F7A20,kSetNetworkAuthority=0x4DAD00,kBundleAuthority=0x3F3C00;
template<std::size_t N,class Read> bool code_matches(Read& read,std::uintptr_t address,std::uint64_t expected) noexcept {
    std::array<unsigned char,N> bytes{};if(!read.value(address,bytes)) return false;
    std::uint64_t hash=14695981039346656037ULL;
    for(const auto byte:bytes) hash=(hash^byte)*1099511628211ULL;
    return hash==expected;
}
template<class Read> bool authority_code(Read& read,std::uintptr_t base) noexcept {
    return code_matches<110>(read,base+kSetNetworkAuthority,0x29C57CCCC97ACD14ULL)
        && code_matches<47>(read,base+0x403BD0,0x5F813B6AEE56C054ULL)
        && code_matches<8>(read,base+0x3FBB30,0xBCBA18632C231B55ULL)
        && code_matches<75>(read,base+kNetworkRecord,0xE0452FD7E4EF6403ULL)
        && code_matches<133>(read,base+kBundleAuthority,0xABFE9B8F63B5D9FEULL);
}
template<class Read> bool network_record(Read& read,std::uintptr_t address,std::uint32_t self,
    std::uint32_t bundle,std::uint32_t entity,std::uint16_t& flags) noexcept {
    if(address<0x10000 || address>UINTPTR_MAX-0x128 || self==UINT32_MAX || bundle==UINT32_MAX || entity==UINT32_MAX) return false;
    std::uint32_t actualSelf{},actualBundle{},actualEntity{};
    // Native constructor 4D9750 binds +C0=self, +C8=component's owning bundle;
    // 4DAD00's entity-authority publication reads +120. This is not a reflected
    // component header. Accept only the observed attached, idle record (8/9),
    // never a constructing, retiring, or pending-transfer record.
    return read.value(address+0xC0,actualSelf) && actualSelf==self
        && read.value(address+0xC8,actualBundle) && actualBundle==bundle
        && read.value(address+0x120,actualEntity) && actualEntity==entity
        && read.value(address+0x124,flags) && (flags==8 || flags==9);
}
enum class Release {waiting,stale,contextUnavailable,authorityRejected,stateRejected,opened};
template<class Context,class Valid,class State,class Authority,class Set,class Publish>
Release release(Context&& context,Valid&& valid,State&& state,Authority&& authority,Set&& set,Publish&& publish) noexcept {
    if(!context()) return Release::contextUnavailable;
    if(!valid()) return Release::stale;
    std::int32_t current{};
    if(!state(current) || (current!=0 && current!=1)) return Release::waiting;
    if(current==1) {
        if(!authority()) return Release::authorityRejected;
        if(!valid()) return Release::stale;
        if(!context()) return Release::contextUnavailable;
        if(!set()) return Release::stateRejected;
    }
    // An accepted command is not proof of release. Require native state zero
    // before clearing the presentation, and recheck the run/weak identities.
    if(!valid()) return Release::stale;
    if(!state(current) || current!=0) return Release::stateRejected;
    if(!context()) return Release::contextUnavailable;
    publish();return Release::opened;
}
inline constexpr std::array<unsigned char,16> kStatePrefix{
    0x40,0x53,0x48,0x83,0xEC,0x20,0x8B,0x02,0x49,0x8B,0xD8,0x48,0x8D,0x54,0x24,0x38};
inline constexpr std::array<unsigned char,16> kVariablePrefix{
    0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x55,0x57,0x41,0x56,0x48,0x8D};
struct Request {
    coo::Generation owner{};std::uint32_t generation{};
    std::array<std::uint32_t,2> revisions{};
    bool enabled() const noexcept {return owner.valid() && generation && (revisions[0] || revisions[1]);}
    friend bool operator==(const Request&,const Request&)=default;
};
inline Request wanted(coo::Generation owner,const Frame& frame) noexcept {
    if(!owner.valid() || !frame.enabled || frame.fault || frame.finished || !frame.consoleScanned
        || !frame.spawnGeneration || frame.section<static_cast<std::uint8_t>(Section::ship)
        || frame.section>static_cast<std::uint8_t>(Section::escape)) return {};
    Request result{owner,frame.spawnGeneration,{}};
    for(std::size_t i=0;i<std::size(kPlacements);++i) {
        const auto& state=frame.native[asset_index(asset(kShip,23,kPlacements[i].slot))];
        if(state.managed && state.active && state.acknowledged && state.poseKnown && state.deviceSynchronized
            && !state.retired && state.generation && state.generation<INT32_MAX && state.position==1.F
            && state.observedPosition==1.F && state.observedRevision==static_cast<std::int32_t>(state.generation))
            result.revisions[i]=state.generation;
    }
    return result;
}
Request request() noexcept;
inline bool placement(const door_native::Row& row,std::size_t index) noexcept {
    return index<std::size(kPlacements) && row.table==kList && row.record==kPlacements[index].record
        && row.authored==kPlacements[index].authored;
}
// A native device settles before its opening graph and network receipt do.
// Retain only these two current-run callbacks while release is pending; a
// receipt is required for mutation, not for keeping its callback scheduled.
inline bool retain_tick(const Request& wanted,const door_native::Row& row,bool owned) noexcept {
    return wanted.owner.valid() && wanted.generation && owned && (placement(row,0) || placement(row,1));
}
inline constexpr std::uint8_t tick_result(std::uint8_t native,bool pending) noexcept {
    return native ? native : static_cast<std::uint8_t>(pending);
}
template<class Read> bool header(Read& read,std::uintptr_t address,std::uint32_t tag,std::uint32_t kind,std::uint64_t offset) noexcept {
    std::array<std::uint32_t,4> words{};
    return read.value(address,words) && words==std::array<std::uint32_t,4>{tag,kind,static_cast<std::uint32_t>(offset),0};
}
// Do not replace the authored opening with a host timer. Require its actual
// final sample and released writer, like One AU's escape-ship retention.
template<class Read> bool open_curve_finished(Read& read,std::uintptr_t graph) noexcept {
    if(graph<0x10000 || graph>UINTPTR_MAX-0x10000
        || !header(read,graph,kGraph,0x808084E9U,0xD20)) return false;
    std::uint64_t count{};std::int64_t relative{},node{};
    if(!read.value(graph+0xB0,count) || count!=7 || !read.value(graph+0xB8,relative) || relative!=0x628
        || !read.value(graph+0xC0,count) || count!=1 || !read.value(graph+0xC8,relative) || relative!=0x9F8
        || !read.value(graph+0x800,node) || node!=0x200) return false;
    const auto action=graph+0xA00,output=graph+0xAD0;
    float duration{};std::uint64_t remaining{};std::uint8_t phase{};std::int32_t writers{};
    std::array<float,4> value{};
    return header(read,action,kGraph,0x808093C4U,0x1788)
        && header(read,output,kGraph,0x808084DFU,0x1900)
        && read.value(action+0x24,duration) && duration==3.5F
        && read.value(action+0x28,remaining) && remaining==0
        && read.value(action+0x30,phase) && phase==0
        && read.value(output+0x20,value) && value==std::array<float,4>{}
        && read.value(output+0x30,writers) && writers==0;
}
template<class Read> bool state_index(Read& read,std::uintptr_t provider,std::int32_t& index) noexcept {
    if(provider<0x10000 || provider>UINTPTR_MAX-0x10000
        || !header(read,provider,kProvider,0x80808870U,0x128)) return false;
    std::uint64_t count{};std::int64_t relative{};
    return read.value(provider+0x30,count) && count==1
        && read.value(provider+0x38,relative) && relative==0x18
        && header(read,provider+0x60,kProvider,0x80808868U,0x1D0)
        && read.value(provider+0x70,relative) && relative==-0x70
        && read.value(provider+0x80,index) && index>=0 && index<4;
}
}
