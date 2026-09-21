#include <Windows.h>
#include "runtime.h"
#include "entry.h"
#include "starting_loadout.h"
#include "../../../runtime/state_account_transaction_helpers.h"
#include "../../../runtime/storage/internal.h"
#include "../../../persistence/persistence.h"
#include "../../runtime.h"
#include "../../../../core/logging/log.h"
#include "../../../../client/hooks/bootflow/gateway_native_read.h"
#include "../../../../client/hooks/bootflow/coo_enemy_readiness.h"
#include <mutex>
#include <cstdio>
namespace dawn::state::activity::vanilla::adieu {
namespace {
std::mutex mutex;Controller controller;Entry entry;
starting_loadout::Once loadout;
std::unique_ptr<coo::script::MissionDocument> document;std::uint64_t selectedRun{},nextPublication{};
bool selected() noexcept {return selectedRun && selectedRun==mission_run_generation();}
bool current() noexcept {return selected() && loadout.applied(selectedRun) && mission_seed_armed() && world_phase()==WorldPhase::arrived;}
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
            if(!size || size>=path.size()) error="cannot resolve DLL-relative script path";
            else document=coo::script::MissionDocument::read(std::filesystem::path(path.data()).parent_path()/L"Dawn"/L"scripts"/L"adieu.lua",kEntryProfile,error);
            if(document && !valid_document(document->views())) {document.reset();error="Adieu native profile mismatch";}
        } catch(const std::exception& e) {error=e.what();}
        std::array<char,768> line{};
        if(document) std::snprintf(line.data(),line.size(),"ev=coo_script mission=adieu result=loaded format=lua fnv1a64=%016llX path=Dawn/scripts/adieu.lua reload=next_process",static_cast<unsigned long long>(document->fingerprint()));
        else std::snprintf(line.data(),line.size(),"ev=coo_script mission=adieu result=failed reason=\"%.*s\"",static_cast<int>((std::min)(error.size(),std::size_t{500})),error.data());
        log(line.data());
    });
    return document!=nullptr;
}
void poll_readiness(std::uint64_t run,std::uint64_t now) noexcept {
    static std::uint64_t lastRun{},next{};std::array<EnemyReceipt,24> pending{};std::size_t count{};
    {const std::lock_guard lock(mutex);if(!current() || run!=selectedRun) return;
        if(lastRun!=run) {lastRun=run;next=0;}if(now<next) return;next=now+500;
        controller.pending_enemies([&](const auto& r) {if(count<pending.size()) pending[count++]=r;});}
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    for(std::size_t i=0;i<count;++i) {
        client::hooks::bootflow::gateway_native::Read read{image};
        observe_readiness(pending[i],client::hooks::bootflow::coo_native::enemy(read,image,pending[i]));
    }
}
}
bool prepare(std::uint64_t run,bool exact) noexcept {
    const std::lock_guard lock(mutex);if(run!=mission_run_generation()) return false;
    if(!exact) {if(selectedRun && selectedRun!=run) {entry.reset();controller.reset();selectedRun=nextPublication=0;}return false;}
    if(!load() || !entry.select(document->views(),controller,run,GetTickCount64())) return false;
    selectedRun=run;return true;
}
bool prepare_starting_loadout() noexcept {
    const std::lock_guard lock(mutex);
    if(!selected() || loadout.applied(selectedRun) || controller.frame().fault || controller.frame().finished) return false;
    static std::uint64_t attemptRun{},nextAttempt{};
    const auto now=GetTickCount64();
    if(attemptRun==selectedRun && now<nextAttempt) return false;
    attemptRun=selectedRun;nextAttempt=now+500;
    using namespace state::runtime::storage;
    const char* failure="account";
    AcquireSRWLockExclusive(&g_stateLock);
    const bool committed=loadout.apply(selectedRun,[&]() noexcept {
        if(selectedRun!=mission_run_generation() || !account::valid(g_state.account)) return false;
        auto candidate=std::make_unique<AccountState>(g_state.account);
        failure="selected_character";
        std::size_t ci=candidate->characterCount;
        for(std::size_t i=0;i<candidate->characterCount;++i) if(candidate->characters[i].selected) {ci=i;break;}
        if(ci==candidate->characterCount) return false;
        auto& character=candidate->characters[ci];
        failure="inventory_serial";
        if(character.nextInventorySerial>=INT32_MAX) return false;
        failure="sidearm_definition";
        build_data::items::Definition definition{};build_data::items::details::Definition details{};
        if(!build_data::find_item_definition_hash(starting_loadout::kSidearm,definition) || definition.bucketId!=0
            || !build_data::find_configured_item_detail(definition.definitionIndex,details)
            || details.definitionHash!=starting_loadout::kSidearm || details.bucketId!=0
            || details.instancedDefinitionState!=build_data::items::details::InstancedDefinitionState::instanced) return false;
        account::inventory::Item sidearm{};
        failure="item_identity";
        if(!state::runtime::detail::next_item_instance_soid(*candidate,sidearm.instanceSoid)) return false;
        sidearm.definitionHash=starting_loadout::kSidearm;sidearm.quantity=1;
        sidearm.mutationSerial=static_cast<std::int32_t>(character.nextInventorySerial++);
        sidearm.sockets.policy=account::inventory::SocketPolicy::nativeDefaults;
        failure="carried_inventory";
        if(!starting_loadout::prepare(character,sidearm,[](std::uint32_t hash) noexcept {
            build_data::items::Definition item{};
            if(!build_data::find_item_definition_hash(hash,item) || item.bucketId==build_data::items::kUnresolvedBucketId)
                return starting_loadout::ItemKind::unknown;
            // Installed character weapon buckets: kinetic 0, energy 1, power 2.
            return item.bucketId<=2?starting_loadout::ItemKind::weapon:starting_loadout::ItemKind::other;
        })) return false;
        failure="native_loadout";
        auto resolved=std::make_unique<middleware::datagen::family4::loadout::ResolvedLoadout>();
        if(!account::valid(*candidate) || !middleware::datagen::family4::loadout::resolve(*candidate,ci,*resolved)
            || selectedRun!=mission_run_generation()) return false;
        failure="save_commit";
        if(!persistence::commit_account(g_state.account,*candidate)) return false;
        g_state.account=*candidate;return true;
    });
    ReleaseSRWLockExclusive(&g_stateLock);
    if(committed) {
        std::array<char,160> line{};std::snprintf(line.data(),line.size(),
            "ev=adieu stage=starting_loadout run=%llu sidearm=%u reset=once_per_run result=committed",
            static_cast<unsigned long long>(selectedRun),starting_loadout::kSidearm);log(line.data());
    } else {
        static std::uint64_t failedRun{},nextReport{};
        if(failedRun!=selectedRun || now>=nextReport) {
            failedRun=selectedRun;nextReport=now+5000;
            std::array<char,180> line{};std::snprintf(line.data(),line.size(),
                "ev=adieu stage=starting_loadout run=%llu result=retry reason=%s",
                static_cast<unsigned long long>(selectedRun),failure);log(line.data());
        }
    }
    return committed;
}
Frame snapshot(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    poll_readiness(run,now);const std::lock_guard lock(mutex);if(!selected() || run!=selectedRun) return {};
    if(current() && ready) static_cast<void>(entry.update(controller,run,now,true));
    nextPublication=now+100;const auto& f=controller.frame();
    static coo::Generation previousOwner{};static Stage previousStage{};static cinematics::Phase previousPhase{};static bool previousFault{};
    if(previousOwner!=controller.owner() || previousStage!=f.stage || previousPhase!=f.cinematic.phase || previousFault!=f.fault) {
        previousOwner=controller.owner();previousStage=f.stage;previousPhase=f.cinematic.phase;previousFault=f.fault;
        std::array<char,260> line{};std::snprintf(line.data(),line.size(),"ev=adieu run=%llu generation=%u stage=%u movie=%u phase=%u healed=%u ghost_retired=%u weapon=%u finished=%u fault=%u",
            static_cast<unsigned long long>(run),f.spawnGeneration,static_cast<unsigned>(f.stage),f.cinematic.movie,
            static_cast<unsigned>(f.cinematic.phase),f.healed?1U:0U,f.ghostRetired?1U:0U,f.weaponGranted?1U:0U,f.finished?1U:0U,f.fault?1U:0U);log(line.data());
    }
    return f;
}
Request request() noexcept {const std::lock_guard lock(mutex);return selected()?Request{controller.owner(),controller.frame()}:Request{};}
std::uint64_t native_run() noexcept {const std::lock_guard lock(mutex);return current()?selectedRun:0;}
bool publication_due(std::uint64_t now) noexcept {const std::lock_guard lock(mutex);return current() && !controller.frame().finished && now>=nextPublication;}
void observe_arrival(coo::Generation owner,std::uint8_t route) noexcept {
    const std::lock_guard lock(mutex);if(selected()) static_cast<void>(controller.arrival(owner,route,GetTickCount64()));
}
bool observe_cinematic(const cinematics::Incident& r) noexcept {
    const std::lock_guard lock(mutex);return current() && controller.cinematic(controller.owner(),r,GetTickCount64());
}
void observe_position(float x,float y,float z) noexcept {const std::lock_guard lock(mutex);if(current()) controller.position(controller.owner(),{x,y,z});}
void observe_scene(std::uint64_t run,std::uint32_t registry,std::uint16_t slot,const scene_wire::Output& r) noexcept {
    const std::lock_guard lock(mutex);if(current() && run==selectedRun) static_cast<void>(controller.scene(controller.owner(),asset(registry,43,slot),r));
}
PlaybackRequest playback_request(std::uint32_t definition) noexcept {
    const std::lock_guard lock(mutex);if(!current() || controller.frame().finished || controller.frame().fault) return {};
    for(const auto& scene:kScenes) if(scene.asset.definition==definition) {
        const auto* state=controller.frame().native.scene(scene.asset);
        if(state && state->requested && state->armed && !state->stopped)
            return {controller.owner(),scene.asset,state->generation,state->selector,state->serial};
    }
    return {};
}
void observe_playback(const PlaybackReceipt& r) noexcept {
    const std::lock_guard lock(mutex);if(!current()) return;
    const auto* state=controller.frame().native.scene(r.request.scene);
    const bool first=state && !state->started;
    const bool finished=state && r.completed && !state->completed;
    if(controller.playback(r,GetTickCount64()) && (first || finished)) {
        std::array<char,220> line{};std::snprintf(line.data(),line.size(),
            "ev=adieu stage=%s run=%llu slot=%u generation=%u selector=%08X serial=%08X cast=ready",
            finished?"scene_completed":"scene_started",static_cast<unsigned long long>(r.request.owner.run),
            r.request.scene.slot,r.request.generation,r.selector,r.serial);log(line.data());
    }
}
void observe_submission(std::uint64_t run,std::uint32_t definition,std::int64_t offset,std::uint32_t bank,std::uint8_t row,std::uint32_t generation) noexcept {
    const auto* b=find(kRoot,53,2);if(!b || b->asset.definition!=definition || b->offset!=offset || bank!=kBank || row>=std::size(kDialogue)) return;
    const std::lock_guard lock(mutex);if(current() && run==selectedRun) static_cast<void>(controller.submitted(controller.owner(),bank,row,generation,GetTickCount64()));
}
void observe_prepared(coo::Generation owner,coo::Asset a) noexcept {const std::lock_guard lock(mutex);if(current()) static_cast<void>(controller.prepared(owner,a));}
void observe_object(const coo::ObjectReceipt& r) noexcept {const std::lock_guard lock(mutex);if(current()) static_cast<void>(controller.object(r));}
void observe_use(std::uint32_t key,std::uint8_t type,std::uint16_t slot,const middleware::bap::activity_message::object_sense::Output& r) noexcept {
    const std::lock_guard lock(mutex);if(current()) static_cast<void>(controller.use(controller.owner(),asset(key,type,slot),r));
}
bool observe_admission(const EnemyReceipt& r) noexcept {const std::lock_guard lock(mutex);return current() && controller.admitted(r);}
void observe_source(std::uint32_t key,std::uint8_t type,std::uint16_t slot,const middleware::bap::activity_message::source_sense::Output& r) noexcept {
    const std::lock_guard lock(mutex);if(current()) static_cast<void>(controller.source(controller.owner(),asset(key,type,slot),r));
}
bool observe_death(const EnemyReceipt& r) noexcept {const std::lock_guard lock(mutex);return current() && controller.died(r);}
void observe_readiness(const EnemyReceipt& r,coo::EnemyReadiness ready) noexcept {const std::lock_guard lock(mutex);if(current()) static_cast<void>(controller.readiness(r,ready));}
GrantRequest grant_request() noexcept {const std::lock_guard lock(mutex);return current()?controller.grant_request():GrantRequest{};}
bool observe_granted(const GrantRequest& r,std::uint64_t item) noexcept {const std::lock_guard lock(mutex);return current() && controller.granted(r,item);}
bool commit_grant(const GrantRequest& r,std::uint64_t item,bool complete,void* context,bool(*commit)(void*) noexcept) noexcept {
    const std::lock_guard lock(mutex);
    if(!current() || !r.valid() || controller.grant_request()!=r || !item || !commit || !commit(context)) return false;
    return !complete || controller.granted(r,item);
}
}
