#pragma once
#include "mission.h"

namespace dawn::state::activity::vanilla::one_au {
inline constexpr auto kLever=asset(kBridge,4,19);
constexpr bool use_subscription(coo::Asset a) noexcept {
    return a==kLever || a==kInteractionAssets[1] || a==kInteractionAssets[2]
        || a==kInteractionAssets[3] || a==kInteractionAssets[4];
}
// Native 8080992F with a 80804FB8 subscription. The native controller owns
// the item criteria, hold duration and accepted-use reply; no predicate override.
template<class W> bool interactable(W& w,std::uint32_t generation,bool active) noexcept {
    const auto absent=[&] {return w.write(0x811C9DC5U,32) && w.write(0,7) && w.write(0x7FFF,16);};
    return w.write(generation^0x80000000U,32) && w.write(0x80000000U,32)
        && w.write(active?1U:0U,1) && w.write(0,1) && w.write(0x80000000U,32)
        && absent() && w.write(0,32) && w.write(0,32) && w.write(0,32)
        && w.write(0,1) && w.write(1,2) && w.write(1,1) && w.write(0x80804FB8U,32)
        && w.write(0,2) && absent() && w.write(0x80000000U,32) && w.write(0,1);
}
}
