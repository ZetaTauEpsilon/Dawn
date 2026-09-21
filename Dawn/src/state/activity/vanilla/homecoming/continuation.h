#pragma once
#include "controller.h"

namespace dawn::state::activity::vanilla::homecoming::continuation {
inline constexpr std::int16_t kSourceActivity=266,kTargetActivity=288;
inline constexpr std::string_view kSourcePackage="mission_towerfall",kTargetPackage="mission_journey";
// Completion of gameplay, an offered movie, and a skip request are not an exit
// receipt. Only the completed outro of this exact controller lease may hand off.
inline bool ready(coo::Generation owner,const Frame& frame) noexcept {
    return owner.valid() && frame.enabled && !frame.fault && frame.finished
        && frame.completion.valid() && frame.completion.owner==owner
        && frame.cinematic.owner==owner && frame.cinematic.movie==cinematics::kOutro
        && frame.cinematic.phase==cinematics::Phase::complete && !frame.cinematic.play;
}
inline std::uint8_t lifetime(const Frame& frame,std::uint8_t fallback) noexcept {
    // State 8 is neither an encodable snapshot lifetime nor a campaign handoff.
    // Keep success alive; the native committed destination owns world teardown.
    return ready(frame.cinematic.owner,frame)?6:fallback;
}
inline std::uint64_t publication_interval(const Frame& frame) noexcept {return frame.finished?1000:100;}
// Small, synchronized receipt query: no full mission Frame copy on camera ticks.
coo::Generation request() noexcept;
}
