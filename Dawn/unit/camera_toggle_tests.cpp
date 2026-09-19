#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numbers>
#include <filesystem>
#include <fstream>
#include <string>

#include "../src/client/hooks/camera/policy.h"
#include "../src/client/camera/camera_settings.h"
#include "../src/client/camera/settings_codec.h"
#include "../src/core/filesystem/path.h"
#include "../src/core/logging/log.h"

namespace dawn::core::log { void write(Channel, Level, std::string_view) noexcept {} }

namespace camera = dawn::client::hooks::camera::detail;
namespace preferences = dawn::client::camera;

namespace {
unsigned checks{};
void check(bool condition, const char* scenario) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", scenario);
        std::exit(1);
    }
}

bool close_float(float a, float b) { return std::fabs(a - b) < 0.00001F; }
bool same(camera::Vector3 a, camera::Vector3 b) {
    return close_float(a.x, b.x) && close_float(a.y, b.y) && close_float(a.z, b.z);
}
float dot(camera::Vector3 a, camera::Vector3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
camera::Vector3 offset(camera::Orientation basis) {
    // Native placement resolves a local backward/shoulder/up offset in this basis.
    const auto f = basis.forward;
    const auto u = basis.up;
    const camera::Vector3 right{f.y * u.z - f.z * u.y,
                                f.z * u.x - f.x * u.z,
                                f.x * u.y - f.y * u.x};
    return {-3.F * f.x + 0.4F * right.x + 0.2F * u.x,
            -3.F * f.y + 0.4F * right.y + 0.2F * u.y,
            -3.F * f.z + 0.4F * right.z + 0.2F * u.z};
}
}

int main() {
    // One held F5 advances once; only another press can advance again.
    camera::ToggleKey key;
    check(!key.poll(false, true, false), "initial release");
    check(key.poll(true, true, false), "first F5 press");
    for (unsigned frame = 0; frame < 120; ++frame) {
        check(!key.poll(true, true, false), "held F5 does not repeat");
    }
    check(!key.poll(false, true, false), "release does not toggle");
    check(key.poll(true, true, false), "second press advances again");

    // An input press owned by Dawn's UI cannot escape into gameplay on menu close.
    check(!key.poll(false, true, true), "menu release");
    check(!key.poll(true, true, true), "menu consumes F5");
    check(!key.poll(true, true, false), "closing menu with F5 held");
    check(!key.poll(false, true, false), "release after menu");
    check(key.poll(true, true, false), "new press after menu");

    // Alt-tab while F5 is down must not change the camera on return.
    check(!key.poll(false, false, false), "background release");
    check(!key.poll(true, false, false), "background press");
    check(!key.poll(true, true, false), "focus returns with F5 held");
    check(!key.poll(false, true, false), "release after focus return");
    check(key.poll(true, true, false), "new press after focus return");
    camera::ToggleKey startup;
    check(!startup.poll(true, true, false), "key held during hook startup");

    // Replay the native sequence through death, cutscene, vehicle, and respawn.
    // Only ordinary first-person gameplay is overridden, and disabling hands it back.
    constexpr std::array requested{4, 4, 5, 9, 0, 11, 12, 4};
    constexpr std::array expected{0, 0, 5, 9, 0, 11, 12, 0};
    for (std::size_t step = 0; step < requested.size(); ++step) {
        check(camera::select_mode(true, true, true, requested[step]) == expected[step],
              "native camera transition sequence");
    }
    check(camera::select_mode(false, true, true, 4) == 4, "off restores native request");
    check(camera::select_mode(true, false, true, 4) == 4, "other player/director untouched");
    check(camera::select_mode(true, true, false, 4) == 4, "constructor/authored caller untouched");
    check(camera::select_mode(true, true, true, -1) == -1, "invalid native mode passed through");

    for (unsigned flags = 0; flags < 16; ++flags) {
        check(camera::apply_front((flags & 1) != 0, (flags & 2) != 0,
                                  (flags & 4) != 0, (flags & 8) != 0 ? 0U : 1U)
                  == (flags == 15),
              "front view requires enabled/front/owned/local together");
    }

    using Mode = preferences::Mode;
    for (const auto initial : {Mode::normal, Mode::rear, Mode::front}) {
        check(preferences::toggled_mode(initial, Mode::rear)
                  == (initial == Mode::rear ? Mode::normal : Mode::rear), "F6 directly toggles rear");
        check(preferences::toggled_mode(initial, Mode::front)
                  == (initial == Mode::front ? Mode::normal : Mode::front), "F7 directly toggles front");
        check(preferences::toggled_mode(initial, Mode::normal) == Mode::normal, "Normal menu control selects normal");
    }
    // Each direct key has its own edge state; holding F6 cannot consume a new F7 press.
    std::array<camera::ToggleKey, 4> directKeys{};
    for (auto& direct : directKeys) { (void)direct.poll(false, true, false); }
    check(directKeys[1].poll(true, true, false), "rear direct press");
    check(directKeys[2].poll(true, true, false), "front press while rear key held");
    check(!directKeys[1].poll(true, true, false), "held rear key does not override front");
    check(directKeys[3].poll(true, true, false), "F8 toggles independently of the camera keys");
    check(!directKeys[3].poll(true, true, false), "held F8 does not flicker visibility");
    // The crash was a missing weapon prepared-view record under environment type 5.
    // Only weapon submissions may be omitted; world/UI and unsupported views pass on.
    for (std::uint32_t type = 0; type < 16; ++type) {
        check(!camera::omit_weapon_draw(false, type), "F9 off forwards every native view");
        check(camera::omit_weapon_draw(true, type) == (type == 2 || type == 3 || type == 4),
              "F9 omits weapon, iron-sight, and scope draws only");
    }
    check(!camera::omit_weapon_draw(true, UINT32_MAX), "unknown view types pass through");
    check(!camera::omit_weapon_draw(false, UINT32_MAX), "unknown disabled view passes through");
    // Restore the native visibility, including changes made by the game while hidden.
    for (unsigned original = 0; original < 256; ++original) {
        camera::VisibilityMask visibility{};
        visibility.hide(static_cast<std::uint8_t>(original));
        check(visibility.hidden && visibility.saved == original, "all original visibility bits retained");
        visibility.update(0, 0);
        check(visibility.saved == original, "zero native update mask changes no bits");
        visibility.update(0x12, 0x3F);
        check(visibility.saved == ((original & 0xC0U) | 0x12U), "native masked changes preserved for restore");
        visibility.update(0, 0xFF);
        check(visibility.saved == 0, "native full hide stays hidden after F9 off");
        visibility.update(0x7F, 0xFF);
        check(visibility.saved == 0x7F, "native full show retained for F9 off");
    }
    check(preferences::mode_from_flags(false, false) == Mode::normal, "load normal preference");
    check(preferences::mode_from_flags(true, false) == Mode::rear, "load rear preference");
    check(preferences::mode_from_flags(true, true) == Mode::front, "load front preference");
    auto current = preferences::mode_from_flags(false, true);
    check(current == Mode::normal, "disabled legacy front preference starts normal");
    camera::ToggleKey cycleKey;
    (void)cycleKey.poll(false, true, false);
    for (const auto expectedMode : {Mode::rear, Mode::front, Mode::normal,
                                    Mode::rear, Mode::front, Mode::normal}) {
        if (cycleKey.poll(true, true, false)) {
            current = preferences::next_mode(current);
        }
        check(current == expectedMode, "F5 cycles normal/rear/front/normal repeatedly");
        if (cycleKey.poll(true, true, false)) {
            current = preferences::next_mode(current);
        }
        check(current == expectedMode, "holding F5 cannot skip a mode");
        check(!cycleKey.poll(false, true, false), "F5 cycle release does not advance");
        check(camera::select_mode(current != Mode::normal, true, true, 4)
                  == (expectedMode == Mode::normal ? 4 : 0),
              "cycle chooses the expected native camera");
        check(camera::apply_front(current != Mode::normal, current == Mode::front, true, 0)
                  == (expectedMode == Mode::front),
              "cycle applies front orientation only in the front step");
    }

    for (const float yaw : {0.F, 0.7F, std::numbers::pi_v<float> / 2.F,
                            std::numbers::pi_v<float>, -2.3F}) {
        for (const float pitch : {-0.8F, 0.F, 0.6F}) {
            const camera::Orientation rear{
                {std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)},
                {-std::sin(pitch) * std::cos(yaw), -std::sin(pitch) * std::sin(yaw), std::cos(pitch)}};
            auto front = rear;
            check(camera::face_front(front), "valid pitched camera accepted");
            check(same(front.forward, {-rear.forward.x, -rear.forward.y, rear.forward.z}),
                  "front camera faces opposite horizontal heading with pitch preserved");
            check(close_float(front.up.z, rear.up.z) && front.up.z > 0.F, "horizon remains upright");
            check(close_float(dot(front.forward, front.up), 0.F), "orientation remains perpendicular");
            check(close_float(dot(front.forward, front.forward), 1.F)
                  && close_float(dot(front.up, front.up), 1.F), "orientation remains normalized");
            const auto behind = offset(rear);
            const auto ahead = offset(front);
            check(same(ahead, {-behind.x, -behind.y, behind.z}),
                  "native offset moves camera to the opposite side at the same height");
            const camera::Vector3 heading{std::cos(yaw), std::sin(yaw), 0.F};
            check(dot(behind, heading) < 0.F && dot(ahead, heading) > 0.F,
                  "camera sits in front of the character");
            check(camera::face_front(front) && same(front.forward, rear.forward)
                  && same(front.up, rear.up), "half-turn applied twice restores rear pose");
        }
    }
    camera::Orientation empty{};
    check(!camera::face_front(empty), "missing native orientation rejected");
    camera::Orientation parallel{{1.F, 0.F, 0.F}, {1.F, 0.F, 0.F}};
    check(!camera::face_front(parallel) && parallel.forward.x == 1.F,
          "invalid orientation left unchanged");
    camera::Orientation invalid{{std::numeric_limits<float>::infinity(), 0.F, 0.F}, {0.F, 0.F, 1.F}};
    check(!camera::face_front(invalid), "infinite native data rejected");
    invalid.forward.x = std::numeric_limits<float>::quiet_NaN();
    check(!camera::face_front(invalid), "NaN native data rejected");
    using preferences::Action;
    using preferences::Bindings;
    using preferences::CaptureKind;
    check(Bindings{}.keys == std::array<std::uint8_t, 4>{0x74, 0x75, 0x76, 0x77},
          "requested defaults are F5 cycle, F6 rear, F7 front, F8 camera only");
    for (std::size_t action = 0; action < preferences::kActionCount; ++action) {
        for (const auto candidate : {0U, 9U, 0x31U, 0x41U, 0x70U, 0x75U, 0x87U, 0xA3U, 254U}) {
            Bindings bindings;
            check(preferences::rebind(bindings, static_cast<Action>(action), candidate), "each action accepts keyboard/unbound values");
            check(bindings.keys[action] == candidate && preferences::valid_bindings(bindings), "rebind keeps actions unique");
            check(preferences::unpack(preferences::pack(bindings)) == bindings, "all four keys publish coherently");
        }
    }
    Bindings swapped;
    check(preferences::rebind(swapped, Action::cycle, 0x75)
          && swapped.keys == std::array<std::uint8_t, 4>{0x75, 0x74, 0x76, 0x77}, "duplicate key swaps camera actions");
    for (const auto invalidKey : {1U, 6U, 8U, 27U, 255U, 256U, UINT32_MAX}) {
        const auto before = swapped;
        check(!preferences::rebind(swapped, Action::rear, invalidKey) && swapped == before,
              "invalid/capture-only keys leave bindings unchanged");
    }
    check(!preferences::rebind(swapped, Action::count, 0x70), "invalid action refused");

    camera::BoundKey rebound;
    check(!rebound.poll(0x74, true, true, false), "startup held default key consumed");
    check(!rebound.poll(0x70, true, true, false), "held newly rebound key consumed");
    check(!rebound.poll(0x70, false, true, false), "rebound release");
    check(rebound.poll(0x70, true, true, false), "fresh rebound key fires");
    check(!rebound.poll(0, true, true, false), "unbound action never fires");
    check(!rebound.poll(0x74, true, true, false), "reset defaults consumes held key");
    check(!rebound.poll(0x74, false, true, true), "menu consumes default release");
    check(!rebound.poll(0x74, true, true, true), "menu consumes default press");
    check(!rebound.poll(0x74, true, true, false), "menu close cannot activate held binding");

    preferences::KeyboardState keyboard{};
    preferences::KeyCapture capture;
    keyboard[0x41] = true;
    capture.arm(keyboard);
    check(capture.poll(keyboard, VK_INSERT).kind == CaptureKind::waiting, "picker ignores initially held key");
    keyboard[0x41] = false; (void)capture.poll(keyboard, VK_INSERT);
    keyboard[0x41] = true;
    const auto picked = capture.poll(keyboard, VK_INSERT);
    check(picked.kind == CaptureKind::picked && picked.key == 0x41, "picker captures new letter press");
    keyboard = {}; capture.arm(keyboard); keyboard[VK_LBUTTON] = true;
    check(capture.poll(keyboard, VK_INSERT).kind == CaptureKind::waiting, "picker ignores its mouse click");
    keyboard[VK_ESCAPE] = true;
    check(capture.poll(keyboard, VK_INSERT).kind == CaptureKind::cancelled, "Escape cancels capture");
    keyboard = {}; capture.arm(keyboard); keyboard[VK_BACK] = true;
    const auto cleared = capture.poll(keyboard, VK_INSERT);
    check(cleared.kind == CaptureKind::picked && cleared.key == 0, "Backspace unbinds action");
    keyboard = {}; capture.arm(keyboard); keyboard[VK_INSERT] = true;
    check(capture.poll(keyboard, VK_INSERT).kind == CaptureKind::reserved, "menu toggle cannot be rebound");

    namespace codec = preferences::detail;
    check(codec::parse_settings("{}").bindings == Bindings{}, "missing keys load requested defaults");
    const auto legacy = codec::parse_settings(R"({"third_person_enabled":true,"front_view":true})");
    check(legacy.mode == Mode::front && legacy.bindings == Bindings{}, "legacy camera preference retains new defaults");
    for (const auto bad : {"-1", "255", "9999999999999999", "112.5", "112oops", "true", "\"112\""}) {
        check(codec::parse_settings(std::string("{\"cycle_key\":") + bad + "}").bindings == Bindings{},
              "malformed stored key keeps default");
    }
    check(codec::parse_settings(R"({"cycle_key":117})").bindings == Bindings{}, "duplicate stored keys fall back to defaults");
    check(codec::parse_settings(R"({"cycle_key":0,"rear_key":0,"front_key":0,"camera_only_key":0})").bindings.keys
          == std::array<std::uint8_t, 4>{}, "all actions can stay unbound after restart");
    check(codec::parse_settings(R"({"cycle_key":112,"cycle_key":113})").bindings == Bindings{}, "repeated stored field refused");
    check(codec::parse_settings("not a settings document").bindings == Bindings{}, "invalid document uses defaults");

    // Real settings I/O stays beside this test executable, never in the installed game.
    dawn::core::path::Buffer testRoot{};
    const auto module = GetModuleHandleW(nullptr);
    check(dawn::core::path::artifact_directory(module, testRoot), "resolve test-owned artifact directory");
    const std::filesystem::path settingsFile = std::filesystem::path(testRoot.chars.data()) / "camera.json";
    std::error_code fileError;
    (void)std::filesystem::remove(settingsFile, fileError);
    check(!fileError, "clear previous test-owned camera fixture");
    preferences::initialize(module);
    check(preferences::mode() == Mode::normal && preferences::bindings() == Bindings{}
          && !preferences::camera_only(), "fresh launch defaults");
    for (std::size_t action = 0; action < preferences::kActionCount; ++action) {
        check(preferences::set_binding(static_cast<Action>(action), 0x70U + static_cast<std::uint32_t>(action)),
              "publish each custom binding through real store");
    }
    const auto custom = preferences::bindings();
    preferences::set_mode(Mode::front);
    preferences::set_camera_only(true);
    preferences::shutdown(); preferences::initialize(module);
    check(preferences::bindings() == custom && preferences::mode() == Mode::front,
          "custom bindings and mode survive disk reload");
    check(!preferences::camera_only(), "HUD and models restore at next launch");
    preferences::reset_bindings();
    preferences::shutdown(); preferences::initialize(module);
    check(preferences::bindings() == Bindings{} && preferences::mode() == Mode::front,
          "reset saves default keys without changing camera mode");
    check(preferences::set_binding(Action::front, 0), "clear real front binding");
    preferences::shutdown(); preferences::initialize(module);
    check(preferences::bindings().keys[2] == 0, "cleared binding survives reload");
    preferences::reset_bindings();
    check(!preferences::set_binding(Action::count, 0x70), "store rejects invalid action");
    const HANDLE locked = CreateFileW(settingsFile.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(locked != INVALID_HANDLE_VALUE, "hold existing settings against replacement");
    preferences::set_mode(Mode::rear);
    (void)CloseHandle(locked);
    preferences::shutdown(); preferences::initialize(module);
    check(preferences::mode() == Mode::front && preferences::bindings() == Bindings{},
          "failed atomic replacement preserves last complete settings");
    preferences::shutdown();
    (void)std::filesystem::remove(settingsFile, fileError);
    check(!fileError, "remove test-owned settings fixture");
    std::printf("PASS: %u camera/input/visibility/settings checks\n", checks);
    return 0;
}
