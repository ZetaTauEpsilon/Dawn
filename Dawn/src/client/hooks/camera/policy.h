#pragma once

#include <cstdint>
#include <cmath>

namespace dawn::client::hooks::camera::detail {

inline constexpr std::int32_t kFollowing = 0;
inline constexpr std::int32_t kFirstPerson = 4;

struct Vector3 final { float x{}, y{}, z{}; };
struct Orientation final { Vector3 forward{}, up{}; };
static_assert(sizeof(Orientation) == 24);

/** The native following result contains forward/up vectors at +0x3C/+0x48.
 * Rotate their heading by pi about world Z, preserving pitch and an upright horizon.
 * Native placement subsequently uses this basis for the offset from the character.
 */
[[nodiscard]] inline bool face_front(Orientation& orientation) noexcept {
    const auto dot = [](Vector3 a, Vector3 b) noexcept {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };
    const float forwardLength = dot(orientation.forward, orientation.forward);
    const float upLength = dot(orientation.up, orientation.up);
    const float perpendicular = dot(orientation.forward, orientation.up);
    if (!std::isfinite(forwardLength) || !std::isfinite(upLength)
        || !std::isfinite(perpendicular) || forwardLength < 0.9F || forwardLength > 1.1F
        || upLength < 0.9F || upLength > 1.1F || std::fabs(perpendicular) > 0.1F) {
        return false;
    }
    orientation.forward.x = -orientation.forward.x;
    orientation.forward.y = -orientation.forward.y;
    orientation.up.x = -orientation.up.x;
    orientation.up.y = -orientation.up.y;
    return true;
}

[[nodiscard]] constexpr bool apply_front(bool enabled, bool front, bool ownedFollowing,
                                        std::uint32_t playerIndex) noexcept {
    return enabled && front && ownedFollowing && playerIndex == 0;
}

/** Only the ordinary local gameplay request may be changed. */
[[nodiscard]] constexpr std::int32_t select_mode(bool enabled, bool localGameplay,
                                                 bool gameplayRequest,
                                                 std::int32_t requested) noexcept {
    return enabled && localGameplay && gameplayRequest && requested == kFirstPerson
               ? kFollowing : requested;
}

/** Consume presses in menus/background too, so returning with a held key cannot toggle. */
struct ToggleKey final {
    bool wasDown{};
    bool observed{};

    [[nodiscard]] bool poll(bool down, bool focused, bool interfaceVisible) noexcept {
        const bool pressed = observed && down && !wasDown;
        observed = true;
        wasDown = down;
        return pressed && focused && !interfaceVisible;
    }
};

/** A new binding starts with a fresh edge, consuming any key already held. */
struct BoundKey final {
    std::uint32_t key{};
    ToggleKey edge{};
    [[nodiscard]] bool poll(std::uint32_t binding, bool down, bool focused, bool menu) noexcept {
        if (key != binding) { key = binding; edge = {}; }
        return edge.poll(binding != 0 && down, focused, menu);
    }
};

/** Native visibility commands carry desired bits plus a mask of bits to replace. */
struct VisibilityMask final {
    std::uint8_t saved{};
    bool hidden{};
    void hide(std::uint8_t current) noexcept { saved = current; hidden = true; }
    void update(std::uint8_t values, std::uint8_t mask) noexcept {
        saved = static_cast<std::uint8_t>((saved & ~mask) | (values & mask));
    }
};

/** A first-person environment (5) requires its weapon view (2..4) to exist even
 * when that view has no drawing. Filter submissions, never the native view list.
 */
[[nodiscard]] constexpr bool omit_weapon_draw(bool enabled, std::uint32_t viewType) noexcept {
    return enabled && viewType >= 2 && viewType <= 4;
}

} // namespace dawn::client::hooks::camera::detail
