#include <Windows.h>
#include "runtime.h"
#include "controller.h"
#include "../runtime.h"
#include "../../../core/logging/log.h"
#include <cstdio>
#include <mutex>
#include "../../../server/runtime/activity/mission_observation_queue.h"

namespace dawn::state::activity::beyond_infinity {
namespace {
std::mutex mutex;
Controller controller;
struct ForestObservation {coo::Generation owner{};std::uint8_t pass{};bool ready{},pending{};} forestObservation;
std::array<server::runtime::activity::mission_device_pose::Inbox<PlateReceipt>,1> poseInbox{};
struct Observation {coo::Generation owner{};PlateReceipt plate{};LensReceipt lens{};Point point{};std::uint32_t revision{};float progress{};bool binding{},position{},complete{},lensObservation{};};
server::runtime::activity::MissionObservationQueue<Observation> observations;
void consume(const Observation& e) noexcept {
    if(e.owner!=controller.owner()) {return;}
    if(e.lensObservation) {static_cast<void>(controller.lens(e.lens,e.complete));}
    else if(e.position) {controller.position(e.owner.run,e.point);}
    else if(e.binding) {static_cast<void>(controller.bind_plate(e.plate));}
    else {static_cast<void>(controller.plate(e.plate,e.revision,e.progress,e.complete));}
}

std::unique_ptr<coo::script::MissionDocument> document;
std::uint64_t selectedRun{},nextPublication{};
coo::StallDiagnostics stalled;
std::uint32_t lastActive{UINT32_MAX},lastComplete{UINT32_MAX};
std::uint8_t lastSection{UINT8_MAX};
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
            else { document=coo::script::MissionDocument::read(std::filesystem::path(path.data()).parent_path()/L"Dawn"/L"scripts"/L"beyond_infinity.lua",kProfile,error); }
            if(document && !valid_document(document->views())) { document.reset();error="Beyond Infinity native profile mismatch"; }
        } catch(const std::exception& e) { error=e.what(); }
        std::array<char,768> line{};
        if(document) { std::snprintf(line.data(),line.size(),"ev=coo_script mission=beyond_infinity result=loaded format=lua fnv1a64=%016llX path=Dawn/scripts/beyond_infinity.lua reload=next_process",static_cast<unsigned long long>(document->fingerprint())); }
        else { std::snprintf(line.data(),line.size(),"ev=coo_script mission=beyond_infinity result=failed reason=\"%.*s\"",static_cast<int>((std::min)(error.size(),std::size_t{500})),error.data()); }
        log(line.data());
    });
    return document!=nullptr;
}
}
bool prepare(std::uint64_t run,bool selected) noexcept {
    const std::lock_guard lock(mutex);
    if(run!=mission_run_generation()) { return false; }
    if(!selected) { controller.reset();observations.reset();poseInbox={};forestObservation={};selectedRun=nextPublication=0;stalled.reset();lastActive=lastComplete=UINT32_MAX;lastSection=UINT8_MAX;return false; }
    if(!load() || !controller.select(document->views(),run)) { return false; }
    if(selectedRun!=run) {observations.reset();poseInbox={};forestObservation={}; stalled.reset();lastActive=lastComplete=UINT32_MAX;lastSection=UINT8_MAX; }
    selectedRun=run;return true;
}
Frame snapshot(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    const std::lock_guard lock(mutex);
    if(run!=selectedRun || run!=mission_run_generation()) { return {}; }
    if(!observations.drain(consume)) {log("ev=mission_observations result=overflow publication=stopped");return {};}
    for(auto& inbox:poseInbox) {inbox.drain([](const PlateReceipt& r,auto sample) {static_cast<void>(controller.plate_pose(r,sample));});}
    if(forestObservation.pending) {
        static_cast<void>(controller.forest_ready(forestObservation.owner,forestObservation.pass,forestObservation.ready));
        forestObservation.pending=false;
    }
    const auto frame=controller.update(run,now,ready && current());
    const auto d=controller.diagnostics();
    if(frame.section!=lastSection || d.active!=lastActive || d.complete!=lastComplete) {
        lastSection=frame.section;lastActive=d.active;lastComplete=d.complete;
        std::array<char,320> line{};
        std::snprintf(line.data(),line.size(),"ev=coo_executor mission=beyond_infinity run=%llu generation=%u section=%u phase=%u active=%08X complete=%08X failure=%u forest_pass=%u mission_finished=%u",
            static_cast<unsigned long long>(run),frame.spawnGeneration,frame.section,static_cast<unsigned>(d.phase),d.active,d.complete,static_cast<unsigned>(d.failure),frame.forestPass,frame.finished?1U:0U);log(line.data());
    }
    if(const auto* graph=controller.graph();graph && d.phase==coo::Phase::running) {
        for(const auto& binding:graph->commands) {
            if(controller.step_state(binding.step).phase!=coo::StepPhase::active) { continue; }
            const auto& spec=graph->definition.steps[binding.step].commands[binding.command];coo::StallReport report{};
            if(!stalled.observe({run,d.incarnation,binding.step,binding.command},controller.missing(spec),now,report)) { continue; }
            std::array<char,512> line{};
            std::snprintf(line.data(),line.size(),"ev=coo_stall mission=beyond_infinity command=%.*s missing=%s registry=%08X type=%u slot=%u waiting_ms=%llu",
                static_cast<int>(binding.id.size()),binding.id.data(),coo::missing_name(report.detail.missing),spec.asset.registry,spec.asset.type,spec.asset.slot,static_cast<unsigned long long>(report.waitingMs));log(line.data());
        }
    }
    nextPublication=now+100;return frame;
}
void observe_forest_readiness(coo::Generation owner,std::uint8_t pass,bool ready) noexcept {
    const std::lock_guard lock(mutex);
    if(!current() || owner!=controller.owner() || pass!=controller.frame().forestPass)return;
    if(forestObservation.owner==owner && forestObservation.pass==pass && forestObservation.ready==ready)return;
    forestObservation={owner,pass,ready,true};nextPublication=0;
}
void observe_forest_terminal(coo::Generation owner,std::uint8_t pass) noexcept {
    const std::lock_guard lock(mutex);
    if(current())static_cast<void>(controller.forest_terminal(owner,pass));
}
Request request() noexcept {
    const std::lock_guard lock(mutex);return current()?Request{controller.owner(),controller.frame()}:Request{};
}
LensRequest lens_request() noexcept {
    const std::lock_guard lock(mutex);
    if(!current()) { return {}; }
    const auto& frame=controller.frame();const auto& lens=controller.lens_owner();
    const auto& native=frame.native[asset_index(kLens)];
    return {controller.owner(),lens,frame.enabled && native.active
        && (!lens.valid() || (lens.owner.run==selectedRun && lens.owner.value==native.generation)),
        frame.lensExposed,frame.lensDestroyed,native.generation};
}
PlateRequest plate_request() noexcept {
    const std::lock_guard lock(mutex);if(!current()) { return {}; }
    const auto& frame=controller.frame();const auto plate=controller.plate_owner();
    const auto& native=frame.native[asset_index(kPlate)];
    return {controller.owner(),plate,frame.plateRevision,frame.enabled && native.active && plate.valid()
        && plate.owner.run==selectedRun && plate.owner.value==native.generation,frame.plateOccupied,frame.lensDestroyed,frame.lensExposed,frame.plateCapture};
}
void observe_plate_pose(const PlateReceipt& r,server::runtime::activity::mission_device_pose::Sample sample) noexcept {
    const std::lock_guard lock(mutex);
    if(!current() || !r.valid() || controller.plate_owner()!=r)return;
    poseInbox[0].submit(r,sample);
}
void observe_plate_binding(const PlateReceipt& receipt) noexcept {
    const std::lock_guard lock(mutex);if(!current()) {return;}
    Observation e{};e.owner=controller.owner();e.plate=receipt;e.binding=true;observations.push(e);
}
void observe_plate(const PlateReceipt& receipt,std::uint32_t revision,float value,bool complete) noexcept {
    const std::lock_guard lock(mutex);if(!current()) {return;}
    Observation e{};e.owner=controller.owner();e.plate=receipt;e.revision=revision;e.progress=value;e.complete=complete;observations.push(e);
}
bool publication_due(std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);return current() && now>=nextPublication;
}
void observe_position(float x,float y,float z) noexcept {
    const std::lock_guard lock(mutex);if(current()) { Observation e{};e.owner=controller.owner();e.position=true;e.point={x,y,z};observations.push(e); }
}
void observe_submission(std::uint64_t run,std::uint32_t definition,std::int64_t offset,
    std::uint32_t bank,std::uint8_t row,std::uint32_t generation) noexcept {
    if(!dialogue_identity(definition,offset,bank,row)) { return; }
    const std::lock_guard lock(mutex);
    if(!current() || run!=selectedRun || !controller.submitted(run,bank,row,generation,GetTickCount64())) { return; }
    std::array<char,192> line{};std::snprintf(line.data(),line.size(),"ev=beyond_infinity stage=dialogue_submitted run=%llu row=%u generation=%u",static_cast<unsigned long long>(run),row,generation);log(line.data());
}
void observe_prepared(coo::Generation owner,coo::Asset asset) noexcept {
    const std::lock_guard lock(mutex);if(current()) { static_cast<void>(controller.prepared(owner,asset)); }
}
void observe_lens(const LensReceipt& receipt,bool dead) noexcept {
    const std::lock_guard lock(mutex);if(!current()) {return;}
    Observation e{};e.owner=controller.owner();e.lensObservation=true;e.lens=receipt;e.complete=dead;observations.push(e);
}
void observe_scene(const SceneReceipt& receipt,bool complete) noexcept {
    const std::lock_guard lock(mutex);
    if(!current() || !controller.scene(receipt,complete)) { return; }
    std::array<char,240> line{};std::snprintf(line.data(),line.size(),"ev=beyond_infinity stage=scene_%s run=%llu registry=%08X slot=%u generation=%u evidence=native_B438B0",complete?"finished":"bound",static_cast<unsigned long long>(receipt.owner.run),receipt.asset.registry,receipt.asset.slot,receipt.owner.value);log(line.data());
}
void observe_transit(coo::Generation owner,std::uint8_t route) noexcept {
    const std::lock_guard lock(mutex);if(!current() || !controller.transit(owner,route)) { return; }
    std::array<char,192> line{};std::snprintf(line.data(),line.size(),
        "ev=beyond_infinity stage=transit_arrived run=%llu generation=%u route=%u evidence=native_membership_tuple",
        static_cast<unsigned long long>(owner.run),owner.value,route);log(line.data());
}
void observe_scene_cue(const SceneReceipt& receipt,std::uint8_t id,std::uint32_t state,std::uint32_t starts) noexcept {
    const std::lock_guard lock(mutex);
    if(!current() || !controller.scene_cue(receipt,id,state,starts)) { return; }
    std::array<char,256> line{};std::snprintf(line.data(),line.size(),
        "ev=beyond_infinity stage=scene_cue run=%llu registry=%08X slot=%u generation=%u cue=%u state=%u starts=%u evidence=native_node_input",
        static_cast<unsigned long long>(receipt.owner.run),receipt.asset.registry,receipt.asset.slot,receipt.owner.value,id,state,starts);log(line.data());
}
void observe_scene_speech(const SceneReceipt& receipt,std::uint8_t row,std::uint32_t state) noexcept {
    const std::lock_guard lock(mutex);
    if(!current() || !controller.scene_speech(receipt,row,state)) { return; }
    std::array<char,256> line{};std::snprintf(line.data(),line.size(),
        "ev=beyond_infinity stage=scene_speech run=%llu registry=%08X slot=%u generation=%u row=%u state=%u evidence=native_node_98",
        static_cast<unsigned long long>(receipt.owner.run),receipt.asset.registry,receipt.asset.slot,receipt.owner.value,row,state);log(line.data());
}

}
