/** Native following-camera selection for ordinary local gameplay, build 86657. */
#include "runtime.h"

#include <Windows.h>
#include <intrin.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "../../../core/logging/log.h"
#include "../../../core/ui/runtime/ui_visibility_runtime.h"
#include "../../camera/camera_settings.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../input/window_focus.h"
#include "../../memory/current_process_memory.h"
#include "../../patterns/image_scan.h"
#include "policy.h"
#include "clean_view.h"
#include "../teleport/runtime.h"

namespace dawn::client::hooks::camera {
namespace {

constexpr std::uintptr_t kSelectorRva = 0x128F290;
constexpr std::uintptr_t kGameplayVtableRva = 0x1C809D0;
constexpr std::uintptr_t kGameplayReturnRva = 0x1295B74;
constexpr std::uintptr_t kDirectorPlayerIndex = 0x340;
constexpr std::uintptr_t kFollowingUpdateRva = 0x1294500;
constexpr std::uintptr_t kFollowingVtableRva = 0x1C803D0;
constexpr std::uintptr_t kCameraOffset = 0x10;
constexpr std::uintptr_t kOrientationOffset = 0x3C;
constexpr std::string_view kSelectorText =
    "44 88 4C 24 20 89 54 24 10 48 89 4C 24 08 53 55 57 41 54 41 55 48 83 EC 50 "
    "48 8B 41 10 48 8D 59 10";
constexpr auto kSelector =
    patterns::signature<patterns::signature_length(kSelectorText)>(kSelectorText);
constexpr std::string_view kFollowingText =
    "40 55 53 41 55 41 56 48 8D AC 24 88 FE FF FF 48 81 EC 78 02 00 00 "
    "44 0F 29 84 24 20 02 00 00";
constexpr auto kFollowing =
    patterns::signature<patterns::signature_length(kFollowingText)>(kFollowingText);

using SelectCamera = bool(__fastcall*)(void*, std::int32_t, float, bool);
using UpdateFollowing = void(__fastcall*)(void*, std::uint32_t, float, void*);
std::array<hooking::detour::Handle, 2> g_handles{};
hooking::CallGate g_gate{};
std::atomic<SelectCamera> g_original{};
std::atomic<UpdateFollowing> g_originalFollowing{};
std::atomic<std::uintptr_t> g_ownedCamera{};
std::atomic_bool g_installed{};
std::atomic_bool g_reportedOverride{};
std::atomic_bool g_reportedFront{};
std::uintptr_t g_base{};
std::array<detail::BoundKey, client::camera::kActionCount> g_keys{};

template <typename T>
[[nodiscard]] bool read(std::uintptr_t address, T& value) noexcept {
    return memory::read_current_process(nullptr, address,
                                         std::as_writable_bytes(std::span<T>(&value, 1)));
}

[[nodiscard]] bool local_gameplay(void* director) noexcept {
    if (director == nullptr) {
        return false;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(director);
    std::uintptr_t vtable = 0;
    std::uint32_t playerIndex = 0xFFFFFFFF;
    return read(address, vtable) && vtable == g_base + kGameplayVtableRva
           && read(address + kDirectorPlayerIndex, playerIndex) && playerIndex == 0;
}

__declspec(noinline) bool __fastcall select_camera(void* director, std::int32_t mode,
                                                   float transition, bool force) noexcept {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    hooking::CallGate::Scope call(g_gate);
    const SelectCamera next = hooking::await_original(g_original);
    const bool local = call.accepts_side_effects() && local_gameplay(director);
    bool overridden = false;
    if (local) {
        // Any subsequent native request, including another following camera, releases ownership.
        g_ownedCamera.store(0, std::memory_order_release);
    }
    if (local && client::camera::third_person_enabled()
        && caller == g_base + kGameplayReturnRva && mode == detail::kFirstPerson) {
        const auto selected = detail::select_mode(true, local, true, mode);
        if (selected != mode) {
            mode = selected;
            overridden = true;
            if (!g_reportedOverride.exchange(true, std::memory_order_relaxed)) {
                core::log::write(core::log::Channel::client, core::log::Level::info,
                                 "ev=camera stage=select result=ok from=4 to=0");
            }
        }
    }
    // The native constructor, transition, target selection, and return value stay authoritative.
    const bool result = next(director, mode, transition, force);
    if (overridden) {
        const auto camera = reinterpret_cast<std::uintptr_t>(director) + kCameraOffset;
        std::uintptr_t vtable = 0;
        if (read(camera, vtable) && vtable == g_base + kFollowingVtableRva) {
            g_ownedCamera.store(camera, std::memory_order_release);
        }
    }
    return result;
}

__declspec(noinline) void __fastcall update_following(void* camera, std::uint32_t playerIndex,
                                                      float delta, void* output) noexcept {
    hooking::CallGate::Scope call(g_gate);
    const UpdateFollowing next = hooking::await_original(g_originalFollowing);
    next(camera, playerIndex, delta, output);
    const auto address = reinterpret_cast<std::uintptr_t>(camera);
    const auto selectedMode = client::camera::mode();
    if (!call.accepts_side_effects() || output == nullptr || address < kCameraOffset
        || !detail::apply_front(selectedMode != client::camera::Mode::normal,
                                selectedMode == client::camera::Mode::front,
                                g_ownedCamera.load(std::memory_order_acquire) == address,
                                playerIndex)
        || !local_gameplay(reinterpret_cast<void*>(address - kCameraOffset))) {
        return;
    }
    detail::Orientation orientation{};
    auto* const basis = static_cast<std::byte*>(output) + kOrientationOffset;
    if (!read(reinterpret_cast<std::uintptr_t>(basis), orientation)
        || !detail::face_front(orientation)) {
        return;
    }
    // This is the native routine's live output buffer, before offset placement/collision.
    // No actor orientation, input state, or persistent camera-object field is changed.
    std::memcpy(basis, &orientation, sizeof(orientation));
    if (!g_reportedFront.exchange(true, std::memory_order_relaxed)) {
        core::log::write(core::log::Channel::client, core::log::Level::info,
                         "ev=camera stage=front result=ok yaw_degrees=180");
    }
}

[[nodiscard]] bool idle() noexcept { return g_gate.idle(); }

[[nodiscard]] bool fail() noexcept {
    core::log::write(core::log::Channel::client, core::log::Level::warn,
                     "ev=camera stage=install result=fail");
    return false;
}

} // namespace

bool install() noexcept {
    if (g_installed.load(std::memory_order_acquire)) {
        return g_gate.accepting();
    }
    g_gate.quiesce();
    g_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::byte* const target = patterns::scan_main_image_unique(kSelector, "camera_selector");
    std::byte* const following = patterns::scan_main_image_unique(kFollowing, "following_camera");
    // RVAs and callers are used only after their actual code/data agree with this build.
    std::uintptr_t update = 0;
    std::uintptr_t setter = 0;
    std::uintptr_t followingUpdate = 0;
    std::uint32_t followingCase = 0;
    std::uint32_t firstPersonCase = 0;
    std::array<std::uint8_t, 9> request{};
    constexpr std::array<std::uint8_t, 9> kRequest{0x48, 0x8B, 0x03, 0x48, 0x8B,
                                                 0xCB, 0xFF, 0x50, 0x58};
    std::array<std::uint8_t, 8> layout{};
    constexpr std::array<std::uint8_t, 8> kLayout{0x49, 0x8D, 0x4E, 0x3C,
                                                0x49, 0x8D, 0x5E, 0x48};
    std::array<std::uint8_t, 14> placement{};
    constexpr std::array<std::uint8_t, 14> kPlacement{
        0x4C, 0x8D, 0x85, 0x98, 0x09, 0x00, 0x00,
        0x48, 0x8D, 0x8D, 0x60, 0x09, 0x00, 0x00};
    if (g_base == 0 || target == nullptr
        || reinterpret_cast<std::uintptr_t>(target) != g_base + kSelectorRva
        || following == nullptr
        || reinterpret_cast<std::uintptr_t>(following) != g_base + kFollowingUpdateRva
        || !read(g_base + kFollowingVtableRva + 0x10, followingUpdate)
        || followingUpdate != g_base + kFollowingUpdateRva
        || !read(g_base + 0x1294A51, layout) || layout != kLayout
        || !read(g_base + 0x12D5A61, placement) || placement != kPlacement
        || !read(g_base + kGameplayVtableRva + 8, update) || update != g_base + 0x12954C0
        || !read(g_base + kGameplayVtableRva + 0x58, setter) || setter != g_base + 0x128F280
        || !read(g_base + kGameplayReturnRva - request.size(), request) || request != kRequest
        || !read(g_base + 0x1290398, followingCase) || followingCase != 0x128F2F6
        || !read(g_base + 0x12903A8, firstPersonCase) || firstPersonCase != 0x128F394) {
        return fail();
    }
    const std::array specs{
        hooking::detour::Spec{target, reinterpret_cast<void*>(&select_camera)},
        hooking::detour::Spec{following, reinterpret_cast<void*>(&update_following)},
    };
    if (!hooking::detour::install(specs, g_handles)) {
        return fail();
    }
    hooking::publish_original(g_original, reinterpret_cast<SelectCamera>(g_handles[0].original));
    hooking::publish_original(g_originalFollowing,
                              reinterpret_cast<UpdateFollowing>(g_handles[1].original));
    g_ownedCamera.store(0, std::memory_order_release);
    g_reportedOverride.store(false, std::memory_order_relaxed);
    g_reportedFront.store(false, std::memory_order_relaxed);
    g_installed.store(true, std::memory_order_release);
    if (!clean_view::install()) {
        core::log::write(core::log::Channel::client, core::log::Level::warn,
                         "ev=camera stage=camera_only_install result=fail");
    }
    g_gate.accept();
    core::log::write(core::log::Channel::client, core::log::Level::info,
                     "ev=camera stage=install result=ok bindings=rebindable defaults=F5,F6,F7,F8");
    return true;
}

void quiesce() noexcept { clean_view::quiesce(); g_gate.quiesce(); }

bool uninstall() noexcept {
    quiesce();
    if (!clean_view::uninstall()) { return false; }
    if (!g_installed.load(std::memory_order_acquire)) {
        return true;
    }
    const std::array entries{
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&select_camera)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&update_following)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&poll_toggle)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::leave)},
    };
    if (hooking::detour::uninstall(g_handles, entries, &idle)
        != hooking::detour::UninstallResult::removed) {
        core::log::write(core::log::Channel::client, core::log::Level::warn,
                         "ev=camera stage=uninstall result=retained");
        return false;
    }
    g_original.store(nullptr, std::memory_order_release);
    g_originalFollowing.store(nullptr, std::memory_order_release);
    g_ownedCamera.store(0, std::memory_order_release);
    g_installed.store(false, std::memory_order_release);
    g_base = 0;
    g_keys = {};
    return true;
}

bool is_installed() noexcept {
    return g_installed.load(std::memory_order_acquire) && g_gate.accepting();
}

__declspec(noinline) void poll_toggle(std::uint32_t playerIndex) noexcept {
    hooking::CallGate::Scope call(g_gate);
    if (!call.accepts_side_effects() || playerIndex != 0) {
        return;
    }
    std::uint32_t entity = UINT32_MAX;
    // Camera-only rendering needs the current owner, not teleport's cached physics object. That cache
    // can belong to the previous destination or be empty when teleport is disabled.
    (void)teleport::read_controlled_entity(entity);
    clean_view::local_player(entity);
    const bool focused = input::game_focused();
    const auto ui = core::ui::runtime::snapshot();
    const auto bindings = client::camera::bindings();
    std::array<bool, client::camera::kActionCount> pressed{};
    for (std::size_t i = 0; i < g_keys.size(); ++i) {
        const auto key = bindings.keys[i];
        const bool down = key != 0 && (GetAsyncKeyState(key) & 0x8000) != 0;
        pressed[i] = g_keys[i].poll(key, down, focused, ui.visible || key == ui.toggleVirtualKey);
    }
    // Consume every key; simultaneous presses have one deterministic mode result.
    if (pressed[1]) {
        client::camera::toggle_mode(client::camera::Mode::rear);
    } else if (pressed[2]) {
        client::camera::toggle_mode(client::camera::Mode::front);
    } else if (pressed[0]) {
        client::camera::cycle_mode();
    }
    if (pressed[3] && clean_view::installed()) {
        client::camera::toggle_camera_only();
    }
}

} // namespace dawn::client::hooks::camera
