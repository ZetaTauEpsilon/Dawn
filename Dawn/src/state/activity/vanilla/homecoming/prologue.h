#pragma once
#include "cinematics.h"
#include "../../Newlight/launchpad/tower.h"
#include "../../omega_ending_transit_rules.h"
#include "../../runtime.h"
#include "../../../../core/logging/log.h"
#include <cstdio>
#include <mutex>

namespace dawn::state::activity::vanilla::homecoming::prologue {
namespace native=omega_ending_transit;
namespace tower=newlight::launchpad::tower;
// The Red War opening is two activities: the Tower cinematic (public activity 2,
// cine_110_twr, scenario 80B4A0EA) plays its authored movie 32DDAD77 in slice set 25,
// then Homecoming itself (activity 266) launches from inside that world. The launch
// adapter drives this chain exactly like Gateway's briefing; the roster, membership and
// authority integration reuse the Tower approach bookend.
inline constexpr std::int16_t kTowerCinematic=tower::kApproachActivity,kMission=266;
inline constexpr std::uint32_t kScenario=tower::kApproachScenario;
inline constexpr std::string_view kPackage="cine_110_twr";
inline constexpr std::uint8_t kSliceSet=25;
inline constexpr std::uint32_t kSliceHash=0x2EA8FB98U;
using Frame=tower::Frame;
enum class Phase : std::uint8_t {
    idle,cinematicRequested,cinematicLoading,preparing,offered,playing,stopping,
    missionRequested,missionLoading,complete,failed
};
struct State {
    Phase phase{};std::uint64_t deadline{},run{},member{},runtime{UINT64_MAX};ActivityInstanceKey activity{};
    native::Teleport host{};std::uint32_t revision{};bool play{};
};
class Sequence {
public:
    void begin(std::uint64_t now) noexcept {s={};s.phase=Phase::cinematicRequested;s.deadline=now+30000;}
    void fail() noexcept {s.play=false;++s.revision;s.phase=Phase::failed;s.deadline=0;}
    void complete() noexcept {s.play=false;s.phase=Phase::complete;s.deadline=0;}
    std::int16_t wanted() const noexcept {
        if(s.phase==Phase::cinematicRequested) {return kTowerCinematic;}
        if(s.phase==Phase::missionRequested && s.host.state==0) {return kMission;}
        return -1;
    }
    bool queued(std::int16_t index,std::uint64_t now) noexcept {
        if(index<0 || index!=wanted()) {return false;}
        s.phase=index==kMission?Phase::missionLoading:Phase::cinematicLoading;s.deadline=now+180000;return true;
    }
    // The exact loaded Tower cinematic scenario is the native chain's arrival receipt.
    void selected(std::uint64_t run,std::int16_t index,std::uint32_t scenario,std::uint64_t now) noexcept {
        if(s.phase!=Phase::cinematicLoading || !run || index!=kTowerCinematic || scenario!=kScenario) {return;}
        s.run=run;s.phase=Phase::preparing;s.deadline=now+90000;s.revision=1;
    }
    Frame frame(std::uint64_t run) const noexcept {
        return {s.run,s.revision,run!=0 && run==s.run && s.phase>=Phase::preparing && s.phase<=Phase::missionLoading,s.play};
    }
    bool incident(std::uint64_t run,const cinematics::Incident& e,std::uint64_t now) noexcept {
        if(run!=s.run || !run || e.registry!=tower::kMovie.registry || e.type!=6 || e.slot!=0 || e.runtime==UINT64_MAX) {return false;}
        if(e.target==5239 && s.phase==Phase::offered) {s.runtime=e.runtime;s.phase=Phase::playing;s.deadline=now+650000;return true;}
        if(e.runtime!=s.runtime) {return false;}
        if(e.target==3338 && s.phase==Phase::playing) {s.play=false;++s.revision;s.phase=Phase::stopping;s.deadline=now+15000;return true;}
        if(e.target!=1685 || (s.phase!=Phase::playing && s.phase!=Phase::stopping)) {return false;}
        s.play=false;++s.revision;s.phase=Phase::missionRequested;s.deadline=now+30000;return true;
    }
    native::Authority project(ActivityInstanceKey activity,std::uint64_t run,std::uint64_t member,native::Observation o,std::uint64_t now) noexcept {
        if(!frame(run).enabled || !activity || !member || member==UINT64_MAX) {return {};}
        if(!s.activity) {
            if(s.phase!=Phase::preparing || o.local.state!=0 || (!o.hasTeleport && o.local!=native::Teleport{})) {return {};}
            s.activity=activity;s.member=member;auto token=static_cast<std::uint8_t>(o.local.token+1);if(!token) {token=1;}
            s.host={1,token,kSliceSet,kSliceHash};
        }
        if(s.activity!=activity || s.member!=member) {return {};}
        if(o.hasTeleport && o.local.token==s.host.token && o.local.sliceSetIndex==s.host.sliceSetIndex && o.local.sliceSetHash==s.host.sliceSetHash) {
            if(s.host.state==1 && o.local.state==3 && o.hasRegion && o.currentRegion==kSliceSet) {
                s.host.state=3;s.phase=Phase::offered;s.play=true;++s.revision;s.deadline=now+60000;
            } else if(s.host.state==3 && o.local.state==0) {s.host.state=0;}
        }
        return {s.host,true,s.host.state!=1,s.host.state==0};
    }
    void tick(std::uint64_t now) noexcept {if(s.deadline && now>=s.deadline) {fail();}}
    const State& state() const noexcept {return s;}
private:
    State s{};
};
inline std::mutex mutex;
inline Sequence sequence;
inline void report(std::string_view event,const State& s) noexcept {
    std::array<char,192> line{};std::snprintf(line.data(),line.size(),
        "ev=homecoming stage=prologue_%.*s phase=%u run=%llu teleport=%d play=%u",
        static_cast<int>(event.size()),event.data(),static_cast<unsigned>(s.phase),
        static_cast<unsigned long long>(s.run),static_cast<int>(s.host.state),s.play?1U:0U);
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
}
inline State state() noexcept {const std::lock_guard lock(mutex);return sequence.state();}
inline bool active() noexcept {const auto p=state().phase;return p>Phase::idle && p<Phase::complete;}
// The orbit-to-cinematic load and the cinematic's own return keep their loading
// presentation suppressed; the mission launch shows the normal Homecoming loading.
inline bool suppress_loading() noexcept {const auto p=state().phase;return p>Phase::idle && p<Phase::missionLoading;}
inline void begin(std::uint64_t now) noexcept {const std::lock_guard lock(mutex);sequence.begin(now);report("begin",sequence.state());}
inline void fail() noexcept {const std::lock_guard lock(mutex);if(sequence.state().phase==Phase::idle) {return;}sequence.fail();report("failed",sequence.state());}
inline void complete() noexcept {const std::lock_guard lock(mutex);sequence.complete();report("complete",sequence.state());}
inline void tick(std::uint64_t now) noexcept {const std::lock_guard lock(mutex);const auto before=sequence.state().phase;sequence.tick(now);if(before!=sequence.state().phase) {report("timeout",sequence.state());}}
inline std::int16_t wanted() noexcept {const std::lock_guard lock(mutex);return sequence.wanted();}
inline bool queued(std::int16_t index,std::uint64_t now) noexcept {const std::lock_guard lock(mutex);const bool ok=sequence.queued(index,now);if(ok) {report("queued",sequence.state());}return ok;}
inline void selected(std::uint64_t run,std::int16_t index,std::uint32_t scenario,std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);const auto before=sequence.state().phase;sequence.selected(run,index,scenario,now);
    if(before!=sequence.state().phase) {report("cinematic_selected",sequence.state());}
}
inline Frame frame(std::uint64_t run) noexcept {const std::lock_guard lock(mutex);return sequence.frame(run);}
inline bool incident(std::uint64_t run,const cinematics::Incident& e,std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);const bool accepted=sequence.incident(run,e,now);if(accepted) {report("cinematic_incident",sequence.state());}return accepted;
}
inline native::Authority project(ActivityInstanceKey activity,std::uint64_t run,std::uint64_t member,bool exact,native::Observation o,std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);const auto before=sequence.state();
    const auto result=exact?sequence.project(activity,run,member,o,now):native::Authority{};
    if(before.host!=sequence.state().host || before.phase!=sequence.state().phase) {report("transit",sequence.state());}
    return result;
}
inline bool matches(const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {return tower::matches(f,key,type,slot);}
template<class W> bool write(W& w,const Frame& f) noexcept {return tower::write(w,f);}
}
