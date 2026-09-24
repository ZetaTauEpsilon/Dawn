#pragma once

namespace dawn::client::hooks::bootflow::tower_watch_probe {

// The legacy scene/spawner pool snapshots are diagnostic work, not mission
// feedback. They invoke native lookups on the game thread on every type-68
// apply (about 8 Hz), including unchanged objectives and revisits. Keep them
// out of normal builds; explicitly opt in only for a diagnostic build.
#if defined(DAWN_ENABLE_TOWER_WATCH_SLOT_PROBES) && DAWN_ENABLE_TOWER_WATCH_SLOT_PROBES
inline constexpr bool kEnabled = true;
#else
inline constexpr bool kEnabled = false;
#endif

template<bool Enabled = kEnabled, class IsTowerWatch>
[[nodiscard]] bool active(IsTowerWatch&& isTowerWatch) noexcept {
    if constexpr (Enabled) {
        return isTowerWatch();
    } else {
        (void)isTowerWatch;
        return false;
    }
}

} // namespace dawn::client::hooks::bootflow::tower_watch_probe
