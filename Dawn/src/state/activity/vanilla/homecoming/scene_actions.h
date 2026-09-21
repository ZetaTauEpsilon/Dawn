#pragma once
// Package-derived by tools/vanilla/homecoming_scene_actions.py; do not hand-edit.
// Read only these actions, not every entry of every active selector each frame.
#include <cstdint>
#include <span>
namespace dawn::state::activity::vanilla::homecoming {
enum class SceneActionKind : std::uint8_t { speech,combat,damage };
struct SceneAction {std::uint32_t node,definition;SceneActionKind kind;std::uint8_t value;};
struct SceneActionPlan {std::uint32_t graph,root;std::span<const SceneAction> actions;};
inline constexpr SceneAction kActions80BEB7EF[]{
    {0x1B40,0x5D68,SceneActionKind::speech,21},
    {0x1D10,0x5E08,SceneActionKind::speech,22},
    {0x2650,0x60F8,SceneActionKind::speech,22},
    {0x2820,0x6180,SceneActionKind::speech,23},
    {0x29F0,0x6228,SceneActionKind::speech,24},
};
inline constexpr SceneAction kActions80C3CF88[]{
    {0xD10,0x2068,SceneActionKind::damage,0},
    {0xE60,0x20E8,SceneActionKind::combat,0},
};
inline constexpr SceneAction kActions80C3DD7A[]{
    {0x6F0,0x20D0,SceneActionKind::damage,0},
    {0xF20,0x2390,SceneActionKind::combat,0},
};
inline constexpr SceneAction kActions80C3DD7D[]{
    {0x58D0,0xEA18,SceneActionKind::damage,1},
};
inline constexpr SceneAction kActions80C3DD7F[]{
    {0x1C30,0x7ED0,SceneActionKind::combat,0},
};
inline constexpr SceneAction kActions80C3DD81[]{
    {0x2650,0x4CF0,SceneActionKind::damage,1},
};
inline constexpr SceneAction kActions80C3DEB0[]{
    {0x2760,0xD3F0,SceneActionKind::combat,0},
    {0x2F10,0xD660,SceneActionKind::combat,0},
    {0x45F0,0xDE48,SceneActionKind::combat,0},
};
inline constexpr SceneAction kActions80C3DEBD[]{
    {0x1CC0,0xBEB0,SceneActionKind::combat,0},
    {0x3AB0,0xC898,SceneActionKind::combat,0},
};
inline constexpr SceneAction kActions80C3DEF4[]{
    {0x10C0,0x5248,SceneActionKind::damage,0},
    {0x13F0,0x5368,SceneActionKind::combat,0},
};
inline constexpr SceneAction kActions80C3DEF5[]{
    {0x2E70,0x100E8,SceneActionKind::damage,5},
    {0x5D90,0x11008,SceneActionKind::combat,5},
    {0x6030,0x11108,SceneActionKind::combat,5},
    {0x7380,0x11768,SceneActionKind::speech,10},
};
inline constexpr SceneAction kActions80C3DF11[]{
    {0x1990,0x5F88,SceneActionKind::combat,0},
    {0x2370,0x6318,SceneActionKind::combat,1},
    {0x3760,0x6938,SceneActionKind::damage,1},
};
inline constexpr SceneAction kActions80C3DF1F[]{
    {0x1C00,0x60E8,SceneActionKind::damage,0},
    {0x1D50,0x6168,SceneActionKind::combat,0},
    {0x2BF0,0x6660,SceneActionKind::damage,5},
    {0x2D40,0x66E8,SceneActionKind::combat,5},
};
inline constexpr SceneActionPlan kSceneActionPlans[]{
    {0x80BEB7EF,0x52A8,kActions80BEB7EF},
    {0x80C3CF88,0x1A88,kActions80C3CF88},
    {0x80C3DD7A,0x1D28,kActions80C3DD7A},
    {0x80C3DD7D,0xC878,kActions80C3DD7D},
    {0x80C3DD7F,0x7388,kActions80C3DD7F},
    {0x80C3DD81,0x3E88,kActions80C3DD81},
    {0x80C3DEB0,0xC2D8,kActions80C3DEB0},
    {0x80C3DEBD,0xB258,kActions80C3DEBD},
    {0x80C3DEF4,0x4B38,kActions80C3DEF4},
    {0x80C3DEF5,0xEDF8,kActions80C3DEF5},
    {0x80C3DF11,0x5528,kActions80C3DF11},
    {0x80C3DF1F,0x5518,kActions80C3DF1F},
};
constexpr const SceneActionPlan* scene_action_plan(std::uint32_t graph,std::uint32_t root) noexcept {
    for(const auto& plan:kSceneActionPlans) if(plan.graph==graph && plan.root==root) return &plan;
    return nullptr;
}
inline constexpr std::uint32_t kSceneActionDefinitions[]{
    0x80B500E3,
    0x80B500EE,
    0x80B502F4,
    0x80B50301,
    0x80B50321,
    0x80B50356,
    0x80B50629,
    0x80B5062D,
    0x80B5088B,
    0x80B50893,
    0x80B5098F,
    0x80B50998,
    0x80B5099E,
    0x80B50A88,
    0x80B50BD4,
};
struct SceneActionSource {std::uint32_t definition,graph,root;};
inline constexpr SceneActionSource kSceneActionSources[]{
    {0x80B500E3,0x80C3DD7A,0x1D28},
    {0x80B500EE,0x80C3DD7D,0xC878},
    {0x80B502F4,0x80C3DD7F,0x7388},
    {0x80B50301,0x80C3DD7F,0x7388},
    {0x80B50321,0x80C3DD7F,0x7388},
    {0x80B50356,0x80C3DD81,0x3E88},
    {0x80B50629,0x80C3DEBD,0xB258},
    {0x80B5062D,0x80C3DEB0,0xC2D8},
    {0x80B5088B,0x80C3CF88,0x1A88},
    {0x80B50893,0x80C3CF88,0x1A88},
    {0x80B5098F,0x80C3DEF4,0x4B38},
    {0x80B50998,0x80BEB7EF,0x52A8},
    {0x80B5099E,0x80C3DEF5,0xEDF8},
    {0x80B50A88,0x80C3DF1F,0x5518},
    {0x80B50BD4,0x80C3DF11,0x5528},
};
}
