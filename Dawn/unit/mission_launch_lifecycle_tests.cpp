#include <Windows.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include "client/activity/mission_launch.h"
#include "state/activity/gateway_intro.h"
#include "state/activity/vanilla/homecoming/prologue.h"
#include "state/activity/vanilla/homecoming/continuation.h"
#include "client/activity/campaign_dialogue.h"
#include "client/activity/mission_launch_options.h"
#include "client/activity/campaign_openings.h"
#include "client/activity/mission_launch_testing.h"
#include "client/hooks/bootflow/mission_prelaunch.h"
#include "state/activity/forced/activity_forced_destination.h"
#include "state/activity/destination/activity_destination_snapshot.h"
#include "state/runtime/storage/internal.h"
#include "core/logging/log.h"
#include "middleware/content/packages/tables/activity_table.h"

namespace {
namespace launch = dawn::client::activity::mission_launch;
namespace intro = dawn::state::activity::gateway_intro;
namespace prologue = dawn::state::activity::vanilla::homecoming::prologue;
namespace forced = dawn::state::activity::forced;
namespace prelaunch = forced::prelaunch;
namespace build = dawn::state::build_data;
namespace destination = dawn::state::activity::destination;
unsigned g_checks{}, g_selects{}, g_commits{}, g_prepareCalls{};
bool g_reenterOrbit{},g_reenteredOrbit{};
dawn::state::activity::coo::Generation g_completedHomecoming{};
bool g_sessionReady{true};unsigned g_leaves{};
std::uint64_t g_submittedNonce{};
std::uint64_t g_now{1000}, g_session{1};
std::int32_t g_step{29};
bool g_hooksReady{}, g_wrongDestination{}, g_descriptorValid{true}, g_dialogueReady{true};
std::int16_t g_dialogueActivity{-1};
std::int16_t g_videoIndex{-1};bool g_videoPlaying{},g_finished{};
std::array<std::byte, 0x1C900> g_manager{};
std::array<std::byte, 0xA40> g_record{};
destination::DestinationSelection g_actual{}, g_submitted{};
void check(bool value, const char* message) {
    ++g_checks; if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
template<class T> void put(std::byte* data, std::size_t offset, T value) {
    std::memcpy(data + offset, &value, sizeof(value));
}
destination::DestinationSelection descriptor(std::int16_t index, std::string_view name) {
    destination::DestinationSelection value{};
    value.previousActivityIndex = value.activityIndex = index; value.reason = 0;
    value.packageNameLength = static_cast<std::uint8_t>(name.size());
    std::memcpy(value.packageName.data(), name.data(), name.size());
    value.descriptorBitLength = 620; value.descriptorNameBit = 100; value.hasDescriptorName = true;
    return value;
}
std::uintptr_t __fastcall world() { return reinterpret_cast<std::uintptr_t>(g_manager.data()); }
bool __fastcall ready(std::uintptr_t) { return g_sessionReady; }
std::uintptr_t __fastcall record(std::uint32_t member) {
    check(member == 0, "native primary member"); return reinterpret_cast<std::uintptr_t>(g_record.data());
}
void* __fastcall construct(void* buffer, std::uint32_t slot, std::int16_t index) {
    check(slot == 0 && (index == static_cast<std::int16_t>(launch::snapshot().index)
        || (launch::snapshot().index==292 && index>=289 && index<=291)
        || (launch::snapshot().index==prologue::kMission && index==prologue::kVideo)), "construct selected native activity or an opening cutscene");
    put(static_cast<std::byte*>(buffer), 2, index); put(static_cast<std::byte*>(buffer), 4, index); return buffer;
}
bool __fastcall valid(const void*) { return g_descriptorValid; }
const char* __fastcall name(std::int16_t index) {
    if(index==289 || index==290 || index==prologue::kVideo) return "";
    if(index==291) return "cine_120_frn";
    for(const auto& entry:launch::openings::kMissions) if(entry.activity==index) return entry.destination.packageName.data();
    if (const auto* variant = dawn::state::activity::strikes::find(index)) { return variant->package.data(); }
    check(false,"only selected native activities are looked up");return "";
}
void __fastcall clear() {}
void __fastcall select(std::uint8_t slot, const void* buffer) {
    ++g_selects; check(slot == 0, "native select slot");
    std::memcpy(&g_submittedNonce,static_cast<const std::byte*>(buffer)+0x10,sizeof g_submittedNonce);
    if(g_reenterOrbit) {
        g_reenterOrbit=false;g_reenteredOrbit=true;
        const auto count=g_selects;
        launch::poll_orbit(reinterpret_cast<std::uintptr_t>(g_manager.data())+0x18);
        check(g_selects==count,"reentrant native update cannot submit a second launch");
    }
    std::int16_t source{}, target{};
    std::memcpy(&source, static_cast<const std::byte*>(buffer) + 2, sizeof(source));
    std::memcpy(&target, static_cast<const std::byte*>(buffer) + 4, sizeof(target));
    if(target==prologue::kVideo) {
        check(g_dialogueActivity==-1,"mission loading dialogue waits for the opening video");
        forced::ForcedDestination selected{};forced::snapshot(selected);
        check(forced::active(selected) && launch::destination_name(selected)=="mission_towerfall","the mission opening is published before its video");
        auto video=descriptor(target,"");
        check(!forced::apply(video),"the opening video keeps its native destination");
        check(source==target,"video selection preserves native identity");
        g_submitted=descriptor(target,name(target));return;
    }
    if(target>=289 && target<=291) {
        check(g_dialogueActivity==-1,"mission loading dialogue waits for all three cutscenes");
        forced::ForcedDestination selected{};forced::snapshot(selected);
        check(!forced::active(selected),"cutscenes keep their own native destinations");
        check(source==target,"cutscene selection preserves native identity");
        g_submitted=descriptor(target,name(target));return;
    }
    check(g_dialogueReady && g_dialogueActivity==launch::snapshot().index,"dialogue policy is delivered before native selection");
    forced::ForcedDestination selected{}; forced::snapshot(selected);
    check(forced::active(selected), "opening enabled before native selection");
    check(source==target && target==launch::snapshot().index,"selection retains its own source and target");
    g_submitted = descriptor(target, name(target));
    if(prelaunch::configured(selected)) check(g_hooksReady,"native publication waits for installed support");
    const auto before=g_submitted;
    check(forced::apply(g_submitted), "native selection receives authored arrival coordinates");
    check(g_submitted.activityIndex==before.activityIndex && g_submitted.previousActivityIndex==before.previousActivityIndex
        && g_submitted.packageName==before.packageName && g_submitted.descriptorBits==before.descriptorBits
        && g_submitted.descriptorBitLength==before.descriptorBitLength,"native identity and descriptor are preserved");
    auto handoff=descriptor(282,"mission_reunion");const auto untouched=handoff;
    check(!forced::apply(handoff) && handoff.packageName==untouched.packageName && handoff.activityIndex==untouched.activityIndex,"an ending handoff is not redirected back to the opening");
    if (g_wrongDestination) { g_submitted = descriptor(282, "mission_reunion"); }
}
void __fastcall commit(std::int32_t enabled) { check(enabled == 1, "native commit enabled"); ++g_commits; }
std::int32_t __fastcall step() { return g_step; }
std::uintptr_t __fastcall video_manager() {return 1;}
bool __fastcall video_playing(std::uintptr_t) {return g_videoPlaying;}
bool __fastcall video_flag(int bit) {check(bit==8,"completion flag is native finished-watching-video");return g_finished;}
void __fastcall video_reset(int bit,bool value) {check((bit==8 || bit==9) && !value,"clear stale finish and skip flags");if(bit==8) g_finished=false;}
void* __fastcall video_index(std::int16_t* out) {*out=g_videoIndex;return out;}
void __fastcall leave(std::int32_t step,std::int32_t reason) {check(step==28 && reason==309,"handoff uses benign native teardown");g_step=28;++g_leaves;}
void orbit() { g_step = 29; launch::poll(); }
void finish_intro() {
    check(launch::snapshot().status==launch::Status::cinematics && g_submitted.activityIndex==289,"Gateway starts with introduction");
    for(std::int16_t i=289;i<=290;++i) {
        const auto before=g_selects;
        g_step=39;g_videoIndex=i;g_finished=true;g_videoPlaying=false;launch::poll();
        check(intro::wanted()<0 && !intro::frame(g_session).enabled,"stale completion cannot activate the briefing");
        g_finished=false;g_videoPlaying=true;launch::poll();
        check(intro::state().phase==intro::Phase::videoPlaying,"native playback is observed");
        g_now+=150000;launch::poll();
        check(g_selects==before && launch::snapshot().busy,"long cinematics do not consume mission arrival timeout");
        g_videoPlaying=false;g_finished=true;launch::poll();
        check(intro::state().phase==intro::Phase::videoReturning && g_selects==before,"completion waits for native cleanup");
        if(i==289) {g_step=29;launch::poll();}
        check(g_selects==before,"native chain owns subsequent selections, including a transient orbit step");
    }
    // Replay the captured retail trace: native selects 291, passes through
    // orbit within one frame, then advertises the exact briefing scenario.
    g_actual=descriptor(291,"cine_120_frn");++g_session;g_step=38;
    intro::selected(g_session,292,intro::kScenario,g_now);
    check(!intro::frame(g_session).enabled,"an unrelated activity cannot bind the briefing");
    intro::selected(g_session,291,intro::kScenario,g_now);
    using namespace dawn::state::activity;
    const ActivityInstanceKey activity{g_session,ActivityIncarnation{1}};
    auto transit=intro::project(activity,g_session,42,true,{},g_now);
    check(transit.publish && transit.host.sliceSetIndex==9 && transit.host.sliceSetHash==intro::kRegistry,"briefing requests authored Tower cinematic region");
    auto arrived=transit.host;arrived.state=3;
    transit=intro::project(activity,g_session,42,true,{arrived,9,true,true},g_now);
    check(intro::frame(g_session).play,"briefing plays only after matching native region receipt");
    arrived.state=0;static_cast<void>(intro::project(activity,g_session,42,true,{arrived,9,true,true},g_now));
    intro::cine::Incident e{};e.registry=intro::kRegistry;e.type=6;e.slot=0;e.runtime=99;e.target=1685;
    check(!intro::incident(g_session,e,g_now),"finish before briefing start is rejected");
    e.target=5239;check(intro::incident(g_session,e,g_now),"briefing start accepted");
    e.target=1685;e.runtime=100;check(!intro::incident(g_session,e,g_now),"unrelated runtime cannot finish briefing");
    e.runtime=99;e.target=3338;check(intro::incident(g_session,e,g_now),"skip requests native cinematic stop");
    const auto before=g_selects;launch::poll();
    check(g_selects==before && intro::suppress_loading(),"skip waits for stop receipt before Mercury loading");
    e.target=1685;check(intro::incident(g_session,e,g_now),"native stop releases mission");
    put(g_manager.data(),0x18+0x182C0+0x140,std::uint8_t{1});
    put(g_manager.data(),0x18+0x182C0+0x148+0x18,std::uint64_t{4321});
    launch::poll();
    check(g_selects==before+1 && !intro::suppress_loading(),"only all three completed cutscenes release Gateway fly-in");
    put(g_manager.data(),0x18+0x182C0+0x148+4,std::int16_t{292});
    put(g_manager.data(),0x18+0x182C0+0x378,std::uint8_t{0});
    launch::poll();check(g_step==28,"native committed Gateway selection tears down briefing");
}
void finish_prologue() {
    check(launch::snapshot().status==launch::Status::cinematics && g_submitted.activityIndex==prologue::kVideo,"Homecoming starts with the opening video");
    const auto before=g_selects;
    g_step=39;g_videoIndex=prologue::kVideo;g_finished=true;g_videoPlaying=false;launch::poll();
    check(prologue::state().phase==prologue::Phase::videoLoading,"stale completion cannot finish the opening video");
    g_finished=false;g_videoPlaying=true;launch::poll();
    check(prologue::state().phase==prologue::Phase::videoPlaying,"native playback of the opening video is observed");
    g_now+=150000;launch::poll();
    check(g_selects==before && launch::snapshot().busy,"the full-length opening does not consume the mission arrival timeout");
    g_videoPlaying=false;g_finished=true;launch::poll();
    check(prologue::state().phase==prologue::Phase::videoReturning && g_selects==before,"opening completion waits for native cleanup");
    g_step=29;launch::poll();g_now+=1000;launch::poll();
    check(g_selects==before && launch::snapshot().busy,"the native chain owns the mission selection through its orbit step");
    // Replay the native chain: retail selects the mission from the video, the published
    // opening applies to that selection, and the roster receipt binds the run.
    auto chained=descriptor(prologue::kMission,"mission_towerfall");chained.previousActivityIndex=prologue::kVideo;chained.reason=5;
    check(forced::apply(chained) && chained.hasArrivalBubbleOverride && chained.arrivalBubbleOverride==9
        && chained.hasSliceSetOverride && chained.sliceSetOverride==72,"the chained mission receives the authored opening coordinates");
    auto sourceless=descriptor(prologue::kMission,"mission_towerfall");sourceless.previousActivityIndex=-1;sourceless.reason=5;
    check(forced::apply(sourceless) && sourceless.arrivalBubbleOverride==9,"a chained selection without a previous activity is still the opening");
    auto foreign=descriptor(prologue::kMission,"mission_towerfall");foreign.previousActivityIndex=282;
    check(!forced::apply(foreign),"a selection from another activity keeps its native destination");
    ++g_session;g_step=33;prologue::selected(g_session,prologue::kMission,dawn::state::activity::vanilla::homecoming::kScenario,g_now);
    launch::poll();
    check(launch::snapshot().busy && !launch::snapshot().inMission && g_selects==before,"the chained mission keeps loading without a competing selection");
    g_actual=chained;g_step=38;launch::poll();
    const auto state=launch::snapshot();
    check(state.status==launch::Status::arrived && !state.busy && state.inMission && state.current_name()=="mission_towerfall"
        && prologue::state().phase==prologue::Phase::complete,"the chained arrival completes the Red War opening");
    check(g_dialogueActivity==prologue::kMission,"arrival keeps the mission dialogue policy");
}
void arrive() {
    g_step = 33; launch::poll();
    check(launch::snapshot().busy && !launch::snapshot().inMission, "loading remains pending");
    g_actual = g_submitted; ++g_session; g_step = 38; launch::poll();
}
void unchanged(const forced::ForcedDestination& expected) {
    forced::ForcedDestination current{}; forced::snapshot(current);
    check(launch::destination_name(current) == launch::destination_name(expected)
        && current.spawnSetHash == expected.spawnSetHash, "waiting or rejected launch preserves existing override");
}
}
namespace dawn::client::activity::campaign_dialogue { bool select(std::int16_t activity) noexcept { g_dialogueActivity=activity; return g_dialogueReady; } }
namespace dawn::state::activity::vanilla::homecoming::continuation { coo::Generation request() noexcept {return g_completedHomecoming;} }
namespace dawn::state::runtime::storage { State g_state{}; SRWLOCK g_stateLock = SRWLOCK_INIT; }
namespace dawn::core::log { void write(Channel, Level, std::string_view) noexcept {} }
namespace dawn::state::activity {
std::uint64_t newest_joined_session() noexcept { return g_session; }
std::uint64_t mission_run_generation() noexcept { return g_session; }
void reset_mission_authority_runtime_initialization() noexcept {}
}
namespace dawn::state::activity::destination {
bool snapshot(std::uint64_t session, DestinationSelection& value) noexcept {
    value = g_actual; return session == g_session && value.packageNameLength != 0;
}
}
namespace dawn::state::build_data {
bool find_scenario_layout(std::string_view name, scenarios::Definition& value) noexcept {
    value = {};
    if(name==intro::kPackage) {value.tag=intro::kScenario;return true;}
    if (name == "mission_reunion") { return true; }
    for (const auto& mission : launch::openings::kMissions) {
        const auto& opening = mission.destination;
        if (name != launch::destination_name(opening)) { continue; }
        std::copy(name.begin(), name.end(), value.name.begin()); value.nameLength = static_cast<std::uint8_t>(name.size());
        std::copy(name.begin(), name.end(), value.spawnStem.begin()); value.spawnStemLength = value.nameLength;
        value.bubbleCount = opening.bubble + 1;
        // Cinematic openings (Exodus region 25) select state 1, not always 0.
        value.bubbleStateCounts[opening.bubble] = static_cast<std::uint8_t>(opening.sliceSet-launch::tables::region_index(opening.bubble)+1);
        value.bubbleMapIndices[opening.bubble] = opening.bubble; return true;
    }
    return false;
}
bool find_spawn_sets(std::string_view name, std::span<spawn_sets::NameHash> values, std::size_t& count) noexcept {
    count = 0;
    for (const auto& mission : launch::openings::kMissions) {
        const auto& opening = mission.destination;
        if (name != launch::destination_name(opening) || !opening.hasSpawnSetHash || values.empty()) { continue; }
        values[0] = {}; values[0].value = opening.spawnSetHash; values[0].pointCount = values[0].inMapPackage = 1;
        values[0].bubbleMask[opening.bubble / 8] = static_cast<std::uint8_t>(1U << (opening.bubble % 8));
        count = 1; return true;
    }
    return false;
}
}
namespace dawn::client::hooks::bootflow {
bool prepare_mission_prelaunch(const forced::ForcedDestination& value) noexcept {
    ++g_prepareCalls; return prelaunch::configured(value) == nullptr || g_hooksReady;
}
}
namespace dawn::client::activity::mission_launch::testing {
std::uint64_t now() noexcept { return g_now; }
std::uintptr_t native_entry(std::uintptr_t rva) noexcept {
    switch (rva) {
    case 0xC03430: return reinterpret_cast<std::uintptr_t>(&world);
    case 0x1788810: case 0x178D740: return reinterpret_cast<std::uintptr_t>(&ready);
    case 0xBFA030: return reinterpret_cast<std::uintptr_t>(&record);
    case 0xC061D0: return reinterpret_cast<std::uintptr_t>(&construct);
    case 0x4D5460: return reinterpret_cast<std::uintptr_t>(&valid);
    case 0xDDECA0: return reinterpret_cast<std::uintptr_t>(&name);
    case 0xBF95D0: return reinterpret_cast<std::uintptr_t>(&::clear);
    case 0xBFB1F0: return reinterpret_cast<std::uintptr_t>(&select);
    case 0xBF97D0: return reinterpret_cast<std::uintptr_t>(&commit);
    case 0xE2D510: return reinterpret_cast<std::uintptr_t>(&step);
    case 0x41B040:return reinterpret_cast<std::uintptr_t>(&video_manager);
    case 0x41B420:return reinterpret_cast<std::uintptr_t>(&video_playing);
    case 0x1764D20:return reinterpret_cast<std::uintptr_t>(&video_flag);
    case 0x1764C60:return reinterpret_cast<std::uintptr_t>(&video_reset);
    case 0xC294B0:return reinterpret_cast<std::uintptr_t>(&video_index);
    case 0xE2DEB0:return reinterpret_cast<std::uintptr_t>(&leave);
    default: return 0;
    }
}
}
int main(int argc, char** argv) {
    check(argc == 2 || (argc==3 && std::string_view(argv[2])=="--homecoming-only"), "provide installed activity table and optional --homecoming-only");
    const bool homecomingOnly=argc==3;
    std::ifstream file(argv[1], std::ios::binary | std::ios::ate); check(file.good(), "fixture opens");
    const auto size = file.tellg(); check(size > 0, "fixture populated");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size)); file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size); check(file.good(), "fixture read");
    static std::array<build::activities::Definition, build::activities::kCapacity> rows{}; std::size_t count{};
    check(dawn::middleware::content::packages::tables::activities::decode(bytes, rows, count), "decode installed catalog");
    check(build::activities::publish(std::span(rows).first(count)), "publish catalog");
    {
        intro::Sequence nativeChain;
        nativeChain.begin(1000);check(nativeChain.queued(289,1000),"queue native campaign chain");
        // Retail can render both videos without polling the 3D camera owner.
        // Its next observable event is the exact briefing's host allocation.
        nativeChain.tick(481000);
        nativeChain.selected(7,291,intro::kScenario,481000);
        check(nativeChain.frame(7).enabled && nativeChain.state().phase==intro::Phase::preparing,
            "briefing binds after full-length native videos even with no intervening camera or orbit polls");
        check(!nativeChain.frame(6).enabled && nativeChain.wanted()<0,
            "stale run and uncompleted briefing cannot launch Gateway");
    }
    put(g_manager.data(), 0x10, std::int32_t{0});
    put(g_manager.data(), 0x18 + 0x1AEF8, std::int32_t{4});
    put(g_manager.data(), 0x18 + 0xE93C, std::int32_t{0});
    check(forced::publish(launch::openings::kOmegaOpening), "previous operator override");

    check(launch::request_opening(1), "Gateway request"); launch::poll();
    check(launch::snapshot().status == launch::Status::preparing && launch::snapshot().busy
        && g_selects == 0 && g_commits == 0, "delayed native publication readiness defers launch");
    unchanged(launch::openings::kOmegaOpening);
    check(!launch::request_opening(2), "preparing request immutable");
    g_now += 10001; launch::poll();
    check(launch::snapshot().status == launch::Status::prelaunchUnavailable && !launch::snapshot().busy
        && g_selects == 0 && g_commits == 0, "missing support times out without submitting any activity");
    unchanged(launch::openings::kOmegaOpening);
    g_descriptorValid = false;
    check(launch::request_opening(1), "rejected descriptor request"); launch::poll();
    check(launch::snapshot().status == launch::Status::descriptorRejected && g_selects == 0,
        "invalid native descriptor never launches");
    unchanged(launch::openings::kOmegaOpening); g_descriptorValid = true;

    g_dialogueReady=false;
    check(launch::request_opening(5),"Tree campaign queues while dialogue flag is pending");launch::poll();
    check(launch::snapshot().status==launch::Status::preparing && g_selects==0 && g_commits==0,
        "campaign launch waits for evaluated native dialogue flag");
    g_now+=10001;launch::poll();
    check(launch::snapshot().status==launch::Status::prelaunchUnavailable && !launch::snapshot().busy
        && g_selects==0,"unavailable flag cannot launch wrong conversation");
    g_dialogueReady=true;orbit();
    check(g_dialogueActivity==-1,"failed launch restores dialogue policy in orbit");
    for (std::size_t i = 0; i < launch::openings::kMissions.size(); ++i) {
        if(homecomingOnly && launch::openings::kMissions[i].activity!=prologue::kMission) continue;
        orbit(); g_hooksReady = false;
        check(launch::request_opening(i), "each opening can queue");
        const auto before = g_selects;
        launch::poll();
        if (prelaunch::configured(launch::openings::kMissions[i].destination)) {
            if(launch::snapshot().status!=launch::Status::preparing || g_selects!=before)
                std::fprintf(stderr,"profile=%zu status=%u selects=%u before=%u\n",i,
                    static_cast<unsigned>(launch::snapshot().status),g_selects,before);
            check(launch::snapshot().status == launch::Status::preparing && g_selects == before,
                "each authored profile waits for hooks");
            g_hooksReady = true; launch::poll();
        }
        if(launch::openings::kMissions[i].activity==prologue::kMission) {
            check(g_selects==before+1 && g_commits==g_selects,"the opening video submits once");
            finish_prologue();
            launch::poll();check(launch::snapshot().inMission,"presence persists after the chained arrival");
            check(!launch::request_opening(i),"in-mission launch blocked after the chained arrival");
            // No camera poll occurs on this captured failure path. Only the
            // current primary-session update may clean up and launch again.
            const auto primary=reinterpret_cast<std::uintptr_t>(g_manager.data())+0x18;
            g_step=28;launch::poll_orbit(primary);
            check(launch::snapshot().inMission,"orbit fallback cannot run during cleanup/loading");
            g_step=29;launch::poll_orbit(primary+0x1C8A0);
            check(launch::snapshot().inMission,"secondary session cannot run the orbit fallback");
            launch::poll_orbit(primary);
            check(!launch::snapshot().inMission && launch::snapshot().status==launch::Status::idle && g_dialogueActivity==-1,
                "camera-less orbit resets Homecoming presence and restores its dialogue lease");
            check(launch::request_opening(0),"Homecoming can queue again without a camera callback");
            const auto selected=g_selects;
            g_reenterOrbit=true;
            launch::poll_orbit(primary);
            check(g_reenteredOrbit && g_selects==selected+1 && launch::snapshot().status==launch::Status::cinematics,
                "primary-session orbit fallback submits the queued opening exactly once");
            launch::poll_orbit(primary);
            check(g_selects==selected+1,"repeated orbit updates do not duplicate native selection");
            finish_prologue();g_step=29;launch::poll_orbit(primary);
            continue;
        }
        if(i==1) finish_intro();
        const auto expected=before+(i==1?2U:1U);
        check(g_selects == expected && g_commits == g_selects && launch::snapshot().status == launch::Status::queued,
            "ready opening submits once");
        launch::poll(); check(g_selects == expected && !launch::snapshot().inMission, "orbit does not imply arrival");
        arrive();
        check(g_dialogueActivity==g_actual.activityIndex,"arrival keeps selected dialogue policy");
        const auto state = launch::snapshot();
        check(state.status == launch::Status::arrived && !state.busy && state.inMission
            && state.current_name() == launch::destination_name(launch::openings::kMissions[i].destination),
            "actual mission arrival resolves selected opening");
        launch::poll(); check(launch::snapshot().inMission, "presence persists after request completion");
        check(!launch::request_opening(i), "in-mission launch blocked");
        orbit(); check(!launch::snapshot().inMission && launch::snapshot().status == launch::Status::idle,
            "return to orbit resets presence and arrival status");
        check(g_dialogueActivity==-1,"each orbit return restores dialogue policy");
    }
    {
        // Director launch as well as launcher launch: completion belongs to the
        // loaded mission, not to a stale UI selection.
        orbit();g_actual=descriptor(266,"mission_towerfall");++g_session;g_step=38;launch::poll();
        const auto before=g_selects,leaves=g_leaves;
        check(!launch::request_opening(12),"UI still cannot replace an active mission");
        launch::poll();check(!launch::snapshot().busy && g_selects==before,"no continuation before the ending receipt");
        g_completedHomecoming={g_session-1,1};launch::poll();
        check(!launch::snapshot().busy,"stale completion cannot queue Exodus");
        g_completedHomecoming={g_session,1};g_actual=descriptor(281,"mission_skybox");launch::poll();
        check(!launch::snapshot().busy,"a different loaded activity cannot consume Homecoming completion");
        g_actual=descriptor(266,"mission_towerfall");g_sessionReady=false;launch::poll();launch::poll();
        check(launch::snapshot().status==launch::Status::preparing && g_selects==before && g_leaves==leaves,
            "completed Homecoming waits for native session readiness without leaving");
        g_sessionReady=true;g_hooksReady=false;launch::poll();
        check(launch::snapshot().status==launch::Status::preparing && g_selects==before,"Exodus waits for its own installed support");
        g_hooksReady=true;put(g_manager.data(),0x18+0x182C0+0x140,std::uint8_t{0});launch::poll();
        check(launch::snapshot().busy && g_selects==before,"no transition can queue without a native nonce");
        put(g_manager.data(),0x18+0x182C0+0x140,std::uint8_t{1});
        put(g_manager.data(),0x18+0x182C0+0x148+0x18,std::uint64_t{6789});
        put(g_manager.data(),0x18+0x182C0+0x148+4,std::int16_t{266});
        launch::poll();
        check(launch::snapshot().status==launch::Status::queued && g_selects==before+1 && g_submitted.activityIndex==288
            && g_submittedNonce==6789 && g_submitted.arrivalBubbleOverride==3 && g_submitted.sliceSetOverride==25,
            "outro queues native Exodus once at its vision opening with the current nonce");
        launch::poll();check(g_leaves==leaves && !launch::suppress_loading() && g_dialogueActivity==288,
            "old activity cannot cause departure or restore the Homecoming dialogue policy");
        put(g_manager.data(),0x18+0x182C0+0x148+4,std::int16_t{288});
        put(g_manager.data(),0x18+0x182C0+0x378,std::uint8_t{0xFF});launch::poll();
        check(g_leaves==leaves,"unfilled native transition cannot leave Homecoming");
        put(g_manager.data(),0x18+0x182C0+0x378,std::uint8_t{0});
        put(g_manager.data(),0x18+0x182C0+0x148+0x18,std::uint64_t{6790});launch::poll();
        check(g_leaves==leaves,"foreign transition nonce cannot leave Homecoming");
        put(g_manager.data(),0x18+0x182C0+0x148+0x18,std::uint64_t{6789});launch::poll();
        check(g_step==28 && g_leaves==leaves+1 && launch::suppress_loading(),"confirmed Exodus selection owns native teardown and loading-only suppression");
        launch::poll();check(g_selects==before+1 && g_leaves==leaves+1,"repeated polls cannot duplicate the selection or departure");
        arrive();
        check(launch::snapshot().status==launch::Status::arrived && launch::snapshot().current_name()=="mission_journey"
            && !launch::suppress_loading(),"new Exodus session completes handoff and clears loading presentation before its vision");
        launch::poll();check(g_selects==before+1,"old Homecoming receipt cannot restart Exodus");
        g_completedHomecoming={};orbit();
        // A new Homecoming run can try again, but support failure never loops.
        g_actual=descriptor(266,"mission_towerfall");++g_session;g_step=38;g_completedHomecoming={g_session,1};
        g_hooksReady=false;launch::poll();launch::poll();g_now+=10001;launch::poll();
        check(launch::snapshot().status==launch::Status::prelaunchUnavailable && !launch::snapshot().busy
            && g_selects==before+1 && g_leaves==leaves+1,"missing Exodus support fails without selecting or tearing down Homecoming");
        launch::poll();check(!launch::snapshot().busy,"a failed handoff does not retry every frame");
        g_completedHomecoming={};g_hooksReady=true;orbit();
    }
    if(homecomingOnly) {
        std::cout << "PASS: " << g_checks << " Homecoming launch, orbit cleanup, replay and reentry checks\n";
        return 0;
    }
    g_hooksReady = true; g_wrongDestination = true;
    for (const auto& variant : dawn::state::activity::strikes::kVariants) {
        g_wrongDestination = false; orbit();
        check(launch::request_variant(variant.package == "strike_pact" ? 9 : 10, variant.difficulty),
            "strike difficulty queues after returning to orbit");
        const auto before = g_selects; launch::poll();
        check(g_selects == before + 1 && launch::snapshot().index == variant.activity,
            "native descriptor uses exact difficulty activity");
        arrive();
        check(launch::snapshot().status == launch::Status::arrived
            && launch::snapshot().currentIndex == variant.activity && g_dialogueActivity == variant.activity,
            "difficulty survives descriptor publication and arrival");
        check(!launch::request_variant(9, dawn::state::activity::strikes::Difficulty::adept),
            "difficulty switching requires return to orbit");
    }
    orbit();
    check(launch::request_variant(9, dawn::state::activity::strikes::Difficulty::grandmaster),
        "Grandmaster queues for wrong-tier arrival check");
    launch::poll();
    g_submitted.activityIndex = 830;
    arrive();
    check(launch::snapshot().status == launch::Status::unexpectedDestination,
        "same strike package at Adept cannot confirm a Grandmaster arrival");
    orbit(); g_wrongDestination = true;
    check(launch::request_opening(1), "wrong destination scenario"); launch::poll(); finish_intro();arrive();
    check(launch::snapshot().status == launch::Status::unexpectedDestination && !launch::snapshot().busy
        && launch::snapshot().inMission && launch::snapshot().current_name() == "mission_reunion",
        "Chosen arrival reports actual mission and ends pending Gateway immediately");
    orbit(); g_wrongDestination = false;
    check(launch::request_opening(1), "Gateway replay"); launch::poll(); finish_intro();arrive();
    check(launch::snapshot().status == launch::Status::arrived, "Gateway replay commits a fresh opening");
    orbit();
    // A Director launch has no Dawn request but must still publish presence.
    g_actual = descriptor(292, "mission_abs"); ++g_session; g_step = 38; launch::poll();
    check(launch::snapshot().inMission && launch::snapshot().current_name() == "mission_abs"
        && !launch::snapshot().busy, "Director launch tracks current mission");
    g_session = 0; launch::poll(); check(!launch::snapshot().inMission, "missing session cannot claim arrival");
    check(g_prepareCalls > 8, "readiness seam exercised");
    std::cout << "PASS: " << g_checks << " prelaunch ordering, real override, arrival, replay and presence checks\n";
}
