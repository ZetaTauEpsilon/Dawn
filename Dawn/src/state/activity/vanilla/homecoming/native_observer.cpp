#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include "runtime.h"
#include "door_native.h"
#include "../../../../core/logging/log.h"
#include "../../coo/native_device_authority.h"
#include "../../../../client/hooks/bootflow/gateway_native_read.h"
#include "../../../../client/hooks/bootflow/coo_native_components.h"
#include "../../../../client/hooks/bootflow/omega_native_readable.h"
namespace dawn::state::activity::vanilla::homecoming {
namespace {
namespace gn=client::hooks::bootflow::gateway_native;
namespace cn=client::hooks::bootflow::coo_native;
struct NativeComponent {std::uint32_t definition,kind,offset;};
std::uintptr_t image() noexcept {return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));}
template<class T> T at(const std::byte* bytes) noexcept {T v{};std::memcpy(&v,bytes,sizeof v);return v;}
bool prefix(const std::byte* b,NativeComponent c) noexcept {return at<std::uint32_t>(b)==c.definition && at<std::uint32_t>(b+4)==c.kind && at<std::uint64_t>(b+8)==c.offset;}
bool copy(const void* source,void* out,std::size_t bytes) noexcept {
    __try { std::memcpy(out,source,bytes);return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}
void observe_native_object(void* raw) noexcept {
    const auto req=request();if(!req.frame.enabled) {return;}gn::Read read{image()};
    const auto source=reinterpret_cast<std::uintptr_t>(raw);std::array<std::byte,16> header{};
    if(!read.copy(source,header) || at<std::uint32_t>(header.data()+4)!=0x80809928U) {return;}
    const AssetBinding* binding{};
    for(const auto& a:kAssets) {if(a.asset.type==4 && prefix(header.data(),{a.asset.definition,0x80809928U,a.offset})) {binding=&a;break;}}
    if(!binding) {return;}const auto& desired=req.frame.native[asset_index(binding->asset)];if(!desired.managed) {return;}
    if(!desired.prepared) {
        std::array<std::byte,0x44> bytes{};
        if(read.copy(source+0x180,bytes) && at<std::uint32_t>(bytes.data())==desired.generation && coo::native_device::inactive_state(bytes)) {observe_prepared(req.owner,binding->asset);}return;
    }
    std::uint32_t generation{},committed{},bundle{};std::uint8_t active{};gn::Weak entity{},again{};std::uintptr_t row{};
    if(!desired.active || !read.value(source+0x180,generation) || generation!=desired.generation
        || !read.value(source+0x2F0,committed) || committed!=generation || !read.value(source+0x188,active) || active!=1
        || !read.value(source+0x440,entity) || !read.entity_row(entity,row) || !read.value(row+0x4C,bundle)
        || !read.value(source+0x440,again) || again!=entity || !read.weak(again) || request().owner!=req.owner) {return;}
    observe_object({{req.owner.run,generation},binding->asset,entity.handle,entity.serial});
}
void poll_bazaar_door() noexcept {
    // The existing camera-frame poll owns native writes. Server publication only
    // drives the door device; this poll releases its authored collision state once.
    const auto wanted=door_request();if(!wanted.enabled()) {return;}
    static DoorRequest applied{};if(applied==wanted) {return;}
    const auto base=image();gn::Read read{base};std::uintptr_t table{};std::uint32_t stride{};
    std::array<unsigned char,16> setter{};
    if(!read.value(base+door_native::kEntities,table) || table<0x10000
        || !read.value(base+door_native::kEntityStride,stride) || stride!=door_native::kStride
        || !read.value(base+door_native::kSetter,setter) || setter!=door_native::kSetterPrefix
        || !client::hooks::bootflow::omega_native_memory::readable(reinterpret_cast<const void*>(table),door_native::kRows*door_native::kStride)) {return;}
    for(std::size_t slot=0;slot<door_native::kRows;++slot) {
        std::array<std::byte,0x98> bytes{};
        if(!copy(reinterpret_cast<const void*>(table+slot*door_native::kStride),bytes.data(),bytes.size())) {continue;}
        const door_native::Row row{at<std::uint32_t>(bytes.data()+4),at<std::uint32_t>(bytes.data()+0xC),
            at<std::uint32_t>(bytes.data()+0x4C),at<std::uint32_t>(bytes.data()+0x88),
            at<std::uint32_t>(bytes.data()+0x8C),at<std::uint64_t>(bytes.data()+0x90)};
        if(!row.live(slot) || !door_native::placement(row) || row.bundle==UINT32_MAX) {continue;}
        std::uintptr_t component{};std::array<std::byte,0x30> componentHeader{};
        if(!cn::component<gn::Read,1024>(read,row.bundle,row.entity,door_native::kComponentKind,component,door_native::kComponentDefinition)
            || !read.copy(component,componentHeader)
            || at<std::uint32_t>(componentHeader.data())!=door_native::kComponentDefinition
            || at<std::uint32_t>(componentHeader.data()+4)!=door_native::kComponentKind
            || at<std::uint32_t>(componentHeader.data()+0x2C)!=row.entity) {continue;}
        // Revalidate the row and the mission request immediately before the engine call.
        std::array<std::byte,0x98> fresh{};
        if(!copy(reinterpret_cast<const void*>(table+slot*door_native::kStride),fresh.data(),fresh.size())
            || std::memcmp(fresh.data(),bytes.data(),bytes.size())!=0 || door_request()!=wanted) {return;}
        // The native named-state setter used by authored destruction (A1FBB0):
        // provider, state name hash, value hash.
        using Set=void(__fastcall*)(void*,const std::uint32_t*,const std::uint32_t*) noexcept;
        const std::uint32_t name=door_native::kStateName,value=door_native::kDestroyed;
        reinterpret_cast<Set>(base+door_native::kSetter)(reinterpret_cast<void*>(component),&name,&value);
        applied=wanted;observe_door_handled();
        std::array<char,224> line{};
        std::snprintf(line.data(),line.size(),"ev=homecoming stage=bazaar_door_state run=%llu generation=%u entity=%08X component=%llX state=destroyed",
            static_cast<unsigned long long>(wanted.owner.run),wanted.generation,row.entity,static_cast<unsigned long long>(component));
        core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
        return;
    }
}
}
