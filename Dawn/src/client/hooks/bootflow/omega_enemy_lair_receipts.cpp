#include "../../../state/activity/vendors/lifetime.h"
#include "../../../state/activity/vendors/presentation.h"
#include "vendor_network_presence.h"
#include "vendor_lifetime_native.h"
#include "../../../state/activity/Newlight/launchpad/runtime.h"
#include "forest_candy_drops.h"
#include "native_generated_population_identity.h"
#include "native_generated_roster.h"
#include "../../../state/activity/vanilla/one_au/runtime.h"
#include "../../../state/activity/vanilla/homecoming/runtime.h"
#include "../../../state/activity/vanilla/adieu/runtime.h"
#include <Windows.h>
#include <intrin.h>
#include <bit>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <mutex>

#include "omega_enemy_lair_receipts.h"
#include "mission_population_observer.h"
#include "hijacked_placements.h"
#include "../../../state/activity/gateway/runtime.h"
#include "../../../state/activity/deadly_trial/runtime.h"
#include "../../../state/activity/deep_storage/runtime.h"
#include "../../../state/activity/strike_pact/runtime.h"
#include "../../../state/activity/hijacked/runtime.h"
#include "native_population_pending.h"
#include "native_population_retirement.h"
#include "native_population_streaming.h"
#include "vance_contact_observer.h"
#include "../../../state/activity/strike_bond/runtime.h"
#include "omega_enemy_native_reference.h"
#include "omega_enemy_native_admission.h"
#include "omega_enemy_native_health.h"
#include "gateway_native_read.h"
#include "coo_enemy_readiness.h"
#include "strike_bond_fire_trace.h"
#include "strike_bond_carriage.h"
#include "coo_native_components.h"
#include "strike_bond_intro_release.h"
#include "strike_bond_boss_cycle.h"
#include "strike_bond_boss_shield.h"
#include "strike_bond_boss_retirement.h"
#include "omega_vex_lattice_probe.h"
#include "strike_bond_target_binding.h"
#include "omega_boss_health.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../core/logging/log.h"
#include "../../../state/activity/omega_enemy_lair_catalog.h"
#include "../../../state/activity/omega_enemy_crown_catalog.h"
#include "../../../state/activity/omega_first_lair_runtime.h"
#include "../../../state/activity/omega_presentation.h"
#include "../../../state/activity/native_population_events.h"
#include "../../../state/activity/open_world_member_observations.h"
#include "../../../state/activity/coo/open_world_member_catalog.h"
#include "../../../state/activity/Newlight/launchpad/welcome.h"

namespace dawn::client::hooks::bootflow {
namespace {
namespace catalog=state::activity::omega_enemy_lair;
namespace crownCatalog=state::activity::omega_enemy_crown;
namespace gateway=state::activity::gateway;
namespace trial=state::activity::deadly_trial;
namespace hijacked=state::activity::hijacked;
namespace deep=state::activity::deep_storage;
namespace strike=state::activity::strike_pact;
namespace garden=state::activity::strike_bond;
namespace launchpad=state::activity::newlight::launchpad;
namespace oneau=state::activity::vanilla::one_au;
namespace hc=state::activity::vanilla::homecoming;
namespace adieu=state::activity::vanilla::adieu;
struct Context final { bool enabled;std::uint64_t run;bool gateway;bool trial{};bool deep{};bool strike{};bool hijacked{};bool garden{};bool launchpad{};bool oneAu{};bool homecoming{};bool adieu{}; };
Context selected_context() noexcept {
    if(const auto run=adieu::native_run()) return {.enabled=true,.run=run,.adieu=true};
    if(const auto run=hc::native_run()) return {.enabled=true,.run=run,.homecoming=true};
    if(const auto run=oneau::native_run()) return {.enabled=true,.run=run,.oneAu=true};
    if(const auto run=launchpad::native_run()) return {.enabled=true,.run=run,.launchpad=true};
    const auto gardenRun=garden::native_run();if(gardenRun) return {true,gardenRun,false,false,false,false,false,true};
    const auto hijackedRun=hijacked::native_run();if(hijackedRun) {return {true,hijackedRun,false,false,false,false,true};}
    const auto deepRun=deep::native_run();if(deepRun) {return {true,deepRun,false,false,true};}
    const auto strikeRun=strike::native_run();if(strikeRun) {return {true,strikeRun,false,false,false,true};}
    const auto trialRun=trial::native_run();if(trialRun) { return {true,trialRun,false,true}; }
    const auto run=gateway::native_run();if(run!=0) { return {true,run,true}; }
    const auto nav=state::activity::omega_presentation::navigation();return {nav.enabled,nav.run,false};
}

using Admission=std::uint64_t(__fastcall*)(void*,const void*) noexcept;
using CandidateEvent=std::uint64_t(__fastcall*)(void*,std::uint32_t) noexcept;
using Retirement=void(__fastcall*)(std::uint32_t,std::uint8_t) noexcept;
hooking::CallGate g_gate;
std::array<hooking::detour::Handle,12> g_handles{};
std::atomic<Admission> g_admission{};
std::atomic<CandidateEvent> g_candidate{};
std::atomic<Retirement> g_retirement{};
std::uintptr_t g_image{};
SRWLOCK g_lock=SRWLOCK_INIT;
std::uint64_t g_run{UINT64_MAX};
unsigned g_lines{},g_seenCount{};
struct Seen final {std::uint32_t actor{},registry{};std::uint16_t source{};};
// A full run now has143 planned admissions including the nonblocking Cabal
// escape. This remains a bounded diagnostic set; state delivery occurs first.
std::array<Seen,256> g_seen{};
std::uint32_t g_candidateRejected{};
std::uint32_t g_parentRejected{};
constexpr unsigned kLineLimit=768;
constexpr std::size_t kCopyLimit=8192;
// Bounded post-identity rejection evidence: one line per boundary/reason/
// registry/source per run. Lines never change delivery or native returns.
struct Reject final {std::uint8_t boundary{},reason{};std::uint32_t registry{};std::uint16_t source{};};
std::array<Reject,64> g_rejects{};unsigned g_rejectCount{};bool g_rejectOverflow{};
constexpr std::uint8_t kBoundaryAdmission=1,kBoundaryDeath=2;
enum : std::uint8_t {
    kRejectRunChanged=1,kRejectStateAdmission,kRejectEventInvalid,kRejectEventClass,
    kRejectHealthInvalid,kRejectDeathBitUnset,kRejectGeneration,kRejectGateClosed,kRejectStateDeath
};
constexpr const char* reject_name(std::uint8_t reason) noexcept {
    switch(reason) {
    case kRejectRunChanged: return "run_changed";
    case kRejectStateAdmission: return "state_admission";
    case kRejectEventInvalid: return "event_invalid";
    case kRejectEventClass: return "event_class";
    case kRejectHealthInvalid: return "health_invalid";
    case kRejectDeathBitUnset: return "death_bit_unset";
    case kRejectGeneration: return "generation";
    case kRejectGateClosed: return "gate_closed";
    case kRejectStateDeath: return "state_death";
    default: return "unknown";
    }
}
[[nodiscard]] bool first_reject(std::uint8_t boundary,std::uint8_t reason,std::uint32_t registry,std::uint16_t source) noexcept {
    for(unsigned i=0;i<g_rejectCount;++i) {
        const auto& seen=g_rejects[i];
        if(seen.boundary==boundary && seen.reason==reason && seen.registry==registry && seen.source==source) {return false;}
    }
    if(g_rejectCount>=g_rejects.size()) {g_rejectOverflow=true;return false;}
    g_rejects[g_rejectCount++]={boundary,reason,registry,source};return true;
}

template<class T> T at(const std::byte* bytes) noexcept {
    T value{};std::memcpy(&value,bytes,sizeof value);return value;
}
struct Ref final {std::uint32_t handle{UINT32_MAX},kind{};std::int64_t offset{};};
static_assert(sizeof(Ref)==16);
bool add(std::uintptr_t address,std::int64_t offset,std::uintptr_t& result) noexcept {
    if(offset>=0) {
        if(static_cast<std::uint64_t>(offset)>UINTPTR_MAX-address) {return false;}
        result=address+static_cast<std::uintptr_t>(offset);
    } else {
        const auto magnitude=static_cast<std::uint64_t>(-(offset+1))+1U;
        if(magnitude>address) {return false;}result=address-static_cast<std::uintptr_t>(magnitude);
    }
    return result>=0x10000;
}
struct Read final {
    std::size_t copied{};
    bool copy(std::uintptr_t source,std::span<std::byte> destination) noexcept {
        if(source<0x10000 || source>UINTPTR_MAX-destination.size()
            || copied>kCopyLimit || destination.size()>kCopyLimit-copied) {return false;}
        copied+=destination.size();SIZE_T size{};
        return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(source),
            destination.data(),destination.size(),&size) && size==destination.size();
    }
    template<class T> bool value(std::uintptr_t source,T& value) noexcept {
        return copy(source,std::as_writable_bytes(std::span{&value,std::size_t{1}}));
    }
    bool resolve(const Ref& reference,std::uintptr_t& output) noexcept {
        if(reference.handle==UINT32_MAX) {return false;}
        std::uintptr_t directory{},registry{};
        if(!value(g_image+0x2439C70,directory) || !value(directory,registry)) {return false;}
        const auto shifted=static_cast<std::uint32_t>(static_cast<std::int32_t>(reference.handle)>>13);
        const auto index=((static_cast<std::uint64_t>(shifted)|0xFFC0000ULL)>>18)&(shifted&0xFFFFU);
        std::uintptr_t table{};
        if(!add(registry,static_cast<std::int64_t>(index*0x40),table)) {return false;}
        std::array<std::byte,0x38> bytes{};if(!copy(table,bytes)) {return false;}
        const auto stride=at<std::int32_t>(bytes.data()+0x30);
        const auto mask=at<std::int32_t>(bytes.data()+0x34);
        std::uintptr_t element{};
        if(stride<=0 || stride>0x100000 || !add(at<std::uintptr_t>(bytes.data()+8),
            static_cast<std::int64_t>(reference.handle&0x1FFFU)*stride,element)) {return false;}
        std::uint64_t relocation{};
        if(element>UINTPTR_MAX-8 || !value(element+8,relocation)) {return false;}
        const auto base=omega_enemy_native_reference::corrected_base(element,relocation,mask);
        return add(static_cast<std::uintptr_t>(base),reference.offset,output);
    }
};
struct Actor final {
    std::uint32_t handle{},entity{UINT32_MAX},parent{UINT32_MAX};
    Ref source{},member{};
    std::uint16_t flags{};
    // Sub-reason when the table walk fails (A0D510 stores the created actor in
    // the 0x1F9D7F8/0x1F9D800 table; +0x48 is its own full handle).
    std::uint8_t reason{};std::uint32_t self{UINT32_MAX};
};
bool actor(Read& read,std::uint32_t handle,Actor& actor) noexcept {
    actor.reason=0;actor.self=UINT32_MAX;
    if(handle==UINT32_MAX) {actor.reason=1;return false;}
    std::uintptr_t base{};std::int32_t stride{};
    if(!read.value(g_image+0x1F9D7F8,base) || !read.value(g_image+0x1F9D800,stride)
        || stride<0x70 || stride>0x100000) {actor.reason=2;return false;}
    std::uintptr_t address{};
    if(!add(base,static_cast<std::int64_t>(handle&0x1FFFU)*stride,address)) {actor.reason=3;return false;}
    std::array<std::byte,0x70> bytes{};
    if(!read.copy(address,bytes)) {actor.reason=4;return false;}
    actor.self=at<std::uint32_t>(bytes.data()+0x48);
    if(actor.self!=handle) {actor.reason=5;return false;}
    actor.handle=handle;actor.flags=at<std::uint16_t>(bytes.data());
    actor.entity=at<std::uint32_t>(bytes.data()+0x4C);
    actor.parent=at<std::uint32_t>(bytes.data()+0x50);
    actor.source=at<Ref>(bytes.data()+0x38);actor.member=at<Ref>(bytes.data()+0x60);
    return true;
}
struct Source final {
    std::uint16_t slot{};
    std::uint32_t registry{},resource{},kind{},generation{},senseGeneration{};
    std::uintptr_t address{};
    // Sub-reason and the native scoped identity actually read at definition+0x30
    // (registry, type, slot). Cycle-2/3 registries share class 8080948F and
    // definition offset 728 with the accepted BF06 sources; a mismatch here names
    // the exact field instead of a bare "reason=7".
    std::uint8_t reason{};
    std::uint32_t nativeRegistry{};std::uint8_t nativeType{};std::int16_t nativeSlot{-1};
    std::uint32_t expectedRegistry{};std::uint16_t expectedSlot{};
};
enum : std::uint8_t {
    kSourceDefinitionRef=1,kSourceUnknownResource,kSourceResolve,kSourceRegistry,
    kSourceType,kSourceSlot,kSourceGenerationRead
};
bool source(Read& read,std::uintptr_t instance,Source& source,bool gatewayContext,bool trialContext,bool deepContext,bool strikeContext,bool hijackedContext,bool gardenContext,bool launchpadContext,bool oneAuContext,bool homecomingContext,bool adieuContext) noexcept {
    source.reason=0;source.nativeRegistry=0;source.nativeType=0;source.nativeSlot=-1;
    source.expectedRegistry=0;source.expectedSlot=0;
    Ref definition{};
    if(!read.value(instance,definition) || definition.kind!=0x8080948FU || (!gatewayContext && !trialContext && !deepContext && !strikeContext && !hijackedContext && !gardenContext && !launchpadContext && !oneAuContext && !homecomingContext && !adieuContext && definition.offset!=0x728)) {
        source.resource=definition.handle;source.kind=definition.kind;source.reason=kSourceDefinitionRef;return false;
    }
    source.resource=definition.handle;source.kind=definition.kind;
    std::uint32_t expectedRegistry{};
    std::uint16_t expectedSlot{};
    if(adieuContext) {
        for(const auto& row:adieu::kSources) {
            if(row.definition==definition.handle && row.offset==definition.offset) {expectedRegistry=row.asset.registry;expectedSlot=row.asset.slot;break;}
        }
        if(!expectedRegistry) {source.reason=kSourceUnknownResource;return false;}
    }
    if(homecomingContext) {
        for(const auto& row:hc::kSpawns) {
            if(row.definition==definition.handle && row.offset==definition.offset) {expectedRegistry=row.registry;expectedSlot=row.source;break;}
        }
        if(!expectedRegistry) {source.reason=kSourceUnknownResource;return false;}
    }
    if(oneAuContext) {
        for(const auto& row:oneau::kSpawns) {
            if(row.definition==definition.handle && row.offset==definition.offset) {expectedRegistry=row.registry;expectedSlot=row.source;break;}
        }
        if(!expectedRegistry) {source.reason=kSourceUnknownResource;return false;}
    }
    if(deepContext) {
        for(const auto& row:deep::kSpawns) {
            if(row.definition==definition.handle && row.offset==definition.offset) {expectedRegistry=row.registry;expectedSlot=row.source;break;}
        }
        if(!expectedRegistry) {source.reason=kSourceUnknownResource;return false;}
    } else if(gardenContext) {
        for(const auto& row:garden::kAssets) {
            if(row.asset.type==1 && row.asset.definition==definition.handle && row.offset==definition.offset) {
                expectedRegistry=row.asset.registry;expectedSlot=row.asset.slot;break;
            }
        }
        if(!expectedRegistry) {source.reason=kSourceUnknownResource;return false;}
    } else if(strikeContext) {
        // The strike catalog pins sources by their native scoped identity (registry, type 1,
        // slot) rather than by descriptor tag/offset, which the SDK export does not carry.
        std::uintptr_t scopedDefinition{};std::array<std::byte,8> scoped{};
        if(!read.resolve(definition,scopedDefinition) || scopedDefinition>UINTPTR_MAX-0x30
            || !read.copy(scopedDefinition+0x30,scoped)) {source.reason=kSourceResolve;return false;}
        const auto registry=at<std::uint32_t>(scoped.data());const auto type=at<std::uint8_t>(scoped.data()+4);
        const auto slot=at<std::int16_t>(scoped.data()+6);
        // all_spawn, not spawn: the latter searches the opening's own thirteen rows, so an actor
        // from any later section resolved to nothing and was rejected as an unknown resource.
        const auto* row=type==1 && slot>=0?strike::all_spawn(registry,static_cast<std::uint16_t>(slot)):nullptr;
        if(!row) {source.nativeRegistry=registry;source.nativeType=type;source.nativeSlot=slot;source.reason=kSourceUnknownResource;return false;}
        expectedRegistry=row->registry;expectedSlot=row->source;
    }
    if(launchpadContext) {
        for(const auto& row:launchpad::kSpawns) {
            if(row.definition==definition.handle && row.offset==definition.offset) {expectedRegistry=row.registry;expectedSlot=row.source;break;}
        }
        if(!expectedRegistry) {source.reason=kSourceUnknownResource;return false;}
    }
    if(hijackedContext) {
        for(const auto& row:hijacked::kSpawns) {
            if(row.definition==definition.handle && row.offset==definition.offset) {expectedRegistry=row.registry;expectedSlot=row.source;break;}
        }
        if(!expectedRegistry) {source.reason=kSourceUnknownResource;return false;}
    }
    if(trialContext) {
        for(const auto& row:trial::kSpawns) {
            if(row.definition==definition.handle && row.offset==definition.offset) { expectedRegistry=row.registry;expectedSlot=row.source;break; }
        }
        if(!expectedRegistry) { source.reason=kSourceUnknownResource;return false; }
    }
    if(gatewayContext) {
        for(const auto& row:gateway::kSpawns) {
            if(row.definition==definition.handle && row.offset==definition.offset) { expectedRegistry=row.registry;expectedSlot=row.source;break; }
        }
        if(expectedRegistry==0) { source.reason=kSourceUnknownResource;return false; }
    }
    for(const auto& row:catalog::kSpawners) {
        if(!gatewayContext && !trialContext && !deepContext && !strikeContext && !hijackedContext && !gardenContext && !launchpadContext && !oneAuContext && !homecomingContext && !adieuContext && catalog::supported_by_encounter(row) && row.resource==definition.handle) {
            expectedRegistry=catalog::kRegistry;expectedSlot=row.slot;break;
        }
    }
    if(expectedRegistry==0) {
        for(const auto& registry:crownCatalog::kRegistries) {
            for(const auto& row:registry.spawners) {
                if(row.resource==definition.handle) {expectedRegistry=registry.key;expectedSlot=row.slot;break;}
            }
            if(expectedRegistry!=0) {break;}
        }
    }
    if(expectedRegistry==0) {source.reason=kSourceUnknownResource;return false;}
    source.expectedRegistry=expectedRegistry;source.expectedSlot=expectedSlot;
    std::uintptr_t nativeDefinition{};
    if(!read.resolve(definition,nativeDefinition) || nativeDefinition>UINTPTR_MAX-0x30) {source.reason=kSourceResolve;return false;}
    std::array<std::byte,8> scoped{};
    if(!read.copy(nativeDefinition+0x30,scoped)) {source.reason=kSourceResolve;return false;}
    source.nativeRegistry=at<std::uint32_t>(scoped.data());
    source.nativeType=at<std::uint8_t>(scoped.data()+4);
    source.nativeSlot=at<std::int16_t>(scoped.data()+6);
    if(source.nativeRegistry!=expectedRegistry) {source.reason=kSourceRegistry;return false;}
    if(source.nativeType!=1) {source.reason=kSourceType;return false;}
    if(source.nativeSlot!=expectedSlot) {source.reason=kSourceSlot;return false;}
    if(instance>UINTPTR_MAX-0x244 || !read.value(instance+0x1FC,source.generation)
        || !read.value(instance+0x244,source.senseGeneration)) {source.reason=kSourceGenerationRead;return false;}
    source.registry=expectedRegistry;source.slot=expectedSlot;source.address=instance;
    return true;
}
#include "hijacked_population_logging.inl"

template<class... Args> void report(const char* format,Args... args) noexcept {
    if(g_lines>=kLineLimit) {return;}
    std::array<char,768> message{};const int count=std::snprintf(message.data(),message.size(),format,args...);
    if(count>0 && static_cast<std::size_t>(count)<message.size()) {
        ++g_lines;core::log::write(core::log::Channel::client,core::log::Level::info,
            {message.data(),static_cast<std::size_t>(count)});
    }
}
void run(std::uint64_t run) noexcept {
    if(g_run!=run) {
        g_run=run;g_lines=0;g_seenCount=0;g_seen={};g_candidateRejected=0;g_parentRejected=0;
        g_rejects={};g_rejectCount=0;g_rejectOverflow=false;
    }
}
/** Caller holds g_lock. Emits one bounded line per new boundary/reason/source key
 * and one overflow notice per run when the table fills. Observe-only. */
void reject(std::uint64_t run,std::uint8_t boundary,std::uint8_t reason,std::uint32_t registry,
            std::uint16_t source,std::uint32_t actor,std::uint32_t detail0,std::uint32_t detail1) noexcept {
    const bool wasOverflow=g_rejectOverflow;
    if(first_reject(boundary,reason,registry,source)) {
        report("ev=omega_enemy_lair stage=reject boundary=%s reason=%s run=%llu registry=%08X source=%u actor=%08X detail0=%08X detail1=%08X",
            boundary==kBoundaryAdmission?"A0D510":"C72390",reject_name(reason),
            static_cast<unsigned long long>(run),registry,source,actor,detail0,detail1);
    } else if(g_rejectOverflow && !wasOverflow) {
        report("ev=omega_enemy_lair stage=reject boundary=%s reason=overflow run=%llu capacity=%zu",
            boundary==kBoundaryAdmission?"A0D510":"C72390",static_cast<unsigned long long>(run),g_rejects.size());
    }
}

/** A0D510 copies source/member backlinks directly from its spawn context. The
 * callback pointer may have moved, so resolve the pre-call full selfhandle anew. */
__declspec(noinline) void observe_admission(std::uint32_t parent,std::uint64_t callRun) noexcept {
    const auto nav=selected_context();
    if(!nav.enabled || nav.run==0) {return;}
    if(nav.run!=callRun) {
        // The run advanced between the pre-call identity copy and this callback.
        // Nothing is admitted; record it once per new run so a silent gap is visible.
        if(!TryAcquireSRWLockExclusive(&g_lock)) {return;}
        run(nav.run);
        reject(nav.run,kBoundaryAdmission,kRejectRunChanged,0,0,UINT32_MAX,parent,
            static_cast<std::uint32_t>(callRun));
        ReleaseSRWLockExclusive(&g_lock);return;
    }
    Read read;read.copied=0x28; // Include the pre-original identity copy in this callback's budget.
    Actor actorState;Source sourceState;Ref definition{};
    std::uintptr_t currentParent{},parentAgain{},linked{};
    std::array<std::byte,0x28> parentHeader{};std::uint32_t handle{UINT32_MAX};
    unsigned rejection{};
    if(!read.resolve({parent,0,0},currentParent)) {rejection=1;}
    else if(!read.copy(currentParent,parentHeader)
        || at<std::uint32_t>(parentHeader.data()+4)!=0x808082ECU
        || at<std::uint32_t>(parentHeader.data()+0x24)!=parent) {rejection=2;}
    else if(currentParent>UINTPTR_MAX-0x1470 || !read.value(currentParent+0x1470,handle)) {rejection=3;}
    else if(!actor(read,handle,actorState)) {rejection=4;}
    else if(!omega_enemy_native_admission::identity(parent,
        at<std::uint32_t>(parentHeader.data()+4),at<std::uint32_t>(parentHeader.data()+0x24),
        handle,actorState.handle,actorState.parent)
        || !read.resolve({actorState.parent,0,0},parentAgain) || parentAgain!=currentParent) {rejection=5;}
    else if(!read.resolve(actorState.source,linked)) {rejection=6;}
    else if(!read.value(linked,definition) || !source(read,linked,sourceState,nav.gateway,nav.trial,nav.deep,nav.strike,nav.hijacked,nav.garden,nav.launchpad,nav.oneAu,nav.homecoming,nav.adieu)) {rejection=7;}
    else if(sourceState.generation==0 || sourceState.generation!=sourceState.senseGeneration) {rejection=8;}

    // Retain the authentic actor/AI-parent origin before progression can detach
    // its source. Entity readiness may arrive later on the placement poll.
    // A generation mismatch still has a verified source; detached unknown actors do not.
    if(nav.hijacked && (rejection==0 || rejection==8) && sourceState.registry==0x3E9B74F3U) {
        hijacked_placements::retain_enemy(nav.run,sourceState.slot,actorState.source.handle,handle);
    }
    // Progression delivery must never depend on the best-effort diagnostic lock.
    // The state observer guarantees serialization, validates the current run,
    // deduplicates full actor IDs, and fails closed on unexpected population.
    bool accepted=false;
    if(rejection==0) {
        accepted=nav.adieu?adieu::observe_admission({nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry}):nav.homecoming?hc::observe_admission({nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry}):nav.oneAu?oneau::observe_admission({nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry}):nav.launchpad?launchpad::observe_admission(
            {nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.garden?garden::observe_admission(
            {nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.hijacked?hijacked::observe_admission(
            {nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.deep?deep::observe_admission(
            {nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.strike?strike::observe_admission(
            {nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.trial?trial::observe_admission(
            {nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.gateway?gateway::observe_admission(
            {nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :state::activity::omega_first_lair::observe_admission(
            {nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry});
    }
    if(nav.homecoming && rejection==0) {
        gateway_native::Read probe{g_image};
        const hc::EnemyReceipt receipt{nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry};
        hc::observe_readiness(receipt,coo_native::enemy(probe,g_image,receipt));
    }
    if(nav.oneAu && rejection==0) {
        gateway_native::Read probe{g_image};
        const oneau::EnemyReceipt receipt{nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry};
        oneau::observe_readiness(receipt,coo_native::enemy(probe,g_image,receipt));
    }
    if(nav.deep && rejection==0) {
        gateway_native::Read probe{g_image};
        const deep::EnemyReceipt receipt{nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry};
        deep::observe_readiness(receipt,coo_native::enemy(probe,g_image,receipt));
    }
    if(nav.hijacked && rejection==0) {
        gateway_native::Read probe{g_image};
        const hijacked::EnemyReceipt receipt{nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry};
        hijacked::observe_readiness(receipt,coo_native::enemy(probe,g_image,receipt));
    }
    if(nav.launchpad && rejection==0) {
        gateway_native::Read probe{g_image};
        const launchpad::EnemyReceipt receipt{nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry};
        launchpad::observe_readiness(receipt,coo_native::enemy(probe,g_image,receipt));
    }
    if(nav.trial && rejection==0) {
        gateway_native::Read probe{g_image};
        const trial::EnemyReceipt receipt{nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry};
        trial::observe_readiness(receipt,coo_native::enemy(probe,g_image,receipt));
    }
    if(nav.gateway && rejection==0) {
        gateway_native::Read probe{g_image};
        const gateway::EnemyReceipt receipt{nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry};
        gateway::observe_readiness(receipt,coo_native::enemy(probe,g_image,receipt));
    }
    if(nav.strike && rejection==0) {
        gateway_native::Read probe{g_image};
        const strike::EnemyReceipt receipt{nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry};
        strike::observe_readiness(receipt,coo_native::enemy(probe,g_image,receipt));
    }
    if(!TryAcquireSRWLockExclusive(&g_lock)) {return;}
    run(nav.run);
    if(nav.hijacked && (rejection==0 || rejection>=6))
        trace_hijacked_birth(nav.run,actorState,sourceState,at<std::uint32_t>(parentHeader.data()),rejection,accepted);
    if(rejection==0) {
        bool seen=false;
        for(unsigned i=0;i<g_seenCount;++i) {
            seen|=g_seen[i].actor==handle && g_seen[i].source==sourceState.slot && g_seen[i].registry==sourceState.registry;
        }
        if(!seen && g_seenCount<g_seen.size()) {
            g_seen[g_seenCount++]={handle,sourceState.registry,sourceState.slot};
            report("ev=omega_enemy_lair stage=admitted run=%llu registry=%08X source=%u resource=%08X kind=%08X actor=%08X entity=%08X parent=%08X source_ref=%08X member_ref=%08X generation=%u native_flags=%04X copied=%zu boundary=A0D510 accepted=%u",
                static_cast<unsigned long long>(nav.run),sourceState.registry,sourceState.slot,sourceState.resource,sourceState.kind,
                handle,actorState.entity,actorState.parent,actorState.source.handle,actorState.member.handle,
                sourceState.generation,actorState.flags,read.copied,accepted?1U:0U);
        }
        if(!accepted) {
            // A fully identified native creation the encounter state refused
            // (wrong run/generation, source not enabled for the current wave,
            // duplicate actor, or over-population). Once per registry/source.
            reject(nav.run,kBoundaryAdmission,kRejectStateAdmission,sourceState.registry,sourceState.slot,
                handle,sourceState.generation,actorState.source.handle);
        }
    } else {
        // Unscoped creation is common outside this encounter. These records are
        // only bounded rejection evidence, not admissions or missing enemies.
        // sub= names the failing field inside actor()/source(); native_* echo the
        // scoped identity actually read at definition+30 for cross-registry checks.
        const auto flag=std::uint32_t{1}<<rejection;
        if((g_parentRejected&flag)==0) {
            g_parentRejected|=flag;
            report("ev=omega_enemy_lair stage=admission_rejected run=%llu reason=%u sub=%u parent=%08X current_parent=%p parent_kind=%08X parent_self=%08X actor=%08X actor_self=%08X actor_parent=%08X source_ref=%08X resource=%08X linked=%p native_registry=%08X native_type=%u native_slot=%d expected_registry=%08X expected_slot=%u generation=%u sense_generation=%u copied=%zu boundary=A0D510",
                static_cast<unsigned long long>(nav.run),rejection,
                rejection==4?actorState.reason:rejection==7?sourceState.reason:0U,
                parent,reinterpret_cast<void*>(currentParent),
                at<std::uint32_t>(parentHeader.data()+4),at<std::uint32_t>(parentHeader.data()+0x24),handle,
                actorState.self,actorState.parent,actorState.source.handle,definition.handle,reinterpret_cast<void*>(linked),
                sourceState.nativeRegistry,sourceState.nativeType,sourceState.nativeSlot,
                sourceState.expectedRegistry,sourceState.expectedSlot,
                sourceState.generation,sourceState.senseGeneration,read.copied);
        }
    }
    if(rejection==8) {
        // A catalogued Crown/Lair source whose authority generation has not been
        // reflected yet (or is retiring). This is the cycle-2/3 hazard: a later
        // registry whose sense generation lags its auth generation. Once per source.
        reject(nav.run,kBoundaryAdmission,kRejectGeneration,sourceState.registry,sourceState.slot,
            handle,sourceState.generation,sourceState.senseGeneration);
    }
    ReleaseSRWLockExclusive(&g_lock);
}

/** The original C72390 resolves its event this way through 9ECC70. Perform only
 * bounded copies here, without invoking its validation/dispatch helper. */
bool event_payload(Read& read,std::uint32_t event,std::array<std::byte,0x3C>& header,
                    std::array<std::byte,0x38>& payload,std::uint32_t& definition) noexcept {
    std::uintptr_t allocator{},buffer{},address{};
    if(!read.value(g_image+0x274F778,allocator) || allocator>UINTPTR_MAX-0x10
        || !read.value(allocator+0x10,buffer) || !add(buffer,event&0xFFFFFU,address)
        || !read.copy(address,header)) {return false;}
    definition=at<std::uint32_t>(header.data()+0x34);
    std::uintptr_t descriptor{};std::uint8_t alignment{};
    if(!read.resolve({definition,0,0},descriptor) || descriptor>UINTPTR_MAX-0x18
        || !read.value(descriptor+0x18,alignment) || alignment==0 || alignment>64
        || (alignment&(alignment-1U))!=0 || address>UINTPTR_MAX-0x3B-alignment) {return false;}
    const auto body=(address+0x3B+alignment)&~static_cast<std::uintptr_t>(alignment-1U);
    return read.copy(body,payload);
}
__declspec(noinline) void observe_candidate(void* instance,std::uint32_t event,
    const hooking::CallGate::Scope& call) noexcept {
    const auto nav=selected_context();
    if(!nav.enabled || nav.run==0) {return;}
    Read read;std::array<std::byte,0xC4> character{};Actor actorState;Source sourceState;
    std::uintptr_t linked{},characterAddress{};
    const auto address=reinterpret_cast<std::uintptr_t>(instance);
    unsigned rejection{};
    if(!read.copy(address,character)) {rejection=1;}
    else if(at<std::uint32_t>(character.data()+4)!=0x80806832U) {rejection=2;}
    else if(!actor(read,at<std::uint32_t>(character.data()+0xC0),actorState)) {rejection=3;}
    else if(!read.resolve({at<std::uint32_t>(character.data()+0x24),0,0},characterAddress)) {rejection=4;}
    else if(characterAddress!=address) {rejection=5;}
    else if(!read.resolve(actorState.source,linked)) {rejection=6;}
    else if(!source(read,linked,sourceState,nav.gateway,nav.trial,nav.deep,nav.strike,nav.hijacked,nav.garden,nav.launchpad,nav.oneAu,nav.homecoming,nav.adieu)) {rejection=7;}
    std::array<std::byte,0x3C> eventHeader{};std::array<std::byte,0x38> payload{};
    std::uint32_t eventDefinition{};bool eventValid=false,healthValid=false,deathAccepted=false;
    Ref healthRef{};std::uintptr_t healthAddress{},memberAddress{};
    std::array<std::byte,0x340> health{};std::array<std::byte,0x194> member{};bool memberValid=false;
    std::uint8_t deathReject{};
    if(rejection==0) {
        eventValid=event_payload(read,event,eventHeader,payload,eventDefinition);
        healthValid=omega_enemy_native_health::owner(actorState.entity,
                at<std::uint32_t>(character.data()+0x2C))
            && address<=UINTPTR_MAX-0x2E8
            && read.value(address+0x2E8,healthRef)
            && healthRef.handle!=UINT32_MAX && healthRef.kind==0x80804BEEU && healthRef.offset==0
            && read.resolve(healthRef,healthAddress) && read.copy(healthAddress,health)
            && omega_enemy_native_health::identity(healthRef.handle,healthRef.kind,healthRef.offset,
                at<std::uint32_t>(health.data()+4),at<std::uint32_t>(health.data()+0x24),
                actorState.entity,at<std::uint32_t>(health.data()+0x2C));
        memberValid=actorState.member.handle!=UINT32_MAX && read.resolve(actorState.member,memberAddress)
            && read.copy(memberAddress,member);
        // Deliver once through the encounter state lock, before optional logs.
        // No native reads or calls execute while that lock is held. The state
        // validates the complete original admission, run and generation again.
        const auto healthFlags=at<std::uint8_t>(health.data()+0x338);
        const bool qualified=omega_enemy_native_health::death(eventValid,eventDefinition,healthValid,
            healthFlags,sourceState.generation,sourceState.senseGeneration);
        if(qualified && call.accepts_side_effects()) {
            deathAccepted=nav.adieu?adieu::observe_death({nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry}):nav.homecoming?hc::observe_death({nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry}):nav.oneAu?oneau::observe_death({nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry}):nav.launchpad?launchpad::observe_death(
            {nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.garden?garden::observe_death(
                {nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.hijacked?hijacked::observe_death(
                {nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.deep?deep::observe_death(
                {nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.strike?strike::observe_death(
                {nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
                :nav.trial?trial::observe_death(
                {nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
                :nav.gateway?gateway::observe_death(
                {nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
                :state::activity::omega_first_lair::observe_death(
                {nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry});
            if(!deathAccepted) {deathReject=kRejectStateDeath;}
        } else if(qualified) {deathReject=kRejectGateClosed;}
        else {
            // Name the first failing qualification exactly as death() orders them.
            using omega_enemy_native_health::DeathRejection;
            switch(omega_enemy_native_health::death_rejection(eventValid,eventDefinition,healthValid,
                healthFlags,sourceState.generation,sourceState.senseGeneration)) {
            case DeathRejection::eventInvalid: deathReject=kRejectEventInvalid;break;
            case DeathRejection::eventClass: deathReject=kRejectEventClass;break;
            case DeathRejection::healthInvalid: deathReject=kRejectHealthInvalid;break;
            case DeathRejection::deathBitUnset: deathReject=kRejectDeathBitUnset;break;
            case DeathRejection::generation: deathReject=kRejectGeneration;break;
            case DeathRejection::none: break;
            }
        }
    }
    if(!TryAcquireSRWLockExclusive(&g_lock)) {return;}
    run(nav.run);
    if(rejection==0) {
        if(deathReject!=0) {
            reject(nav.run,kBoundaryDeath,deathReject,sourceState.registry,sourceState.slot,actorState.handle,
                deathReject==kRejectEventClass?eventDefinition:
                deathReject==kRejectGeneration?sourceState.generation:
                deathReject==kRejectDeathBitUnset?at<std::uint8_t>(health.data()+0x338):event,
                deathReject==kRejectGeneration?sourceState.senseGeneration:actorState.source.handle);
        }
        if(deathAccepted) {
            report("ev=omega_enemy_lair stage=death_accepted run=%llu registry=%08X source=%u actor=%08X source_ref=%08X generation=%u event=%08X health=%08X",
                static_cast<unsigned long long>(nav.run),sourceState.registry,sourceState.slot,actorState.handle,
                actorState.source.handle,sourceState.generation,event,healthRef.handle);
        }
        report("ev=omega_enemy_lair stage=candidate_event run=%llu source=%u resource=%08X actor=%08X entity=%08X character=%08X character_definition=%08X source_ref=%08X member_ref=%08X member_valid=%u member_definition=%08X member_generation=%u member_revision=%u generation=%u native_flags=%04X event=%08X event_valid=%u event_definition=%08X context=%016llX payload00=%08X payload04=%08X payload08=%08X header00=%08X header04=%08X header08=%08X copied=%zu",
            static_cast<unsigned long long>(nav.run),sourceState.slot,sourceState.resource,actorState.handle,
            actorState.entity,at<std::uint32_t>(character.data()+0x24),at<std::uint32_t>(character.data()),
            actorState.source.handle,actorState.member.handle,memberValid?1U:0U,at<std::uint32_t>(member.data()),
            at<std::uint32_t>(member.data()+0x180),at<std::uint32_t>(member.data()+0x190),sourceState.generation,
            actorState.flags,event,eventValid?1U:0U,eventDefinition,
            static_cast<unsigned long long>(at<std::uint64_t>(payload.data()+0x2C)),
            at<std::uint32_t>(payload.data()),at<std::uint32_t>(payload.data()+4),at<std::uint32_t>(payload.data()+8),
            at<std::uint32_t>(eventHeader.data()),at<std::uint32_t>(eventHeader.data()+4),
            at<std::uint32_t>(eventHeader.data()+8),read.copied);
        report("ev=omega_enemy_lair stage=candidate_health run=%llu source=%u actor=%08X event=%08X health_valid=%u health_ref=%08X health_ref_kind=%08X health_definition=%08X health_kind=%08X health_self=%08X health_entity=%08X native_320=%016llX native_328=%016llX native_330=%016llX native_338=%016llX payload_qwords=%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,%016llX copied=%zu",
            static_cast<unsigned long long>(nav.run),sourceState.slot,actorState.handle,event,healthValid?1U:0U,
            healthRef.handle,healthRef.kind,at<std::uint32_t>(health.data()),
            at<std::uint32_t>(health.data()+4),at<std::uint32_t>(health.data()+0x24),
            at<std::uint32_t>(health.data()+0x2C),
            static_cast<unsigned long long>(at<std::uint64_t>(health.data()+0x320)),
            static_cast<unsigned long long>(at<std::uint64_t>(health.data()+0x328)),
            static_cast<unsigned long long>(at<std::uint64_t>(health.data()+0x330)),
            static_cast<unsigned long long>(at<std::uint64_t>(health.data()+0x338)),
            static_cast<unsigned long long>(at<std::uint64_t>(payload.data())),
            static_cast<unsigned long long>(at<std::uint64_t>(payload.data()+8)),
            static_cast<unsigned long long>(at<std::uint64_t>(payload.data()+0x10)),
            static_cast<unsigned long long>(at<std::uint64_t>(payload.data()+0x18)),
            static_cast<unsigned long long>(at<std::uint64_t>(payload.data()+0x20)),
            static_cast<unsigned long long>(at<std::uint64_t>(payload.data()+0x28)),
            static_cast<unsigned long long>(at<std::uint64_t>(payload.data()+0x30)),read.copied);
    } else {
        // One line per rejection reason per Omega run. An unscoped event is
        // explicitly diagnostic: it is not classified as a Lair enemy or death.
        const auto flag=std::uint32_t{1}<<rejection;
        if((g_candidateRejected&flag)==0) {
            g_candidateRejected|=flag;
            report("ev=omega_enemy_lair stage=candidate_unscoped run=%llu reason=%u sub=%u instance=%p event=%08X definition=%08X kind=%08X actor_field=%08X actor=%08X actor_self=%08X character_field=%08X resolved=%p source_ref=%08X resource=%08X native_registry=%08X native_type=%u native_slot=%d expected_registry=%08X expected_slot=%u copied=%zu",
                static_cast<unsigned long long>(nav.run),rejection,
                rejection==3?actorState.reason:rejection==7?sourceState.reason:0U,instance,event,
                at<std::uint32_t>(character.data()),at<std::uint32_t>(character.data()+4),
                at<std::uint32_t>(character.data()+0xC0),actorState.handle,actorState.self,
                at<std::uint32_t>(character.data()+0x24),reinterpret_cast<void*>(characterAddress),
                actorState.source.handle,sourceState.resource,sourceState.nativeRegistry,sourceState.nativeType,
                sourceState.nativeSlot,sourceState.expectedRegistry,sourceState.expectedSlot,read.copied);
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
}
namespace nativeEvents=state::activity::native_population;
namespace pending=native_population_pending;
namespace generatedIdentity=native_generated_population_identity;
namespace generatedRoster=native_generated_roster;
std::mutex g_pendingMutex;
std::mutex g_generatedRosterCacheMutex;
generatedRoster::Cache<> g_generatedRosterCache;
std::atomic_uint64_t g_generatedRosterCacheEpoch{};
struct GeneratedRosterWorker final {
    std::uint32_t self{generatedRoster::kInvalidHandle};
    generatedRoster::ScheduleCursor cursor{};
    std::uint64_t lastSeen{};
};
SRWLOCK g_generatedRosterWorkerLock=SRWLOCK_INIT;
std::array<GeneratedRosterWorker,generatedRoster::kMaximumWorkers> g_generatedRosterWorkers{};
pending::Queue<state::activity::native_population::kProvisionalCapacity> g_pendingBirths;
pending::Queue<8192> g_admittedActors;
struct GeneratedDeathWitness final {
    std::uint32_t actor{UINT32_MAX};
    std::uint32_t entity{UINT32_MAX};
    std::uint32_t parent{UINT32_MAX};
    std::uint8_t healthFlags{};
    std::uint64_t epoch{};
};
std::array<GeneratedDeathWitness,generatedRoster::kMaximumCachedActors> g_earlyGeneratedDeaths{};
std::size_t g_earlyGeneratedDeathCount{};
std::atomic_uint g_nativeLines{};
template<class... Args> void native_report(const char* format,Args... args) noexcept {
    if(g_nativeLines.fetch_add(1,std::memory_order_relaxed)>=128) return;
    std::array<char,512> line{};const auto count=std::snprintf(line.data(),line.size(),format,args...);
    if(count>0 && static_cast<std::size_t>(count)<line.size())
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(count)});
}
#include "vendor_population.inl"
// Shared observer for explicitly registered activity sources. Package definitions,
// salted actor backlinks, typed health interfaces and source generations are
// qualified before copying an event into the state mailbox. No spawn or AI call.
struct SourceIdentity final {
    nativeEvents::Lease lease{};
    std::uint32_t sourceHandle{UINT32_MAX};
    std::uint32_t memberPrefabTag{};
    std::uint32_t completionGroup{UINT32_MAX};
};
bool cached_generated_source(const Actor& actorState,SourceIdentity& output) noexcept;
bool generated_source(Read& read,const Actor& actorState,SourceIdentity& output) noexcept {
    output={};output.completionGroup=UINT32_MAX;
    if(actorState.source.kind==generatedIdentity::kSourceRuntimeClass) {
        generatedIdentity::Identity generated{};
        if(generatedIdentity::qualify(read,g_image,actorState.source,actorState.member,generated)) {
            const auto lease=nativeEvents::lookup_generated(generated.resourceTag,generated.seed,
                generated.workerDefinitionTag,generated.workerDefinitionOffset,
                generated.paletteDefinitionTag,generated.paletteDefinitionOffset);
            if(lease.activity && lease.source.valid()) {
                output.lease=lease;output.sourceHandle=actorState.source.handle;
                output.memberPrefabTag=generated.memberPrefabTag;
                output.completionGroup=generated.completionGroup;return true;
            }
        }
        return cached_generated_source(actorState,output);
    }
    if(actorState.source.handle==UINT32_MAX && cached_generated_source(actorState,output)) return true;
    return false;
}

constexpr std::size_t kGeneratedRosterEntriesPerTick=2U;
constexpr std::size_t kGeneratedRosterCandidateAttemptsPerTick=8U;
// Each selected encounter gets its own original 8192-byte Read budget.  The
// worker and encounter-header/roster reads are separate from each candidate's
// fresh proof read.  This explicit product is the maximum observer work per
// native tick; Read's existing fixed-path budget is not changed.
constexpr std::size_t kGeneratedRosterReadBudget=kCopyLimit;
constexpr std::size_t kGeneratedRosterTickReadBudget=
    (1U+kGeneratedRosterEntriesPerTick+kGeneratedRosterCandidateAttemptsPerTick)
        *kGeneratedRosterReadBudget;
constexpr std::size_t kGeneratedRosterWorkerBytes=0x970U;
constexpr std::size_t kGeneratedRosterSourceBytes=0x150U;
constexpr std::size_t kGeneratedRosterParentBytes=0x28U;
constexpr std::size_t kGeneratedRosterEntityBytes=0x50U;
constexpr std::size_t kGeneratedRosterSceneBytes=8U;
constexpr std::size_t kGeneratedRosterArrayHeaderBytes=20U;
constexpr std::int64_t kGeneratedRosterRowsHeaderOffset=0x118LL;
constexpr std::int64_t kGeneratedRosterPaletteRowsHeaderOffset=0x114LL;
constexpr std::int64_t kGeneratedRosterPaletteFirstRowOffset=0x128LL;
constexpr std::int64_t kGeneratedRosterSourceCountOffset=0x100LL;
constexpr std::int64_t kGeneratedRosterSourceRowsOffset=0x108LL;
constexpr std::int64_t kGeneratedRosterWorkerEntryCountOffset=0x924LL;
constexpr std::int64_t kGeneratedRosterWorkerEntryCapacityOffset=0x850LL;
constexpr std::int64_t kGeneratedRosterWorkerEntriesOffset=0x858LL;
constexpr std::int64_t kGeneratedRosterWorkerEntriesBaseOffset=0x868LL;
constexpr std::int64_t kGeneratedRosterWorkerSeedOffset=generatedIdentity::kWorkerEffectiveSeedOffset;
constexpr std::int64_t kGeneratedRosterWorkerResourceOffset=0x96CL;
constexpr std::int64_t kGeneratedRosterWorkerSelfOffset=0x24LL;
constexpr std::int64_t kGeneratedRosterSourceWorkerOffset=0x148LL;
constexpr std::int64_t kGeneratedRosterSourceEntryOffset=0x14CL;
constexpr std::int64_t kGeneratedRosterSourceDefinitionOffset=0x0LL;
constexpr std::int64_t kGeneratedRosterDefinitionRuntimeOffset=0x8LL;
constexpr std::int64_t kGeneratedRosterDefinitionRowCountOffset=0x110LL;
constexpr std::int64_t kGeneratedRosterDefinitionRowsOffset=0x118LL;
constexpr std::int64_t kGeneratedRosterActorSceneOffset=0x4CL;
constexpr std::int64_t kGeneratedRosterEntitySelfOffset=0x0CL;
constexpr std::int64_t kGeneratedRosterScenePrefabOffset=0x4LL;
constexpr std::int64_t kGeneratedRosterParentSelfOffset=0x24LL;
constexpr std::int64_t kGeneratedRosterParentActorOffset=0x1470LL;
constexpr std::uint32_t kGeneratedRosterParentClass=0x808082ECU;
constexpr std::array<std::byte,16> kGeneratedRosterGetterPrefix{
    std::byte{0x48},std::byte{0x89},std::byte{0x5C},std::byte{0x24},std::byte{0x08},
    std::byte{0x48},std::byte{0x89},std::byte{0x6C},std::byte{0x24},std::byte{0x10},
    std::byte{0x48},std::byte{0x89},std::byte{0x74},std::byte{0x24},std::byte{0x18},
    std::byte{0x57}};

void sync_generated_roster_cache(std::uint64_t epochValue) noexcept {
    if(g_generatedRosterCacheEpoch.load(std::memory_order_acquire)==epochValue) return;
    std::array<generatedRoster::Provenance,generatedRoster::kMaximumCachedActors> snapshot{};
    std::size_t count{};
    {
        std::lock_guard lock(g_generatedRosterCacheMutex);
        count=g_generatedRosterCache.snapshot(snapshot);
    }
    // Do not call the mailbox while holding the cache lock.  Active
    // provenance survives unrelated epoch changes; only released leases are
    // pruned, so a roster absence can never become a death.
    for(std::size_t i=0;i<count;++i) {
        if(nativeEvents::has_lease(snapshot[i].lease)) continue;
        std::lock_guard lock(g_generatedRosterCacheMutex);
        g_generatedRosterCache.erase(snapshot[i].actorHandle,snapshot[i].entityHandle,
            snapshot[i].parentHandle,snapshot[i].lease);
    }
    g_generatedRosterCacheEpoch.store(epochValue,std::memory_order_release);
}

bool cached_generated_source(const Actor& actorState,SourceIdentity& output) noexcept {
    const auto epochValue=nativeEvents::generator_epoch();
    sync_generated_roster_cache(epochValue);
    generatedRoster::Provenance provenance{};
    {
        std::lock_guard lock(g_generatedRosterCacheMutex);
        if(!g_generatedRosterCache.find(actorState.handle,actorState.entity,actorState.parent,provenance)) return false;
    }
    if(!nativeEvents::has_lease(provenance.lease)) return false;
    output.lease=provenance.lease;output.sourceHandle=provenance.sourceHandle;
    output.memberPrefabTag=provenance.memberPrefabTag;output.completionGroup=provenance.completionGroup;
    return true;
}

// Caller owns g_pendingMutex.  This is only a retained native health witness;
// it has no lease and cannot publish a death until roster admission supplies
// the authoritative generated lease.
void discard_early_generated_death(std::uint32_t actor,std::uint32_t entity,
    std::uint32_t parent) noexcept {
    for(std::size_t i=0;i<g_earlyGeneratedDeathCount;) {
        const auto& witness=g_earlyGeneratedDeaths[i];
        if(witness.actor==actor && witness.entity==entity && witness.parent==parent)
            g_earlyGeneratedDeaths[i]=g_earlyGeneratedDeaths[--g_earlyGeneratedDeathCount];
        else ++i;
    }
}

void retain_early_generated_death(std::uint32_t actor,std::uint32_t entity,
    std::uint32_t parent,std::uint8_t healthFlags,std::uint64_t epochValue) noexcept {
    if(!epochValue) return;
    for(std::size_t i=0;i<g_earlyGeneratedDeathCount;++i) {
        const auto& witness=g_earlyGeneratedDeaths[i];
        if(witness.actor==actor && witness.entity==entity && witness.parent==parent) return;
    }
    if(g_earlyGeneratedDeathCount==g_earlyGeneratedDeaths.size()) {
        for(std::size_t i=1;i<g_earlyGeneratedDeathCount;++i)
            g_earlyGeneratedDeaths[i-1]=g_earlyGeneratedDeaths[i];
        --g_earlyGeneratedDeathCount;
    }
    g_earlyGeneratedDeaths[g_earlyGeneratedDeathCount++]={actor,entity,parent,healthFlags,epochValue};
}

// Caller owns g_pendingMutex.  Admission is already queued before this
// function runs, preserving admitted-before-died ordering for an early kill.
void flush_early_generated_death(const generatedRoster::Provenance& provenance,
    std::uint64_t epochValue) noexcept {
    const auto currentEpoch=nativeEvents::generator_epoch();
    for(std::size_t i=0;i<g_earlyGeneratedDeathCount;) {
        const auto& witness=g_earlyGeneratedDeaths[i];
        if(!generatedRoster::current_early_death_epoch(witness.epoch,epochValue,currentEpoch)) {
            g_earlyGeneratedDeaths[i]=g_earlyGeneratedDeaths[--g_earlyGeneratedDeathCount];
            continue;
        }
        if(witness.actor!=provenance.actorHandle || witness.entity!=provenance.entityHandle
            || witness.parent!=provenance.parentHandle) { ++i;continue; }
        const bool healthDeath=omega_enemy_native_health::death(true,0x80804C54U,true,
            witness.healthFlags,provenance.lease.source.generation,
            provenance.lease.source.generation);
        if(!healthDeath) { ++i;continue; }
        nativeEvents::Event death{};death.lease=provenance.lease;
        death.actor={provenance.lease.source,provenance.actorHandle,provenance.entityHandle};
        death.sourceHandle=provenance.sourceHandle;death.kind=nativeEvents::Kind::died;
        death.memberPrefabTag=provenance.memberPrefabTag;
        death.completionGroup=provenance.completionGroup;
        const bool accepted=nativeEvents::submit(death,epochValue);
        if(!accepted && nativeEvents::has_lease(death.lease)) nativeEvents::observation_lost();
        g_earlyGeneratedDeaths[i]=g_earlyGeneratedDeaths[--g_earlyGeneratedDeathCount];
    }
}

bool encounter_source_reference(Read& read,std::uint32_t encounterHandle,Ref& output) noexcept {
    output={};
    std::array<std::byte,kGeneratedRosterGetterPrefix.size()> prefix{};
    if(!read.copy(g_image+0x4F0290,prefix) || prefix!=kGeneratedRosterGetterPrefix) return false;
    using Getter=std::uint8_t(__fastcall*)(std::uint32_t,Ref*) noexcept;
    __try {
        if(reinterpret_cast<Getter>(g_image+0x4F0290)(encounterHandle,&output)==0) return false;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    // 4F0290 returns the source definition reference.  Its resolved object
    // carries the runtime source header checked by the caller.
    return generatedRoster::valid_getter_reference(output);
}

bool generated_worker(Read& read,std::uintptr_t workerAddress,generatedRoster::WorkerTuple& tuple,
    std::uint32_t& workerSelf,std::int32_t& entryCount,std::uintptr_t& entriesAddress,
    std::array<std::byte,kGeneratedRosterWorkerBytes>& workerBytes) noexcept {
    if(workerAddress<0x10000 || workerAddress>UINTPTR_MAX-kGeneratedRosterWorkerBytes
        || !read.copy(workerAddress,workerBytes)
        || generatedIdentity::field<std::uint32_t>(workerBytes,4)!=generatedIdentity::kWorkerRuntimeClass) return false;
    const auto workerDefinition=generatedIdentity::field<Ref>(workerBytes,0);
    workerSelf=generatedIdentity::field<std::uint32_t>(workerBytes,kGeneratedRosterWorkerSelfOffset);
    const auto seed=generatedIdentity::field<std::uint32_t>(workerBytes,kGeneratedRosterWorkerSeedOffset);
    const auto resourceTag=generatedIdentity::field<std::uint32_t>(workerBytes,kGeneratedRosterWorkerResourceOffset);
    const auto entryCapacity=generatedIdentity::field<std::int32_t>(workerBytes,kGeneratedRosterWorkerEntryCapacityOffset);
    entryCount=generatedIdentity::field<std::int32_t>(workerBytes,kGeneratedRosterWorkerEntryCountOffset);
    if(workerSelf==generatedRoster::kInvalidHandle || workerDefinition.handle==generatedRoster::kInvalidHandle
        || workerDefinition.kind!=generatedIdentity::kWorkerRuntimeClass || workerDefinition.offset<=0
        || workerDefinition.offset>=generatedIdentity::kMaximumResourceOffset || (workerDefinition.offset&3)!=0
        || !seed || seed==generatedRoster::kInvalidHandle || !resourceTag || resourceTag==generatedRoster::kInvalidHandle
        || entryCapacity<1 || entryCapacity>4096 || entryCount<0 || entryCount>entryCapacity) return false;
    std::uintptr_t workerDefinitionAddress{};
    if(!read.resolve(workerDefinition,workerDefinitionAddress)
        || workerDefinitionAddress>UINTPTR_MAX-0x28U) return false;
    std::array<std::byte,kGeneratedRosterParentBytes> definitionHeader{};
    if(!read.copy(workerDefinitionAddress,definitionHeader)
        || generatedIdentity::field<std::uint32_t>(definitionHeader,0)!=workerDefinition.handle
        || generatedIdentity::field<std::uint32_t>(definitionHeader,4)!=generatedIdentity::kWorkerReferenceClass) return false;
    const auto entriesRelative=generatedIdentity::field<std::int64_t>(workerBytes,kGeneratedRosterWorkerEntriesOffset);
    return add(workerAddress,entriesRelative,entriesAddress)
        && add(entriesAddress,kGeneratedRosterWorkerEntriesBaseOffset,entriesAddress)
        && (tuple={resourceTag,seed,workerDefinition.handle,static_cast<std::uint32_t>(workerDefinition.offset)},true);
}

bool generated_palette_member(Read& read,
    const std::array<std::byte,kGeneratedRosterSourceBytes>& source,std::int32_t rowIndex,
    std::uint32_t& paletteTag,std::uint32_t& paletteOffset,std::uint32_t& rowCount,
    std::uint32_t& memberPrefabTag,std::uint32_t& completionGroup) noexcept {
    const auto paletteDefinition=generatedIdentity::field<Ref>(source,0);
    const auto sourceCount=generatedIdentity::field<std::int64_t>(source,0xC0);
    const auto sourceRowsRelative=generatedIdentity::field<std::int64_t>(source,0xC8);
    if(paletteDefinition.handle==generatedRoster::kInvalidHandle
        || paletteDefinition.kind!=generatedIdentity::kSourceRuntimeClass
        || paletteDefinition.offset<=0 || paletteDefinition.offset>=generatedIdentity::kMaximumResourceOffset
        || sourceCount<1 || sourceCount>512 || sourceRowsRelative<0
        || sourceRowsRelative>=generatedIdentity::kMaximumResourceOffset) return false;
    std::uintptr_t definitionAddress{};
    if(!read.resolve(paletteDefinition,definitionAddress)
        || definitionAddress>UINTPTR_MAX-0x120U) return false;
    std::array<std::byte,0x120> definition{};
    if(!read.copy(definitionAddress,definition)
        || generatedIdentity::field<std::uint32_t>(definition,0)!=paletteDefinition.handle
        || generatedIdentity::field<std::uint32_t>(definition,4)!=generatedIdentity::kSourceDefinitionClass) return false;
    const auto definitionRuntimeOffset=generatedIdentity::field<std::int64_t>(definition,kGeneratedRosterDefinitionRuntimeOffset);
    const auto authoredRowCount=generatedIdentity::field<std::int64_t>(definition,kGeneratedRosterDefinitionRowCountOffset);
    const auto actorRowsRelative=generatedIdentity::field<std::int64_t>(definition,kGeneratedRosterDefinitionRowsOffset);
    if(authoredRowCount<1 || authoredRowCount>512 || authoredRowCount!=sourceCount
        || definitionRuntimeOffset<0 || definitionRuntimeOffset>=generatedIdentity::kMaximumResourceOffset
        || actorRowsRelative<0 || actorRowsRelative>=generatedIdentity::kMaximumResourceOffset
        || rowIndex<0 || rowIndex>=authoredRowCount) return false;
    if(paletteDefinition.offset>generatedIdentity::kMaximumResourceOffset
        -kGeneratedRosterPaletteFirstRowOffset-actorRowsRelative) return false;
    std::uintptr_t arrayHeaderAddress{};
    if(!add(definitionAddress,actorRowsRelative,arrayHeaderAddress)
        || !add(arrayHeaderAddress,kGeneratedRosterPaletteRowsHeaderOffset,arrayHeaderAddress)
        || arrayHeaderAddress>UINTPTR_MAX-kGeneratedRosterArrayHeaderBytes) return false;
    std::array<std::byte,kGeneratedRosterArrayHeaderBytes> arrayHeader{};
    if(!read.copy(arrayHeaderAddress,arrayHeader)
        || generatedIdentity::field<std::uint32_t>(arrayHeader,0)!=generatedIdentity::kArrayHeaderMarker
        || generatedIdentity::field<std::uint64_t>(arrayHeader,4)!=static_cast<std::uint64_t>(authoredRowCount)
        || generatedIdentity::field<std::uint32_t>(arrayHeader,12)!=generatedIdentity::kPaletteArrayElementClass) return false;
    std::uintptr_t firstRows{};
    if(!add(arrayHeaderAddress,0x14,firstRows)
        || !add(firstRows,static_cast<std::int64_t>(rowIndex)*generatedRoster::kActorRowStride,firstRows)
        || firstRows>UINTPTR_MAX-generatedRoster::kActorRowStride) return false;
    std::array<std::byte,generatedRoster::kActorRowStride> actorRow{};
    if(!read.copy(firstRows,actorRow)) return false;
    const auto rowReference=generatedIdentity::field<Ref>(actorRow,0);
    const auto expectedRowReferenceOffset=definitionRuntimeOffset+sourceRowsRelative
        +generatedIdentity::kActorRowReferenceBaseOffset
        +static_cast<std::int64_t>(rowIndex)*generatedIdentity::kActorRowReferenceStride;
    if(expectedRowReferenceOffset<=0 || expectedRowReferenceOffset>=generatedIdentity::kMaximumResourceOffset
        || rowReference.kind!=generatedIdentity::kPaletteRowReferenceClass
        || rowReference.handle!=paletteDefinition.handle || rowReference.offset!=expectedRowReferenceOffset) return false;
    paletteTag=paletteDefinition.handle;paletteOffset=static_cast<std::uint32_t>(paletteDefinition.offset);
    rowCount=static_cast<std::uint32_t>(authoredRowCount);
    // The authored member/prefab field is the row's +0x10 value; it is only
    // copied as evidence and is never assigned to the native actor.
    memberPrefabTag=generatedIdentity::field<std::uint32_t>(actorRow,0x10);
    completionGroup=generatedIdentity::field<std::uint32_t>(actorRow,0xA8);
    return memberPrefabTag!=0 && memberPrefabTag!=generatedRoster::kInvalidHandle
        && memberPrefabTag!=generatedRoster::kInvalidNativeTag;
}

bool generated_links(Read& read,const generatedRoster::RosterRow& row,std::uint32_t actorRowCount,
    generatedRoster::LinkEvidence& evidence,Actor& actorState) noexcept {
    if(!actor(read,row.actorHandle,actorState) || actorState.entity==generatedRoster::kInvalidHandle
        || actorState.parent==generatedRoster::kInvalidHandle) return false;
    std::uintptr_t parentAddress{};
    std::array<std::byte,0x30> parent{};std::uint32_t parentActor{};
    if(!read.resolve({actorState.parent,0,0},parentAddress) || parentAddress>UINTPTR_MAX-0x1470
        || !read.copy(parentAddress,parent) || !read.value(parentAddress+kGeneratedRosterParentActorOffset,parentActor)) return false;
    std::uintptr_t entityAddress{};std::array<std::byte,kGeneratedRosterEntityBytes> entity{};
    if(!read.resolve({actorState.entity,0,0},entityAddress) || entityAddress>UINTPTR_MAX-kGeneratedRosterEntityBytes
        || !read.copy(entityAddress,entity)) return false;
    const auto sceneHandle=generatedIdentity::field<std::uint32_t>(entity,kGeneratedRosterActorSceneOffset);
    std::uintptr_t sceneAddress{};std::array<std::byte,kGeneratedRosterSceneBytes> scene{};
    if(!read.resolve({sceneHandle,0,0},sceneAddress) || !read.copy(sceneAddress,scene)) return false;
    evidence.row=row;evidence.actorRowCount=actorRowCount;evidence.actorHandle=actorState.handle;
    evidence.actorSelf=actorState.self;evidence.actorParent=actorState.parent;
    evidence.entityHandle=actorState.entity;evidence.entitySelf=generatedIdentity::field<std::uint32_t>(entity,0xC);
    evidence.parentSelf=generatedIdentity::field<std::uint32_t>(parent,0x24);
    evidence.parentEntity=generatedIdentity::field<std::uint32_t>(parent,0x2C);
    evidence.parentActor=parentActor;evidence.sceneHandle=sceneHandle;
    evidence.scenePrefab=generatedIdentity::field<std::uint32_t>(scene,4);
    return evidence.parentSelf!=generatedRoster::kInvalidHandle
        && generatedIdentity::field<std::uint32_t>(parent,4)==kGeneratedRosterParentClass
        && generatedRoster::qualifies(evidence);
}

bool generated_row_stable(Read& read,std::uintptr_t workerAddress,std::uint32_t workerSelf,
    const generatedRoster::WorkerTuple& tuple,std::uintptr_t sourceAddress,const Ref& sourceReference,
    std::uint32_t encounterHandle,std::uint32_t entryIndex,std::uint32_t rosterIndex,
    const generatedRoster::RosterRow& expectedRow,
    std::uint32_t paletteTag,std::uint32_t paletteOffset,const nativeEvents::Lease& lease) noexcept {
    std::uint32_t currentSelf{},currentSeed{},currentResource{};Ref currentDefinition{};
    if(!read.value(workerAddress+kGeneratedRosterWorkerSelfOffset,currentSelf)
        || !read.value(workerAddress+kGeneratedRosterWorkerSeedOffset,currentSeed)
        || !read.value(workerAddress+kGeneratedRosterWorkerResourceOffset,currentResource)
        || !read.copy(workerAddress,std::as_writable_bytes(std::span{&currentDefinition,std::size_t{1}}))
        || currentSelf!=workerSelf || currentSeed!=tuple.seed || currentResource!=tuple.resourceTag
        || currentDefinition.handle!=tuple.workerDefinitionTag
        || currentDefinition.offset!=static_cast<std::int64_t>(tuple.workerDefinitionOffset)) return false;
    std::array<std::byte,kGeneratedRosterSourceBytes> source{};
    if(!read.copy(sourceAddress,source) || !generatedRoster::valid_runtime_source_header(generatedIdentity::field<std::uint32_t>(source,4))
        || generatedIdentity::field<std::uint32_t>(source,kGeneratedRosterSourceWorkerOffset)!=workerSelf
        || generatedIdentity::field<std::int32_t>(source,kGeneratedRosterSourceEntryOffset)!=static_cast<std::int32_t>(entryIndex)) return false;
    Ref currentSource{};
    if(!encounter_source_reference(read,encounterHandle,currentSource)
        || currentSource.handle!=sourceReference.handle || currentSource.kind!=sourceReference.kind
        || currentSource.offset!=sourceReference.offset) return false;
    const auto slots=generatedIdentity::field<std::int32_t>(source,kGeneratedRosterSourceCountOffset);
    const auto rowsRelative=generatedIdentity::field<std::int64_t>(source,kGeneratedRosterSourceRowsOffset);
    if(slots<1 || slots>static_cast<std::int32_t>(generatedRoster::kMaximumRosterSlots)
        || expectedRow.authoredActorRow<0 || rowsRelative<0) return false;
    std::uintptr_t rowAddress{};
    if(!add(sourceAddress,rowsRelative+kGeneratedRosterRowsHeaderOffset,rowAddress)
        || !add(rowAddress,static_cast<std::int64_t>(rosterIndex)*generatedRoster::kRosterRowStride,rowAddress)) return false;
    std::array<std::byte,generatedRoster::kRosterRowStride> row{};
    if(!read.copy(rowAddress,row)) return false;
    const generatedRoster::RosterRow current{generatedIdentity::field<std::uint32_t>(row,0),
        generatedIdentity::field<std::uint32_t>(row,4),generatedIdentity::field<std::int32_t>(row,8)};
    if(current!=expectedRow) return false;
    const auto currentLease=nativeEvents::lookup_generated(tuple.resourceTag,tuple.seed,tuple.workerDefinitionTag,
        tuple.workerDefinitionOffset,paletteTag,paletteOffset);
    return currentLease==lease && nativeEvents::has_lease(lease);
}

struct GeneratedRosterCandidate final { generatedRoster::Provenance provenance{}; };
constexpr std::size_t kGeneratedRosterCandidateCapacity=
    generatedRoster::kMaximumRosterSlots*kGeneratedRosterEntriesPerTick;

GeneratedRosterWorker& select_generated_roster_worker(std::uint32_t self,std::uint64_t now) noexcept {
    GeneratedRosterWorker* selected{};
    for(auto& worker:g_generatedRosterWorkers) if(worker.self==self) { selected=&worker;break; }
    if(!selected) {
        for(auto& worker:g_generatedRosterWorkers) if(worker.self==generatedRoster::kInvalidHandle) { selected=&worker;break; }
    }
    if(!selected) {
        selected=&g_generatedRosterWorkers.front();
        for(auto& worker:g_generatedRosterWorkers) if(worker.lastSeen<selected->lastSeen) selected=&worker;
        *selected={};
    }
    if(selected->self!=self) { *selected={};selected->self=self; }
    selected->lastSeen=now;return *selected;
}

bool cached_generated_roster_row(const generatedRoster::RosterRow& row,
    const Ref& sourceReference,std::uint32_t workerSelf,std::uint32_t entryIndex,
    std::uint64_t epochValue,generatedRoster::Provenance& provenance) noexcept {
    sync_generated_roster_cache(epochValue);
    {
        std::lock_guard lock(g_generatedRosterCacheMutex);
        if(!g_generatedRosterCache.find_matching(row.actorHandle,row.nativeSpawnId,
            static_cast<std::uint32_t>(row.authoredActorRow),sourceReference.handle,
            sourceReference.kind,sourceReference.offset,workerSelf,entryIndex,provenance)) return false;
    }
    return nativeEvents::has_lease(provenance.lease);
}

std::size_t sample_generated_roster(void* workerInstance,std::uint64_t epochValue,
    std::array<GeneratedRosterCandidate,kGeneratedRosterCandidateCapacity>& output) noexcept {
    if(!workerInstance || !epochValue) return 0;
    if(!TryAcquireSRWLockExclusive(&g_generatedRosterWorkerLock)) return 0;
    Read workerRead;std::array<std::byte,kGeneratedRosterWorkerBytes> workerBytes{};
    generatedRoster::WorkerTuple tuple{};std::uint32_t workerSelf{};std::int32_t entryCount{};std::uintptr_t entries{};
    const auto workerAddress=reinterpret_cast<std::uintptr_t>(workerInstance);
    if(!generated_worker(workerRead,workerAddress,tuple,workerSelf,entryCount,entries,workerBytes)
        || !nativeEvents::has_generator(tuple.resourceTag,tuple.seed,tuple.workerDefinitionTag,tuple.workerDefinitionOffset)) {
        ReleaseSRWLockExclusive(&g_generatedRosterWorkerLock);return 0;
    }
    auto& workerState=select_generated_roster_worker(workerSelf,GetTickCount64());
    workerState.cursor.synchronize(workerSelf,tuple);
    std::size_t found{};
    std::size_t qualificationAttempts{};
    for(std::size_t visited=0;visited<kGeneratedRosterEntriesPerTick && entryCount>0;++visited) {
        const auto entryIndex=workerState.cursor.select_entry(static_cast<std::uint32_t>(entryCount));
        if(entryIndex==generatedRoster::kInvalidHandle) continue;
        // Keep the one worker read above separate from each selected
        // encounter's header/source/full bounded roster read.
        Read encounterRead;
        std::uintptr_t entryAddress{};
        if(!add(entries,static_cast<std::int64_t>(entryIndex)*generatedRoster::kEntryStride,entryAddress)) continue;
        std::array<std::byte,generatedRoster::kEntryStride> entry{};
        if(!encounterRead.copy(entryAddress,entry) || generatedIdentity::field<std::uint8_t>(entry,0x18)!=0) continue;
        const auto encounterHandle=generatedIdentity::field<std::uint32_t>(entry,0x34);
        if(encounterHandle==generatedRoster::kInvalidHandle) continue;
        Ref sourceReference{};std::uintptr_t sourceAddress{};
        if(!encounter_source_reference(encounterRead,encounterHandle,sourceReference)
            || !encounterRead.resolve(sourceReference,sourceAddress) || sourceAddress>UINTPTR_MAX-kGeneratedRosterSourceBytes) continue;
        std::array<std::byte,kGeneratedRosterSourceBytes> source{};
        if(!encounterRead.copy(sourceAddress,source)
            || !generatedRoster::valid_runtime_source_header(generatedIdentity::field<std::uint32_t>(source,4))
            || generatedIdentity::field<std::uint32_t>(source,kGeneratedRosterSourceWorkerOffset)!=workerSelf
            || generatedIdentity::field<std::int32_t>(source,kGeneratedRosterSourceEntryOffset)!=static_cast<std::int32_t>(entryIndex)) continue;
        const auto slots=generatedIdentity::field<std::int32_t>(source,kGeneratedRosterSourceCountOffset);
        const auto rowsRelative=generatedIdentity::field<std::int64_t>(source,kGeneratedRosterSourceRowsOffset);
        if(slots<1 || slots>static_cast<std::int32_t>(generatedRoster::kMaximumRosterSlots) || rowsRelative<0) continue;
        std::uintptr_t firstRowAddress{};
        constexpr auto rosterBytes=generatedRoster::kMaximumRosterSlots*generatedRoster::kRosterRowStride;
        if(!add(sourceAddress,rowsRelative+kGeneratedRosterRowsHeaderOffset,firstRowAddress)
            || firstRowAddress>UINTPTR_MAX-rosterBytes) continue;
        std::array<std::byte,rosterBytes> roster{};
        if(!encounterRead.copy(firstRowAddress,roster)) continue;
        const auto rowCount=static_cast<std::uint32_t>(slots);
        const auto initialRow=workerState.cursor.row_cursor(entryIndex,rowCount);
        if(initialRow==generatedRoster::kInvalidHandle) continue;
        for(std::uint32_t rowVisits=0;rowVisits<rowCount;++rowVisits) {
            const auto rosterIndex=workerState.cursor.row_cursor(entryIndex,rowCount);
            if(rosterIndex==generatedRoster::kInvalidHandle) break;
            std::array<std::byte,generatedRoster::kRosterRowStride> rowBytes{};
            std::memcpy(rowBytes.data(),roster.data()+static_cast<std::size_t>(rosterIndex)*generatedRoster::kRosterRowStride,
                rowBytes.size());
            const generatedRoster::RosterRow row{generatedIdentity::field<std::uint32_t>(rowBytes,0),
                generatedIdentity::field<std::uint32_t>(rowBytes,4),generatedIdentity::field<std::int32_t>(rowBytes,8)};
            if(!generatedRoster::valid_row(row,generatedRoster::kMaximumAuthoredRows)) {
                workerState.cursor.advance_row(entryIndex,rowCount);continue;
            }
            generatedRoster::Provenance cached{};
            if(cached_generated_roster_row(row,sourceReference,workerSelf,entryIndex,epochValue,cached)) {
                // A cache hit is only a skip. It does not re-emit admission or
                // flush a death witness; the original admission already did so.
                workerState.cursor.advance_row(entryIndex,rowCount);continue;
            }
            if(qualificationAttempts>=kGeneratedRosterCandidateAttemptsPerTick) break;
            ++qualificationAttempts; // Failed proof attempts consume budget too.
            Read candidateRead;
            std::uint32_t paletteTag{},paletteOffset{},actorRowCount{},memberPrefab{},completionGroup{};
            if(!generated_palette_member(candidateRead,source,row.authoredActorRow,paletteTag,paletteOffset,actorRowCount,memberPrefab,completionGroup)) {
                workerState.cursor.advance_row(entryIndex,static_cast<std::uint32_t>(slots));continue;
            }
            const auto lease=nativeEvents::lookup_generated(tuple.resourceTag,tuple.seed,tuple.workerDefinitionTag,
                tuple.workerDefinitionOffset,paletteTag,paletteOffset);
            if(!lease.activity || !lease.source.valid()) {
                workerState.cursor.advance_row(entryIndex,static_cast<std::uint32_t>(slots));continue;
            }
            generatedRoster::LinkEvidence evidence{};evidence.worker=tuple;evidence.expectedWorker=tuple;
            evidence.workerSelf=workerSelf;evidence.expectedWorkerSelf=workerSelf;evidence.entryIndex=entryIndex;
            evidence.expectedEntryIndex=entryIndex;evidence.authoredPrefab=memberPrefab;
            Actor actorState{};
            if(!generated_links(candidateRead,row,actorRowCount,evidence,actorState)
                || !generated_row_stable(candidateRead,workerAddress,workerSelf,tuple,sourceAddress,sourceReference,encounterHandle,
                    entryIndex,rosterIndex,row,paletteTag,paletteOffset,lease)) {
                workerState.cursor.advance_row(entryIndex,static_cast<std::uint32_t>(slots));continue;
            }
            // Repeat actor/entity/parent/scene proof after the stability read.
            if(!generated_links(candidateRead,row,actorRowCount,evidence,actorState)
                || !generated_row_stable(candidateRead,workerAddress,workerSelf,tuple,sourceAddress,sourceReference,encounterHandle,
                    entryIndex,rosterIndex,row,paletteTag,paletteOffset,lease)) {
                workerState.cursor.advance_row(entryIndex,static_cast<std::uint32_t>(slots));continue;
            }
            if(found>=output.size()) continue;
            auto& candidate=output[found++].provenance;candidate={};
            candidate.lease=lease;candidate.sourceHandle=sourceReference.handle;
            candidate.sourceKind=sourceReference.kind;candidate.sourceOffset=sourceReference.offset;
            candidate.actorHandle=actorState.handle;candidate.entityHandle=actorState.entity;
            candidate.parentHandle=actorState.parent;candidate.workerSelf=workerSelf;
            candidate.entryIndex=entryIndex;candidate.rosterRowIndex=static_cast<std::uint32_t>(rosterIndex);
            candidate.authoredActorRow=static_cast<std::uint32_t>(row.authoredActorRow);
            candidate.nativeSpawnId=row.nativeSpawnId;candidate.memberPrefabTag=memberPrefab;
            candidate.completionGroup=completionGroup;
            workerState.cursor.advance_row(entryIndex,static_cast<std::uint32_t>(slots));
        }
    }
    ReleaseSRWLockExclusive(&g_generatedRosterWorkerLock);return found;
}

void emit_generated_roster_admissions(std::span<const GeneratedRosterCandidate> candidates,
    std::uint64_t epochValue) noexcept {
    for(const auto& candidate:candidates) {
        const auto& provenance=candidate.provenance;
        if(epochValue!=nativeEvents::generator_epoch()) continue;
        sync_generated_roster_cache(epochValue);
        std::lock_guard pendingLock(g_pendingMutex);
        if(epochValue!=nativeEvents::generator_epoch()) continue;
        generatedRoster::CacheIntake intake{};
        {
            std::lock_guard lock(g_generatedRosterCacheMutex);
            intake=g_generatedRosterCache.observe(provenance);
        }
        if(intake==generatedRoster::CacheIntake::duplicate) {
            flush_early_generated_death(provenance,epochValue);
            continue;
        }
        if(intake!=generatedRoster::CacheIntake::accepted) {
            if(intake==generatedRoster::CacheIntake::conflict || intake==generatedRoster::CacheIntake::overflow)
                nativeEvents::observation_lost();
            continue;
        }
        const nativeEvents::Event event{provenance.lease,
            {provenance.lease.source,provenance.actorHandle,provenance.entityHandle},provenance.sourceHandle,
            nativeEvents::Kind::admitted,provenance.memberPrefabTag,provenance.completionGroup};
        if(!nativeEvents::submit(event,epochValue)) {
            std::lock_guard lock(g_generatedRosterCacheMutex);
            g_generatedRosterCache.erase(provenance.actorHandle,provenance.entityHandle,provenance.parentHandle,provenance.lease);
            if(epochValue==nativeEvents::generator_epoch()) nativeEvents::observation_lost();
            continue;
        }
        const auto retained=g_admittedActors.add({event,provenance.parentHandle,nativeEvents::capture(event.lease)});
        if(retained!=pending::Intake::accepted && retained!=pending::Intake::duplicate) nativeEvents::observation_lost();
        flush_early_generated_death(provenance,epochValue);
        observe_vance_contact_admission(event,provenance.parentHandle);
        native_report("ev=native_population_capture stage=generated_roster_admitted actor=%08X entity=%08X parent=%08X source=%08X row=%u spawn=%08X",
            provenance.actorHandle,provenance.entityHandle,provenance.parentHandle,provenance.sourceHandle,
            provenance.authoredActorRow,provenance.nativeSpawnId);
    }
}
void observe_native_generated_population_impl(void* worker) noexcept {
    const auto epochValue=nativeEvents::generator_epoch();
    static_assert(kGeneratedRosterTickReadBudget==(1U+kGeneratedRosterEntriesPerTick
        +kGeneratedRosterCandidateAttemptsPerTick)*kGeneratedRosterReadBudget);
    std::array<GeneratedRosterCandidate,kGeneratedRosterCandidateCapacity> candidates{};
    const auto count=sample_generated_roster(worker,epochValue,candidates);
    if(count && epochValue==nativeEvents::generator_epoch())
        emit_generated_roster_admissions(std::span{candidates}.first(count),epochValue);
}
void retire_generated_roster(std::uint32_t actor,std::uint32_t entity,std::uint32_t parent,
    const nativeEvents::Lease& lease) noexcept {
    const auto epochValue=nativeEvents::generator_epoch();
    {
        std::lock_guard lock(g_pendingMutex);
        discard_early_generated_death(actor,entity,parent);
    }
    sync_generated_roster_cache(epochValue);
    std::lock_guard lock(g_generatedRosterCacheMutex);
    g_generatedRosterCache.erase(actor,entity,parent,lease);
}
bool registered_source(Read& read,const Actor& actorState,nativeEvents::Receipt& receipt) noexcept {
    std::uintptr_t address{},definitionAddress{};Ref definition{};std::uint32_t marker{};
    if(actorState.source.kind!=0x80809A3BU || actorState.source.offset!=0
        || !read.resolve(actorState.source,address) || !read.value(address,definition)
        || definition.kind!=0x8080948FU || definition.offset<4 || definition.offset>0x100000
        || !read.resolve(definition,definitionAddress) || definitionAddress>UINTPTR_MAX-0x30
        || !read.value(definitionAddress-4,marker) || !pending::definition(definition.kind,definition.offset,marker)
        || address>UINTPTR_MAX-0x244) return false;
    std::array<std::byte,8> identity{};std::uint32_t generation{},sense{};
    if(!read.copy(definitionAddress+0x30,identity) || at<std::uint8_t>(identity.data()+4)!=1
        || !read.value(address+0x1FC,generation) || !read.value(address+0x244,sense)
        || !generation || generation!=sense) return false;
    const auto slot=at<std::int16_t>(identity.data()+6);if(slot<0) return false;
    receipt=nativeEvents::capture(definition.handle,at<std::uint32_t>(identity.data()),static_cast<std::uint16_t>(slot),generation);
    return receipt && receipt.lease.activity && receipt.lease.source.valid();
}
namespace streaming {
void remember(const pending::Birth&) noexcept;
void retired(const nativeEvents::Event&) noexcept;
}
void observe_native_admission(std::uint32_t parent,nativeEvents::Creation creation) noexcept {
    if(!creation || parent==UINT32_MAX) {nativeEvents::cancel(creation);return;}
    Read read;std::uintptr_t address{},again{};std::array<std::byte,0x28> header{};
    std::uint32_t handle{UINT32_MAX};Actor actorState;nativeEvents::Lease lease;
    if(!read.resolve({parent,0,0},address) || address>UINTPTR_MAX-0x1470
        || !read.copy(address,header) || at<std::uint32_t>(header.data()+4)!=0x808082ECU
        || at<std::uint32_t>(header.data()+0x24)!=parent || !read.value(address+0x1470,handle)
        || !actor(read,handle,actorState) || actorState.parent!=parent
        || !read.resolve({actorState.parent,0,0},again) || again!=address) {
        native_report("ev=native_population_capture stage=created result=identity_rejected parent=%08X actor=%08X",parent,handle);
        nativeEvents::cancel(creation);return;
    }
    nativeEvents::Receipt sourceReceipt;
    if(!registered_source(read,actorState,sourceReceipt)) {
        native_report("ev=native_population_capture stage=created result=source_rejected parent=%08X actor=%08X source=%08X entity=%08X",
            parent,handle,actorState.source.handle,actorState.entity);
        // A streamed copy can finish construction without any source link. Keep
        // a bounded native call path so restoration is distinguishable from a
        // new population request; observation never changes construction.
        static std::atomic_uint sourceLessTraces{};
        if(actorState.source.handle==UINT32_MAX
            && sourceLessTraces.fetch_add(1,std::memory_order_relaxed)<12) {
            std::array<void*,24> frames{};
            const auto depth=RtlCaptureStackBackTrace(0,static_cast<ULONG>(frames.size()),frames.data(),nullptr);
            std::array<char,256> path{};std::size_t used{};
            for(USHORT i=0;i<depth;++i) {
                const auto frame=reinterpret_cast<std::uintptr_t>(frames[i]);
                if(frame<g_image || frame-g_image>=0x1C00000U)continue;
                const auto written=std::snprintf(path.data()+used,path.size()-used,"%s%llX",
                    used?",":"",static_cast<unsigned long long>(frame-g_image));
                if(written<=0 || static_cast<std::size_t>(written)>=path.size()-used)break;
                used+=static_cast<std::size_t>(written);
            }
            native_report("ev=native_population_capture stage=source_less_path parent=%08X definition=%08X actor=%08X flags=%04X native_rvas=%s",
                parent,at<std::uint32_t>(header.data()),handle,actorState.flags,path.data());
        }
        nativeEvents::cancel(creation);return;
    }
    lease=sourceReceipt.lease;
    std::lock_guard lock(g_pendingMutex);
    const nativeEvents::Event provisional{lease,{lease.source,actorState.handle,actorState.entity,creation.nonce},
        actorState.source.handle,nativeEvents::Kind::admitted};
    nativeEvents::Receipt receipt;
    const auto staged=nativeEvents::stage(creation,provisional,receipt);
    if(staged==nativeEvents::StageResult::ended) {
        native_report("ev=native_population_capture stage=created result=lifetime_ended actor=%08X",actorState.handle);
        return;
    }
    if(staged!=nativeEvents::StageResult::staged) {nativeEvents::observation_lost();return;}
    const auto result=g_pendingBirths.add({provisional,parent,receipt});
    if(result!=pending::Intake::accepted && result!=pending::Intake::duplicate) nativeEvents::observation_lost();
    native_report("ev=native_population_capture stage=created registry=%08X slot=%u actor=%08X entity=%08X parent=%08X result=%u",
        lease.source.source.registry,lease.source.source.slot,actorState.handle,actorState.entity,parent,static_cast<unsigned>(result));
}
// Caller owns g_pendingMutex. This only completes previously witnessed births;
// it never discovers actors by scanning the world or invents a creation event.
void finish_native_admissions(std::uint32_t onlyActor=UINT32_MAX) noexcept {
    for(std::size_t i=0;i<g_pendingBirths.size();) {
        const auto birth=g_pendingBirths[i];const auto& expected=birth.event;
        if(onlyActor!=UINT32_MAX && expected.actor.actor!=onlyActor) {++i;continue;}
        if(!nativeEvents::provisional(birth.receipt,expected)) {
            g_pendingBirths.erase(i);continue; // The authoritative owner was released.
        }
        Read read;Actor current;nativeEvents::Receipt currentReceipt;std::uintptr_t address{};
        std::array<std::byte,0x30> header{};std::uint32_t parentActor{UINT32_MAX};
        if(!actor(read,expected.actor.actor,current) || current.parent!=birth.parent
            || current.source.handle!=expected.sourceHandle || !read.resolve({birth.parent,0,0},address)
            || address>UINTPTR_MAX-0x1470 || !read.copy(address,header)
            || !read.value(address+0x1470,parentActor)
            || !omega_enemy_native_admission::identity(birth.parent,at<std::uint32_t>(header.data()+4),
                at<std::uint32_t>(header.data()+0x24),parentActor,current.handle,current.parent)
            || !registered_source(read,current,currentReceipt) || currentReceipt!=birth.receipt) {
            nativeEvents::observation_lost();++i;
            native_report("ev=native_population_capture stage=attachment result=identity_lost actor=%08X",expected.actor.actor);
            continue;
        }
        if(current.entity==UINT32_MAX || at<std::uint32_t>(header.data()+0x2C)==UINT32_MAX) {++i;continue;}
        if(current.entity!=at<std::uint32_t>(header.data()+0x2C)) {
            nativeEvents::observation_lost();++i;
            native_report("ev=native_population_capture stage=attachment result=entity_mismatch actor=%08X",current.handle);
            continue;
        }
        const auto& lease=currentReceipt.lease;
        nativeEvents::Event complete{lease,{lease.source,current.handle,current.entity,expected.actor.birthNonce},
            current.source.handle,nativeEvents::Kind::admitted};
        if(current.member.handle==lease.source.source.definition) {
            const auto* member=state::activity::open_world_members::lookup(current.member.handle,
                lease.source.source.registry,lease.source.source.slot,current.member.offset);
            if(member)complete.memberCategory=member->category;
        }
        // Keep the mailbox provisional until retirement tracking has room. This
        // makes local capacity pressure retryable and preserves the real actor.
        if(g_admittedActors.full()) {++i;continue;}
        // Best-effort sidecar, captured before the authoritative event can be
        // drained. A busy retry deduplicates the same exact receipt and actor.
        // Failure here never changes native admission, renewal, or AI behavior.
        const bool vendor=vendorPopulation::lifetime::owns(lease);
        if(!vendor) state::activity::open_world_members::capture(birth.receipt,complete,
            {current.member.handle,current.member.kind,current.member.offset});
        const auto admission=nativeEvents::admit(birth.receipt,complete,!vendor);
        const bool accepted=admission==nativeEvents::AdmitResult::admitted
            && (!vendor || vendorPopulation::lifetime::admit(complete));
        if(admission==nativeEvents::AdmitResult::busy) {++i;continue;}
        if(!accepted) {
            state::activity::open_world_members::discard(birth.receipt,complete);
            nativeEvents::observation_lost();
        }
        else {
            const auto retained=g_admittedActors.add({complete,birth.parent,birth.receipt});
            if(retained!=pending::Intake::accepted && retained!=pending::Intake::duplicate) nativeEvents::observation_lost();
            if(vendor) vendorPopulation::remember(complete);
            else {
                observe_vance_contact_admission(complete,birth.parent);
                streaming::remember({complete,birth.parent,birth.receipt});
            }
        }
        native_report("ev=native_population_capture stage=attachment actor=%08X entity=%08X accepted=%u",
            current.handle,current.entity,accepted?1U:0U);
        g_pendingBirths.erase(i);
    }
}
void observe_native_candidate(void* instance,std::uint32_t event) noexcept {
    Read read;std::array<std::byte,0x3C> eventHeader{};std::array<std::byte,0x38> payload{};
    std::uint32_t eventDefinition{};
    if(!event_payload(read,event,eventHeader,payload,eventDefinition) || eventDefinition!=0x80804C54U) return;
    const auto address=reinterpret_cast<std::uintptr_t>(instance);std::uintptr_t resolved{},healthAddress{};
    std::array<std::byte,0xC4> character{};Actor actorState;nativeEvents::Receipt receipt;
    if(address>UINTPTR_MAX-0x2E8 || !read.copy(address,character)
        || at<std::uint32_t>(character.data()+4)!=0x80806832U
        || !actor(read,at<std::uint32_t>(character.data()+0xC0),actorState)
        || !read.resolve({at<std::uint32_t>(character.data()+0x24),0,0},resolved) || resolved!=address
        || !registered_source(read,actorState,receipt)) return;
    const auto& lease=receipt.lease;
    Ref healthRef{};std::array<std::byte,0x340> health{};
    if(!omega_enemy_native_health::owner(actorState.entity,at<std::uint32_t>(character.data()+0x2C))
        || !read.value(address+0x2E8,healthRef) || healthRef.kind!=0x80804BEEU || healthRef.offset!=0
        || !read.resolve(healthRef,healthAddress) || !read.copy(healthAddress,health)
        || !omega_enemy_native_health::identity(healthRef.handle,healthRef.kind,healthRef.offset,
            at<std::uint32_t>(health.data()+4),at<std::uint32_t>(health.data()+0x24),
            actorState.entity,at<std::uint32_t>(health.data()+0x2C))
        || !omega_enemy_native_health::death(true,eventDefinition,true,at<std::uint8_t>(health.data()+0x338),
            lease.source.generation,lease.source.generation)) return;
    if(vendorPopulation::lifetime::owns(lease)) return;
    // A death can beat the next frame poll. Deliver that actor's captured birth
    // first under the same lock, then its independently qualified native death.
    forest_candy_drops::observe_death(actorState.entity,address);
    std::lock_guard lock(g_pendingMutex);
    finish_native_admissions(actorState.handle);
    bool admitted{};nativeEvents::Event death{};
    for(std::size_t i=0;i<g_admittedActors.size();++i) {
        const auto& saved=g_admittedActors[i];
        if(saved.receipt!=receipt || saved.event.actor.actor!=actorState.handle
            || saved.event.actor.entity!=actorState.entity || saved.event.sourceHandle!=actorState.source.handle)continue;
        admitted=true;death=saved.event;death.kind=nativeEvents::Kind::died;break;
    }
    if(admitted && !nativeEvents::submit(death,receipt)
        && nativeEvents::capture(lease)==receipt) nativeEvents::observation_lost();
}
void observe_generated_candidate(void* instance,std::uint32_t event,std::uint64_t epochValue) noexcept {
    if(!epochValue) return;
    Read read;std::array<std::byte,0x3C> eventHeader{};std::array<std::byte,0x38> payload{};
    std::uint32_t eventDefinition{};
    if(!event_payload(read,event,eventHeader,payload,eventDefinition) || eventDefinition!=0x80804C54U) return;
    const auto address=reinterpret_cast<std::uintptr_t>(instance);std::uintptr_t resolved{},healthAddress{};
    std::array<std::byte,0xC4> character{};Actor actorState;SourceIdentity sourceIdentity;
    if(address>UINTPTR_MAX-0x2E8 || !read.copy(address,character)
        || at<std::uint32_t>(character.data()+4)!=0x80806832U
        || !actor(read,at<std::uint32_t>(character.data()+0xC0),actorState)
        || actorState.entity==UINT32_MAX || actorState.parent==UINT32_MAX
        || !read.resolve({at<std::uint32_t>(character.data()+0x24),0,0},resolved) || resolved!=address) return;
    Ref healthRef{};std::array<std::byte,0x340> health{};
    if(!omega_enemy_native_health::owner(actorState.entity,at<std::uint32_t>(character.data()+0x2C))
        || !read.value(address+0x2E8,healthRef) || healthRef.kind!=0x80804BEEU || healthRef.offset!=0
        || !read.resolve(healthRef,healthAddress) || !read.copy(healthAddress,health)
        || !omega_enemy_native_health::identity(healthRef.handle,healthRef.kind,healthRef.offset,
            at<std::uint32_t>(health.data()+4),at<std::uint32_t>(health.data()+0x24),
            actorState.entity,at<std::uint32_t>(health.data()+0x2C))
        ) return;
    const auto healthFlags=at<std::uint8_t>(health.data()+0x338);
    if(!generated_source(read,actorState,sourceIdentity)) {
        // Generated actors may have no actor source backlink.  Keep only a
        // genuine typed-health death witness; the matching roster lease will
        // provide the owner/generation before a death event is published.
        if(actorState.source.handle!=UINT32_MAX || (healthFlags&1U)==0) return;
        // Forest candy: one native drop per witnessed death, before the pending lock (native calls).
        forest_candy_drops::observe_death(actorState.entity,address);
        std::lock_guard lock(g_pendingMutex);
        retain_early_generated_death(actorState.handle,actorState.entity,actorState.parent,healthFlags,epochValue);
        return;
    }
    if(!omega_enemy_native_health::death(true,eventDefinition,true,healthFlags,
        sourceIdentity.lease.source.generation,sourceIdentity.lease.source.generation)) return;
    forest_candy_drops::observe_death(actorState.entity,address);
    // A death can beat the next frame poll. Deliver that actor's captured birth
    // first under the same lock, then its independently qualified native death.
    std::lock_guard lock(g_pendingMutex);
    finish_native_admissions(actorState.handle);
    const bool generated=sourceIdentity.lease.source.source.type==37;
    const nativeEvents::Event death{sourceIdentity.lease,
        {sourceIdentity.lease.source,actorState.handle,actorState.entity},sourceIdentity.sourceHandle,
        nativeEvents::Kind::died,sourceIdentity.memberPrefabTag,sourceIdentity.completionGroup};
    if(!nativeEvents::submit(death,epochValue)
        && !(generated && !nativeEvents::has_lease(death.lease))) nativeEvents::observation_lost();
}
__declspec(noinline) std::uint64_t __fastcall admission_hook(void* instance,const void* context) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    const auto nav=selected_context();
    bool observing{};
    const auto creation=scope.accepts_side_effects()?nativeEvents::begin_creation(observing):nativeEvents::Creation{};
    if(observing && !creation) nativeEvents::observation_lost();
    std::uint32_t parent=UINT32_MAX;
    if(scope.accepts_side_effects() && ((nav.enabled && nav.run!=0) || observing)) {
        Read read;std::array<std::byte,0x28> header{};
        if(read.copy(reinterpret_cast<std::uintptr_t>(instance),header)
            && at<std::uint32_t>(header.data()+4)==0x808082ECU) {
            parent=at<std::uint32_t>(header.data()+0x24);
        }
    }
    const auto result=omega_enemy_native_admission::forward(hooking::await_original(g_admission),
        [&scope]() noexcept {return scope.accepts_side_effects();},
        [callRun=nav.run,creation](std::uint32_t identity) noexcept {
            if(identity!=UINT32_MAX) {observe_admission(identity,callRun);observe_native_admission(identity,creation);}
            else if(creation) native_report("ev=native_population_capture stage=created result=pre_identity_rejected");
        },instance,context,parent);
    nativeEvents::cancel(creation);return result;
}
__declspec(noinline) std::uint64_t __fastcall candidate_hook(void* instance,std::uint32_t event) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    const auto original=hooking::await_original(g_candidate);
    if(scope.accepts_side_effects()) {omega_boss_health::observe_native_death(instance,event,scope);}
    if(scope.accepts_side_effects()) {observe_candidate(instance,event,scope);
        observe_generated_candidate(instance,event,nativeEvents::generator_epoch());
        observe_native_candidate(instance,event);}
    return original(instance,event);
}
bool idle() noexcept {return g_gate.idle();}
bool retirement_slot(Read& read,std::uint32_t handle,native_population_retirement::Slot& slot) noexcept {
    std::array<std::byte,0x28> descriptor{};
    if(!read.copy(g_image+0x1F9D7F0,descriptor)) return false;
    slot.base=at<std::uintptr_t>(descriptor.data()+8);
    slot.stride=at<std::uint32_t>(descriptor.data()+0x20);
    slot.generationOffset=at<std::uint32_t>(descriptor.data()+0x1C);
    slot.mask=at<std::uint32_t>(descriptor.data()+0x24);
    std::uintptr_t cell{};
    return native_population_retirement::valid(slot)
        && add(slot.base,static_cast<std::int64_t>(handle&0x1FFFU)*slot.stride+slot.generationOffset,cell)
        && read.value(cell,slot.generation);
}
__declspec(noinline) void __fastcall retirement_hook(std::uint32_t handle,std::uint8_t mode) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    const auto original=hooking::await_original(g_retirement);
    const auto hijackedRun=scope.accepts_side_effects()?hijacked::native_run():0;
    if(hijackedRun && TryAcquireSRWLockExclusive(&g_lock)) {
        trace_hijacked_retirement(hijackedRun,handle);
        ReleaseSRWLockExclusive(&g_lock);
    }
    pending::Birth captured;native_population_retirement::Slot before;
    bool qualified{};
    if(scope.accepts_side_effects()) {
        std::lock_guard lock(g_pendingMutex);
        finish_native_admissions(handle);
        vendorPopulation::poll();
        for(std::size_t i=0;i<g_admittedActors.size();++i) {
            const auto& birth=g_admittedActors[i];if(birth.event.actor.actor!=handle) continue;
            Read read;Actor current;
            qualified=nativeEvents::capture(birth.event.lease)==birth.receipt
                && actor(read,handle,current)
                && native_population_retirement::identity(birth.event.actor.actor,birth.event.actor.entity,birth.parent,
                    birth.event.sourceHandle,current.handle,current.entity,current.parent,current.source.handle)
                && retirement_slot(read,handle,before);
            if(qualified) captured=birth;
            else {
                nativeEvents::observation_lost();
                native_report("ev=native_population_capture stage=retirement_begin result=identity_rejected actor=%08X entity=%08X parent=%08X source=%08X",
                    handle,current.entity,current.parent,current.source.handle);
            }
            break;
        }
    }
    // Never hold the pending/mailbox lock across native teardown. Native
    // callbacks may run inside it, and their original order must be retained.
    native_population_retirement::forward(original,[&]() noexcept {
        if(!qualified || !scope.accepts_side_effects()) return;
        Read read;native_population_retirement::Slot after;
        const bool released=retirement_slot(read,handle,after) && native_population_retirement::released(before,after);
        auto event=captured.event;event.kind=nativeEvents::Kind::retired;
        if(released && event.lease.source.source.type==37)
            retire_generated_roster(event.actor.actor,event.actor.entity,captured.parent,event.lease);
        std::lock_guard lock(g_pendingMutex);
        const bool vendor=vendorPopulation::lifetime::owns(event.lease);
        const bool accepted=released && (vendor || nativeEvents::submit(event,captured.receipt));
        if(!accepted && nativeEvents::capture(event.lease)==captured.receipt) nativeEvents::observation_lost();
        if(released && !vendor) {observe_vance_contact_retirement(event);streaming::retired(event);}
        vendorPopulation::poll();
        for(std::size_t i=0;i<g_admittedActors.size();++i) {
            if(g_admittedActors[i].event.actor==event.actor) {g_admittedActors.erase(i);break;}
        }
        native_report("ev=native_population_capture stage=retired actor=%08X entity=%08X generation_before=%u generation_after=%u released=%u accepted=%u",
            handle,captured.event.actor.entity,before.generation,after.generation,released?1U:0U,accepted?1U:0U);
    },handle,mode);
}
#include "native_population_streaming.inl"
#include "strike_bond_intro_release.inl"
#include "strike_bond_target_binding.inl"
#include "strike_bond_carriage.inl"
#include "strike_bond_boss_cycle.inl"
#include "strike_bond_boss_shield.inl"
#include "strike_bond_boss_retirement.inl"
#include "strike_bond_fire_trace.inl"

void* target(std::uintptr_t rva,const std::array<std::uint8_t,16>& expected) noexcept {
    Read read;std::array<std::byte,16> actual{};
    if(!read.copy(g_image+rva,actual) || std::memcmp(actual.data(),expected.data(),expected.size())!=0) {
        std::array<char,160> line{};
        const auto size=std::snprintf(line.data(),line.size(),
            "ev=omega_enemy_lair stage=install result=prefix_mismatch rva=%llX",static_cast<unsigned long long>(rva));
        if(size>0 && static_cast<std::size_t>(size)<line.size())
            core::log::write(core::log::Channel::client,core::log::Level::error,{line.data(),static_cast<std::size_t>(size)});
        return nullptr;
    }
    return reinterpret_cast<void*>(g_image+rva);
}
} // namespace

void observe_native_generated_population(void* worker) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    if(!scope.accepts_side_effects())return;
    // This is after the original tick. State 6 queues the native reset;
    // 1001FF0 clears pieces/gateways and state 1 settles disabled workers at
    // state 0. Observe that result instead of treating a sent disable as done.
    const auto epochValue=nativeEvents::generator_epoch();
    Read read;generatedRoster::WorkerTuple tuple{};
    std::array<std::byte,kGeneratedRosterWorkerBytes> bytes{};
    std::uint32_t self{};std::int32_t count{};std::uintptr_t entries{};
    std::uint8_t enabled{},state{};
    if(generated_worker(read,reinterpret_cast<std::uintptr_t>(worker),tuple,self,count,entries,bytes)
        && read.value(reinterpret_cast<std::uintptr_t>(worker)+0x9BA,enabled)
        && read.value(reinterpret_cast<std::uintptr_t>(worker)+0x9BC,state)) {
        const nativeEvents::GeneratorObservation observation{
            tuple.resourceTag,tuple.seed,tuple.workerDefinitionTag,tuple.workerDefinitionOffset,
            generatedIdentity::field<std::uint32_t>(bytes,0x2C),count,
            generatedIdentity::field<std::int32_t>(bytes,0x928),
            generatedIdentity::field<std::int32_t>(bytes,0x92C),
            state,enabled!=0};
        static_cast<void>(nativeEvents::observe_generator(observation,epochValue));
    }
    observe_native_generated_population_impl(worker);
}

std::uint32_t registered_native_forest_owner(void* worker) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    if(!scope.accepts_side_effects() || !worker)return UINT32_MAX;
    Read read;
    std::array<std::byte,kGeneratedRosterWorkerBytes> bytes{};
    generatedRoster::WorkerTuple tuple{};
    std::uint32_t self{};std::int32_t count{};std::uintptr_t entries{};
    if(!generated_worker(read,reinterpret_cast<std::uintptr_t>(worker),tuple,self,count,entries,bytes)
        || !nativeEvents::has_generator(tuple.resourceTag,tuple.seed,
            tuple.workerDefinitionTag,tuple.workerDefinitionOffset))return UINT32_MAX;
    const auto owner=generatedIdentity::field<std::uint32_t>(bytes,0x2C);
    std::uintptr_t pool{};std::uint32_t stride{},identity{},flags{};
    if(owner==UINT32_MAX || !read.value(g_image+0x1F93428,pool)
        || !read.value(g_image+0x1F93430,stride) || stride<0x50 || stride>0x100000)
        return UINT32_MAX;
    std::uintptr_t entity{};
    if(!add(pool,static_cast<std::int64_t>(owner&0x1FFFU)*stride,entity)
        || !read.value(entity+0xC,identity) || identity!=owner
        || !read.value(entity+4,flags) || (flags&4U))return UINT32_MAX;
    return owner;
}

std::size_t registered_native_forest_gate_owners(void* worker,
    std::span<std::uint32_t> owners) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    if(!scope.accepts_side_effects() || registered_native_forest_owner(worker)==UINT32_MAX)return 0;
    const auto address=reinterpret_cast<std::uintptr_t>(worker);
    Read header;std::int32_t count{};std::int64_t relative{};std::uintptr_t gates{};
    if(!header.value(address+0x92C,count) || count<0 || count>128
        || !header.value(address+0x890,relative) || !add(address,relative,gates)
        || !add(gates,0x8A0,gates))return 0;
    std::size_t used{};
    for(std::int32_t i=0;i<count;++i) {
        Read group;std::int32_t parts{};
        const auto gateway=gates+static_cast<std::uintptr_t>(i)*0x360U;
        if(!group.value(gateway,parts) || parts<0 || parts>17)continue;
        for(std::int32_t j=0;j<parts && used<owners.size();++j) {
            Read read;std::array<std::uint32_t,2> weak{};
            if(!read.value(gateway+0x10U+static_cast<std::uintptr_t>(j)*0x30U,weak)
                || weak[1]==UINT32_MAX)continue;
            // Same serial check as native 352310. A reused entity slot is
            // not a child of this generator even if its address is readable.
            const auto handle=weak[1];
            std::uintptr_t directory{},registry{},metadata{},head{},elements{};
            std::int32_t directoryStride{};
            if(!read.value(g_image+0x2439C70,directory) || !read.value(directory,registry)
                || !read.value(directory+0x10,directoryStride)
                || directoryStride<=0 || directoryStride>0x1000)continue;
            const auto index=((static_cast<std::int32_t>(handle)>>31&0x3C00U)|0x3FFU)
                &(handle>>13)&0xFFFFU;
            if(!read.value(registry+static_cast<std::uintptr_t>(index)*directoryStride+0x10,metadata)
                || !metadata || !read.value(metadata,head) || !read.value(metadata+8,elements)
                || !elements)continue;
            std::uint16_t capacity{};std::uint32_t offset{},stride{},serial{};
            if(!read.value(head+0x1C,capacity) || (handle&0x1FFFU)>=capacity
                || !read.value(metadata+0x1C,offset) || !read.value(metadata+0x20,stride)
                || !stride || stride>0x100000)continue;
            std::uintptr_t serialAddress{};
            if(!add(elements,static_cast<std::int64_t>(offset)
                +static_cast<std::int64_t>(handle&0x1FFFU)*stride,serialAddress)
                || !read.value(serialAddress,serial) || serial!=weak[0])continue;
            std::uintptr_t pool{};std::uint32_t entityStride{},identity{},flags{};
            if(!read.value(g_image+0x1F93428,pool) || !read.value(g_image+0x1F93430,entityStride)
                || entityStride<0x50 || entityStride>0x100000)continue;
            std::uintptr_t entity{};
            if(!add(pool,static_cast<std::int64_t>(handle&0x1FFFU)*entityStride,entity)
                || !read.value(entity+0xC,identity) || identity!=handle
                || !read.value(entity+4,flags) || (flags&4U))continue;
            owners[used++]=handle;
        }
    }
    return used;
}

__declspec(noinline) void dispatch_native_population_source(std::uint32_t* instance,std::uint32_t reason,
    const std::byte* authority,NativePopulationDispatch original) noexcept {
    streaming::dispatch_source(instance,reason,authority,original);
}

__declspec(noinline) void retire_strike_bond_boss(std::uintptr_t source,bool allocatorReady) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    if(scope.accepts_side_effects()) garden_retirement::dispatch(source,allocatorReady);
}

void begin_vendor_area_unload() noexcept {
    vendorPopulation::unloading.fetch_add(1,std::memory_order_acq_rel);
}
void finish_vendor_area_unload(bool allocatorReady) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    if(vendorPopulation::unloading.load(std::memory_order_acquire)==1 && scope.accepts_side_effects())
        vendorPopulation::finish_unload(allocatorReady);
    vendorPopulation::unloading.fetch_sub(1,std::memory_order_release);
}

__declspec(noinline) void poll_native_population_admissions() noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    if(!scope.accepts_side_effects()) return;
    poll_mission_population_readiness(g_image,GetTickCount64());
    vendorPopulation::lifetime::prune();
    const auto hijackedRun=hijacked::native_run();
    if(hijackedRun && TryAcquireSRWLockExclusive(&g_lock)) {
        trace_hijacked_attachments(hijackedRun);
        ReleaseSRWLockExclusive(&g_lock);
    }
    {
    std::lock_guard lock(g_pendingMutex);
    finish_native_admissions();
    vendorPopulation::poll();
    streaming::prune();
    for(std::size_t i=0;i<g_admittedActors.size();) {
        const auto& birth=g_admittedActors[i];
        if(nativeEvents::capture(birth.event.lease)!=birth.receipt)
            g_admittedActors.erase(i);
        else {if(!vendorPopulation::lifetime::owns(birth.event.lease)) streaming::remember(birth);++i;}
    }
    streaming::capture_network_roots();
    }
    vendorPopulation::resume();
}

bool install_omega_enemy_lair_receipts() noexcept {
    if(g_handles[0].attached) {return g_gate.accepting();}
    g_image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(g_image==0) {return false;}
    const std::array<hooking::detour::Spec,12> specs{{
        {target(0xA0D510,{0x48,0x89,0x5C,0x24,0x20,0x56,0x48,0x83,0xEC,0x30,0x48,0x8B,0xD9,0x48,0x8B,0xF2}),reinterpret_cast<void*>(&admission_hook)},
        {target(0xC72390,{0x48,0x89,0x5C,0x24,0x10,0x55,0x56,0x57,0x48,0x83,0xEC,0x20,0x48,0x8B,0xE9,0x8B}),reinterpret_cast<void*>(&candidate_hook)},
        {target(0xA85540,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x70,0x8B}),reinterpret_cast<void*>(&retirement_hook)},
        {target(0xBC8F20,{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0x48,0x8D,0x4C,0x24,0x30,0xE8,0x5D}),reinterpret_cast<void*>(&garden_fire::one_tick_hook)},
        {target(0xBC8F80,{0x0F,0x2F,0x89,0xC0,0x0A,0x00,0x00,0x0F,0xB6,0x91,0xC4,0x0A,0x00,0x00,0x76,0x1B}),reinterpret_cast<void*>(&garden_fire::duration_hook)},
        {target(0xBCD330,{0x40,0x56,0x57,0x48,0x83,0xEC,0x48,0x48,0x89,0x5C,0x24,0x68,0x32,0xC0,0x8B,0x59}),reinterpret_cast<void*>(&garden_fire::update_hook)},
        {target(0xC31200,{0x48,0x89,0x4C,0x24,0x08,0x53,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41}),reinterpret_cast<void*>(&garden_fire::eligibility_hook)},
        {target(0xC613E0,{0x48,0x89,0x5C,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57,0x48,0x83,0xEC,0x20,0x44}),reinterpret_cast<void*>(&garden_target::dispatch_hook)},
        {target(0xC5FEF0,{0x40,0x57,0x48,0x83,0xEC,0x20,0x44,0x0F,0xBE,0x49,0x7A,0x49,0x8B,0xF8,0x41,0x83}),reinterpret_cast<void*>(&garden_target::decode_hook)},
        {target(0x1704870,{0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x30,0x8B,0xFA,0x8B,0xD9,0xE8,0x3D}),reinterpret_cast<void*>(&streaming::bind_hook)},
        {target(0x4E3C50,{0x40,0x56,0x48,0x83,0xEC,0x40,0x83,0xB9,0xFC,0x05,0x00,0x00,0xFF,0x48,0x8B,0xF1}),reinterpret_cast<void*>(&streaming::destroy_hook)},
        {target(0x4E8270,{0x40,0x53,0x48,0x83,0xEC,0x50,0x48,0x8B,0x05,0x0B,0x18,0xBC,0x01,0x48,0x33,0xC4}),reinterpret_cast<void*>(&streaming::consume_hook)}
    }};
    for(const auto& spec:specs) if(!spec.target) return false;
    streaming::deleteFacet=reinterpret_cast<streaming::Delete>(target(0x16EE070,
        {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57}));
    if(!streaming::deleteFacet)return false;
    hooking::detour::InstallFailure failure{};
    if(!hooking::detour::install(specs,g_handles,failure)) {
        std::array<char,192> line{};
        const auto size=std::snprintf(line.data(),line.size(),
            "ev=omega_enemy_lair stage=install result=failed stage_id=%u index=%zu native_error=%ld",
            static_cast<unsigned>(failure.stage),failure.index,failure.nativeError);
        if(size>0 && static_cast<std::size_t>(size)<line.size())
            core::log::write(core::log::Channel::client,core::log::Level::error,{line.data(),static_cast<std::size_t>(size)});
        return false;
    }
    hooking::publish_original(g_admission,reinterpret_cast<Admission>(g_handles[0].original));
    hooking::publish_original(g_candidate,reinterpret_cast<CandidateEvent>(g_handles[1].original));
    hooking::publish_original(g_retirement,reinterpret_cast<Retirement>(g_handles[2].original));
    hooking::publish_original(garden_fire::oneTick,reinterpret_cast<garden_fire::OneTick>(g_handles[3].original));
    hooking::publish_original(garden_fire::duration,reinterpret_cast<garden_fire::Duration>(g_handles[4].original));
    hooking::publish_original(garden_fire::update,reinterpret_cast<garden_fire::Update>(g_handles[5].original));
    hooking::publish_original(garden_fire::eligibility,reinterpret_cast<garden_fire::Eligibility>(g_handles[6].original));
    hooking::publish_original(garden_target::dispatch,reinterpret_cast<garden_target::Dispatch>(g_handles[7].original));
    hooking::publish_original(garden_target::decode,reinterpret_cast<garden_target::Decode>(g_handles[8].original));
    hooking::publish_original(streaming::bind,reinterpret_cast<streaming::Bind>(g_handles[9].original));
    hooking::publish_original(streaming::destroy,reinterpret_cast<streaming::Destroy>(g_handles[10].original));
    hooking::publish_original(streaming::consume,reinterpret_cast<streaming::Consume>(g_handles[11].original));
    g_gate.accept();
    core::log::write(core::log::Channel::client,core::log::Level::info,
        "ev=omega_enemy_lair stage=install result=ok admission=A0D510 health_death=C72390 event=80804C54 retirement=A85540 garden_fire_trace=BC8F20,BC8F80,BCD330,C31200 garden_intro_release=AFB11A12:31A03F93 garden_target_binding=C613E0,C5FEF0 mercury_streaming=1704870,16EE070,4E3C50,4E8270 source_dispatch=shared_4E4580");
    return true;
}
void quiesce_omega_enemy_lair_receipts() noexcept {g_gate.quiesce();}
bool uninstall_omega_enemy_lair_receipts() noexcept {
    quiesce_omega_enemy_lair_receipts();if(!g_handles[0].attached) {return true;}
    const std::array<hooking::detour::ProtectedCodeEntry,24> protectedCode{{
        {reinterpret_cast<void*>(&admission_hook)},{reinterpret_cast<void*>(&candidate_hook)},
        {reinterpret_cast<void*>(&observe_admission)},{reinterpret_cast<void*>(&observe_candidate)},
        {reinterpret_cast<void*>(&omega_boss_health::observe_native_death)},
        {reinterpret_cast<void*>(&poll_native_population_admissions)},
        {reinterpret_cast<void*>(&begin_vendor_area_unload)},
        {reinterpret_cast<void*>(&finish_vendor_area_unload)},
        {reinterpret_cast<void*>(&retirement_hook)},
        {reinterpret_cast<void*>(&retire_strike_bond_boss)},
        {reinterpret_cast<void*>(&garden_retirement::dispatch)},
        {reinterpret_cast<void*>(&garden_fire::one_tick_hook)},
        {reinterpret_cast<void*>(&garden_fire::duration_hook)},
        {reinterpret_cast<void*>(&garden_fire::update_hook)},
        {reinterpret_cast<void*>(&garden_fire::eligibility_hook)},
        {reinterpret_cast<void*>(&garden_target::dispatch_hook)},
        {reinterpret_cast<void*>(&garden_target::decode_hook)},
        {reinterpret_cast<void*>(&streaming::bind_hook)},
        {reinterpret_cast<void*>(&streaming::dispatch_source)},
        {reinterpret_cast<void*>(&dispatch_native_population_source)},
        {reinterpret_cast<void*>(&streaming::destroy_hook)},
        {reinterpret_cast<void*>(&streaming::consume_hook)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}
    }};
    if(hooking::detour::uninstall(g_handles,protectedCode,idle)!=hooking::detour::UninstallResult::removed) {return false;}
    g_admission.store(nullptr,std::memory_order_release);g_candidate.store(nullptr,std::memory_order_release);
    g_retirement.store(nullptr,std::memory_order_release);
    streaming::bind.store(nullptr,std::memory_order_release);
    streaming::destroy.store(nullptr,std::memory_order_release);streaming::consume.store(nullptr,std::memory_order_release);
    streaming::reset();
    garden_fire::oneTick.store(nullptr,std::memory_order_release);garden_fire::duration.store(nullptr,std::memory_order_release);
    garden_fire::update.store(nullptr,std::memory_order_release);garden_fire::eligibility.store(nullptr,std::memory_order_release);
    garden_target::dispatch.store(nullptr,std::memory_order_release);garden_target::decode.store(nullptr,std::memory_order_release);
    garden_fire::reset();garden_intro::reset();garden_target::reset();garden_target::reset_replay();garden_carriage::reset();garden_cycle::reset();garden_shield::reset();garden_retirement::reset();
    g_image=0;g_run=UINT64_MAX;g_lines=0;g_seenCount=0;g_seen={};
    hijacked_trace_run(0);
    g_pendingBirths={};g_admittedActors={};vendorPopulation::roots={};vendorPopulation::lines.store(0,std::memory_order_relaxed);g_nativeLines.store(0,std::memory_order_relaxed);
    g_candidateRejected=0;g_parentRejected=0;g_rejects={};g_rejectCount=0;g_rejectOverflow=false;return true;
}
} // namespace dawn::client::hooks::bootflow
