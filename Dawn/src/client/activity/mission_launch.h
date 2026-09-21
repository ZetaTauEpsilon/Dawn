#pragma once
#include <cstdint>
#include <string_view>
#include "../../state/activity/forced/definition.h"
#include "../../state/activity/strike_variants.h"
#include "../../state/activity/nightfall/rules.h"
namespace dawn::client::activity::mission_launch {
enum class Status : std::uint8_t {
    idle, requested, queued, arrived, catalogUnavailable, entryUnavailable,
    overrideActive, returnToOrbit, nativeUnavailable, notReady, descriptorRejected, timedOut,
    manualRejected, preparing, prelaunchUnavailable, unexpectedDestination, cinematics
};
struct Snapshot {
    Status status{Status::idle};
    std::uint16_t index{};
    bool busy{};
    bool manual{};
    state::activity::forced::ForcedDestination destination{};
    bool opening{};
    bool inMission{};
    std::int16_t currentIndex{-1};
    std::array<char, 40> currentPackage{};
    std::uint8_t currentPackageLength{};
    state::activity::nightfall::Options nightfallOptions{};
    [[nodiscard]] std::string_view current_name() const noexcept {
        return {currentPackage.data(), currentPackageLength};
    }
};
/** Render thread: enqueue one immutable public activity ordinal. No native calls here. */
[[nodiscard]] bool request(std::uint16_t index) noexcept;
/** Queues a curated opening; re-arms its run only after game-thread launch validation. */
[[nodiscard]] bool request_opening(std::size_t mission) noexcept;
/** Queues an exact installed Nightfall identity with its existing authored strike opening. */
[[nodiscard]] bool request_variant(std::size_t mission, state::activity::strikes::Difficulty difficulty) noexcept;
[[nodiscard]] bool request_variant(std::size_t mission, state::activity::strikes::Difficulty difficulty,
    state::activity::nightfall::Options options) noexcept;
/** Enqueues a copied draft. The game-frame owner validates and publishes it immediately before launch. */
[[nodiscard]] bool request_manual(std::uint16_t transportIndex,
    const state::activity::forced::ForcedDestination& destination) noexcept;
[[nodiscard]] Snapshot snapshot() noexcept;
/** Existing game-frame owner only. Orbit launch uses retail selection wrappers unchanged. */
void poll() noexcept;
/** Loading-only presentation lease for the confirmed Homecoming -> Exodus handoff. */
[[nodiscard]] bool suppress_loading() noexcept;
/** Native primary-session update owner only. Keeps orbit launches and lease
 * cleanup alive when orbit has no camera-transform callbacks. Never runs
 * the in-world, loading or cinematic paths from this fallback. */
void poll_orbit(std::uintptr_t session) noexcept;
[[nodiscard]] const char* description(Status status) noexcept;
} // namespace dawn::client::activity::mission_launch
