#include <Windows.h>
#include "runtime.h"
#include "bridge_scan.h"
#include "entrance_native.h"
#include "entry.h"
#include "../../runtime.h"
#include "../../../../core/logging/log.h"
#include "../../../../client/hooks/bootflow/gateway_native_read.h"
#include "../../../../client/hooks/bootflow/coo_enemy_readiness.h"
#include "../../../build_data/runtime.h"
#include "../../../build_data/spawn_sets/spawn_set_catalog.h"
#include <mutex>
#include <cstdio>
namespace dawn::state::activity::vanilla::one_au {
namespace {
std::mutex mutex;Controller controller;Entry entry;
std::unique_ptr<coo::script::MissionDocument> document;std::uint64_t selectedRun{},nextPublication{};
coo::StallDiagnostics stalls;std::uint32_t lastActive{UINT32_MAX},lastComplete{UINT32_MAX};std::uint8_t lastSection{UINT8_MAX};bool lastFault{};
bool current() noexcept {return selectedRun && selectedRun==mission_run_generation() && mission_seed_armed() && world_phase()==WorldPhase::arrived;}
void log(std::string_view text) noexcept {core::log::write(core::log::Channel::server,core::log::Level::info,text);}
bool load() noexcept {
    static std::once_flag once;
    std::call_once(once,[] {
        std::string error;
        try {
            HMODULE module{};std::array<wchar_t,32768> path{};
            const bool found=GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&load),&module)!=FALSE;
            const auto size=found?GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size())):0;
            if(!size || size>=path.size()) { error="cannot resolve DLL-relative script path"; }
            else { document=coo::script::MissionDocument::read(std::filesystem::path(path.data()).parent_path()/L"Dawn"/L"scripts"/L"one_au.lua",kEntryProfile,error); }
            if(document && !valid_document(document->views())) { document.reset();error="1AU native profile mismatch"; }
        } catch(const std::exception& e) { error=e.what(); }
        std::array<char,768> line{};
        if(document) { std::snprintf(line.data(),line.size(),"ev=coo_script mission=one_au result=loaded format=lua fnv1a64=%016llX path=Dawn/scripts/one_au.lua reload=next_process",static_cast<unsigned long long>(document->fingerprint())); }
        else { std::snprintf(line.data(),line.size(),"ev=coo_script mission=one_au result=failed reason=\"%.*s\"",static_cast<int>((std::min)(error.size(),std::size_t{500})),error.data()); }
        log(line.data());
    });
    return document!=nullptr;
}
void poll_readiness(std::uint64_t run,std::uint64_t now) noexcept {
    static std::uint64_t lastRun{},next{};static std::size_t cursor{};
    std::array<EnemyReceipt,256> pending{};std::size_t count{},begin{};
    {const std::lock_guard lock(mutex);if(!current() || run!=selectedRun) {return;}
        if(lastRun!=run) {lastRun=run;next=0;cursor=0;}if(now<next) {return;}next=now+500;
        controller.pending_enemies([&](const auto& r) {if(count<pending.size()) {pending[count++]=r;}});
        if(count) {begin=cursor%count;cursor=(begin+12)%count;}}
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    for(std::size_t i=0;i<(std::min)(count,std::size_t{12});++i) {
        client::hooks::bootflow::gateway_native::Read read{image};const auto& r=pending[(begin+i)%count];
        observe_readiness(r,client::hooks::bootflow::coo_native::enemy(read,image,r));
    }
}
}
bool prepare(std::uint64_t run,bool selected) noexcept {
    const std::lock_guard lock(mutex);if(run!=mission_run_generation()) {return false;}
    if(!selected) {entry.reset();controller.reset();selectedRun=nextPublication=0;stalls.reset();return false;}
    if(!load() || !entry.select(document->views(),controller,run,GetTickCount64())) {return false;}
    if(selectedRun!=run) {lastActive=lastComplete=UINT32_MAX;lastSection=UINT8_MAX;lastFault=false;stalls.reset();}
    selectedRun=run;return true;
}
bool opening_mask(std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);
    // Arrival can precede current()'s mission-seed gate during the C9 handoff.
    return selectedRun && selectedRun==mission_run_generation() && controller.frame().enabled
        && controller.frame().cinematic.masking_opening(now);
}
void observe_fly_in_complete() noexcept {
    const std::lock_guard lock(mutex);
    if(selectedRun && selectedRun==mission_run_generation()) {
        static_cast<void>(controller.fly_in_complete(controller.owner()));
    }
}
Frame snapshot(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    poll_readiness(run,now);poll_native_objects();const std::lock_guard lock(mutex);
    if(!current() || run!=selectedRun) {return {};}
    const auto laserIndex=asset_index(asset(kCore,23,48)),ringIndex=asset_index(asset(kCore,23,49));
    const auto priorLaser=controller.frame().native[laserIndex].generation,priorRing=controller.frame().native[ringIndex].generation;
    const auto f=entry.update(controller,run,now,ready);nextPublication=now+100;const auto d=controller.diagnostics();
    const auto& laser=f.native[laserIndex];const auto& ring=f.native[ringIndex];
    if(laser.generation!=priorLaser || ring.generation!=priorRing) {
        std::array<char,320> line{};
        std::snprintf(line.data(),line.size(),"ev=one_au stage=beam_cycle laser=%.3f ring=%.3f laser_generation=%u ring_generation=%u laser_primed=%u east_open=%u west_open=%u overloaded=%u destroyed=%u",
            laser.position,ring.position,laser.generation,ring.generation,f.laserPrimed?1U:0U,
            f.native[asset_index(asset(kCore,23,21))].position==1.F?1U:0U,
            f.native[asset_index(asset(kCore,23,29))].position==1.F?1U:0U,f.interactions[4].inserted?1U:0U,
            static_cast<unsigned>(f.interactions[5].destroyed+f.interactions[6].destroyed+f.interactions[7].destroyed));log(line.data());
    }
    if(d.active!=lastActive || d.complete!=lastComplete || f.section!=lastSection || f.fault!=lastFault) {
        lastActive=d.active;lastComplete=d.complete;lastSection=f.section;lastFault=f.fault;std::array<char,320> line{};
        std::snprintf(line.data(),line.size(),"ev=vanilla_executor mission=one_au run=%llu generation=%u section=%u active=%08X complete=%08X failure=%u finished=%u fault=%u enabled=%u",
            static_cast<unsigned long long>(run),f.spawnGeneration,f.section,d.active,d.complete,static_cast<unsigned>(d.failure),f.finished?1U:0U,f.fault?1U:0U,f.enabled?1U:0U);log(line.data());
    }
    const auto& g=controller.graph().definition;
    for(std::size_t i=0;i<g.steps.size();++i) {
        if(controller.step_state(i).phase!=coo::StepPhase::active) {continue;}
        for(std::size_t j=0;j<g.steps[i].commands.size();++j) {
            coo::StallReport report{};
            if(!stalls.observe({run,d.incarnation,static_cast<std::uint8_t>(i),static_cast<std::uint8_t>(j)},controller.missing(g.steps[i].commands[j]),now,report)) {continue;}
            std::array<char,384> line{};std::snprintf(line.data(),line.size(),"ev=vanilla_stall mission=one_au step=%.*s missing=%s registry=%08X slot=%u expected=%u actual=%u waiting_ms=%llu",
                static_cast<int>(g.steps[i].name.size()),g.steps[i].name.data(),coo::missing_name(report.detail.missing),report.detail.asset.registry,report.detail.asset.slot,report.detail.expected,report.detail.actual,static_cast<unsigned long long>(report.waitingMs));log(line.data());
        }
    }
    return f;
}
Request request() noexcept {const std::lock_guard lock(mutex);return current()?Request{controller.owner(),controller.frame()}:Request{};}
BridgeScanRequest bridge_scan_request() noexcept {
    const std::lock_guard lock(mutex);return current()?bridge_scan_request(controller.owner(),controller.frame()):BridgeScanRequest{};
}
EntranceRequest entrance_request() noexcept {
    const std::lock_guard lock(mutex);return current()?entrance_request(controller.owner(),controller.frame()):EntranceRequest{};
}
std::uint64_t native_run() noexcept {const std::lock_guard lock(mutex);return current()?selectedRun:0;}
void observe_arrival(coo::Generation owner,std::uint8_t route) noexcept {
    const std::lock_guard lock(mutex);
    if(selectedRun && selectedRun==mission_run_generation() && controller.arrival(owner,route,GetTickCount64())) {
        std::array<char,160> line{};std::snprintf(line.data(),line.size(),"ev=one_au stage=cinematic_arrival run=%llu route=%u",static_cast<unsigned long long>(selectedRun),route);log(line.data());
    }
}
bool observe_cinematic(const cinematics::Incident& e) noexcept {
    const std::lock_guard lock(mutex);
    if(!current()) {return false;}
    const auto accepted=controller.cinematic(controller.owner(),e,GetTickCount64());
    std::array<char,220> line{};std::snprintf(line.data(),line.size(),"ev=one_au stage=cinematic_incident run=%llu target=%u registry=%08X runtime=%llX accepted=%u phase=%u",
        static_cast<unsigned long long>(selectedRun),e.target,e.registry,static_cast<unsigned long long>(e.runtime),accepted?1U:0U,static_cast<unsigned>(controller.frame().cinematic.phase));log(line.data());return accepted;
}
void observe_actor(std::uint32_t key,std::uint8_t type,std::uint16_t slot,const transport::Delta& d) noexcept {
    const std::lock_guard lock(mutex);
    if(current() && controller.actor(controller.owner(),key,type,slot,d,GetTickCount64())) {
        std::array<char,180> line{};std::snprintf(line.data(),line.size(),"ev=one_au stage=transport slot=%u program=%u state=%u delivery=%u/%d",slot,d.programRevision,d.programState,d.deliveryRevision,d.deliveryState);log(line.data());
    }
}
void observe_ghost(std::uint32_t key,std::uint8_t type,std::uint16_t slot,const middleware::bap::activity_message::ghost_sense::Output& d) noexcept {
    const std::lock_guard lock(mutex);
    if(current() && controller.ghost(controller.owner(),asset(key,type,slot),d)) {
        std::array<char,180> line{};std::snprintf(line.data(),line.size(),"ev=one_au stage=ghost generation=%d active=%u progress=%.3f completed=%u",d.generation,d.active?1U:0U,d.progress,controller.frame().interactions[0].completed?1U:0U);log(line.data());
    }
}
bool publication_due(std::uint64_t now) noexcept {const std::lock_guard lock(mutex);return current() && controller.frame().enabled && !controller.frame().finished && now>=nextPublication;}
void observe_life(coo::Generation owner,std::uint32_t entity,bool alive) noexcept {
    const std::lock_guard lock(mutex);
    if(current()) {controller.life(owner,entity,alive,GetTickCount64());}
}
recovery::Spawn project_spawn(recovery::Spawn client) noexcept {
    const std::lock_guard lock(mutex);if(!current()) {return client;}
    const auto point=controller.frame().recovery.point;
    if(controller.frame().recovery.phase==recovery::Phase::requested) {
        build_data::scenarios::Definition layout{};build_data::spawn_sets::NameHash hash{};
        if(!build_data::find_scenario_layout(kPackage,layout)
            || !build_data::spawn_sets::find_hash({layout.spawnStem.data(),layout.spawnStemLength},point.spawn,hash)
            || !hash.pointCount) {log("ev=one_au stage=checkpoint_handshake result=invalid_spawn");return client;}
    }
    const auto phase=controller.frame().recovery.phase;const auto result=controller.spawn(client,GetTickCount64());
    if(phase!=controller.frame().recovery.phase) {log("ev=one_au stage=checkpoint_handshake result=advanced");}
    return result;
}
void observe_position(float x,float y,float z) noexcept {const std::lock_guard lock(mutex);if(current()) {controller.position(selectedRun,{x,y,z});}}
void observe_mounted(const MountedPosition& sample) noexcept {
    const std::lock_guard lock(mutex);if(!current() || !controller.mounted(sample)) {return;}
    static coo::Generation previousOwner{};static std::uint32_t previousVehicle{UINT32_MAX};
    if(previousOwner==sample.owner && previousVehicle==sample.vehicle) {return;}
    previousOwner=sample.owner;previousVehicle=sample.vehicle;
    std::array<char,256> line{};
    std::snprintf(line.data(),line.size(),"ev=one_au stage=mounted_position run=%llu generation=%u player=%08X vehicle=%08X seat=%08X position=%.3f,%.3f,%.3f",
        static_cast<unsigned long long>(sample.owner.run),sample.owner.value,sample.player,sample.vehicle,sample.seat,
        sample.position.x,sample.position.y,sample.position.z);log(line.data());
}
void observe_submission(std::uint64_t run,std::uint32_t definition,std::int64_t offset,std::uint32_t bank,std::uint8_t row,std::uint32_t generation) noexcept {
    const auto* b=find(kRoot,53,2);if(!b || definition!=b->asset.definition || offset!=b->offset || bank!=kBank || row>=std::size(kDialogue)) {return;}
    const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.submitted(run,bank,row,generation,GetTickCount64()));}
}
void observe_prepared(coo::Generation owner,coo::Asset a) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.prepared(owner,a));}}
void observe_object(const coo::ObjectReceipt& r) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.object(r));}}
void observe_interceptor_faction(const coo::ObjectReceipt& r,std::int32_t before,std::int32_t after) noexcept {
    const std::lock_guard lock(mutex);if(!current() || r.owner.run!=controller.owner().run) {return;}
    std::array<char,200> line{};std::snprintf(line.data(),line.size(),"ev=one_au stage=interceptor_faction entity=%08X generation=%u before=%d after=%d accepted=%u",r.entity,r.owner.value,before,after,after==-2?1U:0U);log(line.data());
}
void observe_device(std::uint32_t key,std::uint8_t type,std::uint16_t slot,const middleware::bap::activity_message::device_sense::Output& d) noexcept {
    const std::lock_guard lock(mutex);
    if(current() && controller.device(controller.owner(),asset(key,type,slot),d)) {
        std::array<char,200> line{};std::snprintf(line.data(),line.size(),"ev=one_au stage=device_sync registry=%08X slot=%u position=%.3f versions=%d/%d/%d",key,slot,d.values[0],d.revisions[0],d.revisions[1],d.revisions[2]);log(line.data());
    }
}
void observe_source(std::uint32_t key,std::uint8_t type,std::uint16_t slot,const middleware::bap::activity_message::source_sense::Output& d) noexcept {
    const std::lock_guard lock(mutex);
    if(current() && controller.source(controller.owner(),asset(key,type,slot),d)) {
        const auto& state=controller.frame().tactics[spawn_index(asset(key,type,slot))];
        std::array<char,180> line{};std::snprintf(line.data(),line.size(),"ev=one_au stage=task_assignment registry=%08X slot=%u revision=%u group=%d",key,slot,state.revision,state.group);log(line.data());
    }
}
void observe_lost(const ComponentReceipt& r) noexcept {
    const std::lock_guard lock(mutex);
    if(current() && controller.lost(r,GetTickCount64())) {log("ev=one_au stage=cell_recovery result=rearmed");}
}
void observe_binding(const ComponentReceipt& r) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.bind(r));}}
void observe_carry(const ComponentReceipt& r,std::uint32_t holder,bool held) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.carry(r,holder,held));}}
void observe_use(std::uint32_t key,std::uint8_t type,std::uint16_t slot,const middleware::bap::activity_message::object_sense::Output& d) noexcept {
    const std::lock_guard lock(mutex);
    if(current() && controller.use(controller.owner(),asset(key,type,slot),d)) {
        std::array<char,180> line{};std::snprintf(line.data(),line.size(),"ev=one_au stage=accepted_use registry=%08X slot=%u generation=%d revision=%d",key,slot,d.generation,d.useRevision);log(line.data());
    }
}
void observe_destruction(const ComponentReceipt& r,bool dead) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.destruction(r,dead));}}
bool observe_admission(const EnemyReceipt& r) noexcept {
    const std::lock_guard lock(mutex);if(!current()) {return false;}
    const auto accepted=controller.admitted(r);const auto i=asset_index(asset(r.registry,1,r.source));
    if(i<std::size(kAssets) && !accepted) {
        const auto& source=controller.frame().native[i];
        if(r.run==selectedRun && source.sourceOwner==r.owner && source.generation==r.generation+1) {
            std::array<char,224> line{};std::snprintf(line.data(),line.size(),"ev=one_au stage=source_restream run=%llu registry=%08X slot=%u source=%08X generation=%u previous=%u",
                static_cast<unsigned long long>(r.run),r.registry,r.source,r.owner,source.generation,r.generation);log(line.data());
        }
    }
    return accepted;
}
bool observe_death(const EnemyReceipt& r) noexcept {const std::lock_guard lock(mutex);return current() && controller.died(r);}
void observe_readiness(const EnemyReceipt& r,coo::EnemyReadiness v) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.readiness(r,v));}}
}
