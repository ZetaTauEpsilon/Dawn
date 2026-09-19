#pragma once

#include <cstdint>

namespace dawn::client::hooks::camera {

[[nodiscard]] bool install() noexcept;
void quiesce() noexcept;
[[nodiscard]] bool uninstall() noexcept;
[[nodiscard]] bool is_installed() noexcept;
/** Called once per local camera frame by the existing teleport camera hook. */
void poll_toggle(std::uint32_t playerIndex) noexcept;

} // namespace dawn::client::hooks::camera
