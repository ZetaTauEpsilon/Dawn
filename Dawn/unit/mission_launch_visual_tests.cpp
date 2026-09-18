// Offscreen integration test of the production Dawn layout, campaign panel, logo, and DX11 renderer.
// Native game launch and scenario services are isolated; this executable never enters the game.
#define main metadata_fixture_main
#include "mission_launch_metadata_tests.cpp"
#undef main
#include <imgui_internal.h>
#include <backends/imgui_impl_dx11.h>
#include "core/ui/memory/allocator.h"
#include "core/ui/theme/dawn_ui_theme.h"
#include "core/ui/animation/transition/ui_transition_animation.h"
#include "core/ui/modules/registry/ui_module_registry.h"
#include "core/ui/layout/ui_layout_lifecycle.h"
#include "core/logging/log.h"
#include "client/hooks/graphics/textures/graphics_texture_upload.h"
#include "client/ui/runtime/client_ui_module_runtime.h"
#include "../src/client/ui/mission_launch/mission_launch_panel.cpp"

namespace {
namespace ui = dawn::core::ui;
namespace panel = dawn::client::ui::mission_launch;
namespace launch = dawn::client::activity::mission_launch;
namespace openings = launch::openings;
float g_scale = 1.0F;
unsigned g_errors{}, g_requests{};
std::size_t g_requestedMission{};
bool g_missingContent{}, g_spawnPublished{true};
std::size_t g_revision{1};
launch::Snapshot g_launchState{};
ui::layout::StateSnapshot g_layoutState{};
ID3D11Device* g_gpu{};
ID3D11DeviceContext* g_context{};
ID3D11Texture2D* g_target{};
ID3D11RenderTargetView* g_rtv{};
constexpr UINT kWidth = 1500, kHeight = 1000;
void error_callback(ImGuiContext*, void*, const char* message) {
    ++g_errors; std::cerr << "ImGui error: " << message << '\n';
}
void screenshot(const std::filesystem::path& path) {
    D3D11_TEXTURE2D_DESC desc{};
    g_target->GetDesc(&desc); desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* staging{};
    check(SUCCEEDED(g_gpu->CreateTexture2D(&desc, nullptr, &staging)), "screenshot staging");
    g_context->CopyResource(staging, g_target);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    check(SUCCEEDED(g_context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped)), "screenshot readback");
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";
    for (UINT y = 0; y < kHeight; ++y) {
        const auto* row = static_cast<const char*>(mapped.pData) + y * mapped.RowPitch;
        for (UINT x = 0; x < kWidth; ++x) { out.write(row + x * 4, 3); }
    }
    g_context->Unmap(staging, 0); staging->Release();
}
void frame(float width = 1500, float height = 1000, ImGuiID activate = 0, bool visible = true) {
    ImGui::GetIO().DisplaySize = {width, height};
    ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
    if (activate) { ImGui::ActivateItemByID(activate); }
    (void)ui::layout::render(visible);
    ImGui::Render();
    const float clear[]{0.019F, 0.025F, 0.04F, 1};
    g_context->OMSetRenderTargets(1, &g_rtv, nullptr); g_context->ClearRenderTargetView(g_rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    check(g_errors == 0, "production layout emits no ImGui errors");
}
ImGuiWindow* mission_window() {
    for (auto* window : ImGui::GetCurrentContext()->Windows) {
        if (std::strstr(window->Name, "##campaign_missions") && window->Active) { return window; }
    }
    return nullptr;
}
ImGuiID row_id(std::size_t index) {
    auto* window = mission_window(); check(window != nullptr, "campaign list is visible");
    const int key = static_cast<int>(index);
    const ImGuiID seed = ImHashData(&key, sizeof(key), window->IDStack.back());
    return ImHashStr("##launch_opening", 0, seed);
}
void dummy_page() noexcept { ImGui::TextUnformatted("Dawn controls"); }
}
namespace dawn::core::ui::scaling::dpi {
float current() noexcept { return g_scale; }
float pixels(float value) noexcept { return value * g_scale; }
ImVec2 pixels(const ImVec2& value) noexcept { return {value.x * g_scale, value.y * g_scale}; }
}
namespace dawn::core::ui::layout {
StateSnapshot snapshot() noexcept { return g_layoutState; }
namespace internal {
bool context_is_current() noexcept { return ImGui::GetCurrentContext() != nullptr; }
void select_module(std::string_view id) noexcept {
    g_layoutState.selectedStableId.fill(0); g_layoutState.selectedStableIdLength = id.size();
    std::copy(id.begin(), id.end(), g_layoutState.selectedStableId.begin());
}
}
}
namespace dawn::core::ui::layout::credits { void request_open() noexcept {} }
namespace dawn::core::log { void write(Channel, Level, std::string_view) noexcept {} }
namespace dawn::client::ui::movement { void draw() noexcept { dummy_page(); } }
namespace dawn::client::ui::player { void draw() noexcept { dummy_page(); } }
namespace dawn::state::build_data {
bool scenario_layouts_ready() noexcept { return !g_missingContent; }
bool spawn_sets_ready() noexcept { return !g_missingContent && g_spawnPublished; }
std::size_t scenario_layout_count() noexcept { return g_revision; }
bool find_scenario_layout(std::string_view name, scenarios::Definition& value) noexcept {
    value = {};
    if (g_missingContent) { return false; }
    if (name == "mission_reunion") { return true; }
    for (const auto& mission : openings::kMissions) {
        const auto& destination = mission.destination;
        if (name != launch::destination_name(destination)) { continue; }
        std::copy(name.begin(), name.end(), value.name.begin());
        value.nameLength = static_cast<std::uint8_t>(name.size());
        std::copy(name.begin(), name.end(), value.spawnStem.begin());
        value.spawnStemLength = static_cast<std::uint8_t>(name.size());
        value.bubbleCount = destination.bubble + 1;
        value.bubbleStateCounts[destination.bubble] = 1;
        value.bubbleMapIndices[destination.bubble] = destination.bubble;
        return true;
    }
    return false;
}
bool find_spawn_sets(std::string_view name, std::span<spawn_sets::NameHash> values, std::size_t& count) noexcept {
    count = 0;
    for (const auto& mission : openings::kMissions) {
        const auto& destination = mission.destination;
        if (name != launch::destination_name(destination) || !destination.hasSpawnSetHash || values.empty()) { continue; }
        values[0] = {}; values[0].value = destination.spawnSetHash;
        values[0].pointCount = 1; values[0].inMapPackage = 1;
        values[0].bubbleMask[destination.bubble / 8] = static_cast<std::uint8_t>(1U << (destination.bubble % 8));
        count = 1; return true;
    }
    return false;
}
}
namespace dawn::client::activity::mission_launch {
Snapshot snapshot() noexcept { return g_launchState; }
bool request_opening(std::size_t mission) noexcept {
    return request_variant(mission, state::activity::strikes::Difficulty::standard);
}
bool request_variant(std::size_t mission, state::activity::strikes::Difficulty difficulty) noexcept {
    return request_variant(mission,difficulty,state::activity::nightfall::defaults(difficulty));
}
bool request_variant(std::size_t mission, state::activity::strikes::Difficulty difficulty,
    state::activity::nightfall::Options options) noexcept {
    if (g_launchState.busy) { return false; }
    const auto route = openings::resolve(mission, state::build_data::activities::entries(), difficulty);
    if (!route.valid()) { return false; }
    ++g_requests; g_requestedMission = mission;
    g_launchState = {Status::requested, route.transport, true, true, route.destination, true};
    g_launchState.nightfallOptions = state::activity::nightfall::sanitize(difficulty,options);
    return true;
}
const char* description(Status status) noexcept { return status == Status::unexpectedDestination ? "The selected opening did not load. Return to orbit and try again." : "Launching mission opening..."; }
}

int main(int argc, char** argv) {
    using namespace dawn;
    check(argc == 4, "provide installed metadata directory, OTF font, and screenshot directory");
    check(metadata_fixture_main(2, argv) == 0, "installed metadata fixture checks");
    static std::array<state::build_data::activities::Definition, state::build_data::activities::kCapacity> rows{};
    std::size_t count{};
    check(middleware::content::packages::tables::activities::decode(load(g_fixture / "81327CF0.bin"), rows, count), "decode public activities");
    check(state::build_data::activities::publish(std::span(rows).first(count)), "publish installed activities");
    check(ui::memory::initialize(), "production fixed arena");
    ImGui::CreateContext();
    auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.DeltaTime = 1.0F / 60;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigErrorRecoveryEnableAssert = false; io.ConfigErrorRecoveryEnableTooltip = false;
    ImGui::GetCurrentContext()->ErrorCallback = error_callback;
    const auto font = load(argv[2]);
    ImFontConfig config{}; config.FontDataOwnedByAtlas = false; config.RasterizerDensity = 2;
    check(io.Fonts->AddFontFromMemoryTTF(const_cast<std::byte*>(font.data()), static_cast<int>(font.size()), 16, &config) != nullptr, "installed font");
    ImGui::GetStyle().FontSizeBase = 16;
    D3D_FEATURE_LEVEL feature{};
    check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
        &g_gpu, &feature, &g_context)), "offscreen WARP device");
    D3D11_TEXTURE2D_DESC desc{}; desc.Width = kWidth; desc.Height = kHeight;
    desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    check(SUCCEEDED(g_gpu->CreateTexture2D(&desc, nullptr, &g_target)), "render target");
    check(SUCCEEDED(g_gpu->CreateRenderTargetView(g_target, nullptr, &g_rtv)), "render target view");
    check(ImGui_ImplDX11_Init(g_gpu, g_context), "production DX11 backend");
    client::hooks::graphics::textures::Uploaded logo{};
    check(client::hooks::graphics::textures::upload_logo_sheet(g_gpu, logo), "embedded Dawn logo decoded and uploaded");
    D3D11_TEXTURE2D_DESC logoDesc{}; logo.texture->GetDesc(&logoDesc);
    check(logoDesc.Width == 1028 && logoDesc.Height == 1028, "supplied logo dimensions preserved");
    panel::art::prepare(g_gpu);
    ui::theme::apply();
    check(client::ui::runtime::initialize(), "production client navigation");
    const auto registered = ui::modules::registry::snapshot();
    check(registered.entries().size() == 3 && registered.entries().front().stable_id() == "client.mission_launch",
        "campaigns are the default and Forest is not registered");
    for (const auto& item : registered.entries()) { check(item.display_name() != "Forest", "Forest tab removed"); }
    ui::modules::registry::PageRegistration activity, hud, logs;
    check(activity.acquire(ui::modules::Owner::server, "server.activity", "Activity", dummy_page), "activity page");
    check(hud.acquire(ui::modules::Owner::core, "core.hud", "HUD", dummy_page), "hud page");
    check(logs.acquire(ui::modules::Owner::core, "core.logs", "Logs", dummy_page), "logs page");
    std::filesystem::create_directories(argv[3]);
    const std::filesystem::path screens = argv[3];
    for (unsigned i = 0; i < 90; ++i) { frame(); }
    check(std::all_of(panel::g_ready.begin(), panel::g_ready.end(), [](bool ready) { return ready; }), "all curated openings available");
    screenshot(screens / "dawn-osiris.ppm");
    for (std::size_t i = 0; i < openings::kMissions.size(); ++i) {
        if (!openings::listed(openings::kMissions[i])) continue;
        panel::g_campaign = openings::kMissions[i].campaign; g_launchState = {};
        frame(); frame();
        auto* window = mission_window();
        ImGui::SetScrollY(window, i == 0 ? 0.0F : static_cast<float>(i - 1) * 73.0F);
        frame(); frame();
        const auto before = g_requests;
        frame(1500, 1000, row_id(i)); frame(); frame();
        check(g_requests == before + 1 && g_requestedMission == i && g_launchState.index == openings::kMissions[i].activity,
            "each mission and strike row launches its own native activity");
        frame(1500, 1000, row_id(i)); frame();
        check(g_requests == before + 1, "busy mission rows cannot double-launch");
    }
    // A completed request must remain visible as the actual mission, independently of busy.
    panel::g_campaign = 1; panel::g_resetScroll = true;
    g_launchState = {};
    g_launchState.status = launch::Status::arrived; g_launchState.opening = true; g_launchState.inMission = true;
    g_launchState.currentIndex = 292;
    constexpr std::string_view gateway = "mission_abs";
    std::copy(gateway.begin(), gateway.end(), g_launchState.currentPackage.begin());
    g_launchState.currentPackageLength = static_cast<std::uint8_t>(gateway.size());
    frame(); frame();
    check(std::string_view(panel::mission_action(1, g_launchState)) == "IN MISSION"
        && std::string_view(panel::current_title(g_launchState)) == "Gateway", "Gateway active state and title");
    const auto inMissionRequests = g_requests;
    frame(1500, 1000, row_id(1)); frame(1500, 1000, row_id(2)); frame();
    check(g_requests == inMissionRequests, "mission presence blocks launching until orbit");
    screenshot(screens / "dawn-in-mission.ppm");
    g_launchState.status = launch::Status::unexpectedDestination; g_launchState.currentIndex = 282;
    g_launchState.currentPackage.fill(0);
    constexpr std::string_view chosen = "mission_reunion";
    std::copy(chosen.begin(), chosen.end(), g_launchState.currentPackage.begin());
    g_launchState.currentPackageLength = static_cast<std::uint8_t>(chosen.size());
    check(!panel::is_current_mission(1, g_launchState)
        && std::string_view(panel::mission_action(1, g_launchState)) != "LAUNCHING"
        && std::string_view(panel::current_title(g_launchState)) == "Chosen",
        "wrong Chosen arrival cannot mark Gateway active or pending");
    frame(); frame(); screenshot(screens / "dawn-wrong-destination.ppm");
    g_launchState = {};
    const auto orbitRequests = g_requests;
    frame(); frame(1500, 1000, row_id(1)); frame();
    check(g_requests == orbitRequests + 1, "return to orbit enables launch again");
    g_launchState.status = launch::Status::preparing;
    check(std::string_view(panel::mission_action(1, g_launchState)) == "PREPARING",
        "waiting for hooks is distinct from native launch");
    g_launchState = {}; panel::g_campaign = 0; panel::g_resetScroll = true;
    frame(); frame();
    screenshot(screens / "dawn-red-war-1au.ppm");
    const auto hiddenHomecomingRequests = g_requests;
    frame(1500, 1000, row_id(0)); frame();
    check(g_requests == hiddenHomecomingRequests, "Homecoming has no launch button");
    // Exercise the actual campaign tab button with keyboard navigation activation.
    ImGuiWindow* content{};
    for (auto* window : ImGui::GetCurrentContext()->Windows) {
        if (std::strstr(window->Name, "##dawn_content") && !std::strstr(window->Name, "##campaign_missions") && window->Active) { content = window; }
    }
    check(content != nullptr, "content surface found");
    panel::g_campaign=1;
    frame(1500, 1000, content->GetID("Red War")); frame(); frame();
    check(panel::g_campaign==0, "Red War tab exposes 1AU");
    check(std::count_if(openings::kMissions.begin(),openings::kMissions.end(),
        [](const auto& m) {return m.campaign==0 && openings::listed(m);})==1,
        "1AU is the only listed Red War mission");
    frame(1500, 1000, content->GetID("Curse of Osiris")); frame(); frame();
    check(panel::g_campaign == 1, "Curse of Osiris tab switches campaigns");
    frame(1500, 1000, content->GetID("Strikes")); frame(); frame();
    check(panel::g_campaign == 2, "Strikes tab switches to strikes");
    screenshot(screens / "dawn-strikes.ppm");
    frame(1500, 1000, content->GetID("Nightfalls")); frame(); frame();
    check(panel::g_campaign == 3, "dedicated Nightfalls tab opens");
    g_launchState = {}; panel::g_difficulty = panel::strikes::Difficulty::adept;
    frame(); frame(); frame(1500, 1000, row_id(9)); frame();
    panel::g_difficulty = panel::strikes::Difficulty::master;
    check(std::string_view(panel::mission_action(9, g_launchState)) == "LAUNCHING"
        && g_launchState.index == 830, "picker changes preserve actual pending tier status");
    g_launchState.busy = false; g_launchState.inMission = true; g_launchState.currentIndex = 830;
    constexpr std::string_view treeStrike = "strike_pact";
    std::copy(treeStrike.begin(), treeStrike.end(), g_launchState.currentPackage.begin());
    g_launchState.currentPackageLength = static_cast<std::uint8_t>(treeStrike.size());
    check(std::string_view(panel::mission_action(9, g_launchState)) == "IN MISSION",
        "picker changes preserve actual in-mission tier status");
    for (const auto tier : {panel::strikes::Difficulty::adept, panel::strikes::Difficulty::master,
                           panel::strikes::Difficulty::grandmaster}) {
        panel::g_difficulty = tier; g_launchState = {}; frame(); frame();
        const auto requests = g_requests;
        frame(1500, 1000, row_id(9)); frame();
        check(g_requests == requests + 1 && g_launchState.index == panel::selected_route(9).transport,
            "Nightfall row sends its selected difficulty identity");
    }
    g_launchState = {}; frame(); frame(); screenshot(screens / "dawn-nightfalls-gm.ppm");
    const auto modifiersButton=mission_window()->ParentWindow->GetID("Challenge modifiers...");
    frame(1500,1000,modifiersButton);for(unsigned i=0;i<12;++i)frame();
    auto* modifiersWindow=ImGui::FindWindowByName("Nightfall modifiers");
    check(modifiersWindow && modifiersWindow->Active,"GM modifier window opens from selection page");
    check(!modifiersWindow->Hidden,"GM modifier window is rendered");
    screenshot(screens / "dawn-gm-modifiers.ppm");
    panel::g_modifiers[2].startingRevives=2; panel::g_modifiers[2].reviveMinutes=30; panel::g_modifiers[2].powerDelta=35;
    ImGui::GetIO().AddMousePosEvent(modifiersWindow->Pos.x+modifiersWindow->Size.x/2,
        modifiersWindow->Pos.y+modifiersWindow->Size.y-ImGui::GetStyle().WindowPadding.y-16.0F);
    ImGui::GetIO().AddMouseButtonEvent(0,true);frame();
    ImGui::GetIO().AddMouseButtonEvent(0,false);frame();frame();
    check(!modifiersWindow->Active,"Done returns to strike selection");
    panel::g_campaign = 2; panel::g_nightfall = true; frame(); frame();
    const auto strikeVariantRequests = g_requests;
    frame(1500, 1000, row_id(10)); frame();
    check(g_requests == strikeVariantRequests + 1 && g_launchState.index == 813,
        "Strikes tab also launches the Garden Grandmaster variant");
    check(g_launchState.nightfallOptions.startingRevives==2 && g_launchState.nightfallOptions.reviveMinutes==30
        && g_launchState.nightfallOptions.powerDelta==35,"modifier window selection copied into exact GM launch");
    g_launchState = {};
    g_launchState = {}; panel::g_nightfall = false;
    panel::g_campaign=1;panel::g_resetScroll=true;frame();frame();
    const auto before = g_requests;
    g_missingContent = true; ++g_revision; frame(); frame();
    check(std::none_of(panel::g_ready.begin(), panel::g_ready.end(), [](bool ready) { return ready; }), "missing content disables all launch rows");
    frame(1500, 1000, row_id(1)); frame();
    check(g_requests == before, "unavailable row cannot launch");
    screenshot(screens / "dawn-unavailable.ppm");
    g_missingContent = false; ++g_revision; g_spawnPublished = false; frame();
    check(panel::g_ready[0] && !panel::g_ready[1], "opening without a named spawn can load before the spawn catalog");
    g_spawnPublished = true; frame();
    check(panel::g_ready[1], "late spawn publication enables missions without a scenario-count change");
    for (const float dpi : {1.0F, 1.5F, 2.0F}) {
        g_scale = dpi; ui::theme::apply();
        for (const ImVec2 viewport : {ImVec2{1500, 1000}, ImVec2{1280, 720}, ImVec2{760, 720}}) {
            for (unsigned i = 0; i < 4; ++i) { frame(viewport.x, viewport.y); }
        }
    }
    g_scale = 1; ui::theme::apply(); frame(760, 720); frame(760, 720);
    screenshot(screens / "dawn-narrow.ppm");
    for (unsigned i = 0; i < 180; ++i) { frame(1500, 1000, 0, false); }
    check(ImGui::GetDrawData()->TotalVtxCount == 0, "closed Dawn window leaves no logo or panel geometry");
    for (unsigned i = 0; i < 90; ++i) { frame(); }
    screenshot(screens / "dawn-osiris.ppm");
    logs.release(); hud.release(); activity.release(); client::ui::runtime::shutdown();
    client::hooks::graphics::textures::release_logo_sheet(logo);
    panel::art::release(); ImGui_ImplDX11_Shutdown(); ImGui::DestroyContext();
    g_context->ClearState(); g_context->Flush(); g_rtv->Release(); g_target->Release(); g_context->Release();
    check(g_gpu->Release() == 0, "GPU resources released");
    const auto stats = ui::memory::snapshot();
    check(stats.outstandingAllocations == 0 && ui::memory::shutdown(), "fixed arena released");
    std::cout << "PASS: Dawn production layout, " << openings::kMissions.size() - 1 << " launch buttons, Homecoming hidden, preparing/in-mission/wrong-destination/busy/unavailable states, campaign/Strikes/Nightfalls tabs, DPI and close/reopen; zero ImGui errors; "
        << stats.highWaterBytes << "/" << stats.capacityBytes << " arena high water\n";
}
