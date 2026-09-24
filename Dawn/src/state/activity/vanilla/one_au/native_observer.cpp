#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <mutex>
#include "runtime.h"
#include "escape_ship.h"
#include "../../../../core/logging/log.h"
#include "../../coo/native_device_authority.h"
#include "../../../../client/hooks/bootflow/gateway_native_read.h"
#include "../../../../client/hooks/bootflow/coo_enemy_readiness.h"
#include "../../../../client/hooks/bootflow/coo_native_player_mount.h"
namespace dawn::state::activity::vanilla::one_au {
namespace {
namespace gn=client::hooks::bootflow::gateway_native;
namespace cn=client::hooks::bootflow::coo_native;
std::mutex shipMutex;
struct Ship {coo::ObjectReceipt receipt{};std::uintptr_t source{};bool held{};} ship;
struct NativeComponent {std::uint32_t definition,kind,offset;};
// Source placement +580 -> entity resource list -> component's typed definition.
inline constexpr NativeComponent components[]{
    {},{0x80C77FD7U,0x80804221U,0x598},
    {0x80C3D831U,0x80804FB2U,0x388},{0x80C77FD7U,0x80804221U,0x598},
    {0x80C77D2DU,0x80804FB2U,0x388},{0x80C76783U,0x80804B8AU,0x9E8},
    {0x80C76783U,0x80804B8AU,0x9E8},{0x80C76783U,0x80804B8AU,0x9E8}
};
std::uintptr_t image() noexcept {return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));}
template<class T> T at(const std::byte* bytes) noexcept {T v{};std::memcpy(&v,bytes,sizeof v);return v;}
bool prefix(const std::byte* b,NativeComponent c) noexcept {return at<std::uint32_t>(b)==c.definition && at<std::uint32_t>(b+4)==c.kind && at<std::uint64_t>(b+8)==c.offset;}
bool current(gn::Read& read,const ComponentReceipt& r,std::uintptr_t address) noexcept {
    const auto i=interaction_index(r.asset);if(!r.valid() || i>=std::size(components)) {return false;}
    const auto req=request();if(!req.frame.enabled || req.owner.run!=r.owner.run || req.frame.interactions[i].binding!=r) {return false;}
    const auto& desired=req.frame.native[asset_index(r.asset)];if(!desired.active || desired.generation!=r.owner.value) {return false;}
    std::array<std::byte,0x30> c{};std::array<std::byte,16> s{};std::uintptr_t resolved{};gn::Weak entity{},again{};
    std::uint32_t generation{},committed{};std::uint8_t active{};
    return read.copy(address,c) && prefix(c.data(),components[i]) && at<std::uint32_t>(c.data()+0x24)==r.component && at<std::uint32_t>(c.data()+0x2C)==r.entity
        && read.resolve(r.component,resolved) && resolved==address
        && read.copy(r.source,s) && prefix(s.data(),{r.asset.definition,0x80809928U,0x4C8})
        && read.value(r.source+0x180,generation) && generation==r.owner.value
        && read.value(r.source+0x2F0,committed) && committed==generation
        && read.value(r.source+0x188,active) && active==1
        && read.value(r.source+0x440,entity) && entity.handle==r.entity && entity.serial==r.serial && read.weak(entity)
        && read.value(r.source+0x440,again) && again==entity && request().owner==req.owner;
}
ComponentReceipt identify(void* raw) noexcept {
    const auto req=request();if(!req.frame.enabled) {return {};}
    const auto address=reinterpret_cast<std::uintptr_t>(raw);std::array<std::byte,0x30> header{};gn::Read read{image()};
    if(!read.copy(address,header)) {return {};}
    for(std::size_t i=0;i<std::size(components);++i) {
        const auto& s=req.frame.interactions[i];if(!s.armed || !prefix(header.data(),components[i])) {continue;}
        gn::Read check{image()};if(current(check,s.binding,address)) {return s.binding;}
    }
    return {};
}
void interceptor_allegiance(gn::Read& read,const Request& req,const coo::ObjectReceipt& receipt,
                           std::uint32_t bundle,std::uintptr_t source) noexcept {
    if(receipt.source!=asset(kAccess,4,6) || req.frame.finished
        || req.frame.cinematic.phase!=cinematics::Phase::gameplay) {return;}
    // 80FDB7F3 is the authored vehicle radar component, not a combatant.
    std::uintptr_t radar{};std::array<std::byte,0x30> header{};
    if(!cn::component<gn::Read,1024>(read,bundle,receipt.entity,0x80804098U,radar)
        || !read.copy(radar,header) || !prefix(header.data(),{0x80FDB7F3U,0x80804098U,0x80})) {return;}
    static std::mutex mutex;static coo::ObjectReceipt applied{};
    const std::scoped_lock lock(mutex);if(applied==receipt) {return;}
    // SET_FACTION's native entity setter. NONE (-1) falls back to authored
    // Cabal allegiance; REMOVED (-2) yields neutral in 4F1BD0. Apply once at
    // creation so subsequent native boarding/ownership changes remain in charge.
    const auto base=image();std::array<std::byte,16> setter{},getter{};
    constexpr std::uint8_t setCode[]{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x48};
    constexpr std::uint8_t getCode[]{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0xE8,0x52,0x2D,0xEE,0xFF,0x48,0x8B};
    if(!read.copy(base+0x3D50C0,setter) || std::memcmp(setter.data(),setCode,16)
        || !read.copy(base+0x4F1BB0,getter) || std::memcmp(getter.data(),getCode,16)
        || request().owner!=req.owner) {return;}
    gn::Weak live{};std::uint32_t generation{},committed{};std::uint8_t active{};
    const auto currentRequest=request();const auto& desired=currentRequest.frame.native[asset_index(receipt.source)];
    if(currentRequest.owner!=req.owner || !desired.active || desired.generation!=receipt.owner.value
        || !read.value(source+0x180,generation) || generation!=receipt.owner.value
        || !read.value(source+0x2F0,committed) || committed!=generation
        || !read.value(source+0x188,active) || active!=1
        || !read.value(source+0x440,live) || live.handle!=receipt.entity || live.serial!=receipt.serial || !read.weak(live)) {return;}
    using Get=std::int32_t*(__fastcall*)(std::int32_t*,std::uint32_t);
    using Set=void(__fastcall*)(std::uint32_t,std::int8_t);
    auto get=reinterpret_cast<Get>(base+0x4F1BB0);std::int32_t before{},after{};
    get(&before,receipt.entity);
    reinterpret_cast<Set>(base+0x3D50C0)(receipt.entity,-2);
    get(&after,receipt.entity);applied=receipt;
    // Runs only from the existing native source callback; no new interception.
    observe_interceptor_faction(receipt,before,after);
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
    const auto interactionSlot=interaction_index(binding->asset);
    if((interactionSlot==1 || interactionSlot==3) && desired.active && desired.acknowledged
        && req.frame.interactions[interactionSlot].binding.valid()) {
        std::uint32_t observedGeneration{},observedCommit{};std::uint8_t observedActive{};gn::Weak observedEntity{};
        if(read.value(source+0x180,observedGeneration) && observedGeneration==desired.generation
            && read.value(source+0x2F0,observedCommit) && observedCommit==observedGeneration
            && read.value(source+0x188,observedActive) && observedActive==1
            && read.value(source+0x440,observedEntity) && !read.weak(observedEntity)
            && request().owner==req.owner) {observe_lost(req.frame.interactions[interactionSlot].binding);return;}
    }
    if(!desired.active || !read.value(source+0x180,generation) || generation!=desired.generation
        || !read.value(source+0x2F0,committed) || committed!=generation || !read.value(source+0x188,active) || active!=1
        || !read.value(source+0x440,entity) || !read.entity_row(entity,row) || !read.value(row+0x4C,bundle)
        || !read.value(source+0x440,again) || again!=entity || !read.weak(again) || request().owner!=req.owner) {return;}
    const coo::ObjectReceipt objectReceipt{{req.owner.run,generation},binding->asset,entity.handle,entity.serial};
    observe_object(objectReceipt);
    if(binding->asset==asset(kCore,4,16)) {
        const std::scoped_lock lock(shipMutex);
        if(ship.receipt!=objectReceipt || ship.source!=source) {ship={objectReceipt,source};}
    }
    interceptor_allegiance(read,req,objectReceipt,bundle,source);
    const auto i=interaction_index(binding->asset);if(i>=std::size(components)) {return;}
    std::uintptr_t ptr{};std::array<std::byte,0x30> bytes{};gn::Read componentRead{image()};
    if(!cn::component<gn::Read,1024>(componentRead,bundle,entity.handle,components[i].kind,ptr)
        || !read.copy(ptr,bytes) || !prefix(bytes.data(),components[i]) || at<std::uint32_t>(bytes.data()+0x2C)!=entity.handle) {return;}
    const ComponentReceipt receipt{{req.owner.run,generation},binding->asset,source,entity.handle,entity.serial,at<std::uint32_t>(bytes.data()+0x24)};
    observe_binding(receipt);
    if(i>=5) {
        gn::Read check{image()};std::uint8_t flags{};
        if(current(check,receipt,ptr) && check.value(ptr+0x338,flags)) {observe_destruction(receipt,(flags&1U)!=0);}
    }
}
void poll_escape_ship() noexcept {
    // The existing camera-frame poll owns native writes. Server publication
    // only drives the device's approach; it must not call this setter.
    const std::scoped_lock lock(shipMutex);if(!ship.source) {return;}
    const auto req=request();const auto& r=ship.receipt;
    const auto wanted=[&](const Request& q) {
        const auto& s=q.frame.native[asset_index(asset(kCore,4,16))];
        return q.frame.enabled && !q.frame.finished && !q.frame.fault && !q.frame.recovery.active()
            && q.frame.cinematic.phase==cinematics::Phase::gameplay && q.frame.interactions[4].inserted
            && q.owner==req.owner && q.owner.run==r.owner.run && s.active && s.acknowledged
            && s.generation==r.owner.value;
    };
    if(!wanted(req)) {ship={};return;}
    const auto base=image();gn::Read read{base};std::uint32_t bundle{};
    const auto live=[&](std::uint32_t& resources) {
        std::array<std::byte,16> header{};std::uint32_t generation{},committed{};
        std::uint8_t active{};gn::Weak entity{},again{};std::uintptr_t row{};
        return read.copy(ship.source,header) && prefix(header.data(),{r.source.definition,0x80809928U,0x4C8})
            && read.value(ship.source+0x180,generation) && generation==r.owner.value
            && read.value(ship.source+0x2F0,committed) && committed==generation
            && read.value(ship.source+0x188,active) && active==1
            && read.value(ship.source+0x440,entity) && entity.handle==r.entity && entity.serial==r.serial
            && read.entity_row(entity,row) && read.value(row+0x4C,resources)
            && read.value(ship.source+0x440,again) && again==entity && read.weak(again);
    };
    std::uintptr_t graph{};
    if(!live(bundle) || !cn::component<gn::Read,1024>(read,bundle,r.entity,0x808084E9U,graph,0x80C77DA7U)
        || !escape_ship::at_pickup(read,graph)) {return;}
    // SET_ENTITY_VARIABLE, already used by the engine integration for Omega.
    constexpr std::uint8_t code[]{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x55,0x57,0x41,0x56,0x48,0x8D};
    std::array<std::byte,16> entry{};std::uint32_t again{};
    if(!read.copy(base+0x576420,entry) || std::memcmp(entry.data(),code,16)
        || !live(again) || again!=bundle || !wanted(request())) {return;}
    using Set=void(__fastcall*)(std::uint32_t,const std::uint32_t*,const float*);
    // Refresh the animation inputs while the curve is inactive. This keeps its
    // final sample presented, without restarting the graph or entering departure.
    for(const auto& v:escape_ship::kPickupVariables) {
        alignas(16) const std::array<float,4> value{v.value,v.value,v.value,v.value};
        reinterpret_cast<Set>(base+0x576420)(r.entity,&v.name,value.data());
    }
    if(!ship.held) {
        ship.held=true;std::array<char,192> line{};
        std::snprintf(line.data(),line.size(),"ev=one_au stage=escape_ship_pickup run=%llu generation=%u entity=%08X path=%.3f",
            static_cast<unsigned long long>(r.owner.run),r.owner.value,r.entity,escape_ship::kPickup);
        core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
    }
}
void observe_native_player(std::uint32_t entity) noexcept {
    static std::atomic<std::uint32_t> player{UINT32_MAX};static std::atomic<std::uint64_t> next{};
    if(entity!=UINT32_MAX) {player.store(entity);}else {entity=player.load();}
    const auto now=GetTickCount64();if(now<next.load() || entity==UINT32_MAX) {return;}next.store(now+100);
    const auto req=request();if(!req.frame.enabled || req.frame.cinematic.phase!=cinematics::Phase::gameplay) {return;}
    gn::Read read{image()};std::uintptr_t table{},health{};std::uint32_t stride{},flags{},bundle{},again{};std::uint8_t dead{};
    if(!read.value(image()+0x1F93428,table) || !read.value(image()+0x1F93430,stride) || stride<0x50 || stride>0x100000) {return;}
    const auto row=table+static_cast<std::uintptr_t>(entity&0x1FFFU)*stride;
    if(!read.value(row+4,flags) || (flags&4U) || !read.value(row+0x4C,bundle)
        || !cn::component<gn::Read,1024>(read,bundle,entity,0x80804B8AU,health)
        || !read.value(health+0x338,dead) || !read.value(row+0x4C,again) || again!=bundle
        || !read.value(health+0x2C,again) || again!=entity || request().owner!=req.owner) {return;}
    observe_life(req.owner,entity,(dead&1U)==0);
}
void poll_native_objects() noexcept {
    // Like Trial's Pike observer, poll independently of the foot physics body:
    // boarding removes that body, while the controlled rider keeps its transform.
    const auto mountedRequest=request();
    if(mountedRequest.frame.enabled && !mountedRequest.frame.finished
        && !mountedRequest.frame.recovery.active()
        && mountedRequest.frame.cinematic.phase==cinematics::Phase::gameplay) {
        gn::Read read{image()};cn::NativeMount native{image()};cn::MountedPlayer sample{};
        if(native.valid(read) && cn::controlled_player(read,native,sample)
            && request().owner==mountedRequest.owner) {
            if(sample.vehicle!=UINT32_MAX) {
                observe_mounted({mountedRequest.owner,sample.player,sample.vehicle,sample.seat,
                    {sample.position[0],sample.position[1],sample.position[2]}});
            } else {observe_position(sample.position[0],sample.position[1],sample.position[2]);}
            observe_native_player(sample.player);
        }
    }
    observe_native_player(UINT32_MAX);
    const auto req=request();if(!req.frame.enabled) {return;}
    for(const auto& s:req.frame.interactions) {if(s.binding.valid()) {observe_native_object(reinterpret_cast<void*>(s.binding.source));}}
}
void observe_native_carry(void* raw,Holder holder,Controlled controlled) noexcept {
    const auto binding=identify(raw);const auto i=interaction_index(binding.asset);if(!binding.valid() || (i!=1 && i!=3)) {return;}
    gn::Read read{image()};const auto ptr=reinterpret_cast<std::uintptr_t>(raw);std::uint8_t state{};std::uint64_t context{};
    if(!read.value(ptr+0x470,state) || !read.value(ptr+0x478,context)) {return;}
    const bool held=state==3 || (state==1 && context!=UINT64_MAX);std::uint32_t player=UINT32_MAX;
    if(held && holder) {holder(raw,&player);}if(held && state==1 && player==UINT32_MAX && controlled) {controlled(&player);}
    gn::Read check{image()};if(current(check,binding,ptr)) {observe_carry(binding,player,held);}
}
}
