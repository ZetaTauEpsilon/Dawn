#include <Windows.h>
#include "runtime.h"
#include "controller.h"
#include "ending_cadence.h"
#include "../runtime.h"
#include "../../../core/logging/log.h"
#include <cstdio>
#include <mutex>
namespace dawn::state::activity::gateway {
namespace {
std::mutex mutex;
Controller controller;
coo::ReadinessSchedule readinessSchedule;
EndingCadence endingCadence;
coo::StallDiagnostics stalled,background;
std::uint64_t selectedRun{};
std::unique_ptr<coo::script::MissionDocument> document;
std::bitset<std::size(kVolumes)> loggedVolumes;
std::uint32_t loggedActive{UINT32_MAX},loggedTimeouts{};
std::uint8_t loggedSection{UINT8_MAX};
coo::Phase loggedPhase{coo::Phase::idle};
void log(std::string_view message) noexcept {
    core::log::write(core::log::Channel::server,core::log::Level::info,message);
}
bool load() noexcept {
    static std::once_flag once;
    std::call_once(once,[] {
        std::string error;
        try {
            HMODULE module{};std::array<wchar_t,32768> path{};
            const bool found=GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&load),&module)!=FALSE;
            const auto size=found?GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size())):0;
            if(size==0 || size>=path.size()) { error="cannot resolve DLL-relative script path"; }
            else { document=coo::script::MissionDocument::read(std::filesystem::path(path.data()).parent_path()/L"Dawn"/L"scripts"/L"gateway.lua",kProfile,error); }
            if(document && !valid_document(document->views())) { document.reset();error="Gateway native binding validation failed"; }
        } catch(const std::exception& exception) { error=exception.what(); }
        std::array<char,768> line{};
        if(document) {
            std::snprintf(line.data(),line.size(),"ev=coo_script mission=gateway result=loaded format=lua fnv1a64=%016llX path=Dawn/scripts/gateway.lua scope=ending reload=next_process",
                static_cast<unsigned long long>(document->fingerprint()));
        } else { std::snprintf(line.data(),line.size(),"ev=coo_script mission=gateway result=failed reason=\"%.*s\"",static_cast<int>((std::min)(error.size(),std::size_t{500})),error.data()); }
        log(line.data());
    });
    return document!=nullptr;
}
}
bool prepare(std::uint64_t run,bool selected) noexcept {
    const std::lock_guard lock(mutex);
    if(run!=mission_run_generation()) { return false; }
    if(!selected) {
        if(selectedRun!=0) {
            std::array<char,192> line{};
            std::snprintf(line.data(),line.size(),"ev=gateway stage=reset run=%llu reason=destination_changed",
                static_cast<unsigned long long>(selectedRun));log(line.data());
        }
        controller.reset();endingCadence.reset();stalled.reset();background.reset();readinessSchedule.reset();selectedRun=0;loggedVolumes.reset();loggedActive=UINT32_MAX;loggedSection=UINT8_MAX;loggedTimeouts=0;loggedPhase=coo::Phase::idle;return false;
    }
    if(!load()) { return false; }
    if(run!=selectedRun) { stalled.reset();background.reset();readinessSchedule.reset();endingCadence.reset();loggedVolumes.reset();loggedActive=UINT32_MAX;loggedSection=UINT8_MAX;loggedTimeouts=0;loggedPhase=coo::Phase::idle; }
    if(!controller.select(document->views(),run)) { return false; }
    selectedRun=run;return true;
}
// Copy pending identities; the client observer samples native state after this lock is released.
coo::ReadinessRequest<EnemyReceipt> readiness_request(std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);
    if(!selectedRun || selectedRun!=mission_run_generation() || !mission_seed_armed()
        || world_phase()!=WorldPhase::arrived) { return {}; }
    return readinessSchedule.request<EnemyReceipt, 252>(selectedRun,now,
        [&](auto visit) noexcept { controller.pending_enemies(visit); });
}

ObjectRequest object_request() noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return {}; }
    const std::lock_guard lock(mutex);if(!selectedRun || selectedRun!=mission_run_generation()) { return {}; }
    const auto& frame=controller.frame();return {{selectedRun,frame.spawnGeneration},frame.objects,frame.enabled};
}
void observe_object(std::size_t index,const coo::ObjectReceipt& receipt,bool applied,float position,std::int16_t revision) noexcept {
    const std::lock_guard lock(mutex);
    if(receipt.owner.run!=selectedRun || receipt.owner.run!=mission_run_generation() || !controller.object(index,receipt,applied,position,revision)) { return; }
    std::array<char,256> line{};std::snprintf(line.data(),line.size(),"ev=coo_object mission=gateway run=%llu object=%u entity=%08X controller=%08X phase=%u revision=%d",
        static_cast<unsigned long long>(receipt.owner.run),static_cast<unsigned>(index),receipt.entity,receipt.controller,
        static_cast<unsigned>(controller.frame().objects[index].phase),static_cast<int>(revision));log(line.data());
}
void observe_readiness(const EnemyReceipt& receipt,coo::EnemyReadiness value) noexcept {
    const std::lock_guard lock(mutex);if(receipt.run!=selectedRun || receipt.run!=mission_run_generation()) { return; }
    static_cast<void>(controller.readiness(receipt,value));
}
void observe_capacity(std::uint64_t run,coo::PopulationCapacity value) noexcept {
    const std::lock_guard lock(mutex);
    if(run && run==selectedRun && run==mission_run_generation() && mission_seed_armed()
        && world_phase()==WorldPhase::arrived) controller.capacity(value);
}
Frame snapshot(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    const std::lock_guard lock(mutex);
    if(run!=selectedRun || run!=mission_run_generation()) { return {}; }
    const auto frame=controller.update(run,now,ready && mission_seed_armed() && world_phase()==WorldPhase::arrived);
    const bool sectionChanged=frame.section!=loggedSection;
    if(sectionChanged) { loggedVolumes.reset();loggedSection=frame.section; }
    for(std::size_t i=0;i<std::size(kVolumes);++i) {
        if(controller.seen()[i] && !loggedVolumes[i]) {
            loggedVolumes.set(i);std::array<char,320> line{};const auto& v=kVolumes[i];
            std::snprintf(line.data(),line.size(),"ev=gateway stage=volume_entered run=%llu registry=%08X type=60 slot=%u name=%.*s evidence=local_player_position",
                static_cast<unsigned long long>(run),v.registry,v.slot,static_cast<int>(v.name.size()),v.name.data());log(line.data());
        }
    }
    const auto d=controller.diagnostics();
    if(sectionChanged || d.active!=loggedActive || d.phase!=loggedPhase || controller.timeouts()!=loggedTimeouts) {
        loggedActive=d.active;loggedPhase=d.phase;loggedTimeouts=controller.timeouts();std::array<char,320> line{};
        std::snprintf(line.data(),line.size(),"ev=coo_executor mission=gateway scope=ending run=%llu phase=%u active=%08X complete=%08X failure=%u dialogue_timeouts=%u section=%u mission_finished=%u",
            static_cast<unsigned long long>(run),static_cast<unsigned>(d.phase),d.active,d.complete,static_cast<unsigned>(d.failure),loggedTimeouts,frame.section,frame.finished?1U:0U);log(line.data());
    }
    if(const auto* graph=controller.graph();graph && d.phase==coo::Phase::running) {
        unsigned lines{};
        for(const auto& binding:graph->commands) {
            if(controller.step_state(binding.step).phase!=coo::StepPhase::active || lines>=8) { continue; }
            const auto& spec=graph->definition.steps[binding.step].commands[binding.command];coo::StallReport report{};
            if(!stalled.observe({run,d.incarnation,binding.step,binding.command},controller.missing(spec),now,report)) { continue; }
            ++lines;std::array<char,512> line{};
            std::snprintf(line.data(),line.size(),"ev=coo_stall mission=gateway step=%.*s command=%.*s missing=%s registry=%08X type=%u slot=%u expected=%u actual=%u detail=%u waiting_ms=%llu",
                static_cast<int>(graph->definition.steps[binding.step].name.size()),graph->definition.steps[binding.step].name.data(),
                static_cast<int>(binding.id.size()),binding.id.data(),coo::missing_name(report.detail.missing),report.detail.asset.registry,
                report.detail.asset.type,report.detail.asset.slot,report.detail.expected,report.detail.actual,report.detail.detail,
                static_cast<unsigned long long>(report.waitingMs));log(line.data());
        }
    }
    if(frame.enabled && !frame.finished) {
        unsigned lines{};
        controller.background_diagnostics([&](std::size_t i,coo::StallDetail detail) noexcept {
            coo::StallReport report{};
            if(lines>=8 || !background.observe({run,d.incarnation,static_cast<std::uint8_t>(i/8),static_cast<std::uint8_t>(i%8)},detail,now,report)) { return; }
            ++lines;std::array<char,400> line{};const auto capacity=controller.capacity();
            std::snprintf(line.data(),line.size(),"ev=coo_readiness_stall mission=gateway missing=%s registry=%08X type=%u slot=%u expected=%u actual=%u actor=%08X capacity_known=%u allocated=%u maximum=%u waiting_ms=%llu",
                coo::missing_name(detail.missing),detail.asset.registry,detail.asset.type,detail.asset.slot,detail.expected,detail.actual,detail.detail,
                capacity.known?1U:0U,capacity.used,capacity.maximum,static_cast<unsigned long long>(report.waitingMs));log(line.data());
        });
    }
    endingCadence.snapshot(run,now,frame);return frame;
}
bool publication_due(std::uint64_t now) noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return false; }
    const std::lock_guard lock(mutex);
    return selectedRun==mission_run_generation() && endingCadence.due(selectedRun,now);
}
std::uint64_t native_run() noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return 0; }
    const std::lock_guard lock(mutex);return selectedRun==mission_run_generation()?selectedRun:0;
}
cannon::Request cannon_request() noexcept {
    const std::lock_guard lock(mutex);
    // Use the same completed end-clear join as type23/2's visual effect. Remain
    // blocked while this selected run is still preparing or awaiting enemies.
    return cannon::request(selectedRun,mission_run_generation(),controller.frame().finalCannon);
}
EnemyReceipt marcher(std::uint32_t actor) noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) return {};
    const std::lock_guard lock(mutex);
    return selectedRun && selectedRun==mission_run_generation()?controller.marcher(actor):EnemyReceipt{};
}
EndingRequest ending_request() noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return {}; }
    const std::lock_guard lock(mutex);
    return selectedRun==mission_run_generation()?controller.ending_request():EndingRequest{};
}
void observe_module(const ModuleReceipt& receipt,bool dead) noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return; }
    const std::lock_guard lock(mutex);
    if(receipt.run!=selectedRun || receipt.run!=mission_run_generation() || !controller.module(receipt,dead)) { return; }
    std::array<char,256> line{};
    std::snprintf(line.data(),line.size(),"ev=gateway stage=module_%s run=%llu generation=%u source=%016llX entity=%08X serial=%08X health=%08X evidence=native_health",
        dead?"destroyed":"bound",static_cast<unsigned long long>(receipt.run),receipt.generation,static_cast<unsigned long long>(receipt.source),receipt.entity,receipt.serial,receipt.health);log(line.data());
}
void observe_scene(const SceneReceipt& receipt,bool completed) noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return; }
    const std::lock_guard lock(mutex);
    if(receipt.run!=selectedRun || receipt.run!=mission_run_generation() || !controller.scene(receipt,completed)) { return; }
    std::array<char,256> line{};
    std::snprintf(line.data(),line.size(),"ev=gateway stage=vance_scene_%s run=%llu generation=%u group=%08X sensor=%08X selector=%08X evidence=native_B438B0",
        completed?"completed":"started",static_cast<unsigned long long>(receipt.run),receipt.generation,receipt.group,receipt.sensor,receipt.selector);log(line.data());
}
void observe_vance(const SceneReceipt& receipt,VanceMilestone milestone) noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return; }
    const std::lock_guard lock(mutex);
    const auto now=GetTickCount64();
    if(receipt.run!=selectedRun || receipt.run!=mission_run_generation() || !controller.vance(receipt,milestone,now)) { return; }
    std::array<char,288> line{};
    std::snprintf(line.data(),line.size(),"ev=gateway stage=vance_%s run=%llu generation=%u group=%08X sensor=%08X selector=%08X cue_ms=%llu evidence=native_B438B0_path",
        milestone==VanceMilestone::turned?"turned":"conversation_started",static_cast<unsigned long long>(receipt.run),receipt.generation,
        receipt.group,receipt.sensor,receipt.selector,static_cast<unsigned long long>(now));log(line.data());
}
bool observe_enemy(const EnemyReceipt& receipt,bool death) noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return false; }
    const std::lock_guard lock(mutex);
    if(receipt.run!=selectedRun || receipt.run!=mission_run_generation()) { return false; }
    const bool accepted=death?controller.died(receipt):controller.admitted(receipt);
    if(accepted) {
        std::array<char,256> line{};
        std::snprintf(line.data(),line.size(),"ev=gateway stage=%s run=%llu registry=%08X source=%u actor=%08X owner=%08X generation=%u evidence=native_%s",
            death?"death":"admission",static_cast<unsigned long long>(receipt.run),receipt.registry,receipt.source,receipt.actor,receipt.owner,receipt.generation,death?"health_death":"actor_creation");log(line.data());
    }
    return accepted;
}
bool observe_admission(const EnemyReceipt& receipt) noexcept { return observe_enemy(receipt,false); }
bool observe_death(const EnemyReceipt& receipt) noexcept { return observe_enemy(receipt,true); }
void observe_prepared(std::uint64_t run,std::uint32_t generation,std::uint8_t index) noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return; }
    const std::lock_guard lock(mutex);
    if(run!=selectedRun || run!=mission_run_generation() || !controller.prepared(run,generation,index)) { return; }
    std::array<char,192> line{};
    std::snprintf(line.data(),line.size(),"ev=gateway stage=object_prepared run=%llu generation=%u index=%u evidence=native_inactive_apply",static_cast<unsigned long long>(run),generation,index);log(line.data());
}
void observe_position(float x,float y,float z) noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return; }
    const auto run=mission_run_generation();const std::lock_guard lock(mutex);
    if(run==selectedRun) { controller.position(run,{x,y,z}); }
}
void observe_submission(std::uint64_t run,std::uint32_t definition,std::int64_t offset,
    std::uint32_t bank,std::uint8_t row,std::uint32_t generation) noexcept {
    if(definition!=kDialogueAsset.definition || offset!=0x1408 || bank!=kBank || row>=16
        || run!=mission_run_generation() || !mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return; }
    const std::lock_guard lock(mutex);
    if(run!=selectedRun || !controller.submitted(run,bank,row,generation,GetTickCount64())) { return; }
    std::array<char,240> line{};
    std::snprintf(line.data(),line.size(),"ev=gateway stage=dialogue_submitted run=%llu bank=%08X row=%u generation=%u evidence=native_dispatch",
        static_cast<unsigned long long>(run),bank,row,generation);log(line.data());
}
} // namespace dawn::state::activity::gateway
