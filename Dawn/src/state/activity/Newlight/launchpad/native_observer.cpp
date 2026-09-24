#include <Windows.h>
#include "runtime.h"
#include "ghost_native.h"
#include "lighting_native.h"
#include "lighting_scene.h"
#include <intrin.h>
#include "entrance_native.h"
#include "cache_native.h"
#include "shutter_native.h"
#include "../../coo/native_device_authority.h"
#include "../../../../client/hooks/bootflow/gateway_native_read.h"
#include "../../../../client/hooks/bootflow/coo_native_player_mount.h"
#include "../../../../client/hooks/bootflow/internal.h"
#include "../../../../core/logging/log.h"
#include <mutex>
#include <cstdio>

namespace dawn::state::activity::newlight::launchpad {
namespace {
namespace gn=client::hooks::bootflow::gateway_native;
namespace cn=client::hooks::bootflow::coo_native;
std::mutex mutex;
coo::Generation observedOwner{};
std::array<std::uintptr_t,kObjects.size()> sources{};
coo::Generation shutterOwner{};
shutter::Physical physicalShutter{};
std::array<shutter::Physical,8> shutterCandidates{};
std::uintptr_t shutterGate{};
coo::Generation lightingOwner{};
lighting::scene::Binding lightingScene{};
template<class T> T at(const std::byte* bytes) noexcept {T v{};std::memcpy(&v,bytes,sizeof v);return v;}
std::uintptr_t image() noexcept {return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));}
void poll_shutter(const Request& req) noexcept {
    if(!req.owner.valid() || req.frame.cinematic.ending()) {return;}
    shutter::Physical anchor{},physical{};std::array<shutter::Physical,8> candidates{};std::uintptr_t source{};
    {const std::lock_guard lock(mutex);if(shutterOwner!=req.owner) {return;}
        anchor=physicalShutter;candidates=shutterCandidates;source=shutterGate;}
    if(!source || !anchor.address || native_owner()!=req.owner) {return;}
    gn::Read selection{image()};
    if(!shutter::next(selection,source,anchor,candidates,physical)) {return;}
    struct Exchange {
        std::uint64_t compare_exchange(std::uintptr_t address,std::uint64_t expected,std::uint64_t desired) noexcept {
            return static_cast<std::uint64_t>(InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(address),
                static_cast<LONG64>(desired),static_cast<LONG64>(expected)));
        }
        void enable(std::uintptr_t address,std::uint32_t mask) noexcept {
            InterlockedOr(reinterpret_cast<volatile LONG*>(address),static_cast<LONG>(mask));
        }
    } exchange;
    gn::Read read{image()};const auto result=shutter::bind(read,exchange,image(),source,physical);
    static coo::Generation lastOwner{};static shutter::Bound lastResult{shutter::Bound::invalid};
    static gn::Weak lastDevice{};
    if(lastOwner!=req.owner || lastResult!=result || lastDevice!=physical.device) {
        lastOwner=req.owner;lastResult=result;lastDevice=physical.device;
        std::array<char,192> line{};std::snprintf(line.data(),line.size(),
            "ev=launchpad stage=breach_shutter_binding result=%u entity=%08X device=%08X source=%p",
            static_cast<unsigned>(result),physical.entity.handle,physical.device.handle,reinterpret_cast<void*>(source));
        core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
    }
}
}
void observe_native_shutter(coo::Generation owner,std::uint32_t entity,bool placedAtDoor) noexcept {
    if(!owner.valid() || native_owner()!=owner) {return;}
    gn::Read read{image()};shutter::Physical physical{};
    if(!shutter::capture(read,entity,physical)) {return;}
    const std::lock_guard lock(mutex);
    if(shutterOwner!=owner) {shutterOwner=owner;shutterGate=0;physicalShutter={};shutterCandidates={};}
    if(placedAtDoor) {physicalShutter=physical;}
    for(auto& candidate:shutterCandidates) {
        gn::Read current{image()};std::uintptr_t row{};
        if(candidate.entity==physical.entity || !shutter::identity(current,candidate,row)) {
            candidate=physical;return;
        }
    }
}
void observe_native_shutter_gate(void* raw) noexcept {
    const auto source=reinterpret_cast<std::uintptr_t>(raw);gn::Read read{image()};
    if(!shutter::gate(read,source)) {return;}
    const auto owner=native_owner();if(!owner.valid()) {return;}
    const std::lock_guard lock(mutex);
    if(shutterOwner!=owner) {shutterOwner=owner;physicalShutter={};shutterCandidates={};}
    shutterGate=source;
}
void observe_native_lighting_scene(void* raw) noexcept {
    const auto owner=native_owner();if(!owner.valid()) {return;}
    lighting::scene::Binding known{};
    {const std::lock_guard lock(mutex);if(lightingOwner==owner) {known=lightingScene;}}
    gn::Read cached{image()};std::uintptr_t resolved{};
    if(known.address==reinterpret_cast<std::uintptr_t>(raw)
        && cached.weak(known.entity) && cached.weak(known.device)
        && cached.resolve(known.device.handle,resolved) && resolved==known.address) {return;}
    gn::Read read{image()};lighting::scene::Binding binding{};
    if(!lighting::scene::capture(read,reinterpret_cast<std::uintptr_t>(raw),binding)
        || native_owner()!=owner) {return;}
    {const std::lock_guard lock(mutex);lightingOwner=owner;lightingScene=binding;}
}
void apply_native_lighting_switch(void* raw,float requested,char snap) noexcept {
    const auto source=reinterpret_cast<std::uintptr_t>(raw),base=image();
    const auto command=lighting_scene_command();if(!command.owner.valid()) {return;}
    gn::Read receipt{base};
    if(!lighting::scene::accepted_switch(receipt,source,command.revision,requested,snap)) {return;}
    lighting::scene::Binding remembered{},before{};
    {const std::lock_guard lock(mutex);if(lightingOwner!=command.owner) {return;}remembered=lightingScene;}
    gn::Read refresh{base};
    if(!lighting::scene::refresh(refresh,remembered,before)) {return;}
    const auto address=before.address;
    gn::Read early{base};
    if(!lighting::scene::pending(early,address)) {return;}
    // We are returning from the accepted logical switch's native setter, inside
    // its event job. Still validate TLS; never manufacture a simulation context.
    if(!lighting::simulation_context(early,base,__readgsqword(0x58))) {return;}
    constexpr std::array<std::uint8_t,16> prefix{0x40,0x53,0x48,0x83,0xEC,0x60,0x0F,0xB7,
        0x41,0x2C,0x4C,0x8B,0xD2,0x25,0xFF,0x1F};
    std::array<std::uint8_t,16> actual{};
    if(!early.value(base+0xDF6510,actual) || actual!=prefix) {return;}
    gn::Read first{base};lighting::scene::Binding checked{};
    if(!lighting::scene::capture(first,address,checked) || checked!=before) {return;}
    gn::Read second{base};
    if(lighting_scene_command()!=command || !lighting::scene::capture(second,address,checked)
      || checked!=before || !lighting::scene::pending(second,address)) {return;}
    const auto record=lighting::position_record(command.revision);
    // Same narrow authority admission as the placed rifle shutters. The native
    // consumer owns position, dirty notification, sound, scene time and curves.
    auto* authority=reinterpret_cast<volatile LONG*>(base+0x26BE0E0+4U*((before.entity.handle&0x1FFFU)/32U));
    InterlockedOr(authority,static_cast<LONG>(1U<<(before.entity.handle&31U)));
    using Apply=void(__fastcall*)(void*,const void*);
    reinterpret_cast<Apply>(base+0xDF6510)(reinterpret_cast<void*>(address),record.data());
    gn::Read after{base};std::int32_t revision{},immediate{};float target{};
    if(after.value(address+0x960,revision) && revision==static_cast<std::int32_t>(command.revision)
        && after.value(address+0x964,immediate) && immediate==-1 && after.value(address+0x37C,target) && target==1.F) {
        core::log::write(core::log::Channel::client,core::log::Level::info,
            "ev=launchpad stage=authored_light_scene result=native_rising_edge device=80FA2F0A sound=m0_lighting_spectacle snap=0");
    }
}
void observe_native_object(void* raw) noexcept {
    const auto req=request();if(!req.frame.enabled) {return;}
    const auto source=reinterpret_cast<std::uintptr_t>(raw);gn::Read read{image()};std::array<std::byte,16> header{};
    if(!read.copy(source,header) || at<std::uint32_t>(header.data()+4)!=0x80809928U) {return;}
    const AssetBinding* binding{};
    for(const auto& a:kAssets) if(a.asset.type==4 && at<std::uint32_t>(header.data())==a.asset.definition
        && at<std::uint64_t>(header.data()+8)==a.offset) {binding=&a;break;}
    if(!binding) {return;}const auto& desired=req.frame.native[asset_index(binding->asset)];
    {
        const std::lock_guard lock(mutex);
        if(observedOwner!=req.owner) {observedOwner=req.owner;sources={};}
        sources[object_index(binding->asset)]=source;
    }
    // The native source may be constructed before its mission command. Keep its
    // validated address so a later request can observe preparation without a second callback.
    if(!desired.managed) {return;}
    if(!desired.prepared) {
        std::array<std::byte,0x44> bytes{};
        if(read.copy(source+0x180,bytes) && at<std::uint32_t>(bytes.data())==desired.generation && coo::native_device::inactive_state(bytes)
            && native_owner()==req.owner) {observe_prepared(req.owner,binding->asset);}return;
    }
    std::uint32_t generation{},committed{};std::uint8_t active{};gn::Weak entity{},again{};
    if(!desired.active || !read.value(source+0x180,generation) || generation!=desired.generation
        || !read.value(source+0x2F0,committed) || committed!=generation || !read.value(source+0x188,active) || active!=1
        || !read.value(source+0x440,entity) || !read.weak(entity)
        || !read.value(source+0x440,again) || again!=entity || !read.weak(again) || native_owner()!=req.owner) {return;}
    observe_object({{req.owner.run,generation},binding->asset,entity.handle,entity.serial});
}
void poll_native_objects() noexcept {
    const auto req=request();if(!req.frame.enabled) {return;}
    poll_shutter(req);
    // Only the loose first Vandal needs a direct native start. Named members
    // already own their entry programs; observe those rather than queuing twice.
    static coo::Generation entranceOwner{};static EnemyReceipt queuedEntrance{};
    if(entranceOwner!=req.owner) {entranceOwner=req.owner;queuedEntrance={};}
    for(std::size_t index=0;index<=std::size(kAmbushCues);++index) {
        if(!entrance::wanted(req,index)) {continue;}
        entrance::Binding before{},checked{},after{};gn::Read first{image()};
        constexpr std::array<std::uint8_t,16> prefix{0x48,0x83,0xEC,0x38,0x45,0x0F,0xB6,0xD8,0x4C,0x8B,0xC2,0x48,0x85,0xD2,0x0F,0x84};
        std::array<std::uint8_t,16> actual{};
        if(first.value(image()+0xC66590,actual) && actual==prefix && entrance::sample(first,image(),req,before,index)) {
            const auto current=request();gn::Read second{image()};
            const auto actor=entrance::actor(req,index);
            if(current.owner==req.owner && entrance::actor(current,index)==actor
                && entrance::sample(second,image(),current,checked,index) && checked==before) {
                using Start=bool(__fastcall*)(void*,const void*,std::uint8_t) noexcept;
                if(!index && !before.started && queuedEntrance!=actor) {
                    if(reinterpret_cast<Start>(image()+0xC66590)(reinterpret_cast<void*>(before.channel),reinterpret_cast<void*>(before.entry),1)) {
                        queuedEntrance=actor;
                    }
                }
                gn::Read final{image()};
                if(native_owner()==req.owner && entrance::sample(final,image(),current,after,index) && after.playing
                    && after.character==before.character && after.channel==before.channel && after.entity==before.entity) {observe_entrance(req.owner,actor);}
            }
        }
    }
    if(req.frame.ghost.phase!=ghost::Phase::dormant && req.frame.ghost.phase!=ghost::Phase::retired) {
        static coo::Generation owner{};static std::uint64_t next{};static unsigned previous{UINT_MAX};
        const auto now=GetTickCount64();
        if(owner!=req.owner || now>=next) {
            owner=req.owner;next=now+100;unsigned reason{};gn::Read read{image()};
            const auto sample=ghost::sample(read,image(),req.frame.ghostActor,reason);
            if(native_owner()==req.owner) {observe_ghost(req.owner,req.frame.ghostActor,sample);}
            if(reason!=previous) {
                previous=reason;std::array<char,128> line{};
                std::snprintf(line.data(),line.size(),"ev=launchpad stage=ghost_reader result=%u actor=%08X",reason,req.frame.ghostActor.actor);
                core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
            }
        }
    }
    if(!req.frame.finished && req.frame.cinematic.phase==cinematics::Phase::gameplay) {
        gn::Read read{image()};cn::NativeMount native{image()};cn::MountedPlayer sample{};
        const auto state=!native.valid(read)?1:!cn::controlled_player(read,native,sample)?2:3;
        if(state==3 && native_owner()==req.owner) {
            observe_position(sample.position[0],sample.position[1],sample.position[2]);
        }
        static coo::Generation owner{};static int previous{};static std::uint64_t next{};
        const auto now=GetTickCount64();
        if(owner!=req.owner || (state!=previous && now>=next)) {
            owner=req.owner;previous=state;next=now+2000;
            core::log::write(core::log::Channel::client,core::log::Level::info,state==1
                ?"ev=launchpad stage=position_reader result=signature_mismatch":state==2
                ?"ev=launchpad stage=position_reader result=controlled_entity_unavailable"
                :"ev=launchpad stage=position_reader result=observed");
        }
    }
    std::array<std::uintptr_t,kObjects.size()> pending{};
    {const std::lock_guard lock(mutex);if(observedOwner!=req.owner) {return;}pending=sources;}
    for(const auto source:pending) {if(source) {observe_native_object(reinterpret_cast<void*>(source));}}
    for(std::size_t pickup=1;pickup<std::size(kPickups);++pickup) {
        if(!req.frame.pickups[pickup].armed || req.frame.pickups[pickup].used) {continue;}
        const auto source=pending[object_index(kPickups[pickup])];cache::Binding before{},after{};gn::Read first{image()};
        if(!cache::sample(first,source,req,pickup,before)) {continue;}
        const auto current=request();gn::Read second{image()};
        if(current.owner==req.owner && cache::sample(second,source,current,pickup,after) && before==after) {
            observe_cache_looted(req.owner,before.object);
        }
    }
    if(req.frame.lightRequested && !req.frame.light) {
        const auto source=pending[object_index(lighting::kSource)];lighting::Binding before{},checked{};
        gn::Read first{image()};
        const auto revision=req.frame.native[asset_index(lighting::kSource)].generation;
        const unsigned status=!source?1:!lighting::sample(first,image(),source,req,before)?3
            :!lighting::accepted(revision,before.revision,before.current,before.target)?4:0;
        static coo::Generation lightOwner{};static unsigned previous{UINT_MAX};
        if(lightOwner!=req.owner || previous!=status) {
            lightOwner=req.owner;previous=status;const auto& wanted=req.frame.native[asset_index(lighting::kSource)];
            std::array<char,192> line{};std::snprintf(line.data(),line.size(),
                "ev=launchpad stage=light_reader result=%u generation=%u prepared=%u active=%u bound=%u native_revision=%d",
                status,wanted.generation,wanted.prepared?1U:0U,wanted.active?1U:0U,wanted.acknowledged?1U:0U,status==0 || status==4?before.revision:-2);
            core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
        }
        if(status) {return;}
        const auto current=request();gn::Read second{image()};
        if(current.owner!=req.owner || !lighting::sample(second,image(),source,current,checked) || checked!=before) {return;}
        observe_lights(req.owner,checked.object);
    }
}
}
