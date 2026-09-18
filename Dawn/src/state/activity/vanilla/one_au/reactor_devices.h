#pragma once
#include "vent_cycle.h"
namespace dawn::state::activity::vanilla::one_au::reactor_devices {
// laser_object 80B3D4E2 -> entity 80B7117C -> graph 80B71179
// ring_object  80B3D4E8 -> entity 80B71218 -> graph 80C3ED9F
//
// The graph's root has SEPARATE flow/action tables; a float gate also names an
// input. Only input 6D408B83 is device position. The ring's other inputs are
// three damage flags and a busy flag, written by the graph itself.
//
// Ring position .1 selects flow 4, the entire draw/wait/throw sequence.
// Positions .2/.3/.4 disable outer/middle/inner sections through latched flags.
// Their nested failure branches own the judder curves and electrical effects;
// the normal curves multiply by the same flags. Position 0 resets ALL flags.
//
// Laser position 0 initializes all idle values plus the damage/enable flags;
// .1 is the named laser_idle state and .2 the named laser_fire state. The fire
// branch includes 3s-delayed 7.5s effects, a screen effect delayed 3.2s for 6s,
// and core curves lasting 10.25s. A 7.5s selector window truncated these tails.
// The healthy idle values in 0 and .1 match; 0 is not a separate low-power mode.
inline constexpr float kRingInitial=0.F,kRingCycle=.1F;
inline constexpr float kRingOuterFailed=.2F,kRingMiddleFailed=.3F,kRingInnerFailed=.4F;
inline constexpr float kLaserInitial=0.F,kLaserIdle=.1F,kLaserFire=.2F;
static_assert(kRingCycle>.0901F && kRingCycle<=.1001F
    && kLaserIdle>.0901F && kLaserIdle<=.1001F
    && kLaserFire>.1901F && kLaserFire<=.2001F);
inline constexpr float kPowerOn=1.F,kPowerOff=0.F;
constexpr float laser(VentCycle::Phase phase) noexcept {
    return phase==VentCycle::Phase::windup || phase==VentCycle::Phase::surging?kLaserFire:kLaserIdle;
}
constexpr float ring(VentCycle::Phase phase) noexcept {
    return phase==VentCycle::Phase::idle || phase==VentCycle::Phase::cooldown?kRingInitial:kRingCycle;
}

class Rings final {
public:
    // A failed section skips its guarded 3s draw plus .25s wait. Delay the
    // surviving draws by that amount, retaining the existing laser/alarm clock.
    static constexpr std::uint64_t kSectionMs=3250,kTraversalMs=13750;
    // Hold each selector over multiple 100ms authority publications. Native
    // flag writes have zero duration; no object/device acknowledgement is needed.
    static constexpr std::uint64_t kCommandHoldMs=250;
    float sample(std::uint64_t now,VentCycle::Phase phase,std::uint64_t elapsed,std::uint8_t destroyed) noexcept {
        if(now>latest_) {latest_=now;}
        if(destroyed>wanted_) {wanted_=destroyed<3?destroyed:3;}
        if(cycling_) {
            // Damage gates require the native busy flag to clear at the end of
            // the throw. Cancelling .1 during a draw would strand that flag.
            if(phase!=VentCycle::Phase::cooldown || latest_-cycleStart_<kTraversalMs-damaged_*kSectionMs) {return kRingCycle;}
            cycling_=false;
        }
        if(damaged_ && latest_-damageStart_<kCommandHoldMs) {return resting();}
        if(damaged_<wanted_) {
            ++damaged_;damageStart_=latest_;
            return resting(); // Never skip an intermediate flag, even for simultaneous kills.
        }
        if(damaged_==3 || phase==VentCycle::Phase::idle || phase==VentCycle::Phase::cooldown
            || elapsed<damaged_*kSectionMs) {return resting();}
        cycling_=true;cycleStart_=latest_;
        return kRingCycle;
    }
    std::uint8_t damaged() const noexcept {return damaged_;}
private:
    float resting() const noexcept {
        // Leave failed flags latched between shots. Returning 0 would repair
        // the rings and stop the native sparking/judder branches every cooldown.
        constexpr float modes[]{kRingInitial,kRingOuterFailed,kRingMiddleFailed,kRingInnerFailed};
        return modes[damaged_];
    }
    std::uint64_t latest_{},cycleStart_{},damageStart_{};
    std::uint8_t damaged_{},wanted_{};bool cycling_{};
};
}
