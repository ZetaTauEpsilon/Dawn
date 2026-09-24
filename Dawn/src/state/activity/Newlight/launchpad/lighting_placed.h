#pragma once
#include "lighting_native.h"

namespace dawn::state::activity::newlight::launchpad::lighting {
// The room contains an authored controller in addition to the type-4 clone.
// Match the full native identity AND encoded placement, never proximity alone.
struct Placed {gn::Weak entity{},device{};friend bool operator==(const Placed&,const Placed&)=default;};
// Discovery observes devices already passed by the native tick. Remember only
// generation-checked handles; selection rechecks placement against the source.
template<class Read> bool capture(Read& read,std::uintptr_t address,Placed& out) noexcept {
    Placed p{};gn::Ref header{};std::uint32_t self{},owner{},property{},actual{},flags{};
    std::uintptr_t resolved{},row{};
    if(!read.value(address,header) || header.handle!=0x80C7069BU || header.kind!=0x80803910U || header.offset!=0xA78
        || !read.value(address+0x24,self) || !read.make_weak(self,p.device) || !read.resolve(self,resolved) || resolved!=address
        || !read.value(address+0x70,property) || property!=UINT32_MAX
        || !read.value(address+0x2C,owner) || !read.make_weak(owner,p.entity) || !read.entity_row(p.entity,row)
        || !read.value(row+0xC,actual) || actual!=owner || !read.value(row+4,flags) || (flags&5U)
        || !read.weak(p.device) || !read.weak(p.entity)) {return false;}
    out=p;return true;
}
// Original 9ED060 dereferences this thread-local queue while DF6C70 emits
// position effects. Camera/render callbacks do not have it. Never synthesize TLS.
template<class Read> bool simulation_context(Read& read,std::uintptr_t image,std::uintptr_t tls) noexcept {
    std::uint32_t index{},depth{};std::uintptr_t block{},queue{};
    return tls>=0x10000 && read.value(image+0x330F0E0,index) && index<1024
        && read.value(tls+static_cast<std::uintptr_t>(index)*8,block) && block>=0x10000
        && block<=UINTPTR_MAX-0x2428 && read.value(block+0x2428,queue) && queue>=0x10000
        && read.value(queue,depth) && depth>0 && depth<=64;
}
template<class Read> bool placed(Read& read,const Binding& source,std::uint32_t handle,Placed& out) noexcept {
    Placed p{};gn::Ref header{};std::uintptr_t address{},row{},sourceRow{};
    std::uint32_t self{},owner{},actual{},flags{},property{};std::int32_t revision{};float target{};
    std::array<std::byte,16> origin{},other{};
    if(handle==source.self || !read.make_weak(handle,p.device) || !read.resolve(handle,address)
        || !read.value(address,header) || header.handle!=0x80C7069BU || header.kind!=0x80803910U || header.offset!=0xA78
        || !read.value(address+0x24,self) || self!=handle || !read.value(address+0x2C,owner)
        || owner==source.object.entity || !read.make_weak(owner,p.entity) || !read.entity_row(p.entity,row)
        || !read.value(row+0xC,actual) || actual!=owner || !read.value(row+4,flags) || (flags&5U)
        || !read.value(address+0x70,property) || property!=UINT32_MAX
        || !read.value(address+0x960,revision) || revision!= -1
        || !read.value(address+0x37C,target) || target!=0.F
        || !read.entity_row({source.object.serial,source.object.entity},sourceRow)
        || !read.copy(row+0xD0,origin) || !read.copy(sourceRow+0xD0,other) || origin!=other
        || std::all_of(origin.begin(),origin.end(),[](auto byte){return byte==std::byte{};})
        || !read.weak(p.device) || !read.weak(p.entity)) {return false;}
    out=p;return true;
}
template<class Read,std::size_t N> bool select(Read& read,const Binding& source,
    const std::array<Placed,N>& candidates,Placed& out) noexcept {
    Placed selected{};unsigned count{};
    for(const auto& candidate:candidates) {
        if(candidate.device.handle==UINT32_MAX || !read.weak(candidate.device) || !read.weak(candidate.entity)) {continue;}
        Placed checked{};
        if(!placed(read,source,candidate.device.handle,checked) || checked!=candidate) {continue;}
        if(count && checked==selected) {continue;}
        selected=checked;if(++count>1) {return false;}
    }
    if(count!=1) {return false;}out=selected;return true;
}
// Native decoded dynamic list: one 0x60-byte record. The game's DF6510 consumer
// owns the position setter, interpolation, effects and dirty notification.
// Neither DF6510 nor its DF6C70 setter may be called from camera polling:
// both ultimately enqueue native effects through the simulation-thread TLS.
inline std::array<std::byte,0x60> position_record(std::uint32_t generation) noexcept {
    std::array<std::byte,0x60> bytes{};
    const auto put=[&]<class T>(std::size_t offset,T value){std::memcpy(bytes.data()+offset,&value,sizeof value);};
    put(0,std::int32_t{1});put(0x10,std::uint32_t{0x80805063U});
    put(0x20,std::int32_t{-1});put(0x24,std::int32_t{-1});put(0x28,1.F);
    put(0x2C,std::int32_t{-1});put(0x30,std::int32_t{-1});put(0x34,0.F);
    put(0x38,static_cast<std::int32_t>(generation));put(0x3C,std::int32_t{-1});put(0x40,1.F);
    return bytes;
}
}
