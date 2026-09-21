#pragma once
#include <cstdint>
namespace dawn::state::activity::vanilla::adieu::registries {
// The gameplay registry is in the explicitly sliced second object array.
constexpr bool required(std::uint32_t scenario,std::uint32_t object,std::uint32_t key,std::uint64_t mask) noexcept {
    return scenario==0x80B5E01FU && object==0x80B5E357U && key==0x8577EEB1U && mask==8;
}
}
