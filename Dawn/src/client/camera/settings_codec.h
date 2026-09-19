#pragma once

#include <charconv>
#include <string_view>

#include "camera_settings.h"

namespace dawn::client::camera::detail {

inline constexpr std::array<std::string_view, kActionCount> kBindingNames{
    "\"cycle_key\"", "\"rear_key\"", "\"front_key\"", "\"camera_only_key\""};

/** Read one exact scalar from our small, flat settings document. */
[[nodiscard]] inline std::string_view scalar(std::string_view text, std::string_view key) noexcept {
    const auto at = text.find(key);
    if (at == std::string_view::npos || text.find(key, at + key.size()) != std::string_view::npos) { return {}; }
    auto value = text.substr(at + key.size());
    const auto colon = value.find_first_not_of(" \t\r\n");
    if (colon == std::string_view::npos || value[colon] != ':') { return {}; }
    value.remove_prefix(colon + 1);
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) { return {}; }
    value.remove_prefix(begin);
    const auto end = value.find_first_of(",}");
    if (end == std::string_view::npos) { return {}; }
    value = value.substr(0, end);
    const auto last = value.find_last_not_of(" \t\r\n");
    return last == std::string_view::npos ? std::string_view{} : value.substr(0, last + 1);
}

struct StoredSettings final { Mode mode{Mode::normal}; Bindings bindings{}; };

[[nodiscard]] inline StoredSettings parse_settings(std::string_view text) noexcept {
    StoredSettings result;
    const auto first = text.find_first_not_of(" \t\r\n");
    const auto last = text.find_last_not_of(" \t\r\n");
    if (first == std::string_view::npos || text[first] != '{' || text[last] != '}') { return result; }
    result.mode = mode_from_flags(scalar(text, "\"third_person_enabled\"") == "true",
                                  scalar(text, "\"front_view\"") == "true");
    for (std::size_t i = 0; i < kActionCount; ++i) {
        const auto value = scalar(text, kBindingNames[i]);
        if (value.empty()) { continue; }
        std::uint32_t key{};
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), key);
        if (parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() && valid_key(key)) {
            result.bindings.keys[i] = static_cast<std::uint8_t>(key);
        }
    }
    if (!valid_bindings(result.bindings)) { result.bindings = {}; }
    return result;
}

} // namespace dawn::client::camera::detail
