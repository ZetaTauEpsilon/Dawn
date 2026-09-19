#pragma once

#include <cstdint>

namespace dawn::client::hooks::camera::clean_view {
[[nodiscard]] bool install() noexcept;
void quiesce() noexcept;
[[nodiscard]] bool uninstall() noexcept;
[[nodiscard]] bool installed() noexcept;
void local_player(std::uint32_t entity) noexcept;
}
