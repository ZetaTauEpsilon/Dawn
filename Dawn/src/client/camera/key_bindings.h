#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dawn::client::camera {

enum class Action : std::size_t { cycle, rear, front, cameraOnly, count };
inline constexpr auto kActionCount = static_cast<std::size_t>(Action::count);
inline constexpr std::uint32_t kNoKey = 0;

struct Bindings final {
    // Windows virtual keys: F5, F6, F7, F8. Zero means unbound.
    std::array<std::uint8_t, kActionCount> keys{0x74, 0x75, 0x76, 0x77};
    friend bool operator==(const Bindings&, const Bindings&) = default;
};

[[nodiscard]] constexpr bool valid_key(std::uint32_t key) noexcept {
    // Mouse clicks cannot capture themselves; Escape cancels and Backspace clears.
    return key == kNoKey || (key >= 9 && key <= 254 && key != 27);
}

[[nodiscard]] constexpr bool valid_bindings(const Bindings& bindings) noexcept {
    for (std::size_t i = 0; i < kActionCount; ++i) {
        if (!valid_key(bindings.keys[i])) { return false; }
        for (std::size_t j = 0; j < i; ++j) {
            if (bindings.keys[i] != kNoKey && bindings.keys[i] == bindings.keys[j]) { return false; }
        }
    }
    return true;
}

/** Reusing another camera action's key swaps their bindings, avoiding double actions. */
[[nodiscard]] constexpr bool rebind(Bindings& bindings, Action action, std::uint32_t key) noexcept {
    const auto index = static_cast<std::size_t>(action);
    if (index >= kActionCount || !valid_key(key) || !valid_bindings(bindings)) { return false; }
    const auto old = bindings.keys[index];
    for (std::size_t i = 0; i < kActionCount; ++i) {
        if (i != index && key != kNoKey && bindings.keys[i] == key) { bindings.keys[i] = old; }
    }
    bindings.keys[index] = static_cast<std::uint8_t>(key);
    return true;
}

/** Publish all four keys together so a swap never exposes a duplicate midway through. */
[[nodiscard]] constexpr std::uint32_t pack(const Bindings& bindings) noexcept {
    std::uint32_t result{};
    for (std::size_t i = 0; i < kActionCount; ++i) {
        result |= static_cast<std::uint32_t>(bindings.keys[i]) << (i * 8);
    }
    return result;
}
[[nodiscard]] constexpr Bindings unpack(std::uint32_t value) noexcept {
    Bindings result;
    for (std::size_t i = 0; i < kActionCount; ++i) {
        result.keys[i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
    return result;
}

using KeyboardState = std::array<bool, 256>;
enum class CaptureKind { waiting, picked, cancelled, reserved };
struct CaptureResult final { CaptureKind kind{}; std::uint32_t key{}; };

/** Capture only fresh keyboard presses; an already-held key cannot become a new binding. */
struct KeyCapture final {
    KeyboardState previous{};
    void arm(const KeyboardState& down) noexcept { previous = down; }
    [[nodiscard]] CaptureResult poll(const KeyboardState& down, std::uint32_t menuKey) noexcept {
        const auto old = previous;
        previous = down;
        if (down[27] && !old[27]) { return {CaptureKind::cancelled, 0}; }
        if (down[8] && !old[8]) { return {CaptureKind::picked, kNoKey}; }
        for (std::uint32_t key = 9; key <= 254; ++key) {
            if (!down[key] || old[key] || !valid_key(key)) { continue; }
            return {key == menuKey ? CaptureKind::reserved : CaptureKind::picked, key};
        }
        return {};
    }
};

} // namespace dawn::client::camera
