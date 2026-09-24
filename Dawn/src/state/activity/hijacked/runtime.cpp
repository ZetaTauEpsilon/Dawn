#include <Windows.h>
#include "runtime.h"
#include "controller.h"
#include "../coo/objective_delivery.h"
#include "../runtime.h"
#include "../../../core/logging/log.h"
#include <cstdio>
#include <mutex>
#include "../../../server/runtime/activity/mission_observation_queue.h"

namespace dawn::state::activity::hijacked {
namespace {
std::mutex mutex;
Controller controller;
std::array<server::runtime::activity::mission_device_pose::Inbox<PlateReceipt>,std::size(kPlates)> poseInbox{};
coo::ReadinessSchedule readinessSchedule;
coo::ObjectiveDelivery objectiveDelivery;
enum class ObservationKind {position,plateBinding,plateProgress,contest,scanBinding,scanPlayback};
struct Observation {
    ObservationKind kind{};coo::Generation owner{};Point point{};PlateReceipt plate{};ScanReceipt scan{};
    ScanPlayback playback{};std::uint32_t revision{};float progress{};bool flag{};
};
server::runtime::activity::MissionObservationQueue<Observation> observations;
struct ContestObservation {PlateReceipt plate{};std::array<EnemyPosition,256> positions{};std::size_t count{};bool complete{};};
std::array<ContestObservation,16> contests;
std::size_t contestCount{};
void consume(const Observation& e) noexcept {
    if(e.owner!=controller.owner()) {return;}
    switch(e.kind) {
    case ObservationKind::position:controller.position(e.owner.run,e.point);break;
    case ObservationKind::plateBinding:static_cast<void>(controller.bind_plate(e.plate));break;
    case ObservationKind::plateProgress:static_cast<void>(controller.plate(e.plate,e.revision,e.progress,e.flag));break;
    case ObservationKind::contest: {
        const auto& sample=contests[e.revision];
        static_cast<void>(controller.contested_positions(sample.plate,std::span(sample.positions).first(sample.count),sample.complete));break;
    }
    case ObservationKind::scanBinding:static_cast<void>(controller.bind_scan(e.scan));break;
    case ObservationKind::scanPlayback:static_cast<void>(controller.scan_playback(e.scan,e.playback,e.flag));break;
    }
}

std::unique_ptr<coo::script::MissionDocument> document;
std::uint64_t selectedRun{},nextPublication{};
coo::StallDiagnostics stalled;
std::uint32_t lastActive{UINT32_MAX},lastComplete{UINT32_MAX};
std::uint8_t lastSection{UINT8_MAX};
std::bitset<std::size(kSpawns)> loggedRetirements;
bool current() noexcept { return selectedRun && selectedRun==mission_run_generation() && mission_seed_armed() && world_phase()==WorldPhase::arrived; }
void log(std::string_view text) noexcept { core::log::write(core::log::Channel::server,core::log::Level::info,text); }
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
            else { document=coo::script::MissionDocument::read(std::filesystem::path(path.data()).parent_path()/L"Dawn"/L"scripts"/L"hijacked.lua",kProfile,error); }
            if(document && !valid_document(document->views())) { document.reset();error="Hijacked native profile mismatch"; }
        } catch(const std::exception& e) { error=e.what(); }
        std::array<char,768> line{};
        if(document) { std::snprintf(line.data(),line.size(),"ev=coo_script mission=hijacked result=loaded format=lua fnv1a64=%016llX path=Dawn/scripts/hijacked.lua reload=next_process",static_cast<unsigned long long>(document->fingerprint())); }
        else { std::snprintf(line.data(),line.size(),"ev=coo_script mission=hijacked result=failed reason=\"%.*s\"",static_cast<int>((std::min)(error.size(),std::size_t{500})),error.data()); }
        log(line.data());
    });
    return document!=nullptr;
}
}
bool prepare(std::uint64_t run,bool selected) noexcept {
    const std::lock_guard lock(mutex);if(run!=mission_run_generation()) {return false;}
    if(!selected) {controller.reset();objectiveDelivery={};readinessSchedule.reset();observations.reset();poseInbox={};contestCount=0;selectedRun=nextPublication=0;stalled.reset();return false;}
    // The process-owned document is immutable after load(), and selectedRun is
    // published only after Controller::select succeeds. Repeated roster updates
    // must not revalidate the entire script while the game thread waits here.
    // Deselection above and each new run still take the full selection path.
    if(run && selectedRun==run && document && controller.owner().run==run) {return true;}
    if(!load() || !controller.select(document->views(),run)) {return false;}
    if(selectedRun!=run) {objectiveDelivery={};readinessSchedule.reset();observations.reset();poseInbox={};contestCount=0;loggedRetirements.reset();lastActive=lastComplete=UINT32_MAX;lastSection=UINT8_MAX;stalled.reset();}
    selectedRun=run;return true;
}
namespace {
void log_receipt(const char* stage,std::uint64_t run,std::uint32_t value) noexcept {
    std::array<char,192> line{};std::snprintf(line.data(),line.size(),"ev=hijacked stage=%s run=%llu value=%u",stage,static_cast<unsigned long long>(run),value);log(line.data());
}

}
coo::ReadinessRequest<EnemyReceipt> readiness_request(std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);
    if(!current()) return {};
    return readinessSchedule.request<EnemyReceipt,256>(selectedRun,now,
        [&](auto visit) noexcept { controller.pending_enemies(visit); });
}
Frame snapshot(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    const std::lock_guard lock(mutex);if(!current() || run!=selectedRun) {return {};}
    if(!observations.drain(consume)) {log("ev=mission_observations result=overflow publication=stopped");return {};}
    for(auto& inbox:poseInbox) {inbox.drain([](const PlateReceipt& r,auto sample) {static_cast<void>(controller.plate_pose(r,sample));});}
    contestCount=0;
    auto f=controller.update(run,now,ready);nextPublication=now+100;
    f.presentation=objectiveDelivery.project(controller.owner(),f.presentation);const auto d=controller.diagnostics();
    for(std::size_t i=0;i<std::size(kSpawns);++i) {
        const auto& spawn=kSpawns[i];const auto a=find(spawn.registry,1,spawn.source)->asset;
        const auto& s=f.native[asset_index(a)];if(!s.retired || loggedRetirements[i]) {continue;}
        loggedRetirements.set(i);std::array<char,320> line{};
        std::snprintf(line.data(),line.size(),"ev=hijacked_retirement stage=request run=%llu owner_generation=%u registry=%08X source=%u requested_generation=%u requested_count=0 requested_policy=0 section=%u",
            static_cast<unsigned long long>(run),f.spawnGeneration,spawn.registry,spawn.source,s.generation,f.section);log(line.data());
    }
    if(d.active!=lastActive || d.complete!=lastComplete || f.section!=lastSection) {
        lastActive=d.active;lastComplete=d.complete;lastSection=f.section;std::array<char,320> line{};
        std::snprintf(line.data(),line.size(),"ev=coo_executor mission=hijacked run=%llu generation=%u section=%u phase=%u active=%08X complete=%08X failure=%u mission_finished=%u",
            static_cast<unsigned long long>(run),f.spawnGeneration,f.section,static_cast<unsigned>(d.phase),d.active,d.complete,static_cast<unsigned>(d.failure),f.finished?1U:0U);log(line.data());
    }
    if(const auto* g=controller.graph();g && d.phase==coo::Phase::running) for(const auto& b:g->commands) {
        if(controller.step_state(b.step).phase!=coo::StepPhase::active) {continue;}coo::StallReport report{};
        if(!stalled.observe({run,d.incarnation,b.step,b.command},controller.missing(g->definition.steps[b.step].commands[b.command]),now,report)) {continue;}
        std::array<char,384> line{};std::snprintf(line.data(),line.size(),"ev=coo_stall mission=hijacked command=%.*s missing=%s registry=%08X slot=%u expected=%u actual=%u waiting_ms=%llu",
            static_cast<int>(b.id.size()),b.id.data(),coo::missing_name(report.detail.missing),report.detail.asset.registry,report.detail.asset.slot,report.detail.expected,report.detail.actual,static_cast<unsigned long long>(report.waitingMs));log(line.data());
    }return f;
}
Request request() noexcept {const std::lock_guard lock(mutex);return current()?Request{controller.owner(),controller.frame()}:Request{};}
std::uint64_t native_run() noexcept {const std::lock_guard lock(mutex);return current()?selectedRun:0;}
void observe_objective_readiness(coo::Generation owner,std::uint32_t handle,std::uintptr_t component,std::uintptr_t content,bool ready,std::uint32_t logicalRevision,std::uint32_t visibleRow) noexcept {
    const std::lock_guard lock(mutex);
    if(!current() || owner!=controller.owner() || !controller.frame().enabled)return;
    objectiveDelivery.select(owner);
    if(objectiveDelivery.observe(owner,handle,component,content,ready)) {nextPublication=0;}
    if(logicalRevision==controller.frame().presentation.revision)objectiveDelivery.acknowledge(owner,logicalRevision,visibleRow);
}
bool publication_due(std::uint64_t now) noexcept {const std::lock_guard lock(mutex);return current() && controller.frame().enabled && now>=nextPublication;}
void observe_position(float x,float y,float z) noexcept {
    const std::lock_guard lock(mutex);if(!current()) {return;}
    const bool before=controller.cleanup_entered();controller.position(selectedRun,{x,y,z},GetTickCount64());
    if(!before && controller.cleanup_entered()) {
        std::array<char,256> line{};
        std::snprintf(line.data(),line.size(),
            "ev=hijacked_retirement stage=cleanup_point_entered run=%llu position=%.3f,%.3f,%.3f cue=user_location",
            static_cast<unsigned long long>(selectedRun),static_cast<double>(x),static_cast<double>(y),static_cast<double>(z));
        log(line.data());
    }
}
void observe_submission(std::uint64_t run,std::uint32_t definition,std::int64_t offset,std::uint32_t bank,std::uint8_t row,std::uint32_t generation) noexcept {
    if(!dialogue_identity(definition,offset,bank,row)) {return;}const std::lock_guard lock(mutex);
    if(current() && controller.submitted(run,bank,row,generation,GetTickCount64())) {log_receipt("dialogue_submitted",run,row);}
}
void observe_prepared(coo::Generation owner,coo::Asset a) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.prepared(owner,a));}}
void observe_object(const coo::ObjectReceipt& r) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.object(r));}}
bool observe_admission(const EnemyReceipt& r) noexcept {
    const std::lock_guard lock(mutex);if(!current()) {return false;}
    const bool fault=controller.frame().populationFault;const bool accepted=controller.admitted(r);
    if(!fault && controller.frame().populationFault) {
        std::array<char,256> line{};std::snprintf(line.data(),line.size(),
            "ev=hijacked stage=population_overflow run=%llu generation=%u registry=%08X source=%u actor=%08X owner=%08X",
            static_cast<unsigned long long>(r.run),r.generation,r.registry,r.source,r.actor,r.owner);log(line.data());
    }return accepted;
}
bool observe_death(const EnemyReceipt& r) noexcept {const std::lock_guard lock(mutex);const bool ok=current() && controller.died(r);if(ok) {log_receipt("enemy_died",r.run,r.source);}return ok;}
bool suspend_exterior(coo::Generation owner,std::uint16_t slot,std::span<const EnemyReceipt> survivors) noexcept {
    const std::lock_guard lock(mutex);return current() && controller.suspend_exterior(owner,slot,survivors);
}
bool resume_exterior(coo::Generation owner,std::uint16_t slot) noexcept {
    const std::lock_guard lock(mutex);return current() && controller.resume_exterior(owner,slot);
}
BossRequest boss_request() noexcept {const std::lock_guard lock(mutex);return current()?controller.boss_request():BossRequest{};}
bool observe_boss_position(const EnemyReceipt& r,std::uint8_t stage,std::uint32_t revision) noexcept {const std::lock_guard lock(mutex);return current() && controller.boss_position(r,stage,revision);}
bool observe_health(const EnemyReceipt& r,float fraction) noexcept {const std::lock_guard lock(mutex);return current() && controller.health_event(r,fraction,GetTickCount64());}
void observe_readiness(const EnemyReceipt& r,coo::EnemyReadiness v) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.readiness(r,v));}}
LivingEnemies living_enemies() noexcept {
    const std::lock_guard lock(mutex);LivingEnemies out{};if(!current() || !controller.frame().enabled) {return out;}
    out.owner=controller.owner();controller.living_enemies([&](const EnemyReceipt& r) {if(out.count<out.actors.size()) {out.actors[out.count++]=r;}});return out;
}
LivingEnemies retirement_enemies() noexcept {
    const std::lock_guard lock(mutex);LivingEnemies out{};if(!current() || !controller.frame().enabled) {return out;}
    out.owner=controller.owner();controller.retirement_enemies([&](const EnemyReceipt& r) {if(out.count<out.actors.size()) {out.actors[out.count++]=r;}});return out;
}
PlateRequest plate_request(std::size_t i) noexcept {const std::lock_guard lock(mutex);return current()?controller.plate_request(i):PlateRequest{};}
void observe_plate_pose(const PlateReceipt& r,server::runtime::activity::mission_device_pose::Sample sample) noexcept {
    const std::lock_guard lock(mutex);
    if(!current() || !r.valid() || controller.plate_request(r.index).plate!=r)return;
    poseInbox[r.index].submit(r,sample);
}
void observe_plate_binding(const PlateReceipt& r) noexcept {const std::lock_guard lock(mutex);if(current()) {Observation e{};e.kind=ObservationKind::plateBinding;e.owner=controller.owner();e.plate=r;observations.push(e);}}
void observe_plate(const PlateReceipt& r,std::uint32_t revision,float value,bool complete) noexcept {const std::lock_guard lock(mutex);if(current()) {Observation e{};e.kind=ObservationKind::plateProgress;e.owner=controller.owner();e.plate=r;e.revision=revision;e.progress=value;e.flag=complete;observations.push(e);}}
void observe_contested_positions(const PlateReceipt& r,std::span<const EnemyPosition> positions,bool complete) noexcept {
    const std::lock_guard lock(mutex);if(!current() || !r.valid() || positions.size()>256) {return;}
    if(contestCount==contests.size()) {observations.fail();return;}
    Observation queued{};queued.kind=ObservationKind::contest;queued.owner=controller.owner();queued.revision=static_cast<std::uint32_t>(contestCount);
    if(!observations.push(queued)) {return;}
    auto& event=contests[contestCount++];event.plate=r;event.count=positions.size();event.complete=complete;
    for(std::size_t i=0;i<positions.size();++i) {event.positions[i]=positions[i];}
}
ScanRequest scan_request(std::size_t i) noexcept {const std::lock_guard lock(mutex);return current()?controller.scan_request(i):ScanRequest{};}
void observe_scan_binding(const ScanReceipt& r) noexcept {const std::lock_guard lock(mutex);if(current()) {Observation e{};e.kind=ObservationKind::scanBinding;e.owner=controller.owner();e.scan=r;observations.push(e);}}
void observe_scan_playback(const ScanReceipt& r,ScanPlayback playback,bool participant) noexcept {const std::lock_guard lock(mutex);if(current()) {Observation e{};e.kind=ObservationKind::scanPlayback;e.owner=controller.owner();e.scan=r;e.playback=playback;e.flag=participant;observations.push(e);}}
}
