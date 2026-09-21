#include "mission_launch_panel.h"
#include "mission_launch_cards.h"
#include "../../activity/mission_launch.h"
#include "../../activity/mission_launch_options.h"
#include "../../activity/campaign_openings.h"
#include <array>
#include <cstdio>
#include <limits>

namespace dawn::client::ui::mission_launch {
namespace {
namespace launch = client::activity::mission_launch;
namespace openings = launch::openings;
namespace strikes = state::activity::strikes;
namespace nightfall = state::activity::nightfall;
unsigned g_campaign{1};
bool g_nightfall{};
strikes::Difficulty g_difficulty{strikes::Difficulty::adept};
strikes::Difficulty g_refreshedDifficulty{strikes::Difficulty::standard};
std::array<nightfall::Options, 3> g_modifiers{
    nightfall::defaults(strikes::Difficulty::adept),
    nightfall::defaults(strikes::Difficulty::master),
    nightfall::defaults(strikes::Difficulty::grandmaster)};
nightfall::Options selected_modifiers() noexcept {
    const auto index = g_difficulty == strikes::Difficulty::grandmaster ? 2U
        : g_difficulty == strikes::Difficulty::master ? 1U : 0U;
    return g_modifiers[index];
}
bool g_resetScroll{}, g_scenariosReady{}, g_spawnsReady{};
std::array<bool, openings::kMissions.size()> g_ready{};
std::size_t g_layoutRevision{(std::numeric_limits<std::size_t>::max)()}, g_catalogRevision{};
launch::ManualScratch g_validation{};

unsigned selected_group() noexcept {
    return g_campaign >= 2 ? 2U : g_campaign;
}

strikes::Difficulty selected_difficulty() noexcept {
    return g_campaign == 3 || (g_campaign == 2 && g_nightfall)
        ? g_difficulty : strikes::Difficulty::standard;
}
openings::Route selected_route(std::size_t index) noexcept {
    return openings::resolve(index, state::build_data::activities::entries(),
        openings::kMissions[index].campaign == 2 ? selected_difficulty() : strikes::Difficulty::standard);
}
void refresh() noexcept {
    const auto rows = state::build_data::activities::entries();
    const auto revision = state::build_data::scenario_layout_count();
    const bool scenariosReady = state::build_data::scenario_layouts_ready();
    const bool spawnsReady = state::build_data::spawn_sets_ready();
    if (revision == g_layoutRevision && rows.size() == g_catalogRevision
        && scenariosReady == g_scenariosReady && spawnsReady == g_spawnsReady
        && g_refreshedDifficulty == selected_difficulty()) { return; }
    g_layoutRevision = revision; g_catalogRevision = rows.size();
    g_scenariosReady = scenariosReady; g_spawnsReady = spawnsReady;
    g_refreshedDifficulty = selected_difficulty();
    for (std::size_t i = 0; i < openings::kMissions.size(); ++i) {
        const auto route = selected_route(i);
        state::build_data::scenarios::Definition selected{};
        g_ready[i] = route.valid() && scenariosReady && (!route.destination.hasSpawnSetHash || spawnsReady)
            && state::build_data::find_scenario_layout(rows[route.transport].name(), selected)
            && launch::validate_manual(route.destination, g_validation) == launch::ManualError::none;
    }
}

void campaign_tabs() noexcept {
    constexpr std::array<const char*, 4> names{"Red War", "Curse of Osiris", "Strikes", "Nightfalls"};
    const float scale = card_scale();
    const unsigned columns = ImGui::GetContentRegionAvail().x < 700.0F * scale ? 2U : 4U;
    const float width = (std::max)(1.0F, (ImGui::GetContentRegionAvail().x
        - ImGui::GetStyle().ItemSpacing.x * static_cast<float>(columns - 1)) / static_cast<float>(columns));
    for (unsigned i = 0; i < names.size(); ++i) {
        if (i % columns != 0) { ImGui::SameLine(); }
        const unsigned campaign = i;
        const bool selected = g_campaign == campaign;
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(selected ? ImGuiCol_Header : ImGuiCol_FrameBg));
        if (ImGui::Button(names[i], {width, 38.0F * scale})) {
            g_campaign = campaign; g_resetScroll = true;
        }
        ImGui::PopStyleColor();
        if (selected) {
            const auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRectFilled({a.x, b.y - 3.0F * scale}, b,
                ImGui::GetColorU32(ImGuiCol_CheckMark));
        }
    }
}

void variant_controls() noexcept {
    if (g_campaign < 2) { return; }
    ImGui::Spacing();
    if (g_campaign == 2) {
        int variant = g_nightfall ? 1 : 0;
        ImGui::SetNextItemWidth((std::min)(260.0F * card_scale(), ImGui::GetContentRegionAvail().x));
        if (ImGui::Combo("##strike_variant", &variant, "Standard strike\0Nightfall\0")) {
            g_nightfall = variant == 1; g_resetScroll = true;
        }
    }
    if (selected_difficulty() != strikes::Difficulty::standard) {
        constexpr std::array tiers{strikes::Difficulty::adept, strikes::Difficulty::master, strikes::Difficulty::grandmaster};
        int difficulty = g_difficulty == tiers[0] ? 0 : g_difficulty == tiers[1] ? 1 : 2;
        ImGui::TextDisabled("DIFFICULTY");
        ImGui::SetNextItemWidth((std::min)(260.0F * card_scale(), ImGui::GetContentRegionAvail().x));
        if (ImGui::Combo("##nightfall_difficulty", &difficulty, "Adept\0Master\0Grandmaster (GM)\0")) {
            g_difficulty = tiers[static_cast<std::size_t>(difficulty)]; g_resetScroll = true;
        }
        const auto status = launch::snapshot();
        ImGui::BeginDisabled(status.busy || status.inMission);
        if (ImGui::Button("Challenge modifiers...")) ImGui::OpenPopup("Nightfall modifiers");
        ImGui::EndDisabled();
        ImGui::SetNextWindowSize({(std::min)(520.0F * card_scale(),ImGui::GetIO().DisplaySize.x-48.0F),
            (std::min)(480.0F * card_scale(),ImGui::GetIO().DisplaySize.y-80.0F)},ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Nightfall modifiers",nullptr,ImGuiWindowFlags_NoSavedSettings)) {
            (void)ImGui::BeginChild("##modifier_options",{0,-42.0F * card_scale()});
            ImGui::BeginDisabled(status.busy || status.inMission);
            auto& options = g_modifiers[static_cast<std::size_t>(difficulty)];
            const bool gm = g_difficulty == strikes::Difficulty::grandmaster;
            ImGui::BeginDisabled(gm);
            ImGui::Checkbox("Locked equipment and mods", &options.lockedEquipment);
            ImGui::Checkbox("Disable infinite ammo", &options.disableInfiniteAmmo);
            ImGui::Checkbox("Extinguish: a wipe ends the run", &options.extinguish);
            ImGui::Checkbox("Limited revives", &options.limitedRevives);
            ImGui::EndDisabled();
            if (options.limitedRevives) {
                int revives = options.startingRevives;
                int minutes = options.reviveMinutes;
                ImGui::SetNextItemWidth((std::min)(200.0F * card_scale(), ImGui::GetContentRegionAvail().x));
                if (ImGui::SliderInt("Starting revives", &revives, 0, 4)) options.startingRevives = static_cast<std::uint8_t>(revives);
                ImGui::SetNextItemWidth((std::min)(200.0F * card_scale(), ImGui::GetContentRegionAvail().x));
                if (ImGui::SliderInt("Revives expire (minutes)", &minutes, 5, 45)) options.reviveMinutes = static_cast<std::uint16_t>(minutes);
            }
            if (gm) ImGui::TextWrapped("GM base rules are required. Solo: one death is a fireteam wipe.");
            if (gm) {
                int delta = options.powerDelta;
                ImGui::SetNextItemWidth((std::min)(200.0F * card_scale(), ImGui::GetContentRegionAvail().x));
                if (ImGui::SliderInt("Minimum power disadvantage",&delta,30,50)) options.powerDelta=static_cast<std::uint8_t>(delta);
                ImGui::TextDisabled("Teleport and noclip are disabled in GM.");
            }
            ImGui::Spacing();
            ImGui::TextWrapped("Dawn completion bonus: up to %d Glimmer.", nightfall::rewards::amount(g_difficulty));
            ImGui::TextWrapped("Glimmer rewards last until the client closes.");
            if (ImGui::Button("Reset modifiers")) options = nightfall::defaults(g_difficulty);
            ImGui::TextDisabled("Modifiers are fixed when you launch.");
            ImGui::EndDisabled();
            ImGui::EndChild();
            if (ImGui::Button("Done",{ImGui::GetContentRegionAvail().x,32.0F * card_scale()})) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
}
bool matches_mission(std::size_t index, std::int16_t activity) noexcept {
    const auto& mission = openings::kMissions[index];
    if (activity == mission.activity) { return true; }
    const auto* variant = strikes::find(activity);
    return mission.campaign == 2 && variant
        && variant->package == launch::destination_name(mission.destination);
}
bool is_current_mission(std::size_t index, const launch::Snapshot& status) noexcept {
    return status.inMission && matches_mission(index, status.currentIndex)
        && status.current_name() == launch::destination_name(openings::kMissions[index].destination);
}
const char* mission_action(std::size_t index, const launch::Snapshot& status) noexcept {
    if (is_current_mission(index, status)) { return "IN MISSION"; }
    if (status.busy && status.opening && matches_mission(index, static_cast<std::int16_t>(status.index))
        && launch::destination_name(status.destination) == launch::destination_name(openings::kMissions[index].destination)) {
        return status.status == launch::Status::preparing ? "PREPARING" : "LAUNCHING";
    }
    return g_ready[index] ? "LAUNCH >" : "UNAVAILABLE";
}
const char* current_title(const launch::Snapshot& status) noexcept {
    for (const auto& mission : openings::kMissions) {
        if (status.current_name() == launch::destination_name(mission.destination)) { return mission.title; }
    }
    const auto rows = state::build_data::activities::entries();
    if (status.currentIndex >= 0 && static_cast<std::size_t>(status.currentIndex) < rows.size()
        && rows[status.currentIndex].name() == status.current_name()) {
        const auto& text = state::build_data::activities::presentation(static_cast<std::uint16_t>(status.currentIndex));
        if (text.title[0]) { return text.title.data(); }
    }
    return status.current_name() == "mission_reunion" ? "Chosen" : "Current activity";
}

bool mission_row(std::size_t index, unsigned ordinal, const launch::Snapshot& status) noexcept {
    const auto& mission = openings::kMissions[index];
    const float scale = card_scale();
    const float width = (std::max)(1.0F, ImGui::GetContentRegionAvail().x);
    const float height = 54.0F * scale;
    const bool ready = g_ready[index];
    ImGui::PushID(static_cast<int>(index));
    const bool active = is_current_mission(index, status);
    ImGui::BeginDisabled(!ready || status.busy || status.inMission);
    const bool clicked = ImGui::InvisibleButton("##launch_opening", {width, height}, ImGuiButtonFlags_EnableNav);
    ImGui::EndDisabled();
    const bool hovered = ready && !status.busy && !status.inMission
        && (ImGui::IsItemHovered() || ImGui::IsItemFocused());
    const auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(a, b, ImGui::GetColorU32(active ? ImGuiCol_Header
        : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), 3.0F * scale);
    if (hovered || active) { draw->AddRect(a, b, ImGui::GetColorU32(ImGuiCol_CheckMark), 3.0F * scale); }
    std::array<char, 8> number{};
    (void)std::snprintf(number.data(), number.size(), "%02u", ordinal);
    const float inset = width >= 300.0F * scale ? 56.0F * scale : 14.0F * scale;
    if (inset > 20.0F * scale) {
        draw->AddText({a.x + 16.0F * scale, a.y + 19.0F * scale}, ImGui::GetColorU32(ImGuiCol_TextDisabled), number.data());
    }
    const char* action = mission_action(index, status);
    const float actionWidth = ImGui::CalcTextSize(action).x;
    const float textRight = (std::max)(a.x + inset + 1.0F, b.x - actionWidth - 32.0F * scale);
    draw->PushClipRect({a.x + inset, a.y}, {textRight, b.y}, true);
    draw->AddText({a.x + inset, a.y + 9.0F * scale}, ImGui::GetColorU32(ImGuiCol_Text), mission.title);
    std::array<char, 80> subtitle{};
    auto shownDifficulty = selected_difficulty();
    const auto* actualVariant = strikes::find(active ? status.currentIndex
        : status.busy && status.opening && matches_mission(index, static_cast<std::int16_t>(status.index))
            ? static_cast<std::int16_t>(status.index) : -1);
    if (actualVariant) { shownDifficulty = actualVariant->difficulty; }
    const char* difficultyName = mission.campaign == 2 ? strikes::name(shownDifficulty)
        : "";
    (void)std::snprintf(subtitle.data(), subtitle.size(), "%s%s%s", mission.location,
        difficultyName[0] ? " / " : "", difficultyName);
    draw->AddText({a.x + inset, a.y + 30.0F * scale}, ImGui::GetColorU32(ImGuiCol_TextDisabled), subtitle.data());
    draw->PopClipRect();
    draw->AddText({b.x - actionWidth - 16.0F * scale, a.y + 19.0F * scale},
        ImGui::GetColorU32(active || (ready && !status.inMission) ? ImGuiCol_Text : ImGuiCol_TextDisabled), action);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("%s%s", mission.description, status.inMission ? "\nReturn to orbit to launch a mission."
            : ready ? "" : "\nThis opening is not available in the installed content.");
    }
    ImGui::PopID();
    return clicked;
}
} // namespace

void draw() noexcept {
    const float scale = card_scale();
    campaign_tabs();
    variant_controls();
    refresh();
    ImGui::Dummy({0, 10.0F * scale});
    const bool wide = ImGui::GetContentRegionAvail().x >= 740.0F * scale;
    const float remaining = ImGui::GetContentRegionAvail().y;
    const float footer = ImGui::GetTextLineHeightWithSpacing() * 2.0F + 14.0F * scale;
    const bool table = wide && ImGui::BeginTable("##campaign_body", 2, ImGuiTableFlags_SizingStretchProp);
    if (table) {
        ImGui::TableSetupColumn("##campaign_identity", ImGuiTableColumnFlags_WidthFixed, 245.0F * scale);
        ImGui::TableSetupColumn("##mission_list", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextColumn();
    }
    const auto icon = g_campaign == 0 ? state::build_data::activities::Icon::redWar
                                     : g_campaign == 1 ? state::build_data::activities::Icon::osiris
                                     : state::build_data::activities::Icon::strike;
    const float extent = (table ? 84.0F : 42.0F) * scale;
    (void)draw_icon(icon, ImGui::GetCursorScreenPos(), extent);
    ImGui::Dummy({extent, extent});
    if (!table) { ImGui::SameLine(); }
    ImGui::BeginGroup();
    ImGui::TextDisabled(g_campaign == 0 ? "CAMPAIGN 01" : g_campaign == 1 ? "CAMPAIGN 02"
                                                               : "VANGUARD");
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * (table ? 1.9F : 1.5F));
    ImGui::TextWrapped("%s", g_campaign == 0 ? "The Red War" : g_campaign == 1 ? "Curse of Osiris"
        : selected_difficulty() == strikes::Difficulty::standard ? "Strikes" : "Nightfalls");
    ImGui::PopFont();
    ImGui::EndGroup();
    ImGui::Spacing();
    ImGui::TextWrapped("%s", g_campaign == 0 ? "Defend the Last City from the Red Legion, then board the Almighty and stop its assault on the Sun."
                                            : g_campaign == 1 ? "Find Osiris. Step into the Infinite Forest."
                                            : selected_difficulty() == strikes::Difficulty::standard
                                                ? "Take on the threats within the Infinite Forest."
                                                : "Challenge Tree of Probabilities and A Garden World at your chosen difficulty.");
    ImGui::Spacing();
    if (table) {
        ImGui::Separator(); ImGui::Spacing();
        ImGui::TextWrapped("Select an activity to begin at its opening.");
        ImGui::TextDisabled("Launch from orbit.");
        ImGui::TableNextColumn();
    }
    const auto count = static_cast<unsigned>(std::count_if(openings::kMissions.begin(), openings::kMissions.end(),
        [](const auto& mission) { return openings::listed(mission) && mission.campaign == selected_group(); }));
    const char* kind = g_campaign >= 2 ? "STRIKE" : "MISSION";
    ImGui::TextDisabled("%u %s%s", count, kind, count == 1 ? "" : "S");
    ImGui::Spacing();
    const float height = (std::max)(60.0F * scale,
        (table ? remaining - ImGui::GetTextLineHeightWithSpacing() : ImGui::GetContentRegionAvail().y) - footer);
    if (ImGui::BeginChild("##campaign_missions", {0, height}, ImGuiChildFlags_None)) {
        if (g_resetScroll) { ImGui::SetScrollY(0); g_resetScroll = false; }
        const auto status = launch::snapshot();
        for (const auto i : openings::kDisplayOrder) {
            if (!openings::listed(openings::kMissions[i])
                || openings::kMissions[i].campaign != selected_group()) { continue; }
            if (mission_row(i, openings::mission_number(i), status)) {
                (void)launch::request_variant(i, selected_difficulty(), selected_modifiers());
            }
        }
    }
    ImGui::EndChild();
    if (table) { ImGui::EndTable(); }
    ImGui::Spacing(); ImGui::Separator();
    const auto latest = launch::snapshot();
    const auto progress = nightfall::progress();
    if (progress.outcome == nightfall::Outcome::failed) {
        ImGui::TextWrapped("Nightfall failed. Score: %u / Defeats: %u",progress.score,progress.kills);
    } else if (progress.outcome == nightfall::Outcome::completed) {
        ImGui::TextWrapped("Nightfall complete. Score: %u / Defeats: %u",progress.score,progress.kills);
    } else if (latest.inMission && nightfall::active()) {
        ImGui::TextWrapped("Run score: %u / Defeats: %u / Revives: %u",progress.score,progress.kills,static_cast<unsigned>(progress.revives));
    }
    if (latest.inMission) {
        const auto* variant = strikes::find(latest.currentIndex);
        ImGui::TextWrapped("In mission: %s%s%s", current_title(latest), variant ? " / " : "",
            variant ? strikes::name(variant->difficulty) : "");
        if (latest.status == launch::Status::unexpectedDestination) {
            ImGui::TextWrapped("%s", launch::description(latest.status));
        } else {
            ImGui::TextDisabled("Return to orbit to launch another mission.");
        }
    } else if (latest.opening && latest.status != launch::Status::idle && latest.status != launch::Status::arrived) {
        ImGui::TextWrapped("%s", launch::description(latest.status));
    } else if (state::build_data::activities::entries().empty() || !g_scenariosReady || !g_spawnsReady) {
        ImGui::TextWrapped("%s", state::build_data::activities::extraction_failed()
            ? "Campaign content could not be read." : "Reading campaign content...");
    } else {
        ImGui::TextDisabled("Select a mission to launch its opening. Start from orbit.");
    }
}
} // namespace dawn::client::ui::mission_launch
