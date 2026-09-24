#pragma once
#include <cstdint>

namespace dawn::state::activity::vanilla::one_au {
// One clock for the two native graphs. Ring 80C3ED9F draws in three 3s stages,
// each followed by a .25s wait, then waits 3s before its 1/.6/.3s throw curves.
// Laser 80B71179 starts its visible burst 3s into a 10.5s firing sequence. Start
// that sequence at the ring's last wait so the burst and throw coincide.
// The 30s period is measured from the reference; the remaining 9.75s is the
// clamshell exposure window. Native graphs own the motion, audio and screen FX.
class VentCycle final {
public:
    enum class Phase : std::uint8_t { idle, charging, windup, surging, cooldown };
    static constexpr std::uint64_t kChargingMs=9750,kWindupMs=3000,kSurgingMs=7500,kCooldownMs=9750;
    static constexpr std::uint64_t kCycleMs=kChargingMs+kWindupMs+kSurgingMs+kCooldownMs;
    // One initial idle window before the first draw; subsequent cycles do not
    // repeat it. The clock starts with the encounter, independently of VFX replies.
    static constexpr std::uint64_t kInitialIdleMs=250;
    static_assert(kCycleMs==30000);
    Phase sample(std::uint64_t now) noexcept {
        if(!started_) {started_=true;origin_=latest_=now;}
        if(now>latest_) {latest_=now;}
        const auto age=latest_-origin_;
        if(age<kInitialIdleMs) {return Phase::idle;}
        const auto elapsed=(age-kInitialIdleMs)%kCycleMs;
        if(elapsed<kChargingMs) {return Phase::charging;}
        if(elapsed<kChargingMs+kWindupMs) {return Phase::windup;}
        if(elapsed<kChargingMs+kWindupMs+kSurgingMs) {return Phase::surging;}
        return Phase::cooldown;
    }
    std::uint64_t elapsed() const noexcept {
        const auto age=latest_-origin_;
        return age<kInitialIdleMs?0:(age-kInitialIdleMs)%kCycleMs;
    }
private:
    std::uint64_t origin_{},latest_{};bool started_{};
};
}
