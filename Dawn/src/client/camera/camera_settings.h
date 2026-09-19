#pragma once

#include "key_bindings.h"

namespace dawn::client::camera {

enum class Mode { normal, rear, front };

/** Disabled legacy settings always represent normal, even with a remembered front view. */
[[nodiscard]] constexpr Mode mode_from_flags(bool enabled, bool front) noexcept {
    return enabled ? (front ? Mode::front : Mode::rear) : Mode::normal;
}

[[nodiscard]] constexpr Mode next_mode(Mode current) noexcept {
    switch (current) {
    case Mode::normal: return Mode::rear;
    case Mode::rear: return Mode::front;
    case Mode::front: return Mode::normal;
    }
    return Mode::normal;
}

[[nodiscard]] constexpr Mode toggled_mode(Mode current, Mode target) noexcept {
    return current == target ? Mode::normal : target;
}

/** Camera preference lives in Dawn/camera.json alongside the loaded DLL. */
void initialize(void* module) noexcept;
void shutdown() noexcept;
[[nodiscard]] Mode mode() noexcept;
void set_mode(Mode value) noexcept;
void cycle_mode() noexcept;
void toggle_mode(Mode target) noexcept;
[[nodiscard]] Bindings bindings() noexcept;
[[nodiscard]] bool set_binding(Action action, std::uint32_t key) noexcept;
void reset_bindings() noexcept;
[[nodiscard]] bool third_person_enabled() noexcept;
/** Session-only: start each launch with the HUD and models visible. */
[[nodiscard]] bool camera_only() noexcept;
void set_camera_only(bool enabled) noexcept;
void toggle_camera_only() noexcept;

} // namespace dawn::client::camera
