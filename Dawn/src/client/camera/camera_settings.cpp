#include "camera_settings.h"

#include <Windows.h>
#include <array>
#include <atomic>
#include <cstdio>

#include "../../core/filesystem/path.h"
#include "../../core/logging/log.h"
#include "settings_codec.h"

namespace dawn::client::camera {
namespace {

SRWLOCK g_lock = SRWLOCK_INIT;
std::atomic<Mode> g_mode{Mode::normal};
std::atomic_uint32_t g_bindings{pack(Bindings{})};
std::atomic_bool g_cameraOnly{};
core::path::Buffer g_path{};
bool g_pathResolved{};
constexpr std::size_t kFileCapacity = 1024;

/** Caller holds g_lock. Replace the complete document only after the write succeeds. */
void store_locked() noexcept {
    const auto mode = g_mode.load(std::memory_order_relaxed);
    const auto keys = unpack(g_bindings.load(std::memory_order_relaxed)).keys;
    std::array<char, kFileCapacity> document{};
    const int size = std::snprintf(document.data(), document.size(),
        "{\n  \"third_person_enabled\": %s,\n  \"front_view\": %s,\n"
        "  \"cycle_key\": %u,\n  \"rear_key\": %u,\n  \"front_key\": %u,\n"
        "  \"camera_only_key\": %u\n}\n",
        mode != Mode::normal ? "true" : "false", mode == Mode::front ? "true" : "false",
        static_cast<unsigned>(keys[0]), static_cast<unsigned>(keys[1]),
        static_cast<unsigned>(keys[2]), static_cast<unsigned>(keys[3]));
    bool stored = false;
    auto temporary = g_path;
    if (g_pathResolved && size > 0 && static_cast<std::size_t>(size) < document.size()
        && core::path::append(temporary, L".tmp")) {
        const HANDLE file = CreateFileW(temporary.chars.data(), GENERIC_WRITE, 0, nullptr,
                                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            DWORD written{};
            stored = WriteFile(file, document.data(), static_cast<DWORD>(size), &written, nullptr)
                     && written == static_cast<DWORD>(size);
            stored = CloseHandle(file) != FALSE && stored;
            if (stored) {
                stored = MoveFileExW(temporary.chars.data(), g_path.chars.data(),
                                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
            }
            if (!stored) { (void)DeleteFileW(temporary.chars.data()); }
        }
    }
    if (!stored) {
        core::log::write(core::log::Channel::client, core::log::Level::warn,
                         "ev=camera stage=store result=fail");
    }
}

void publish_mode_locked(Mode mode) noexcept {
    g_mode.store(mode, std::memory_order_release);
    store_locked();
    constexpr std::array messages{"ev=camera stage=mode mode=normal",
                                  "ev=camera stage=mode mode=rear", "ev=camera stage=mode mode=front"};
    core::log::write(core::log::Channel::client, core::log::Level::info,
                     messages[static_cast<std::size_t>(mode)]);
}

void load() noexcept {
    const HANDLE file = CreateFileW(g_path.chars.data(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { return; }
    std::array<char, kFileCapacity> buffer{};
    DWORD size{};
    const bool read = ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &size, nullptr) != FALSE;
    (void)CloseHandle(file);
    if (!read || size == 0 || size == buffer.size()) { return; }
    const auto settings = detail::parse_settings({buffer.data(), size});
    g_mode.store(settings.mode, std::memory_order_release);
    g_bindings.store(pack(settings.bindings), std::memory_order_release);
}

} // namespace

void initialize(void* module) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    g_mode.store(Mode::normal, std::memory_order_release);
    g_bindings.store(pack(Bindings{}), std::memory_order_release);
    g_cameraOnly.store(false, std::memory_order_release);
    g_pathResolved = core::path::artifact_directory(module, g_path)
                     && core::path::append(g_path, L"\\camera.json");
    if (g_pathResolved) { load(); }
    ReleaseSRWLockExclusive(&g_lock);
}

void shutdown() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    g_mode.store(Mode::normal, std::memory_order_release);
    g_bindings.store(pack(Bindings{}), std::memory_order_release);
    g_cameraOnly.store(false, std::memory_order_release);
    g_path = {};
    g_pathResolved = false;
    ReleaseSRWLockExclusive(&g_lock);
}

Mode mode() noexcept { return g_mode.load(std::memory_order_acquire); }

void set_mode(Mode value) noexcept {
    if (value != Mode::normal && value != Mode::rear && value != Mode::front) { return; }
    AcquireSRWLockExclusive(&g_lock);
    publish_mode_locked(value);
    ReleaseSRWLockExclusive(&g_lock);
}

void cycle_mode() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    publish_mode_locked(next_mode(g_mode.load(std::memory_order_relaxed)));
    ReleaseSRWLockExclusive(&g_lock);
}

void toggle_mode(Mode target) noexcept {
    if (target != Mode::normal && target != Mode::rear && target != Mode::front) { return; }
    AcquireSRWLockExclusive(&g_lock);
    publish_mode_locked(toggled_mode(g_mode.load(std::memory_order_relaxed), target));
    ReleaseSRWLockExclusive(&g_lock);
}

bool third_person_enabled() noexcept { return mode() != Mode::normal; }
Bindings bindings() noexcept { return unpack(g_bindings.load(std::memory_order_acquire)); }

bool set_binding(Action action, std::uint32_t key) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    auto value = bindings();
    const bool accepted = rebind(value, action, key);
    if (accepted) {
        g_bindings.store(pack(value), std::memory_order_release);
        store_locked();
    }
    ReleaseSRWLockExclusive(&g_lock);
    return accepted;
}

void reset_bindings() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    g_bindings.store(pack(Bindings{}), std::memory_order_release);
    store_locked();
    ReleaseSRWLockExclusive(&g_lock);
}

bool camera_only() noexcept { return g_cameraOnly.load(std::memory_order_acquire); }

void set_camera_only(bool enabled) noexcept {
    g_cameraOnly.store(enabled, std::memory_order_release);
    core::log::write(core::log::Channel::client, core::log::Level::info,
        enabled ? "ev=camera stage=camera_only enabled=1" : "ev=camera stage=camera_only enabled=0");
}

void toggle_camera_only() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    set_camera_only(!g_cameraOnly.load(std::memory_order_relaxed));
    ReleaseSRWLockExclusive(&g_lock);
}

} // namespace dawn::client::camera
