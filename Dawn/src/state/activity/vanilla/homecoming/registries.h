#pragma once
#include <cstdint>

namespace dawn::state::activity::vanilla::homecoming::registries {
// Homecoming's plaza registry sits in the second registry array of bubble 6 with an
// explicit slice, so the ordinary roster walk classifies it as not relevant. This
// catalog admits its descriptor layout to the scenario cache exactly like the lost
// sector and open-world catalogs do; it never makes the object ordinary, adds it to a
// wire roster, or enables a native source. The mission roster admits it by key.
struct Group final { std::uint32_t scenario, object, key; std::uint8_t bubble; };
inline constexpr Group kGroups[]{
    {0x80B500BCU, 0x80B50746U, 0x28A6B21FU, 6},
};
[[nodiscard]] constexpr bool required(std::uint32_t scenario, std::uint32_t objectTag,
    std::uint32_t key, std::uint64_t explicitBubbleMask) noexcept {
    for (const auto& row : kGroups) {
        if (row.scenario == scenario && row.object == objectTag && row.key == key
            && explicitBubbleMask == (std::uint64_t{1} << row.bubble)) { return true; }
    }
    return false;
}
} // namespace dawn::state::activity::vanilla::homecoming::registries
