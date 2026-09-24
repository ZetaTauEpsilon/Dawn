/** Build 86657: suppress drawing without removing the renderer's required views. */
#include "clean_view.h"

#include <Windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <string_view>

#include "../../../core/logging/log.h"
#include "../../camera/camera_settings.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../patterns/image_scan.h"
#include "../bootflow/gateway_native_read.h"
#include "policy.h"

namespace dawn::client::hooks::camera::clean_view {
namespace {
namespace native = bootflow::gateway_native;
constexpr auto absent = UINT32_MAX;
constexpr std::uintptr_t kViews = 0x11D4670;
constexpr std::uintptr_t kInherit = 0x58C280;
constexpr std::uintptr_t kVisibility = 0x1169110;
constexpr std::uintptr_t kSubmit = 0x11D5A70;
constexpr std::uintptr_t kUiBuild = 0x132B890;
constexpr std::uintptr_t kPreparedType = 0x608;
constexpr std::uintptr_t kPreparedFrame = 0xA88;
constexpr std::uintptr_t kUiPacket = 0x36F18;
constexpr std::uintptr_t kWorld = 0x281E620;
constexpr std::uintptr_t kHead = 0x2E5B988;
constexpr std::uintptr_t kWorldView = 0x2E5E6F0;
constexpr std::uintptr_t kWeaponView = 0x2E5EC40;
constexpr std::uintptr_t kUiView = 0x2E5F190;
constexpr std::uintptr_t kNext = 0x528;
constexpr std::uintptr_t kViewType = 0x530;
constexpr std::string_view kViewsText =
    "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 "
    "41 56 48 83 EC 40 49 8B D9 41 8B F8 48 8B F2 48 8B E9";
constexpr std::string_view kInheritText =
    "8B C2 83 F9 FF 74 60 83 F8 FF 74 5B 8B D1 81 E1 FF 1F 00 00";
constexpr std::string_view kVisibilityText =
    "8B C2 45 22 C1 C1 F8 0D 81 E2 FF 1F 00 00 44 8B D0 "
    "49 81 CA 00 00 FC 0F 0F B7 C0 49 C1 EA 12 4C 23 D0";
constexpr auto viewsSignature = patterns::signature<patterns::signature_length(kViewsText)>(kViewsText);
constexpr auto inheritSignature = patterns::signature<patterns::signature_length(kInheritText)>(kInheritText);
constexpr auto visibilitySignature = patterns::signature<patterns::signature_length(kVisibilityText)>(kVisibilityText);
constexpr std::string_view kSubmitText =
    "4C 8B DC 55 53 56 41 55 41 57 49 8D AB 58 F2 FF FF 48 81 EC 80 0E 00 00";
constexpr std::string_view kUiBuildText =
    "40 55 56 57 41 54 41 56 48 8D AC 24 A0 D1 FF FF B8 60 2F 00 00";
constexpr auto submitSignature = patterns::signature<patterns::signature_length(kSubmitText)>(kSubmitText);
constexpr auto uiBuildSignature = patterns::signature<patterns::signature_length(kUiBuildText)>(kUiBuildText);
// The native stage dispatcher returns false for an empty stage. UI has a separate,
// nullable packet; its consumer checks that packet before submitting any UI draws.
constexpr std::string_view kEmptyStage =
    "40 32 F6 8B 81 0C 06 00 00 4C 8B F9 4C 63 EA 44 0F A3 E8 "
    "44 88 4C 24 41 4C 89 44 24 58 48 89 5C 24 50 0F 83 E5 07 00 00";
constexpr std::string_view kUiPacketConsumer = "48 8B B7 18 6F 03 00 48 85 F6 74 44";
constexpr std::string_view kUiPacketEmpty = "4C 89 A0 18 6F 03 00 4C 89 A7 B8 00 00 00";
constexpr std::string_view kVisibilityBody =
    "41 0F AF 52 30 8B C2 49 03 42 08 45 0F B6 D1 41 F6 D2 44 22 50 54 "
    "0F BE 50 52 45 0A D0 44 88 50 54 83 EA 01 74 0E 83 FA 02 75 3E "
    "48 81 C1 E0 00 00 00 EB 1F F3 0F 10 48 40 0F 57 C0 0F 2E C8 7A 0B "
    "75 09 48 81 C1 B0 00 00 00 EB 07 48 81 C1 C8 00 00 00 48 85 C9 74 11 "
    "0F B7 40 50 48 8B 49 08 48 C1 E0 06 44 88 54 08 34";
constexpr std::string_view kViewsTail =
    "48 8B 44 24 70 48 89 88 28 05 00 00 48 8B 44 24 38 48 89 05 B3 71 C8 01";
constexpr std::string_view kWorldLink =
    "48 8D 05 1A A0 C8 01 0F BA F1 0A 48 89 05 E7 9F C8 01";

using BuildViews = void(__fastcall*)(void*, void*, std::uint32_t, void*, void*);
using Inherit = void(__fastcall*)(std::uint32_t, std::uint32_t);
using Visibility = void(__fastcall*)(void*, std::uint32_t, std::uint8_t, std::uint8_t);
using Submit = bool(__fastcall*)(void*, std::int32_t, void*, bool, std::uint32_t, void*, std::uint32_t);
using UiBuild = void(__fastcall*)(void*, void*);
std::atomic<BuildViews> g_views{};
std::atomic<Inherit> g_inherit{};
std::atomic<Visibility> g_visibility{};
std::atomic<Submit> g_submit{};
std::atomic<UiBuild> g_uiBuild{};
std::array<hooking::detour::Handle, 5> g_handles{};
hooking::CallGate g_gate{};
SRWLOCK g_lock = SRWLOCK_INIT;
std::atomic_bool g_installed{};
std::atomic_uint32_t g_local{absent};
std::uintptr_t g_base{};

struct Model {
    std::uint32_t proxy{absent};
    native::Weak scene{}, owner{}, proxyIdentity{};
    bool candidate{}, dirty{};
    // This state belongs to the actual renderer datum, not its reusable low index.
    native::Weak renderer{};
    std::uintptr_t record{}, world{};
    detail::VisibilityMask visibility{};
};
std::array<Model, 8192> g_models{};
std::uint32_t g_classifiedPlayer{absent};
ULONGLONG g_classifiedAt{};
bool g_wasEnabled{};
bool g_reported{};
std::atomic_bool g_frameEnabled{}, g_reportedWeapon{}, g_reportedHud{};
std::atomic_uint32_t g_hiddenCount{};

template<std::size_t N>
[[nodiscard]] bool matches(std::uintptr_t rva, const std::array<patterns::PatternByte, N>& expected) noexcept {
    native::Read read{g_base};
    std::array<std::byte, N> bytes{};
    if (!read.copy(g_base + rva, bytes)) { return false; }
    for (std::size_t i = 0; i < N; ++i) {
        if (expected[i].exact && bytes[i] != expected[i].value) { return false; }
    }
    return true;
}

[[nodiscard]] bool owner_is_player(native::Read& read, native::Weak owner,
                                    std::uint32_t player) noexcept {
    if (player == absent || !read.weak(owner)) { return false; }
    std::uintptr_t objects{};
    std::uint32_t stride{};
    if (!read.value(g_base + 0x1F93428, objects) || !objects
        || !read.value(g_base + 0x1F93430, stride) || stride < 0x50 || stride > 0x10000) { return false; }
    auto handle = owner.handle;
    for (unsigned depth = 0; depth < 32 && handle != absent; ++depth) {
        std::uint32_t self{}, parent{}, flags{};
        const auto row = objects + static_cast<std::uintptr_t>(handle & 0x1FFFU) * stride;
        if (!read.value(row + 0xC, self) || self != handle || !read.value(row + 4, flags)
            || (flags & 4U) != 0) { return false; }
        if (handle == player) { return true; }
        if (!read.value(row + 0x3C, parent) || parent == handle) { return false; }
        handle = parent;
    }
    return false;
}

[[nodiscard]] bool renderer(native::Read& read, std::uint32_t proxy,
                             native::Weak& identity, std::uintptr_t& record) noexcept {
    std::uintptr_t ignored{}, allocation{};
    std::uint32_t handle{};
    return read.resolve(proxy, ignored, &allocation) && read.value(allocation + 4, handle)
        && handle != absent && read.make_weak(handle, identity)
        && read.resolve(handle, ignored, &record);
}

[[nodiscard]] bool current_renderer(native::Read& read, const Model& model) noexcept {
    std::uintptr_t ignored{}, record{};
    return model.world != 0 && read.weak(model.renderer)
        && read.resolve(model.renderer.handle, ignored, &record) && record == model.record;
}

// Called only on the native render producer, while holding g_lock.
void restore(Model& model, Visibility next) noexcept {
    if (!model.visibility.hidden) { return; }
    native::Read read{g_base};
    std::uintptr_t world{};
    if (read.value(g_base + kWorld, world) && world == model.world && current_renderer(read, model)) {
        next(reinterpret_cast<void*>(world), model.renderer.handle, model.visibility.saved, 0xFF);
    }
    model.visibility.hidden = false;
}

__declspec(noinline) void __fastcall inherit_visibility(std::uint32_t scene,
                                                         std::uint32_t proxy) noexcept {
    hooking::CallGate::Scope call(g_gate);
    hooking::await_original(g_inherit)(scene, proxy);
    if (!call.accepts_side_effects() || scene == absent || proxy == absent) { return; }
    native::Read read{g_base};
    std::uintptr_t component{};
    std::uint32_t self{}, owner{};
    native::Weak sceneWeak{}, ownerWeak{}, proxyWeak{};
    if (!read.resolve(scene, component) || !read.value(component + 0x24, self) || self != scene
        || !read.value(component + 0x2C, owner) || owner == absent
        || !read.make_weak(scene, sceneWeak) || !read.make_weak(owner, ownerWeak)
        || !read.make_weak(proxy, proxyWeak)) { return; }
    AcquireSRWLockExclusive(&g_lock);
    const auto start = static_cast<std::size_t>(proxy * 2654435761U) % g_models.size();
    for (std::size_t probe = 0; probe < g_models.size(); ++probe) {
        auto& model = g_models[(start + probe) % g_models.size()];
        if (model.proxy == proxy || model.proxy == absent) {
            model.proxy = proxy;
            model.scene = sceneWeak;
            model.owner = ownerWeak;
            model.proxyIdentity = proxyWeak;
            model.dirty = true;
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
}

__declspec(noinline) void __fastcall update_visibility(void* world, std::uint32_t handle,
                                                       std::uint8_t values, std::uint8_t mask) noexcept {
    hooking::CallGate::Scope call(g_gate);
    const auto next = hooking::await_original(g_visibility);
    // Native visibility changes still update the saved mask while the actor is hidden.
    // This includes a final native update while shutdown waits for the next render frame.
    if (g_hiddenCount.load(std::memory_order_acquire) != 0) {
        AcquireSRWLockExclusive(&g_lock);
        for (auto& model : g_models) {
            if (!model.visibility.hidden || model.renderer.handle != handle
                || model.world != reinterpret_cast<std::uintptr_t>(world)) { continue; }
            native::Read read{g_base};
            if (current_renderer(read, model)) {
                model.visibility.update(values, mask);
                values = 0;
                mask = 0xFF;
                break;
            }
        }
        next(world, handle, values, mask);
        ReleaseSRWLockExclusive(&g_lock);
    } else {
        next(world, handle, values, mask);
    }
}

/** Accept only the three nodes built by 11D4670's ordinary player-view path. */
[[nodiscard]] bool gameplay_views(native::Read& read, std::uintptr_t& worldNext) noexcept {
    std::uintptr_t head{}, tail{}, uiNext{};
    std::uint32_t type{}, weaponType{}, uiType{};
    if (!read.value(g_base + kHead, head) || head != g_base + kWorldView
        || !read.value(head + kViewType, type) || (type != 1 && type != 5)
        || !read.value(head + kNext, worldNext)
        || !read.value(g_base + kUiView + kViewType, uiType) || uiType != 7
        || !read.value(g_base + kUiView + kNext, uiNext) || uiNext != 0) { return false; }
    if (worldNext == g_base + kUiView) { return type == 1; }
    return type == 5 && worldNext == g_base + kWeaponView
        && read.value(worldNext + kViewType, weaponType) && weaponType >= 2 && weaponType <= 4
        && read.value(worldNext + kNext, tail) && tail == g_base + kUiView;
}

__declspec(noinline) void __fastcall build_views(void* camera, void* alternate,
                                                std::uint32_t layout, void* size, void* uiSize) noexcept {
    hooking::CallGate::Scope call(g_gate);
    hooking::await_original(g_views)(camera, alternate, layout, size, uiSize);
    native::Read viewRead{g_base};
    std::uintptr_t worldNext{}, world{};
    const auto player = g_local.load(std::memory_order_acquire);
    const bool enabled = call.accepts_side_effects() && client::camera::camera_only()
        && layout == 0 && player != absent && gameplay_views(viewRead, worldNext)
        && viewRead.value(g_base + kWorld, world) && world != 0;
    const auto next = hooking::await_original(g_visibility);
    AcquireSRWLockExclusive(&g_lock);
    const auto now = GetTickCount64();
    const bool classify = enabled && (!g_wasEnabled || player != g_classifiedPlayer
                                      || now - g_classifiedAt >= 1000);
    std::uint32_t hidden = 0;
    for (auto& model : g_models) {
        if (model.proxy == absent) { continue; }
        if (!enabled) { restore(model, next); continue; }
        native::Read read{g_base};
        if (classify || model.dirty) {
            model.candidate = read.weak(model.scene) && read.weak(model.proxyIdentity)
                && owner_is_player(read, model.owner, player);
            model.dirty = false;
        }
        if (!model.candidate) { restore(model, next); continue; }
        native::Weak identity{};
        std::uintptr_t record{};
        if (!read.weak(model.scene) || !read.weak(model.proxyIdentity)
            || !owner_is_player(read, model.owner, player)
            || !renderer(read, model.proxy, identity, record)) {
            restore(model, next);
            model.candidate = false;
            continue;
        }
        if (model.visibility.hidden && (model.renderer != identity || model.record != record || model.world != world)) {
            restore(model, next);
        }
        if (!model.visibility.hidden) {
            bool duplicate = false;
            for (const auto& other : g_models) {
                if (&other != &model && other.visibility.hidden && other.renderer == identity
                    && other.world == world) { duplicate = true; break; }
            }
            if (duplicate) { continue; }
            std::uint8_t flags{};
            if (!read.value(record + 0x54, flags)) { continue; }
            model.renderer = identity;
            model.record = record;
            model.world = world;
            model.visibility.hide(flags);
        }
        next(reinterpret_cast<void*>(world), identity.handle, 0, 0xFF);
        ++hidden;
    }
    g_hiddenCount.store(hidden, std::memory_order_release);
    g_wasEnabled = enabled;
    if (classify) { g_classifiedPlayer = player; g_classifiedAt = now; }
    ReleaseSRWLockExclusive(&g_lock);
    // NEVER unlink the weapon or UI view. Native stage jobs retain references to
    // their prepared records; removing the weapon made 11D5AA3 dereference null.
    g_frameEnabled.store(enabled, std::memory_order_release);
    if (enabled && !g_reported && hidden != 0) {
        g_reported = true;
        core::log::write(core::log::Channel::client, core::log::Level::info,
            "ev=camera stage=camera_only_models result=ok local_models=hidden view_chain=preserved");
    }
}

__declspec(noinline) bool __fastcall submit_stage(void* view, std::int32_t stage,
    void* context, bool flag, std::uint32_t excluded, void* extra, std::uint32_t options) noexcept {
    hooking::CallGate::Scope call(g_gate);
    const auto next = hooking::await_original(g_submit);
    if (call.accepts_side_effects() && g_frameEnabled.load(std::memory_order_acquire)
        && client::camera::camera_only() && view != nullptr) {
        // The original immediately reads view+0x60C, so this preceding field has
        // the same native lifetime. No render node, prepared view, or mask changes.
        const auto type = *reinterpret_cast<const std::uint32_t*>(
            reinterpret_cast<std::uintptr_t>(view) + kPreparedType);
        if (detail::omit_weapon_draw(true, type)) {
            if (!g_reportedWeapon.exchange(true, std::memory_order_relaxed)) {
                core::log::write(core::log::Channel::client, core::log::Level::info,
                    "ev=camera stage=camera_only_weapon result=ok draw=skipped view_chain=preserved");
            }
            return false; // Same result as the native empty-stage branch at 11D62AD.
        }
    }
    return next(view, stage, context, flag, excluded, extra, options);
}

__declspec(noinline) void __fastcall build_ui(void* manager, void* view) noexcept {
    hooking::CallGate::Scope call(g_gate);
    // Keep UI updates, buffer rotation, resource lifetime, and finalization intact.
    hooking::await_original(g_uiBuild)(manager, view);
    if (!call.accepts_side_effects() || !g_frameEnabled.load(std::memory_order_acquire)
        || !client::camera::camera_only() || view == nullptr) { return; }
    native::Read read{g_base};
    const auto prepared = reinterpret_cast<std::uintptr_t>(view);
    std::uint32_t type{};
    std::uintptr_t frame{};
    if (!read.value(prepared + kPreparedType, type) || type != 7
        || !read.value(prepared + kPreparedFrame, frame) || frame == 0) { return; }
    // Publish the native "no UI drawing this frame" result. This is a draw packet,
    // NOT a view pointer: 1160323 explicitly accepts null. The manager retains its
    // own buffer reference and produces the next frame normally, including F9 off.
    *reinterpret_cast<std::uintptr_t*>(frame + kUiPacket) = 0;
    if (!g_reportedHud.exchange(true, std::memory_order_relaxed)) {
        core::log::write(core::log::Channel::client, core::log::Level::info,
            "ev=camera stage=camera_only_hud result=ok draw_packet=empty view_chain=preserved");
    }
}

[[nodiscard]] bool idle() noexcept {
    return g_gate.idle() && g_hiddenCount.load(std::memory_order_acquire) == 0;
}
}

bool install() noexcept {
    if (g_installed.load(std::memory_order_acquire)) { return g_gate.accepting(); }
    g_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto views = patterns::scan_main_image_unique(viewsSignature, "camera_clean_views");
    const auto inherit = patterns::scan_main_image_unique(inheritSignature, "camera_model_owner");
    const auto visibility = patterns::scan_main_image_unique(visibilitySignature, "camera_model_visibility");
    const auto submit = patterns::scan_main_image_unique(submitSignature, "camera_weapon_draw");
    const auto uiBuild = patterns::scan_main_image_unique(uiBuildSignature, "camera_ui_packet");
    if (!views || !inherit || !visibility || !submit || !uiBuild
        || reinterpret_cast<std::uintptr_t>(views) != g_base + kViews
        || reinterpret_cast<std::uintptr_t>(inherit) != g_base + kInherit
        || reinterpret_cast<std::uintptr_t>(visibility) != g_base + kVisibility
        || reinterpret_cast<std::uintptr_t>(submit) != g_base + kSubmit
        || reinterpret_cast<std::uintptr_t>(uiBuild) != g_base + kUiBuild
        || !matches(0x11D5AA0, patterns::signature<patterns::signature_length(kEmptyStage)>(kEmptyStage))
        || !matches(0x11D62AD, patterns::signature<6>("40 0F B6 C6 EB DC"))
        || !matches(0x132BCD5, patterns::signature<7>("48 8B 86 88 0A 00 00"))
        || !matches(0x115FD1A, patterns::signature<7>("83 BB 08 06 00 00 07"))
        || !matches(0x132BD0F, patterns::signature<patterns::signature_length(kUiPacketEmpty)>(kUiPacketEmpty))
        || !matches(0x116031C, patterns::signature<patterns::signature_length(kUiPacketConsumer)>(kUiPacketConsumer))
        || !matches(0x1169140, patterns::signature<patterns::signature_length(kVisibilityBody)>(kVisibilityBody))
        || !matches(0x11D47BD, patterns::signature<patterns::signature_length(kViewsTail)>(kViewsTail))
        || !matches(0x11D4C1F, patterns::signature<patterns::signature_length(kWorldLink)>(kWorldLink))
        || !matches(0x1152240, patterns::signature<3>("8B 50 04"))
        || !matches(0x5589F0, patterns::signature<11>("8B 41 0C 89 02 8B 41 3C 83 F8 FF"))) { return false; }
    const std::array specs{
        hooking::detour::Spec{views, reinterpret_cast<void*>(&build_views)},
        hooking::detour::Spec{inherit, reinterpret_cast<void*>(&inherit_visibility)},
        hooking::detour::Spec{visibility, reinterpret_cast<void*>(&update_visibility)},
        hooking::detour::Spec{submit, reinterpret_cast<void*>(&submit_stage)},
        hooking::detour::Spec{uiBuild, reinterpret_cast<void*>(&build_ui)},
    };
    if (!hooking::detour::install(specs, g_handles)) { return false; }
    hooking::publish_original(g_views, reinterpret_cast<BuildViews>(g_handles[0].original));
    hooking::publish_original(g_inherit, reinterpret_cast<Inherit>(g_handles[1].original));
    hooking::publish_original(g_visibility, reinterpret_cast<Visibility>(g_handles[2].original));
    hooking::publish_original(g_submit, reinterpret_cast<Submit>(g_handles[3].original));
    hooking::publish_original(g_uiBuild, reinterpret_cast<UiBuild>(g_handles[4].original));
    g_installed.store(true, std::memory_order_release);
    g_gate.accept();
    core::log::write(core::log::Channel::client, core::log::Level::info,
                    "ev=camera stage=camera_only_install result=ok default_key=F8 revision=preserve_views");
    return true;
}

void local_player(std::uint32_t entity) noexcept { g_local.store(entity, std::memory_order_release); }
bool installed() noexcept { return g_installed.load(std::memory_order_acquire) && g_gate.accepting(); }
void quiesce() noexcept {
    g_gate.quiesce();
    g_frameEnabled.store(false, std::memory_order_release);
}

bool uninstall() noexcept {
    quiesce();
    if (!g_installed.load(std::memory_order_acquire)) { return true; }
    // A render frame restores masks on its owning thread before the DLL may detach.
    if (g_hiddenCount.load(std::memory_order_acquire) != 0) { return false; }
    const std::array entries{
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&build_views)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&inherit_visibility)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&update_visibility)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&submit_stage)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&build_ui)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::leave)},
    };
    if (hooking::detour::uninstall(g_handles, entries, &idle) != hooking::detour::UninstallResult::removed) { return false; }
    g_installed.store(false, std::memory_order_release);
    g_views.store(nullptr, std::memory_order_release);
    g_inherit.store(nullptr, std::memory_order_release);
    g_visibility.store(nullptr, std::memory_order_release);
    g_submit.store(nullptr, std::memory_order_release);
    g_uiBuild.store(nullptr, std::memory_order_release);
    for (auto& model : g_models) { model = {}; }
    g_local.store(absent, std::memory_order_release);
    g_classifiedPlayer = absent;
    g_classifiedAt = 0;
    g_wasEnabled = false;
    g_reported = false;
    g_frameEnabled.store(false, std::memory_order_release);
    g_reportedWeapon.store(false, std::memory_order_release);
    g_reportedHud.store(false, std::memory_order_release);
    g_base = 0;
    return true;
}
}
