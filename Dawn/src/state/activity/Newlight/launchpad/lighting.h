#pragma once
#include "native_catalog.h"
#include "../../coo/native_device_authority.h"

namespace dawn::state::activity::newlight::launchpad::lighting {
// Keep the breach_group lighting channel separate from the m0_power_up door
// channel. The latter is shared by the rifle shutters, not the room lamps.
inline constexpr auto kSource=asset(kBreach,4,1);
inline constexpr std::size_t kAuthorityBits=573;
constexpr bool accepted(std::uint32_t generation,std::int32_t revision,float current,float target) noexcept {
    return generation>0 && generation<0x7FFFFFFFU
        && revision==static_cast<std::int32_t>(generation) && current==1.F && target==1.F;
}
// Type-4 dynamic state 80805063 is consumed by DF6510 on the native object
// update. Keep power/lock revisions absent. Apply this switch immediately:
// DF6C70 publishes the property notification before smooth interpolation;
// the authored device_position getter DF3ED0 reads current (+370), not target.
// A matching position/snap revision makes that first notification carry 1.
// Calling DF6BD0 from camera polling can stall the world job's event queue.
template<class Writer> bool write(Writer& w,std::uint32_t generation) noexcept {
    if(!generation || generation>=0x7FFFFFFFU) {return false;}
    const auto begin=w.bit_count();
    return coo::native_device::object_header(w,generation,true,1)
        && w.write(1,1) && w.write(0x80805063U,32)
        && w.write(0x7FFFFFFFU,32) && w.write(0x7FFFFFFFU,32) && w.write(0x3F800000U,32)
        && w.write(0x7FFFFFFFU,32) && w.write(0x7FFFFFFFU,32) && w.write(0,32)
        && w.write(generation^0x80000000U,32) && w.write(generation^0x80000000U,32)
        && w.write(0x3F800000U,32)
        && w.bit_count()-begin==kAuthorityBits;
}
}
