#pragma once
#include "controller.h"
#include "lighting.h"
#include "../../../../client/hooks/bootflow/coo_native_components.h"
#include "../../../../client/hooks/bootflow/gateway_native_read.h"

namespace dawn::state::activity::newlight::launchpad::lighting {
namespace gn=client::hooks::bootflow::gateway_native;
struct Binding {
    coo::ObjectReceipt object{};std::uintptr_t device{};std::uint32_t self{};std::int32_t revision{-1};float current{},target{};
    friend bool operator==(const Binding&,const Binding&)=default;
};
// This is a receipt for the breach_group command, not proof of rendered light.
template<class Read> bool sample(Read& read,std::uintptr_t image,std::uintptr_t source,const Request& req,Binding& out,bool includeCompleted=false) noexcept {
    const auto& wanted=req.frame.native[asset_index(kSource)];
    if(!req.owner.valid() || !req.frame.enabled || !req.frame.lightRequested || (req.frame.light && !includeCompleted)
        || !wanted.active || !wanted.prepared || !wanted.acknowledged) {return false;}
    gn::Ref header{};gn::Weak entity{},again{};std::uint32_t applied{},committed{},bundle{},authority{};
    std::uint8_t active{};std::uintptr_t row{},device{},resolved{};
    if(!read.value(source,header) || header.handle!=kSource.definition || header.kind!=0x80809928U || header.offset!=0x4C8
        || !read.value(source+0x180,applied) || applied!=wanted.generation
        || !read.value(source+0x2F0,committed) || committed!=applied
        || !read.value(source+0x188,active) || active!=1
        || !read.value(source+0x440,entity) || !read.entity_row(entity,row)
        || !read.value(row+0x4C,bundle)
        || !client::hooks::bootflow::coo_native::component<Read,1024>(read,bundle,entity.handle,0x80803910U,device)) {return false;}
    Binding b{};std::uint32_t owner{},placed{};
    if(!read.value(device,header) || header.handle!=0x80C7069BU || header.kind!=0x80803910U || header.offset!=0xA78
        || !read.value(device+0x24,b.self) || !read.resolve(b.self,resolved) || resolved!=device
        || !read.value(device+0x2C,owner) || owner!=entity.handle
        || !read.value(device+0x70,placed) || placed==UINT32_MAX
        // DF5070 initializes all device revisions to -1. The first native
        // position command is revision 0; rejecting -1 strands a fresh room.
        || !read.value(device+0x960,b.revision) || b.revision< -1 || b.revision>=INT32_MAX
        || !read.value(device+0x370,b.current) || !coo::native_atom::finite(b.current) || b.current<0.F || b.current>1.F
        || !read.value(device+0x37C,b.target) || !coo::native_atom::finite(b.target) || b.target<0.F || b.target>1.F
        || !read.value(image+0x26BE0E0+4U*((entity.handle&0x1FFFU)/32U),authority)
        || !(authority&(1U<<(entity.handle&31U)))
        || !read.value(source+0x440,again) || again!=entity || !read.weak(again)) {return false;}
    b.object={{req.owner.run,applied},kSource,entity.handle,entity.serial};b.device=device;out=b;return true;
}
}
