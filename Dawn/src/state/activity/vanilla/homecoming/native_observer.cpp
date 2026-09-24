#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include "runtime.h"
#include "door_native.h"
#include "ship_barrier.h"
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
bool update_ship_barrier(void* raw) noexcept {
    // The native DF7FF0 device tick is the only caller. The camera callback
    // crashed inside physics removal at 98D8C: its TLS+50 allocator was null.
    // Require both native allocator services even here; a thread id is not a
    // valid substitute for the engine's scoped execution context.
    namespace sb=ship_barrier;
    const auto base=image();gn::Read entryRead{base};
    const auto deviceAddress=reinterpret_cast<std::uintptr_t>(raw);std::uint32_t deviceEntity{};
    if(!sb::header(entryRead,deviceAddress,sb::kDevice,0x80803910U,0xA78)
        || !entryRead.value(deviceAddress+0x2C,deviceEntity) || deviceEntity==UINT32_MAX) return false;
    struct Component {gn::Weak weak{};std::uintptr_t address{};};
    struct Cached {
        gn::Weak entity{};door_native::Row row{};
        Component graph{},provider{},device{};bool reported{};
        ULONGLONG nextAttempt{};sb::Release lastFailure{sb::Release::waiting};
    };
    const auto wanted=sb::request();
    // Position can settle before its final network receipt or 3.5-second fade.
    // Keep the native callback alive while those independent receipts arrive.
    if(!wanted.owner.valid() || !wanted.generation) return false;
    std::uintptr_t table{};std::uint32_t stride{};
    std::array<unsigned char,16> stateCode{},variableCode{};
    if(!entryRead.value(base+door_native::kEntities,table) || table<0x10000
        || !entryRead.value(base+door_native::kEntityStride,stride) || stride!=door_native::kStride
        || !entryRead.value(base+sb::kSetState,stateCode) || stateCode!=sb::kStatePrefix
        || !entryRead.value(base+sb::kSetVariable,variableCode) || variableCode!=sb::kVariablePrefix) return false;
    const auto row_at=[&](std::uint32_t entity,door_native::Row& row) {
        std::array<std::byte,0x98> bytes{};
        if(!copy(reinterpret_cast<const void*>(table+(entity&0x1FFFU)*stride),bytes.data(),bytes.size())) return false;
        row={at<std::uint32_t>(bytes.data()+4),at<std::uint32_t>(bytes.data()+0xC),at<std::uint32_t>(bytes.data()+0x4C),
            at<std::uint32_t>(bytes.data()+0x88),at<std::uint32_t>(bytes.data()+0x8C),at<std::uint64_t>(bytes.data()+0x90)};
        return row.live(entity&0x1FFFU);
    };
    const auto owned=[&](std::uint32_t entity) {
        gn::Read read{base};std::uint32_t word{};
        return read.value(base+sb::kAuthority+4*((entity&0x1FFFU)>>5U),word) && (word&(1U<<(entity&31U)))!=0;
    };
    const auto component=[&](const door_native::Row& row,std::uint32_t kind,std::uint32_t tag,Component& c) {
        gn::Read read{base};std::uint32_t self{};
        return cn::component<gn::Read,1024>(read,row.bundle,row.entity,kind,c.address,tag)
            && read.value(c.address+0x24,self) && read.make_weak(self,c.weak);
    };
    const auto context=[&] {
        gn::Read read{base};DWORD index{};
        if(!read.value(base+sb::kAllocatorTlsIndex,index) || index==TLS_OUT_OF_INDEXES) return false;
        const auto error=GetLastError();const auto tls=reinterpret_cast<std::uintptr_t>(TlsGetValue(index));SetLastError(error);
        return sb::allocator_ready(read,tls,[](std::uintptr_t method) {
            MEMORY_BASIC_INFORMATION memory{};
            return method>=0x10000 && VirtualQuery(reinterpret_cast<void*>(method),&memory,sizeof memory)==sizeof memory
                && memory.State==MEM_COMMIT && !(memory.Protect&(PAGE_GUARD|PAGE_NOACCESS))
                && (memory.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
        });
    };
    const auto now=GetTickCount64();
    // Bind the actual device being updated. No full entity-table scan and no
    // native calls for the neighbouring door from this device's callback.
    door_native::Row row{};
    if(!row_at(deviceEntity,row) || row.entity!=deviceEntity || row.bundle==UINT32_MAX) return false;
    if(!sb::retain_tick(wanted,row,owned(row.entity))) return false;
    // Concurrent callbacks must not retire the second barrier while the first
    // owns the cache. Its next native tick can retry under the same guards.
    static std::atomic_flag updating=ATOMIC_FLAG_INIT;
    if(updating.test_and_set(std::memory_order_acquire)) return true;
    struct Done {std::atomic_flag& flag;~Done() {flag.clear(std::memory_order_release);}} done{updating};
    static sb::Request previous{};static std::array<Cached,2> cached{};
    if(wanted!=previous) {previous=wanted;cached={};}
    bool pending{};
    for(std::size_t i=0;i<cached.size();++i) {
        if(!sb::placement(row,i) || !owned(row.entity)) continue;
        pending=true;
        if(!wanted.revisions[i]) continue;
        auto& c=cached[i];
        if(c.entity.handle==UINT32_MAX) {
            Cached candidate{};candidate.row=row;gn::Read read{base};
            if(!read.make_weak(row.entity,candidate.entity)
                || !component(row,0x808084E9U,sb::kGraph,candidate.graph)
                || !component(row,0x80808870U,sb::kProvider,candidate.provider)
                || !component(row,0x80803910U,sb::kDevice,candidate.device)
                || candidate.device.address!=deviceAddress) continue;
            c=candidate;
        }
        if(c.device.address!=deviceAddress || c.entity.handle!=deviceEntity) {c={};continue;}
        const auto valid=[&] {
            gn::Read read{base};door_native::Row fresh{};
            if(!read.weak(c.entity) || !row_at(c.entity.handle,fresh) || fresh!=c.row || !sb::placement(fresh,i)
                || !owned(c.entity.handle) || sb::request()!=wanted) return false;
            for(const auto* part:{&c.graph,&c.provider,&c.device}) {
                std::uintptr_t resolved{};std::uint32_t owner{},self{};
                if(!read.weak(part->weak) || !read.resolve(part->weak.handle,resolved) || resolved!=part->address
                    || !read.value(resolved+0x24,self) || self!=part->weak.handle
                    || !read.value(resolved+0x2C,owner) || owner!=c.entity.handle) return false;
            }
            float position{},target{};std::int32_t revision{};
            return sb::header(read,c.device.address,sb::kDevice,0x80803910U,0xA78)
                && read.value(c.device.address+0x370,position) && position==1.F
                && read.value(c.device.address+0x37C,target) && target==1.F
                && read.value(c.device.address+0x960,revision) && revision==static_cast<std::int32_t>(wanted.revisions[i]);
        };
        if(!valid()) {c={};continue;}
        if(now<c.nextAttempt) continue;
        gn::Read read{base};std::int32_t state{};
        if(!sb::open_curve_finished(read,c.graph.address) || !sb::state_index(read,c.provider.address,state)) continue;
        // Do not coerce unrelated authored states 2/3. State 1 is the exact
        // collidable fallback observed after this door's opening curve ended.
        if(state!=0 && state!=1) continue;
        std::uint16_t authorityBefore{},authorityAfter{};std::int32_t observed{state};
        const auto authorize=[&] {
            gn::Read auth{base};std::uintptr_t provider{},allocation{};std::uint32_t bundle{};
            if(!sb::authority_code(auth,base) || !valid()
                || !auth.resolve(c.provider.weak.handle,provider,&allocation) || provider!=c.provider.address
                || !auth.value(allocation+0x10,bundle) || bundle!=c.row.bundle) return false;
            using Lookup=std::uint32_t*(__fastcall*)(std::uint32_t*,std::uint32_t) noexcept;
            const auto lookup=reinterpret_cast<Lookup>(base+sb::kNetworkRecord);
            std::uint32_t handle{UINT32_MAX};lookup(&handle,bundle);
            using Allowed=bool(__fastcall*)(std::uint32_t) noexcept;
            const auto allowed=reinterpret_cast<Allowed>(base+sb::kBundleAuthority);
            // Untracked bundles have no ownership record and are already local.
            if(handle==UINT32_MAX) return valid() && allowed(bundle);
            gn::Weak weak{};std::uintptr_t record{};
            if(!auth.make_weak(handle,weak) || !auth.resolve(handle,record)
                || !sb::network_record(auth,record,handle,bundle,c.entity.handle,authorityBefore)) return false;
            std::uint32_t fresh{UINT32_MAX};lookup(&fresh,bundle);
            std::uintptr_t resolved{};std::uint16_t flags{};
            if(fresh!=handle || !valid() || !auth.weak(weak) || !auth.resolve(handle,resolved) || resolved!=record
                || !sb::network_record(auth,record,handle,bundle,c.entity.handle,flags) || flags!=authorityBefore) return false;
            if(flags==8) {
                // Grant only this exact, acknowledged post-scan barrier. Keep
                // ownership for its remaining lifetime; native unload destroys
                // the record. No process-wide permission bypass or raw bit edit.
                using Grant=void(__fastcall*)(void*,bool) noexcept;
                reinterpret_cast<Grant>(base+sb::kSetNetworkAuthority)(reinterpret_cast<void*>(record),true);
            }
            lookup(&fresh,bundle);
            return fresh==handle && valid() && auth.weak(weak) && auth.resolve(handle,resolved) && resolved==record
                && sb::network_record(auth,record,handle,bundle,c.entity.handle,authorityAfter)
                && authorityAfter==9 && allowed(bundle);
        };
        const auto result=sb::release(context,valid,[&](std::int32_t& value) {
            gn::Read after{base};const bool ok=sb::state_index(after,c.provider.address,value);observed=value;return ok;
        },authorize,[&] {
            using Set=bool(__fastcall*)(void*,const std::uint32_t*,const std::uint32_t*) noexcept;
            return reinterpret_cast<Set>(base+sb::kSetState)(reinterpret_cast<void*>(c.provider.address),&sb::kStateName,&sb::kOpen);
        },[&] {
            using Variable=void(__fastcall*)(std::uint32_t,const std::uint32_t*,const float*) noexcept;
            alignas(16) const std::array<float,4> open{};
            reinterpret_cast<Variable>(base+sb::kSetVariable)(c.entity.handle,&sb::kPresentation,open.data());
        });
        if(result==sb::Release::stale) {c={};continue;}
        if(result!=sb::Release::opened) {
            c.nextAttempt=now+500;
            if(result!=c.lastFailure) {
                c.lastFailure=result;std::array<char,256> line{};
                std::snprintf(line.data(),line.size(),"ev=homecoming stage=ship_barrier_rejected run=%llu generation=%u slot=%u entity=%08X reason=%s state=%d authority_before=%u authority_after=%u",
                    static_cast<unsigned long long>(wanted.owner.run),wanted.generation,sb::kPlacements[i].slot,c.entity.handle,
                    result==sb::Release::contextUnavailable?"allocator_context":result==sb::Release::authorityRejected?"bundle_authority":result==sb::Release::stateRejected?"named_state":"not_ready",
                    observed,static_cast<unsigned>(authorityBefore),static_cast<unsigned>(authorityAfter));
                core::log::write(core::log::Channel::client,core::log::Level::warn,line.data());
            }
            continue;
        }
        pending=false;
        c.nextAttempt=0;c.lastFailure=sb::Release::waiting;
        if(!c.reported) {
            c.reported=true;std::array<char,256> line{};
            std::snprintf(line.data(),line.size(),"ev=homecoming stage=ship_barrier_handoff run=%llu generation=%u slot=%u entity=%08X state_before=%d state_after=%d authority_before=%u authority_after=%u presentation=0 context=device_tick acceptance=needs_gameplay",
                static_cast<unsigned long long>(wanted.owner.run),wanted.generation,sb::kPlacements[i].slot,c.entity.handle,state,observed,
                static_cast<unsigned>(authorityBefore),static_cast<unsigned>(authorityAfter));
            core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
        }
    }
    return pending;
}
}
