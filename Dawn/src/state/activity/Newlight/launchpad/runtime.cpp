#include "tower.h"
#include <Windows.h>
#include "runtime.h"
#include "../../gateway_intro.h"
#include "entry.h"
#include "lighting.h"
#include "../../runtime.h"
#include "../../../../core/logging/log.h"
#include "../../../../client/hooks/bootflow/gateway_native_read.h"
#include "../../../../client/hooks/bootflow/coo_enemy_readiness.h"
#include <cstdio>
#include <mutex>

namespace dawn::state::activity::newlight::launchpad {
namespace {
std::mutex mutex;
Controller controller;
Entry entry;
std::unique_ptr<coo::script::MissionDocument> document;
std::uint64_t selectedRun{},nextPublication{};
coo::StallDiagnostics stalled;
std::uint32_t lastActive{UINT32_MAX},lastComplete{UINT32_MAX};std::uint8_t lastSection{UINT8_MAX};
std::uint32_t lastPresentation{UINT32_MAX};
cinematics::Phase lastCinematic{cinematics::Phase::dormant};
ghost::Phase lastGhost{ghost::Phase::dormant};std::uint8_t lastGhostNode{UINT8_MAX};
bool selected() noexcept {return selectedRun && selectedRun==mission_run_generation();}
bool current() noexcept {return selected() && mission_seed_armed() && world_phase()==WorldPhase::arrived;}
void log(std::string_view text) noexcept {core::log::write(core::log::Channel::server,core::log::Level::info,text);}
bool load() noexcept {
    static std::once_flag once;
    std::call_once(once,[] {
        std::string error;
        try {
            HMODULE module{};std::array<wchar_t,32768> path{};
            const bool found=GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&load),&module)!=FALSE;
            const auto size=found?GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size())):0;
            if(!size || size>=path.size()) {error="cannot resolve DLL-relative script path";}
            else {document=coo::script::MissionDocument::read(std::filesystem::path(path.data()).parent_path()/L"Dawn"/L"scripts"/L"launchpad.lua",kEntryProfile,error);}
            if(document && !valid_document(document->views())) {document.reset();error="Launchpad native profile mismatch";}
        } catch(const std::exception& e) {error=e.what();}
        std::array<char,768> line{};
        if(document) {std::snprintf(line.data(),line.size(),"ev=coo_script mission=launchpad result=loaded format=lua fnv1a64=%016llX path=Dawn/scripts/launchpad.lua reload=next_process",static_cast<unsigned long long>(document->fingerprint()));}
        else {std::snprintf(line.data(),line.size(),"ev=coo_script mission=launchpad result=failed reason=\"%.*s\"",static_cast<int>((std::min)(error.size(),std::size_t{500})),error.data());}
        log(line.data());
    });return document!=nullptr;
}
void poll_readiness(std::uint64_t run,std::uint64_t now) noexcept {
    static std::uint64_t next{},lastRun{};static std::size_t cursor{};
    std::array<EnemyReceipt,128> pending{};std::size_t count{},begin{};
    {const std::lock_guard lock(mutex);if(!current() || run!=selectedRun) {return;}
        if(run!=lastRun) {next=0;cursor=0;lastRun=run;}if(now<next) {return;}next=now+500;
        controller.pending_enemies([&](const auto& r) {if(count<pending.size()) {pending[count++]=r;}});
        if(count) {begin=cursor%count;cursor=(begin+12)%count;}}
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    for(std::size_t i=0;i<(std::min)(count,std::size_t{12});++i) {
        client::hooks::bootflow::gateway_native::Read read{image};const auto& r=pending[(begin+i)%count];
        observe_readiness(r,client::hooks::bootflow::coo_native::enemy(read,image,r));
    }
}
}
bool prepare(std::uint64_t run,bool active) noexcept {
    const std::lock_guard lock(mutex);if(run!=mission_run_generation()) {return false;}
    if(!active && controller.owner().valid() && tower::state().origin==controller.owner()) {return false;}
    // This publisher also visits patrol/foreign activity rosters during the same
    // mission. They cannot end Launchpad's run or resurrect its cleared squads.
    // World teardown advances mission_run_generation; only that ends ownership.
    if(!active) {
        if(selectedRun && selectedRun!=run) {entry.reset();controller.reset();selectedRun=nextPublication=0;stalled.reset();}
        return false;
    }
    if(run && selectedRun==run && document && controller.owner().run==run) {return true;}
    if(!load() || !entry.select(document->views(),controller,run,GetTickCount64())) {return false;}
    selectedRun=run;lastActive=lastComplete=lastPresentation=UINT32_MAX;lastSection=UINT8_MAX;lastCinematic=cinematics::Phase::dormant;lastGhost=ghost::Phase::dormant;lastGhostNode=UINT8_MAX;stalled.reset();return true;
}
Frame snapshot(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    poll_readiness(run,now);const std::lock_guard lock(mutex);
    if(!selected() || run!=selectedRun) {return {};}
    const auto f=ready?entry.update(controller,run,now,true):controller.frame();nextPublication=now+100;
    const auto d=controller.diagnostics();
    if(f.presentation.published && f.presentation.revision!=lastPresentation) {
        lastPresentation=f.presentation.revision;
        std::array<char,256> line{};
        std::snprintf(line.data(),line.size(),"ev=launchpad stage=presentation run=%llu section=%u objective=%08X revision=%u active=%u marker=%08X/%u/%u",
            static_cast<unsigned long long>(run),f.section,f.presentation.event,f.presentation.revision,f.presentation.active?1U:0U,
            f.presentation.marker.asset.registry,f.presentation.marker.asset.type,f.presentation.marker.asset.slot);
        log(line.data());
    }
    if(f.ghost.phase!=lastGhost || f.ghost.node!=lastGhostNode) {
        lastGhost=f.ghost.phase;lastGhostNode=f.ghost.node;
        std::array<char,192> line{};std::snprintf(line.data(),line.size(),"ev=launchpad stage=ghost phase=%u generation=%u native_graph_node=%u lights_complete=%u",
            static_cast<unsigned>(lastGhost),f.ghost.publication.program.generation,lastGhostNode,f.ghost.ready()?1U:0U);log(line.data());
    }
    if(d.active!=lastActive || d.complete!=lastComplete || f.section!=lastSection || f.cinematic.phase!=lastCinematic) {
        lastActive=d.active;lastComplete=d.complete;lastSection=f.section;std::array<char,320> line{};
        lastCinematic=f.cinematic.phase;
        std::snprintf(line.data(),line.size(),"ev=coo_executor mission=launchpad run=%llu generation=%u section=%u active=%08X complete=%08X failure=%u finished=%u fault=%u cinematic=%u movie=%u fly_in=%u",
            static_cast<unsigned long long>(run),f.spawnGeneration,f.section,d.active,d.complete,static_cast<unsigned>(d.failure),f.finished?1U:0U,f.fault?1U:0U,static_cast<unsigned>(lastCinematic),f.cinematic.movie,f.cinematic.flyInComplete?1U:0U);log(line.data());
    }
    const auto& graph=controller.graph().definition;
    for(std::size_t i=0;i<graph.steps.size();++i) if(controller.step_state(i).phase==coo::StepPhase::active) {
        for(std::size_t j=0;j<graph.steps[i].commands.size();++j) {
            coo::StallReport report{};
            if(!stalled.observe({run,d.incarnation,static_cast<std::uint8_t>(i),static_cast<std::uint8_t>(j)},controller.missing(graph.steps[i].commands[j]),now,report)) {continue;}
            std::array<char,384> line{};std::snprintf(line.data(),line.size(),"ev=coo_stall mission=launchpad step=%.*s missing=%s registry=%08X slot=%u expected=%u actual=%u waiting_ms=%llu",
                static_cast<int>(graph.steps[i].name.size()),graph.steps[i].name.data(),coo::missing_name(report.detail.missing),report.detail.asset.registry,report.detail.asset.slot,report.detail.expected,report.detail.actual,static_cast<unsigned long long>(report.waitingMs));log(line.data());
        }
    }return f;
}
Request request() noexcept {const std::lock_guard lock(mutex);return selected()?Request{controller.owner(),controller.frame()}:Request{};}
coo::Generation native_owner() noexcept {const std::lock_guard lock(mutex);return selected()?controller.owner():coo::Generation{};}
std::uint64_t native_run() noexcept {const std::lock_guard lock(mutex);return current()?selectedRun:0;}
bool publication_due(std::uint64_t now) noexcept {const std::lock_guard lock(mutex);return selected() && controller.frame().enabled && now>=nextPublication;}
bool opening_mask(std::uint64_t now) noexcept {const std::lock_guard lock(mutex);return selected() && controller.frame().cinematic.masking_opening(now);}
void observe_fly_in_complete() noexcept {
    const std::lock_guard lock(mutex);
    if(current() && controller.fly_in_complete(controller.owner(),GetTickCount64())) {
        log("ev=launchpad stage=fly_in_complete result=accepted");
    }
}
void observe_arrival(coo::Generation owner,std::uint8_t route) noexcept {const std::lock_guard lock(mutex);if(selected()) {static_cast<void>(controller.arrival(owner,route,GetTickCount64()));}}
bool observe_retirement(coo::Generation owner) noexcept {
    const std::lock_guard lock(mutex);
    if(!current() || !controller.retired(owner)) {return false;}
    log("ev=launchpad stage=ending_retirement result=native_cleanup_complete");return true;
}
void observe_region(coo::Generation owner,std::int32_t region) noexcept {
    const std::lock_guard lock(mutex);
    if(current() && controller.region(owner,region)) {
        std::array<char,128> line{};std::snprintf(line.data(),line.size(),"ev=launchpad stage=area_entry region=%d source=native_region result=accepted",region);log(line.data());
    }
}
bool observe_cinematic(const cinematics::Incident& e) noexcept {
    if(gateway_intro::incident(mission_run_generation(),e,GetTickCount64())) {
        std::array<char,128> line{};std::snprintf(line.data(),line.size(),"ev=gateway_intro stage=cinematic target=%u phase=%u",e.target,static_cast<unsigned>(gateway_intro::state().phase));log(line.data());return true;
    }
    if(tower::incident(mission_run_generation(),e,GetTickCount64())) {return true;}
    const std::lock_guard lock(mutex);if(!selected()) {return false;}
    const bool accepted=controller.cinematic(controller.owner(),e,GetTickCount64());
    std::array<char,192> line{};std::snprintf(line.data(),line.size(),"ev=launchpad stage=cinematic target=%u registry=%08X accepted=%u phase=%u",e.target,e.registry,accepted?1U:0U,static_cast<unsigned>(controller.frame().cinematic.phase));log(line.data());return accepted;
}
void finish_handoff(coo::Generation owner) noexcept {
    const auto handoff=tower::state();const std::lock_guard lock(mutex);
    if(handoff.phase==tower::Phase::complete && handoff.origin==owner && controller.owner()==owner
        && controller.tower_arrived(owner)) {static_cast<void>(entry.update(controller,owner.run,GetTickCount64(),true));}
}
void observe_position(float x,float y,float z) noexcept {
    const std::lock_guard lock(mutex);if(!current()) {return;}
    controller.position(selectedRun,{x,y,z});
}
void observe_submission(std::uint64_t run,std::uint32_t definition,std::int64_t offset,std::uint32_t bank,std::uint8_t row,std::uint32_t generation) noexcept {
    const auto* b=find(kRoot,53,2);if(!b || definition!=b->asset.definition || offset!=b->offset || bank!=kBank || row>=std::size(kDialogue)) {return;}
    const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.submitted(run,bank,row,generation,GetTickCount64()));}
}
void observe_prepared(coo::Generation owner,coo::Asset a) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.prepared(owner,a));}}
void observe_object(const coo::ObjectReceipt& r) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.object(r));}}
void observe_lights(coo::Generation owner,const coo::ObjectReceipt& r) noexcept {
    const std::lock_guard lock(mutex);
    if(current() && controller.lights(owner,r)) {log("ev=launchpad stage=lights result=native_position_accepted");}
}
LightingSceneCommand lighting_scene_command() noexcept {
    const std::lock_guard lock(mutex);
    if(!current()) {return {};}
    const auto& frame=controller.frame();const auto& source=frame.native[asset_index(lighting::kSource)];
    if(!frame.enabled || !frame.lightRequested || frame.cinematic.ending()
        || !source.managed || !source.active || !source.prepared
        || !source.generation || source.generation>=0x7FFFFFFFU) {return {};}
    return {controller.owner(),source.generation};
}
void observe_device(std::uint32_t key,std::uint8_t type,std::uint16_t slot,const middleware::bap::activity_message::device_sense::Output& d) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.device(controller.owner(),asset(key,type,slot),d));}}
void observe_actor(std::uint32_t key,std::uint8_t type,std::uint16_t slot,const middleware::bap::activity_message::combatant_sense::Output& d) noexcept {const std::lock_guard lock(mutex);if(current()) {controller.actor(controller.owner(),asset(key,type,slot),d);}}
void observe_passenger(std::uint32_t key,std::uint8_t type,std::uint16_t slot,const middleware::bap::activity_message::native_sense::Passenger& d) noexcept {
    const std::lock_guard lock(mutex);
    if(current() && controller.passenger(controller.owner(),asset(key,type,slot),d)) {
        log(d.type==2?"ev=launchpad stage=walker result=native_attachment_accepted":"ev=launchpad stage=walker result=native_release_accepted");
    }
}
void observe_source(std::uint32_t key,std::uint8_t type,std::uint16_t slot,const middleware::bap::activity_message::squad_sense::Output& d) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.source(controller.owner(),asset(key,type,slot),d));}}
void observe_use(std::uint32_t key,std::uint8_t type,std::uint16_t slot,const middleware::bap::activity_message::object_sense::Output& d) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.use(controller.owner(),asset(key,type,slot),d));}}
bool observe_admission(const EnemyReceipt& r) noexcept {const std::lock_guard lock(mutex);return current() && controller.admitted(r);}
void observe_entrance(coo::Generation owner,const EnemyReceipt& r) noexcept {
    const std::lock_guard lock(mutex);if(current() && controller.entrance(owner,r)) {
        std::array<char,128> line{};std::snprintf(line.data(),line.size(),
            "ev=launchpad stage=ambush result=native_sequence_playing source=%u actor=%08X",r.source,r.actor);log(line.data());
    }
}
void observe_cache_looted(coo::Generation owner,const coo::ObjectReceipt& binding) noexcept {
    const std::lock_guard lock(mutex);
    if(current() && controller.cache_looted(owner,binding)) {
        std::array<char,160> line{};std::snprintf(line.data(),line.size(),"ev=launchpad stage=cache_used registry=%08X source=%u entity=%08X result=accepted",
            binding.source.registry,binding.source.slot,binding.entity);log(line.data());
    }
}
void observe_ghost(coo::Generation owner,const EnemyReceipt& actor,ghost::Sample sample) noexcept {
    const std::lock_guard lock(mutex);if(current()) {controller.ghost_sample(owner,actor,sample);}
}
bool observe_death(const EnemyReceipt& r) noexcept {const std::lock_guard lock(mutex);return current() && controller.died(r);}
void observe_readiness(const EnemyReceipt& r,coo::EnemyReadiness v) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.readiness(r,v));}}
GrantRequest grant_request() noexcept {const std::lock_guard lock(mutex);return current()?controller.grant_request():GrantRequest{};}
bool commit_grant(const GrantRequest& r,std::uint64_t item,bool complete,void* mutation,bool(*commit)(void*) noexcept) noexcept {
    const std::lock_guard lock(mutex);const auto expected=controller.grant_request();
    if(!current() || !r.owner.valid() || r.owner!=expected.owner || r.binding!=expected.binding
        || r.pickup!=expected.pickup || r.seed!=expected.seed || !item || item==UINT64_MAX || !commit || !commit(mutation)) {return false;}
    return !complete || controller.granted(r,item);
}
bool observe_granted(const GrantRequest& r,std::uint64_t item) noexcept {const std::lock_guard lock(mutex);return current() && controller.granted(r,item);}
}
