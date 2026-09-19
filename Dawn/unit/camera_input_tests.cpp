// Headless regression test using the production Camera page and Dear ImGui event queue.
// No window, game process, or physical keyboard input is created or controlled.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <imgui.h>

#include "../src/client/ui/camera/camera_panel.cpp"
#include "../src/core/logging/log.h"
#include "../src/core/filesystem/path.h"

namespace {
unsigned checks{};
bool focused = true;
dawn::core::ui::runtime::VisibilitySnapshot visibility{true, true, true, VK_INSERT};
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
}
namespace dawn::core::log { void write(Channel, Level, std::string_view) noexcept {} }
namespace dawn::core::ui::runtime { VisibilitySnapshot snapshot() noexcept { return visibility; } }
namespace dawn::core::ui::scaling::dpi { float current() noexcept { return 1.0F; } }
namespace dawn::client::input { bool game_focused() noexcept { return focused; } }
namespace dawn::client::hooks::camera { bool is_installed() noexcept { return true; } }
namespace dawn::client::hooks::camera::clean_view { bool installed() noexcept { return true; } }

namespace panel = dawn::client::ui::camera;
namespace settings = dawn::client::camera;
namespace {
void frame(bool draw = true) {
    ImGui::NewFrame();
    ImGui::SetNextWindowSize({900.0F, 700.0F});
    ImGui::Begin("Camera input test");
    if (draw) { panel::draw(); }
    ImGui::End();
    ImGui::EndFrame();
}
void arm(settings::Action action) {
    // The same capture state initialized by clicking a key in the production page.
    panel::g_capturing = action;
    panel::g_capture.arm(panel::keyboard());
    panel::g_reservedKey = false;
}
void tap(ImGuiKey key) {
    // Both messages arrive before the next render; the physical key is already up.
    ImGui::GetIO().AddKeyEvent(key, true);
    ImGui::GetIO().AddKeyEvent(key, false);
    frame();
}
}

int main() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {1000.0F, 800.0F};
    io.DeltaTime = 0.1F; // Reproduce capture when rendering is slower than a quick tap.
    unsigned char* pixels{};
    int width{}, height{};
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    dawn::core::path::Buffer directory{};
    const auto module = GetModuleHandleW(nullptr);
    check(dawn::core::path::artifact_directory(module, directory), "test-owned settings directory");
    const auto file = std::filesystem::path(directory.chars.data()) / "camera.json";
    std::error_code error;
    (void)std::filesystem::remove(file, error);
    check(!error, "clear only test-owned camera settings");
    settings::initialize(module);
    frame(); frame();

    arm(settings::Action::cycle);
    tap(ImGuiKey_F3);
    check(settings::bindings().keys[0] == VK_F3, "quick F3 tap binds through the production Camera page");
    check(panel::g_capturing == settings::Action::count, "successful F3 capture ends the picker");
    frame();

    for (std::size_t action = 0; action < settings::kActionCount; ++action) {
        for (int key = VK_F1; key <= VK_F24; ++key) {
            arm(static_cast<settings::Action>(action));
            tap(static_cast<ImGuiKey>(ImGuiKey_F1 + key - VK_F1));
            check(settings::bindings().keys[action] == key, "F1-F24 bind to every camera action");
            check(settings::valid_bindings(settings::bindings()), "function-key conflict swaps remain valid");
            frame();
        }
    }
    const auto saved = settings::bindings();
    settings::shutdown(); settings::initialize(module);
    check(settings::bindings() == saved, "function-key bindings survive real settings reload");

    for (int key = VK_F1; key <= VK_F24; ++key) {
        const auto expected = std::string("F") + std::to_string(key - VK_F1 + 1);
        check(panel::key_name(static_cast<std::uint32_t>(key)).data() == expected,
              "function-key labels show F1-F24");
    }
    for (const auto [named, native] : std::array{
        std::pair{ImGuiKey_Enter, VK_RETURN}, std::pair{ImGuiKey_KeypadEnter, VK_RETURN},
        std::pair{ImGuiKey_Keypad0, VK_NUMPAD0}, std::pair{ImGuiKey_LeftArrow, VK_LEFT},
        std::pair{ImGuiKey_1, static_cast<int>('1')}, std::pair{ImGuiKey_Z, static_cast<int>('Z')}}) {
        arm(settings::Action::cycle); tap(named);
        check(settings::bindings().keys[0] == native, "ordinary and keypad keys remain bindable");
        frame();
    }

    io.AddKeyEvent(ImGuiKey_F3, true); frame();
    arm(settings::Action::cycle);
    frame();
    check(panel::g_capturing == settings::Action::cycle, "preheld F3 does not bind");
    io.AddKeyEvent(ImGuiKey_F3, false); frame();
    tap(ImGuiKey_F3);
    check(settings::bindings().keys[0] == VK_F3, "fresh F3 after release binds");
    frame();

    arm(settings::Action::cycle); tap(ImGuiKey_Escape);
    check(settings::bindings().keys[0] == VK_F3 && panel::g_capturing == settings::Action::count,
          "quick Escape cancels without changing the binding");
    frame();
    arm(settings::Action::cycle); tap(ImGuiKey_Backspace);
    check(settings::bindings().keys[0] == 0, "quick Backspace unbinds");
    frame();
    arm(settings::Action::cycle); tap(ImGuiKey_Insert);
    check(settings::bindings().keys[0] == 0 && panel::g_reservedKey, "menu key remains reserved");
    frame();
    tap(ImGuiKey_A);
    check(settings::bindings().keys[0] == 'A', "quick letter can replace the reserved key");
    frame();

    arm(settings::Action::cycle); focused = false; tap(ImGuiKey_F3);
    check(settings::bindings().keys[0] == 'A' && panel::g_capturing == settings::Action::count,
          "losing focus cancels pending capture");
    frame(); focused = true; frame();
    arm(settings::Action::cycle); visibility.visible = false; tap(ImGuiKey_F3);
    check(settings::bindings().keys[0] == 'A' && panel::g_capturing == settings::Action::count,
          "closing menu cancels pending capture");
    frame(); visibility.visible = true; frame();
    arm(settings::Action::cycle); frame(false); tap(ImGuiKey_F3);
    check(settings::bindings().keys[0] == 'A' && panel::g_capturing == settings::Action::count,
          "leaving Camera page cancels pending capture");

    settings::shutdown();
    (void)std::filesystem::remove(file, error);
    check(!error, "remove test-owned camera settings");
    ImGui::DestroyContext();
    std::printf("PASS: %u Camera page queued-key checks\n", checks);
}
