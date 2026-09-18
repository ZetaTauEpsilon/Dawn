#pragma once
#include <bit>
#include <cstdint>
namespace dawn::state::activity::vanilla::one_au {
// Native 80804F48: position, power and lock each have a revision and snap flag.
// A position alone cannot move an unpowered or locked device.
template<class W> bool device_authority(W& w,float position,float power,float lock,std::uint32_t revision,bool snap) noexcept {
    if(!revision || revision>=32767) {return false;}
    const auto channel=[&](float value) {
        return w.write(std::bit_cast<std::uint32_t>(value),32) && w.write(revision+0x8000U,16) && w.write(snap?1U:0U,1);
    };
    return channel(position) && channel(power) && channel(lock);
}
}
