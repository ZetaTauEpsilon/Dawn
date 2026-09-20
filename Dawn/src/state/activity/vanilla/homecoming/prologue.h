#pragma once
#include "native_catalog.h"
#include "../../../build_data/activities/activity_catalog.h"
#include "../../../../core/logging/log.h"
#include <array>
#include <cstdio>
#include <mutex>
#include <span>
#include <string_view>

namespace dawn::state::activity::vanilla::homecoming::prologue {
// The Red War opening is the pre-rendered video activity that precedes Homecoming in the
// public activity table: entry 265 has no scenario package, carries the same video
// presentation block as Gateway's nameless entries 289 and 290, and its first chain link
// is the mission (266, link kind 2), the kind retail advances by itself. The launch
// adapter queues the video with the mission's opening override already published; the
// exact loaded mission scenario is the chain's arrival receipt. Should the client stay in
// orbit after the video instead, the adapter launches the mission itself.
inline constexpr std::int16_t kVideo=265,kMission=266;
inline constexpr std::uint32_t kVideoHash=0xF07A75D3U,kMissionHash=0x62D85FB3U;
// Retail passes through orbit between the video and the mission within a frame or two;
// an orbit step that outlasts this grace means the native chain did not follow.
inline constexpr std::uint64_t kOrbitGraceMs=15000;
inline bool catalog_valid(std::span<const build_data::activities::Definition> rows) noexcept {
    return rows.size()>static_cast<std::size_t>(kMission) && rows[kVideo].hash==kVideoHash && rows[kVideo].name().empty()
        && rows[kMission].hash==kMissionHash && rows[kMission].name()==kPackage;
}
enum class Phase : std::uint8_t {
    idle,videoRequested,videoLoading,videoPlaying,videoReturning,missionRequested,missionLoading,complete,failed
};
struct State {Phase phase{};std::uint64_t deadline{},orbitSince{},run{};};
class Sequence {
public:
    void begin(std::uint64_t now) noexcept {s={};s.phase=Phase::videoRequested;s.deadline=now+30000;}
    void fail() noexcept {s.phase=Phase::failed;s.deadline=0;}
    void complete() noexcept {s.phase=Phase::complete;s.deadline=0;}
    std::int16_t wanted() const noexcept {
        if(s.phase==Phase::videoRequested) {return kVideo;}
        if(s.phase==Phase::missionRequested) {return kMission;}
        return -1;
    }
    bool queued(std::int16_t index,std::uint64_t now) noexcept {
        if(index<0 || index!=wanted()) {return false;}
        // The opening runs about two and a half minutes; presentation can stop the camera poll.
        s.phase=index==kMission?Phase::missionLoading:Phase::videoLoading;
        s.deadline=now+(index==kVideo?600000:180000);s.orbitSince=0;return true;
    }
    // Native playback of the queued video at step 39 and its completion. Afterwards retail
    // advances into the mission itself; only an orbit step that outlasts the chain grace
    // hands the mission launch to the adapter.
    void video(std::int32_t step,std::int16_t index,bool playing,bool finished,std::uint64_t now) noexcept {
        if(step==39 && index==kVideo) {
            if(s.phase==Phase::videoLoading && playing && !finished) {s.phase=Phase::videoPlaying;s.deadline=now+600000;}
            if(s.phase==Phase::videoPlaying && !playing && finished) {s.phase=Phase::videoReturning;s.deadline=now+180000;s.orbitSince=0;}
        }
        if(s.phase!=Phase::videoReturning) {return;}
        if(step!=29) {s.orbitSince=0;return;}
        if(!s.orbitSince) {s.orbitSince=now;return;}
        if(now-s.orbitSince>=kOrbitGraceMs) {s.phase=Phase::missionRequested;s.deadline=now+30000;s.orbitSince=0;}
    }
    // The exact loaded Homecoming scenario is the chain's arrival receipt, whichever side
    // selected it.
    void selected(std::uint64_t run,std::int16_t index,std::uint32_t scenario,std::uint64_t now) noexcept {
        if(s.phase<Phase::videoLoading || s.phase>Phase::missionLoading || !run || index!=kMission || scenario!=kScenario) {return;}
        s.run=run;
        if(s.phase!=Phase::missionLoading) {s.phase=Phase::missionLoading;s.deadline=now+180000;s.orbitSince=0;}
    }
    void tick(std::uint64_t now) noexcept {if(s.deadline && now>=s.deadline) {fail();}}
    const State& state() const noexcept {return s;}
private:
    State s{};
};
inline std::mutex mutex;
inline Sequence sequence;
inline void report(std::string_view event,const State& s) noexcept {
    std::array<char,160> line{};std::snprintf(line.data(),line.size(),
        "ev=homecoming stage=prologue_%.*s phase=%u run=%llu",
        static_cast<int>(event.size()),event.data(),static_cast<unsigned>(s.phase),static_cast<unsigned long long>(s.run));
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
}
inline State state() noexcept {const std::lock_guard lock(mutex);return sequence.state();}
inline bool active() noexcept {const auto p=state().phase;return p>Phase::idle && p<Phase::complete;}
// The orbit-to-video load and the video's own return keep their loading presentation
// suppressed; the mission load shows the normal Homecoming loading.
inline bool suppress_loading() noexcept {const auto p=state().phase;return p>Phase::idle && p<Phase::missionLoading;}
inline void begin(std::uint64_t now) noexcept {const std::lock_guard lock(mutex);sequence.begin(now);report("begin",sequence.state());}
inline void fail() noexcept {const std::lock_guard lock(mutex);if(sequence.state().phase==Phase::idle) {return;}sequence.fail();report("failed",sequence.state());}
inline void complete() noexcept {const std::lock_guard lock(mutex);sequence.complete();report("complete",sequence.state());}
inline void tick(std::uint64_t now) noexcept {const std::lock_guard lock(mutex);const auto before=sequence.state().phase;sequence.tick(now);if(before!=sequence.state().phase) {report("timeout",sequence.state());}}
inline std::int16_t wanted() noexcept {const std::lock_guard lock(mutex);return sequence.wanted();}
inline bool queued(std::int16_t index,std::uint64_t now) noexcept {const std::lock_guard lock(mutex);const bool ok=sequence.queued(index,now);if(ok) {report("queued",sequence.state());}return ok;}
inline void video(std::int32_t step,std::int16_t index,bool playing,bool finished,std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);const auto before=sequence.state().phase;sequence.video(step,index,playing,finished,now);
    if(before!=sequence.state().phase) {report("native_video",sequence.state());}
}
inline void selected(std::uint64_t run,std::int16_t index,std::uint32_t scenario,std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);const auto before=sequence.state().phase;sequence.selected(run,index,scenario,now);
    if(before!=sequence.state().phase) {report("mission_selected",sequence.state());}
}
}
