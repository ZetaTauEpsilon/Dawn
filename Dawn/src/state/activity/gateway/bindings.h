// Native identifiers and allowed operations. Mission decisions live in scripts/gateway.lua.
#pragma once
#include "../coo/mission_script.h"
#include "catalog.h"
namespace dawn::state::activity::gateway {
inline constexpr coo::Asset kVanceScene{0xBA0B27A0U,0x80F46DE0U,43,5};
inline constexpr std::uint32_t kVanceAscentMs=22640, kVanceFinishMs=31000;
inline constexpr std::uint32_t kReturnCueMs=8960; // First "Please" in native blocked-gate exchange.
inline constexpr coo::Asset kModule{kRoot,kScenario,0,0};
inline constexpr coo::Asset kLanding{0x85742F3EU,0x80F470E5U,60,359};
inline constexpr coo::Asset kRecess{0x85742F3EU,0x80F470E5U,60,361};
inline constexpr coo::Asset kObjective{kRoot,0x80F47420U,68,0};
inline constexpr coo::Asset kDialogueAsset{kRoot,0x80F47426U,53,2};
// Event clock keys use slot as the dialogue row; they are never published as native objects.
inline constexpr coo::Asset kBlockedDialogueClock{kRoot,0x80F47426U,53,5};
inline constexpr coo::script::Capability kCapabilities[]{
    {"forest.x266","*",{coo::Operation::observation,kModule,1024,coo::Wait::observed}},
    {"forest.hydra_dead","*",{coo::Operation::observation,kModule,1025,coo::Wait::observed}},
    {"opening.module","composition",{coo::Operation::mechanic,{0x986985D0U,0x80F46DB0U,0,0},1U,coo::Wait::requested}},
    {"opening.checked","composition",{coo::Operation::observation,{0x00000000U,0x00000000U,0,0},0U,coo::Wait::observed}},
    {"landing.entered","*",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,359},0U,coo::Wait::observed}},
    {"objective.find_gateway","*",{coo::Operation::objective,{0x986985D0U,0x80F47420U,68,0},3369893117U,coo::Wait::requested}},
    {"dialogue.ikora_gateway","*",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},1U,coo::Wait::nativeReady}},
    {"recess.entered","*",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,361},0U,coo::Wait::observed}},
    {"marchers.start","*",{coo::Operation::mechanic,{0x986985D0U,0x80F46DB0U,0,0},10U,coo::Wait::requested}},
    {"recess.initial","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},1U,coo::Wait::requested}},
    {"recess.reinforce_trigger","*",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,367},1U,coo::Wait::observed}},
    {"recess.reinforcements","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},2U,coo::Wait::requested}},
    {"recess.cleared","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},1U,coo::Wait::completed}},
    {"recess.reinforcements_cleared","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},2U,coo::Wait::completed}},
    {"recess.cannons","*",{coo::Operation::device,{0x986985D0U,0x80F46DB0U,0,0},1U,coo::Wait::nativeReady}},
    {"shelf.entered","*",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,369},0U,coo::Wait::observed}},
    {"shelf.initial","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},3U,coo::Wait::requested}},
    {"shelf.reinforce_trigger","*",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,372},3U,coo::Wait::observed}},
    {"shelf.reinforcements","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},4U,coo::Wait::requested}},
    {"end.entered","*",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,375},0U,coo::Wait::observed}},
    {"end.initial","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},5U,coo::Wait::requested}},
    {"end.reinforce_trigger","*",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,379},5U,coo::Wait::observed}},
    {"end.reinforcements","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},6U,coo::Wait::requested}},
    {"end.cleared","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},5U,coo::Wait::completed}},
    {"end.reinforcements_cleared","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},6U,coo::Wait::completed}},
    {"end.cannon","*",{coo::Operation::device,{0x986985D0U,0x80F46DB0U,0,0},2U,coo::Wait::requested}},
    {"dialogue.vance","*",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},2U,coo::Wait::nativeReady}},
    {"lighthouse.landed","*",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,378},0U,coo::Wait::observed}},
    {"shelf.passed","*",{coo::Operation::observation,{0x85742F3EU,0x80F470E5U,60,375},256U,coo::Wait::observed}},
    {"mainland.outskirts","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},7U,coo::Wait::requested}},
    {"mainland.center","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},8U,coo::Wait::requested}},
    {"objective.forest_gate","*",{coo::Operation::objective,{0x986985D0U,0x80F47420U,68,0},4154833763U,coo::Wait::requested}},
    {"mainland.intro","*",{coo::Operation::observation,{0x4B946B28U,0x80F46EC0U,60,439},0U,coo::Wait::observed}},
    {"dialogue.forest_gate","*",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},3U,coo::Wait::nativeReady}},
    {"mainland.cleared","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},8U,coo::Wait::completed}},
    {"forest.approached","*",{coo::Operation::observation,{0x4B946B28U,0x80F46EC0U,60,441},0U,coo::Wait::observed}},
    {"dialogue.at_gate","*",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},4U,coo::Wait::nativeReady}},
    {"forest.blocked","*",{coo::Operation::observation,{0x4B946B28U,0x80F46EC0U,60,448},0U,coo::Wait::observed}},
    {"dialogue.blocked","*",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},5U,coo::Wait::nativeReady}},
    {"vance.return_cue","*",{coo::Operation::eventAfter,kBlockedDialogueClock,kReturnCueMs,coo::Wait::observed},300000},
    {"objective.bring_sagira","*",{coo::Operation::objective,{0x986985D0U,0x80F47420U,68,0},4197838318U,coo::Wait::requested}},
    {"return.center","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},9U,coo::Wait::requested}},
    {"return.outskirts","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},10U,coo::Wait::requested}},
    {"objective.return","*",{coo::Operation::objective,{0x986985D0U,0x80F47420U,68,0},3034087627U,coo::Wait::requested}},
    {"return.contact","*",{coo::Operation::observation,{0x4B946B28U,0x80F46EC0U,60,461},512U,coo::Wait::observed}},
    {"dialogue.descendants","*",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},6U,coo::Wait::nativeReady}},
    {"finale.approached","*",{coo::Operation::observation,{0x4B946B28U,0x80F46EC0U,60,461},0U,coo::Wait::observed}},
    {"finale.wave_1","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},11U,coo::Wait::completed}},
    {"finale.wave_2","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},12U,coo::Wait::completed}},
    {"finale.wave_3","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},13U,coo::Wait::completed}},
    {"finale.support","*",{coo::Operation::population,{0x986985D0U,0x80F46DB0U,0,0},14U,coo::Wait::requested}},
    {"module.expose","*",{coo::Operation::mechanic,{0x4B946B28U,0x80F46F23U,4,32},20U,coo::Wait::requested}},
    {"objective.module","*",{coo::Operation::objective,{0x986985D0U,0x80F47420U,68,0},2164099943U,coo::Wait::requested}},
    {"dialogue.module","*",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},7U,coo::Wait::nativeReady}},
    {"module.destroyed","*",{coo::Operation::observation,{0x4B946B28U,0x80F46F23U,4,32},513U,coo::Wait::observed}},
    {"lighthouse.unlock","*",{coo::Operation::mechanic,{0x4B946B28U,0x80F46F23U,4,32},21U,coo::Wait::requested}},
    {"objective.enter","*",{coo::Operation::objective,{0x986985D0U,0x80F47420U,68,0},2970444885U,coo::Wait::requested}},
    {"dialogue.timelines","*",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},8U,coo::Wait::nativeReady}},
    {"lighthouse.entered","*",{coo::Operation::observation,{0xBA0B27A0U,0x80F46DCDU,60,11},0U,coo::Wait::observed}},
    {"dialogue.old_place","*",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},9U,coo::Wait::nativeReady}},
    {"objective.vance","*",{coo::Operation::objective,{0x986985D0U,0x80F47420U,68,0},1915741729U,coo::Wait::requested}},
    {"vance.approached","*",{coo::Operation::observation,{0xBA0B27A0U,0x80F46DCDU,60,13},0U,coo::Wait::observed}},
    {"dialogue.come_closer","*",{coo::Operation::dialogue,{0x986985D0U,0x80F47426U,53,2},10U,coo::Wait::nativeReady}},
    {"vance.scene","*",{coo::Operation::scene,{0xBA0B27A0U,0x80F46DE0U,43,5},1U,coo::Wait::nativeReady}},
    {"vance.ascent_cue","*",{coo::Operation::eventAfter,kVanceScene,kVanceAscentMs,coo::Wait::observed},300000},
    {"lighthouse.raise","*",{coo::Operation::device,{0xBA0B27A0U,0x80F46DD0U,23,0},1U,coo::Wait::requested}},
    {"lighthouse.light","*",{coo::Operation::device,{0xBA0B27A0U,0x80F46DD3U,23,1},1U,coo::Wait::requested}},
    {"vance.ending_cue","*",{coo::Operation::eventAfter,kVanceScene,kVanceFinishMs,coo::Wait::observed},300000},
    {"mission.finish","*",{coo::Operation::complete,{0x986985D0U,0x80F46DB0U,0,0},6U,coo::Wait::requested}},
    {"travel.reset","*",{coo::Operation::mechanic,kModule,30,coo::Wait::requested}},
    {"vance.greeting","*",{coo::Operation::mechanic,kVanceScene,1,coo::Wait::requested}},
    {"vance.turned","*",{coo::Operation::observation,kVanceScene,1,coo::Wait::observed}},
    {"dialogue.finished.9","*",{coo::Operation::observation,kDialogueAsset,9,coo::Wait::observed}},
    {"dialogue.finished.10","*",{coo::Operation::observation,kDialogueAsset,10,coo::Wait::observed}},
    {"population.cleared.1","*",{coo::Operation::observation,kModule,1,coo::Wait::observed}},
    {"population.cleared.3","*",{coo::Operation::observation,kModule,3,coo::Wait::observed}},
    {"population.cleared.4","*",{coo::Operation::observation,kModule,4,coo::Wait::observed}},
    {"population.cleared.5","*",{coo::Operation::observation,kModule,5,coo::Wait::observed}},
    {"population.contact.9","*",{coo::Operation::observation,kModule,265,coo::Wait::observed}},
    {"population.contact.8","*",{coo::Operation::observation,kModule,264,coo::Wait::observed}},

};
inline constexpr coo::script::ModuleCapability kModules[]{{"opening",{kModule,1}}};
inline constexpr coo::script::FactCapability kFacts[]{{"opening.checked",0}};
// Beacon references use existing entity providers. They do not create objects.
inline constexpr coo::script::MarkerCapability kMarkers[]{
    {"forest_gate",{{0x4B946B28U,0x80F46EC0U,60,448},{}}},
    {"lighthouse_portal",{{0x4B946B28U,0x80F46F11U,4,26},{}}},
    {"module",{{0x4B946B28U,0x80F46F23U,4,32},{}}},
    {"vance",{{0xBA0B27A0U,0x80F46DDDU,1,4},{}}}
};
inline constexpr coo::script::Profile kNativeBindings{"gateway.ending.v2","otherMissions",coo::Schema::otherMissions,
    kCapabilities,kModules,kFacts,kDialogue,kObjectives,{},{},kMarkers};
} // namespace dawn::state::activity::gateway
