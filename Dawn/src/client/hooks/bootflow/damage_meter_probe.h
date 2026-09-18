#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace dawn::client::hooks::bootflow::damage_meter_probe {

// B7E3C0 is the only recovered native point that names an attacker, a target and an
// absolute amount together, which is what an outgoing-damage HUD needs. Nothing yet
// establishes which of its calls the local player owns, nor whether it observes every
// outgoing hit. This classifies and counts every call so a capture answers both before
// anything consumes the amounts. Observe-only: it never reaches a native pointer.

/** Where one damage summary sat relative to the local player's controlled entity. */
enum class Direction : std::uint8_t {
    /** No local controlled entity resolved; the row proves nothing about direction. */
    unknown,
    /** Attacker is the local entity, salt included. */
    outgoing,
    /** Attacker indexes the local pool row under a different salt. */
    outgoingRow,
    /** Target is the local entity, salt included. */
    incoming,
    /** Target indexes the local pool row under a different salt. */
    incomingRow,
    /** Both ends are the local entity. */
    self,
    /** Neither end is the local player. */
    other,
    count,
};

inline constexpr std::size_t kDirectionCount = static_cast<std::size_t>(Direction::count);

/** Stable log names, in Direction order. A capture is compared across runs by them. */
inline constexpr std::array<const char*, kDirectionCount> kDirectionNames{
    "unknown", "out", "out_row", "in", "in_row", "self", "other"};

/** @param direction Direction to name. @return Its log name, or the unknown one out of range. */
[[nodiscard]] constexpr const char* text(Direction direction) noexcept {
    const auto index = static_cast<std::size_t>(direction);
    return index < kDirectionCount ? kDirectionNames[index] : kDirectionNames[0];
}

/** The low 13 bits index the entity pool; the rest distinguishes recycled entries. */
[[nodiscard]] constexpr std::uint32_t row(std::uint32_t handle) noexcept {
    return handle & 0x1FFFU;
}

/**
 * Places one summary against the local player without trusting a masked handle.
 * A row-only match is reported separately rather than accepted: it says the summary
 * carries a differently salted handle, which a meter would have to account for.
 * @param attacker Summary attacker handle. @param target Summary target handle.
 * @param local Local controlled entity, or UINT32_MAX when it did not resolve.
 */
[[nodiscard]] constexpr Direction classify(std::uint32_t attacker, std::uint32_t target,
                                           std::uint32_t local) noexcept {
    if (local == UINT32_MAX) { return Direction::unknown; }
    const bool attacks = attacker == local, receives = target == local;
    if (attacks && receives) { return Direction::self; }
    if (attacks) { return Direction::outgoing; }
    if (receives) { return Direction::incoming; }
    if (attacker != UINT32_MAX && row(attacker) == row(local)) { return Direction::outgoingRow; }
    if (target != UINT32_MAX && row(target) == row(local)) { return Direction::incomingRow; }
    return Direction::other;
}

/** Running totals for one reporting window. Every field is observe-only. */
struct Window final {
    /** Summaries seen. */
    std::uint64_t calls{};
    /** Summaries per Direction, indexed by its value. */
    std::array<std::uint64_t, kDirectionCount> byDirection{};
    /** Summaries whose killed, mode and regions arguments were set. */
    std::uint64_t killed{}, modeSet{}, regionsPresent{};
    /** Amounts that were finite, that were not, and finite ones at or below zero. */
    std::uint64_t finite{}, nonFinite{}, nonPositive{};
    /** Sum of every finite amount, and of the outgoing ones alone. */
    double amountSum{}, outgoingSum{};
    /** Extremes of the finite amounts. Read them only while finite is non-zero. */
    float amountMin{std::numeric_limits<float>::infinity()};
    float amountMax{-std::numeric_limits<float>::infinity()};

    /** @param direction Direction to total. @return Its count, or zero out of range. */
    [[nodiscard]] std::uint64_t count(Direction direction) const noexcept {
        const auto index = static_cast<std::size_t>(direction);
        return index < byDirection.size() ? byDirection[index] : 0;
    }
};

/**
 * Adds one summary to a window.
 * @param window Window to total into.
 * @param direction Placement against the local player.
 * @param killed Native killed argument. @param mode Native mode argument.
 * @param regions True when the native regions argument was non-null. It is never read:
 * its layout is unrecovered, so the capture records only that one was supplied.
 * @param amount Native amount argument, finite or not.
 */
inline void observe(Window& window, Direction direction, bool killed, bool mode, bool regions,
                    float amount) noexcept {
    ++window.calls;
    const auto index = static_cast<std::size_t>(direction);
    if (index < window.byDirection.size()) { ++window.byDirection[index]; }
    if (killed) { ++window.killed; }
    if (mode) { ++window.modeSet; }
    if (regions) { ++window.regionsPresent; }
    // A non-finite amount is counted and never allowed into a sum or an extreme.
    if (!std::isfinite(amount)) { ++window.nonFinite; return; }
    ++window.finite;
    if (amount <= 0.0F) { ++window.nonPositive; }
    window.amountSum += static_cast<double>(amount);
    if (amount < window.amountMin) { window.amountMin = amount; }
    if (amount > window.amountMax) { window.amountMax = amount; }
    if (direction == Direction::outgoing || direction == Direction::outgoingRow) {
        window.outgoingSum += static_cast<double>(amount);
    }
}

} // namespace dawn::client::hooks::bootflow::damage_meter_probe
