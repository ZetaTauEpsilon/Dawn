#include <Windows.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "../../../core/logging/log.h"
#include "../../../core/settings/settings.h"
#include "../../../state/activity/Newlight/launchpad/runtime.h"
#include "../../../state/activity/vanilla/one_au/runtime.h"
#include "../../../state/activity/runtime.h"
#include "internal.h"

namespace dawn::client::hooks::bootflow {
namespace {

using core::log::kLineCapacity;

/**
 * The narrow channel release: one channel, with a blend time, so the world fades in.
 * Anchored on the load of the channel key, then run on through the argument spills, because the
 * wildcarded frame size leaves the head too short to be unique.
 */
constexpr std::string_view kReleaseSignatureText =
    "48 83 EC ? 8B 02 0F 57 C0 F3 0F 10 0D ? ? ? ? 48 8D 54 24 ? F3 0F 11 44 24";
/** Compiled pattern bytes of the signature text above. */
constexpr auto kReleaseSignature =
    signature<signature_length(kReleaseSignatureText)>(kReleaseSignatureText);

/**
 * The fade manager accessor: one load-effective-address of its static object, then a return.
 * Every displacement is wildcarded, so the match runs past the return to stay unique. The bytes
 * after it are the next function's prologue and its thread-block read in this build.
 */
constexpr std::string_view kAccessorSignatureText =
    "48 8D 05 ? ? ? ? C3 48 63 C0 E9 ? ? ? ? 48 83 EC ? 65 48 8B 04 25 58 00 00 00";
/** Compiled pattern bytes of the signature text above. */
constexpr auto kAccessorSignature =
    signature<signature_length(kAccessorSignatureText)>(kAccessorSignatureText);

/** Byte offsets inside the accessor, used to decode the object address from its operand. */
struct AccessorLayout {
    /** The 4-byte displacement follows the 2-byte load opcode. */
    static constexpr std::size_t displacement = 3;
    /** The instruction after the load, which the displacement is relative to. */
    static constexpr std::size_t nextInstruction = 7;
};

/** The world-transition fade channel. Stage two of the transition arms it with opaque black. */
constexpr std::uint32_t kWorldTransitionChannel = 0x57572DAC;
/** The channel's colour is a static initialiser, so it needs no lookup. */
constexpr std::array<float, 4> kOpaqueBlack{0.0F, 0.0F, 0.0F, 1.0F};
/** Blend seconds, so the world fades in rather than popping. */
constexpr float kFadeInSeconds = 0.5F;

// Native E4FAB0 acquires the same fade channel that E4FA70 releases. Both call
// E4F630: acquire uses mode 1 (fade to opaque), release mode 0 (fade to clear).
// This is a native call from the existing camera-frame integration, not a detour.
constexpr std::string_view kAcquireSignatureText =
    "48 83 EC 48 8B 02 0F 57 C0 F3 0F 11 44 24 30 48 8D 54 24 58 4D 8B C8 F3 0F 11 5C 24 28 41 B8 01 00 00 00";
constexpr auto kAcquireSignature = signature<signature_length(kAcquireSignatureText)>(kAcquireSignatureText);
using ReleaseChannel = std::int64_t(__fastcall*)(void*, std::uint32_t*, float*, float) noexcept;
using AcquireChannel = void(__fastcall*)(void*, std::uint32_t*, float*, float) noexcept;

void* g_manager{nullptr};
std::atomic<ReleaseChannel> g_release{nullptr};
std::atomic<AcquireChannel> g_acquire{nullptr};
std::atomic_bool g_openingHeld{false};
std::atomic_bool g_logged{false};

} // namespace

/** Re-arms the one line the release logs, so the next load reports its own. */
void rearm_fade_release() noexcept {
    g_logged.store(false, std::memory_order_release);
}

/** Releases the world-transition fade channel. The spawn gate decides when. */
void release_world_fade(bool flyInComplete) noexcept {
    // Only the admitted native arrival boundary can arm the mission’s opening mask.
    if (flyInComplete) {
        state::activity::newlight::launchpad::observe_fly_in_complete();
        state::activity::vanilla::one_au::observe_fly_in_complete();
    }
    // The loading mask is visual only; the native spawn gate may finish while
    // the movie prepares. Its usual fade release must not expose that camera.
    if (g_acquire.load(std::memory_order_acquire)
        && (state::activity::newlight::launchpad::opening_mask(GetTickCount64())
            || state::activity::vanilla::one_au::opening_mask(GetTickCount64()))) { poll_opening_fade(); return; }
    const ReleaseChannel release = g_release.load(std::memory_order_acquire);
    if (release == nullptr || g_manager == nullptr || !core::settings::get().client.fadeRelease) {
        return;
    }
    std::uint32_t channel = kWorldTransitionChannel;
    std::array<float, 4> colour = kOpaqueBlack;
    (void)release(g_manager, &channel, colour.data(), kFadeInSeconds);
    if (g_logged.exchange(true, std::memory_order_relaxed)) {
        return;
    }
    std::array<char, kLineCapacity> line{};
    const int written = std::snprintf(line.data(),
                                      line.size(),
                                      "ev=bootflow stage=fade_release result=issued channel=0x%X",
                                      kWorldTransitionChannel);
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

/** Runs on the existing camera frame, independently of the native spawn gate. */
void poll_opening_fade() noexcept {
    const auto acquire=g_acquire.load(std::memory_order_acquire);
    const auto release=g_release.load(std::memory_order_acquire);
    if (!acquire || !release || !g_manager) { return; }
    const bool wanted=core::settings::get().client.fadeRelease
        && (state::activity::newlight::launchpad::opening_mask(GetTickCount64())
            || state::activity::vanilla::one_au::opening_mask(GetTickCount64()));
    std::uint32_t channel=kWorldTransitionChannel;
    auto colour=kOpaqueBlack;
    if (wanted) {
        if (state::activity::world_phase()==state::activity::WorldPhase::idle) { return; }
        // Reassert across the native C9 fade: it shares this transition channel.
        // Zero blend becomes the native minimum duration, so the mask is opaque
        // for the initial arrival and while the opening's camera is installed.
        acquire(g_manager,&channel,colour.data(),0.F);
        if (!g_openingHeld.exchange(true,std::memory_order_relaxed)) {
            core::log::write(core::log::Channel::client,core::log::Level::info,
                "ev=launchpad stage=opening_mask active=1 source=fly_in_complete");
        }
    } else if (g_openingHeld.exchange(false,std::memory_order_relaxed)) {
        // Release on genuine playback, timeout, reset, or mission change. This
        // cleanup also runs after spawning has stopped calling its native gate.
        (void)release(g_manager,&channel,colour.data(),kFadeInSeconds);
        core::log::write(core::log::Channel::client,core::log::Level::info,
            "ev=launchpad stage=opening_mask active=0");
    }
}

/** Finds the fade release and its manager object. */
bool install_fade_release() noexcept {
    std::byte* const release = scan_main_image_unique(kReleaseSignature, "fade_release_channel");
    std::byte* const accessor = scan_main_image_unique(kAccessorSignature, "fade_manager_accessor");
    if (release == nullptr || accessor == nullptr) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=fade_release result=fail reason=target");
        return false;
    }
    // The manager is the static object the accessor returns. The address comes from that
    // instruction's own operand, not from a stored offset.
    g_manager = resolve_relative(accessor + AccessorLayout::displacement,
                                 accessor + AccessorLayout::nextInstruction);
    g_release.store(reinterpret_cast<ReleaseChannel>(release), std::memory_order_release);
    const auto acquire=scan_main_image_unique(kAcquireSignature,"fade_acquire_channel");
    g_acquire.store(reinterpret_cast<AcquireChannel>(acquire),std::memory_order_release);
    if (!acquire) {
        core::log::write(core::log::Channel::client,core::log::Level::warn,
            "ev=launchpad stage=opening_mask result=unavailable reason=native_acquire");
    }
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     "ev=bootflow stage=fade_release result=ok");
    return true;
}

/** Clears the fade release it found. */
void uninstall_fade_release() noexcept {
    g_release.store(nullptr, std::memory_order_release);
    g_acquire.store(nullptr,std::memory_order_release);
    g_openingHeld.store(false,std::memory_order_release);
    g_manager = nullptr;
    g_logged.store(false, std::memory_order_release);
}

} // namespace dawn::client::hooks::bootflow
