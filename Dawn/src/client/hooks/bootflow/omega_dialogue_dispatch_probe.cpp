#include "garden_ending_native.h"
#include "../../../state/activity/vanilla/one_au/runtime.h"
#include "../../../state/activity/vanilla/homecoming/runtime.h"
#include "../../../state/activity/vanilla/adieu/runtime.h"
#include "../../../state/activity/gateway/runtime.h"
#include "../../../state/activity/beyond_infinity/runtime.h"
#include "../../../state/activity/deep_storage/runtime.h"
#include "../../../state/activity/hijacked/runtime.h"
#include "../../../state/activity/Newlight/launchpad/runtime.h"
#include "../../../state/activity/deadly_trial/runtime.h"
#include "../../../state/activity/strike_pact/runtime.h"
#include "../../../state/activity/strike_bond/runtime.h"
#include <Windows.h>
#include "deadly_trial_presentation.h"
#include "hijacked_presentation.h"
#include "../graphics/hijacked_frame_timing.h"
#include <intrin.h>

#include <array>
#include <bit>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

#include "../../../core/filesystem/path.h"
#include "forest_tuner_record.h"
#include "forest_tuner_state.h"
#include "native_authority_bitmap.h"
#include "omega_forest_recipe.h"
#include "beyond_infinity_forest_recipe.h"
#include "../../../state/activity/beyond_infinity/forest_selection.h"
#include "omega_forest_scope.h"
#include "omega_enemy_forest_receipts_runtime.h"
#include "omega_enemy_lair_receipts.h"
#include "omega_dialogue_bank.h"
#include "gate_trace_cache.h"
#include "adventure_cue_observer.h"
#include "adventure_dialogue_observer.h"
#include "omega_teardown_native.h"
#include "launchpad_retirement_native.h"
#include "../../../state/activity/Newlight/launchpad/runtime.h"
#include "../../../middleware/crypto/random_bytes.h"
#include "../../../state/activity/destination/activity_destination_snapshot.h"
#include "../../../state/activity/forced/activity_forced_destination.h"
#include "../../../core/logging/log.h"
#include "../../../state/activity/runtime.h"
#include "../../../state/activity/omega_presentation.h"
#include "../../../state/activity/omega_ending.h"
#include "../../hooking/call_gate.h"
#include "../../../state/build_data/runtime.h"
#include "../teleport/runtime.h"
#include "../../hooking/detour.h"
#include "internal.h"

namespace dawn::client::hooks::bootflow {
namespace {
std::array<std::atomic_uint64_t, 5U> g_towerWatchSlotFingerprints{};

/**
 * Observation-only probe over the type-53 dialogue component's authority consumer chain:
 * authority apply (+0x1009B60, memcpy of the decoded 0x1008 struct into component+0x180),
 * the record scan (+0x100A180, dispatch condition: generation changed AND time != 0 AND
 * mode == 2), and the row dispatch (+0x10097D0, bank row = record index). It answers, from
 * the client's own memory, whether the emitted Ghost body decoded to the expected record
 * and where the dispatch chain stops. No game state is written.
 */
constexpr std::uintptr_t kDialogueApplyRva = 0x1009B60U;
constexpr std::array<std::byte, 21> kDialogueApplyPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC},
    std::byte{0x30}, std::byte{0x44}, std::byte{0x8B}, std::byte{0x02}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0xD9}, std::byte{0x4C}, std::byte{0x8B}, std::byte{0x4A},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x8D}, std::byte{0x4C}, std::byte{0x24},
    std::byte{0x20}};
constexpr std::uintptr_t kDialogueScanRva = 0x100A180U;
constexpr std::array<std::byte, 21> kDialogueScanPrefix{
    std::byte{0x41}, std::byte{0x56}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC},
    std::byte{0x50}, std::byte{0x44}, std::byte{0x8B}, std::byte{0x09}, std::byte{0x4C},
    std::byte{0x8B}, std::byte{0xF1}, std::byte{0x48}, std::byte{0x8B}, std::byte{0x05},
    std::byte{0xDD}, std::byte{0xFA}, std::byte{0x42}, std::byte{0x01}, std::byte{0x41},
    std::byte{0x8B}};
constexpr std::uintptr_t kDialogueDispatchRva = 0x10097D0U;
constexpr std::array<std::byte, 21> kDialogueDispatchPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC},
    std::byte{0x30}, std::byte{0x44}, std::byte{0x8B}, std::byte{0x11}, std::byte{0x45},
    std::byte{0x8B}, std::byte{0xC2}, std::byte{0x48}, std::byte{0x8B}, std::byte{0x05},
    std::byte{0x8D}, std::byte{0x04}, std::byte{0x43}, std::byte{0x01}, std::byte{0x41},
    std::byte{0x81}};
/** Type-23 gate authority apply; committed channel state in the proven +0x1C0/0x18 window. */
constexpr std::uintptr_t kGateApplyRva = 0x10699C0U;
/** 8080390E device channel setters; identical prologue; authority-bit gate at entry. */
constexpr std::uintptr_t kDeviceChannel0SetterRva = 0xDF6BD0U;
constexpr std::uintptr_t kDeviceChannel1SetterRva = 0xDF7120U;
constexpr std::array<std::byte, 21> kDeviceSetterPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x08},
    std::byte{0x48}, std::byte{0x89}, std::byte{0x74}, std::byte{0x24}, std::byte{0x10},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x30},
    std::byte{0x0F}, std::byte{0xB7}, std::byte{0x41}, std::byte{0x2C}, std::byte{0x41},
    std::byte{0x8B}};
/** The .data object-authority bitmap the setters and the gate tick both consult. */
constexpr std::uintptr_t kObjectAuthorityTableRva = 0x26BE0E0U;
/**
 * Scene/component destructor that faults during the region teardown (assert PC +0x4E3C08 =
 * this+0xC8). It resolves the component from the handle at param[0] through the shared registry
 * (global +0x2439C70), then runs an EXTRA free only when the resolved object's +0x40 field is
 * NOT the absent-hash sentinel 0x811C9DC5. Observe-only: read that field before the original runs.
 */
constexpr std::uintptr_t kSceneDestructorRva = 0x4E3B40U;
constexpr std::array<std::byte, 16> kSceneDestructorPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x08},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x20},
    std::byte{0x44}, std::byte{0x8B}, std::byte{0x09}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9}};
/** Index-heap unregister FUN_7ff68ee77c00: searches a list for the resolved object's key and, when
 * absent, indexes list[-2] out of bounds (the crash). Guard it to skip when the entry is absent. */
constexpr std::uintptr_t kIndexFreeRva = 0x4D7C00U;
/** Native roster receiver and the same current-bubble accessors it invokes. */
constexpr std::uintptr_t kRosterApplyRva=0x3CCE50U;
constexpr std::array<std::byte,16> kRosterApplyPrefix{
    std::byte{0x40},std::byte{0x55},std::byte{0x56},std::byte{0x48},std::byte{0x83},std::byte{0xEC},
    std::byte{0x48},std::byte{0x48},std::byte{0x8B},std::byte{0xEA},std::byte{0x48},std::byte{0x8B},
    std::byte{0xF1},std::byte{0xE8},std::byte{0x5E},std::byte{0xC6}};
constexpr std::array<std::byte,8> kBubbleContextPrefix{
    std::byte{0x48},std::byte{0x8D},std::byte{0x05},std::byte{0x39},std::byte{0x48},std::byte{0xB6},std::byte{0x01},std::byte{0xC3}};
constexpr std::array<std::byte,17> kBubbleReadPrefix{
    std::byte{0x48},std::byte{0x89},std::byte{0x5C},std::byte{0x24},std::byte{0x10},std::byte{0x57},
    std::byte{0x48},std::byte{0x83},std::byte{0xEC},std::byte{0x20},std::byte{0x48},std::byte{0x8B},
    std::byte{0x59},std::byte{0x08},std::byte{0x48},std::byte{0x8B},std::byte{0xFA}};
/** Infinite Forest map generator (kind 0x80804EF6, instance 2763EC97/37/1): observe-only
 *  Gate-2 probes from FOREST-GENERATOR-RE. Authority record at instance+0x180, sense record
 *  at instance+0x734, both 0x5B4 bytes. */
constexpr std::uintptr_t kForestCreateRva = 0x103E450U;
constexpr std::uintptr_t kForestApplyRva = 0x103E8E0U;
constexpr std::uintptr_t kForestSenseRva = 0x103D820U;
constexpr std::size_t kForestAuthorityOffset = 0x180U;
constexpr std::size_t kForestSenseOffset = 0x734U;
constexpr std::size_t kForestRecordBytes = 0x5B4U;
constexpr std::array<std::byte, 16> kForestCreatePrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9}, std::byte{0x48}, std::byte{0x81}, std::byte{0xC1},
    std::byte{0x80}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 16> kForestApplyPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x30}, std::byte{0x44}, std::byte{0x8B},
    std::byte{0x02}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xD9},
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0x4A}, std::byte{0x08}};
constexpr std::array<std::byte, 16> kForestSensePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xFA}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xD9}};
constexpr std::array<std::byte, 24> kIndexFreePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x08},
    std::byte{0x48}, std::byte{0x89}, std::byte{0x6C}, std::byte{0x24}, std::byte{0x10},
    std::byte{0x48}, std::byte{0x89}, std::byte{0x74}, std::byte{0x24}, std::byte{0x18},
    std::byte{0x57}, std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x20}};

/** Map-generator WORKER (kind 0x80805017, def class 0x80804FED embedded in the 808099D6
 * map objects of pkgs 03A6/03A7, one per forest segment): the native evaluator that resolves
 * the first type-37 sensor in scope (+0x103E700), reads its authority record, and either
 * applies host-committed activation (seed echo @auth+0x550, 32+64 bools) or SELF-GENERATES
 * when the authority gate (+0x4E7F70 -> +0x4E8060 bubble masks) passes, writing results into
 * the sensor's sense record. Tick key 0x128CC0BD4; state machine byte at instance+0x9BC. */
constexpr std::uintptr_t kForestWorkerCreateRva = 0xFFE820U;
constexpr std::uintptr_t kForestWorkerTickRva = 0x10059A0U;
constexpr std::uintptr_t kForestOwnerAuthoritySetterRva = 0x403BD0U;
constexpr std::array<std::byte, 16> kForestOwnerAuthoritySetterPrefix{
    std::byte{0x81}, std::byte{0xE1}, std::byte{0xFF}, std::byte{0x1F},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x4C}, std::byte{0x8D},
    std::byte{0x05}, std::byte{0x03}, std::byte{0xA5}, std::byte{0x2B},
    std::byte{0x02}, std::byte{0x8B}, std::byte{0xC1}, std::byte{0x83}};
constexpr std::size_t kForestWorkerStateOffset = 0x9BCU;
constexpr std::array<std::byte, 16> kForestWorkerCreatePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x20}, std::byte{0x57}};
constexpr std::array<std::byte, 16> kForestWorkerTickPrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x49},
    std::byte{0x89}, std::byte{0x5B}, std::byte{0x10}, std::byte{0x49},
    std::byte{0x89}, std::byte{0x73}, std::byte{0x18}, std::byte{0x49},
    std::byte{0x89}, std::byte{0x7B}, std::byte{0x20}, std::byte{0x55}};
// NOTE: the map factory (+0x56D9B0) is already detoured by omega_ikora_origin_probe, which
// installs first; a second competing detour here failed its prefix verification against the
// JMP-patched site and aborted this probe's whole install (dropping the index-heap guard â€” the
// 2026-08-27 tunnel freeze). Forest construction logging lives in that probe's hook instead.

/** Device CONFIGURE handler: resolves the property-name handle in the config record and
 * registers the +0x70 property binding; a zero handle leaves the device store-only. */
constexpr std::uintptr_t kDeviceConfigureRva = 0xDF5070U;
constexpr std::array<std::byte, 21> kDeviceConfigurePrefix{
    std::byte{0x40}, std::byte{0x57}, std::byte{0x41}, std::byte{0x56}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x38}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xF9}, std::byte{0xC7}, std::byte{0x41}, std::byte{0x70}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0x8B}, std::byte{0x0A},
    std::byte{0x45}};
/** Type-68 directive authority apply (records at +0x190, stride 0xF8, ring at +0x478). */
constexpr std::uintptr_t kDirectiveApplyRva = 0x1009C00U;
constexpr std::array<std::byte, 21> kDirectiveApplyPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x57}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x40}, std::byte{0x44},
    std::byte{0x8B}, std::byte{0x02}, std::byte{0x4C}, std::byte{0x8B}, std::byte{0xF9},
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0x4A}, std::byte{0x08}, std::byte{0x48},
    std::byte{0x8D}};

/** Decoded 0x80804F77 struct copied to component+0x180; records at +8, stride 0x20. */
constexpr std::size_t kAuthorityBaseOffset = 0x180U;
constexpr std::size_t kRecordBaseOffset = 0x188U;
constexpr std::size_t kRecordTimeOffset = 0x190U;
constexpr std::size_t kRecordRefOffset = 0x198U;
constexpr std::size_t kRecordGenerationOffset = 0x1A0U;
constexpr std::size_t kRecordModeOffset = 0x1A4U;
constexpr std::size_t kProcessedBaseOffset = 0x1188U;
/** Global handle-table registry pointer the scan's bank resolve reads (DAT_7ff690dd9c70). */
constexpr std::uintptr_t kHandleRegistryPointerRva = 0x2439C70U;

using DialogueApply = void(__fastcall*)(std::byte* component,
                                        const std::byte* packet) noexcept;
using DialogueScan = void(__fastcall*)(std::byte* component) noexcept;
using DialogueDispatch = void(__fastcall*)(std::byte* component,
                                           std::int32_t index) noexcept;

hooking::detour::Handle g_applyHandle{};
hooking::detour::Handle g_scanHandle{};
hooking::detour::Handle g_dispatchHandle{};
std::atomic<DialogueApply> g_applyOriginal{nullptr};
std::atomic<DialogueScan> g_scanOriginal{nullptr};
std::atomic<DialogueDispatch> g_dispatchOriginal{nullptr};
std::atomic_uint32_t g_applyCount{};
std::atomic_uint64_t g_scanCount{};
std::atomic_uint64_t g_lastScanState{UINT64_MAX};
/**
 * Delivery diagnostics (lane I, 2026-09-06). Run 27456 lost rows 15/16 because the type-53/68
 * authority stopped arriving at all after Crown arrival: the encoder's Crown branch dropped the
 * whole 82FB58B7 group from the type-5 push. The 09-05 vista row 7 was lost the other way round
 * (body present, client never applied). Both classes look identical in the old log (the apply
 * hook logs every 16th call only), so the scan tick now reports an authority-silence episode as
 * soon as no apply has reached the live component for kApplySilenceMs, and every apply audits
 * the rows it carried. All reject lines are capped and de-duplicated per (reason,row,generation).
 */
constexpr std::uint64_t kApplySilenceMs = 10000ULL;
constexpr std::uint32_t kSilenceLogCap = 8U;
constexpr std::uint32_t kRejectLogCap = 64U;
constexpr std::uint32_t kDialogueBankHandle = 0x80F1FD07U;
constexpr std::size_t kDialogueRowCount = 34U;
constexpr std::size_t kRecordStride = 0x20U;
std::atomic_uint64_t g_lastApplyTick{0};
std::atomic_bool g_applySilent{false};
std::atomic_uint32_t g_silenceLogs{0};
std::atomic_uint32_t g_rejectLogs{0};
SRWLOCK g_rejectLock = SRWLOCK_INIT;
struct RejectMemo final {
    std::uint64_t identity{};
    std::uint32_t sightings{};
};
std::array<RejectMemo, 16> g_rejectMemo{};
std::uint32_t g_rejectMemoNext{0};
hooking::detour::Handle g_directiveHandle{};
std::atomic<DialogueApply> g_directiveOriginal{nullptr};
std::atomic_uint32_t g_directiveCount{};
std::atomic_uint64_t g_lastDirectiveState{UINT64_MAX};
hooking::detour::Handle g_gateHandle{};
std::atomic<DialogueApply> g_gateOriginal{nullptr};
std::atomic_uint32_t g_gateCount{};
GateTraceCache<> g_gateTrace;
// Native DF6BD0/DF7120 receive channel values in XMM1 (float ABI).
using DeviceSetter = void(__fastcall*)(std::byte* device, float value, char snap,
                                       std::uint32_t revision) noexcept;
hooking::detour::Handle g_deviceCh0Handle{};
hooking::detour::Handle g_deviceCh1Handle{};
std::atomic<DeviceSetter> g_deviceCh0Original{nullptr};
std::atomic<DeviceSetter> g_deviceCh1Original{nullptr};
std::atomic_uint32_t g_deviceLogCount{};
// The native configure handler RETURNS a value (undefined8); dropping it corrupted the
// caller's view of configure success and broke device presentation (the missing portal).
using DeviceConfigure = std::uint64_t(__fastcall*)(std::byte* device,
                                                   const std::byte* config) noexcept;
hooking::detour::Handle g_deviceConfigureHandle{};
std::atomic<DeviceConfigure> g_deviceConfigureOriginal{nullptr};
std::atomic_uint32_t g_deviceConfigureCount{};
/** The barrier wall's own device, captured when its configure registers "vex_wall". */
std::atomic<std::byte*> g_vexWallDevice{nullptr};
std::atomic_uint32_t g_vexWallPushState{};
using SceneDestructor = void(__fastcall*)(std::uint32_t* component) noexcept;
hooking::detour::Handle g_destructorHandle{};
std::atomic<SceneDestructor> g_destructorOriginal{nullptr};
std::atomic_uint32_t g_destructorCount{};
std::atomic_uint32_t g_destructorSkips{};
using IndexFree = void(__fastcall*)(int* list, std::uint32_t index) noexcept;
hooking::detour::Handle g_indexFreeHandle{};
std::atomic<IndexFree> g_indexFreeOriginal{nullptr};
std::atomic_uint32_t g_indexFreeSkips{};
using RosterApply=void(__fastcall*)(void*,const void*) noexcept;
using BubbleContext=void*(__fastcall*)() noexcept;
using BubbleRead=std::uintptr_t(__fastcall*)(void*,std::uint32_t*) noexcept;
hooking::detour::Handle g_rosterHandle{};
hooking::CallGate g_rosterGate{};
std::atomic<RosterApply> g_rosterOriginal{};
std::atomic<BubbleContext> g_bubbleContext{};
std::atomic<BubbleRead> g_bubbleRead{};
std::atomic_uint32_t g_retirementRejects{};
using ForestCreateFn = std::uint64_t(__fastcall*)(void*) noexcept;
using ForestPairFn = std::uint64_t(__fastcall*)(void*, void*) noexcept;
hooking::detour::Handle g_forestCreateHandle{};
hooking::detour::Handle g_forestApplyHandle{};
hooking::detour::Handle g_forestSenseHandle{};
std::atomic<ForestCreateFn> g_forestCreateOriginal{nullptr};
std::atomic<ForestPairFn> g_forestApplyOriginal{nullptr};
std::atomic<ForestPairFn> g_forestSenseOriginal{nullptr};
std::atomic_uint32_t g_forestCreateCount{};
std::atomic_uint32_t g_forestApplyCount{};
std::atomic_uint32_t g_forestSenseCount{};
std::atomic_uint64_t g_forestAuthorityHash{};
std::atomic_uint64_t g_forestSenseHash{};
std::atomic_uint32_t g_forestDumpBudget{12};
/** Per-slot object-block record processor (+0x4D7470): decodes one slot's {auth, sense}
 * sections and clears the pool object's pending byte (+0x18) when both process cleanly. A
 * zero return aborts the REMAINING records of the same message, so one bad record can starve
 * every later object of seeding. Logged when it fails. */
constexpr std::uintptr_t kRecordProcessorRva = 0x4D7470U;
constexpr std::array<std::byte, 16> kRecordProcessorPrefix{
    std::byte{0x40}, std::byte{0x57}, std::byte{0x41}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57},
    std::byte{0xB8}, std::byte{0x98}, std::byte{0x78}, std::byte{0x00},
    std::byte{0x00}, std::byte{0xE8}, std::byte{0xAE}, std::byte{0x56}};
using RecordProcessorFn = std::uint64_t(__fastcall*)(void*, void*) noexcept;
/** AllRecordsInBubbleSeeded (+0x4D6530, on container+0x28): the commit tick's veto. In the
 * committed Omega context, our own empty forest-group records are unseeded, so when the
 * pending set is exclusively forest bubbles (8..14), force the verdict TRUE: the sweep that
 * follows (Bubble_InstantiateReplicatedContent) is exactly the native path that constructs
 * the replicated map-generator worker. Lighthouse (15) and every non-Omega state keep the
 * native verdict. */
constexpr std::uintptr_t kSeedCheckRva = 0x4D6530U;
constexpr std::array<std::byte, 16> kSeedCheckPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x48}, std::byte{0x89}, std::byte{0x7C},
    std::byte{0x24}, std::byte{0x20}, std::byte{0x55}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0xEC}, std::byte{0x48}, std::byte{0x83}};
constexpr std::size_t kSeedCheckPoolOffset = 0x28U;
constexpr std::size_t kMaskBOffset = 0x10EC4U;
using SeedCheckFn = std::uint64_t(__fastcall*)(void*) noexcept;
hooking::detour::Handle g_seedCheckHandle{};
std::atomic<SeedCheckFn> g_seedCheckOriginal{nullptr};
std::atomic_uint32_t g_seedForceCount{};
hooking::detour::Handle g_recordProcessorHandle{};
std::atomic<RecordProcessorFn> g_recordProcessorOriginal{nullptr};
std::atomic_uint32_t g_recordProcessorCalls{};
std::atomic_uint32_t g_recordProcessorFails{};

/** The generator sensor instance, stashed at create so the enrollment sampler can watch it. */
std::atomic<void*> g_forestSensorPtr{nullptr};
[[nodiscard]] bool readable(const std::byte* ptr, std::size_t size) noexcept;
hooking::detour::Handle g_forestWorkerCreateHandle{};
hooking::detour::Handle g_forestWorkerTickHandle{};
using ForestOwnerAuthoritySetter = void(__fastcall*)(std::uint32_t, std::uint8_t) noexcept;
std::atomic<ForestOwnerAuthoritySetter> g_forestOwnerAuthoritySetter{nullptr};
std::atomic<ForestPairFn> g_forestWorkerCreateOriginal{nullptr};
std::atomic<ForestPairFn> g_forestWorkerTickOriginal{nullptr};
std::atomic_uint32_t g_forestWorkerCreateCount{};
std::atomic_uint32_t g_forestWorkerTickCount{};
std::atomic_uint32_t g_forestWorkerLastState{0xFFFFFFFFU};

template <typename Value>
[[nodiscard]] Value read_value(const std::byte* source) noexcept {
    Value value{};
    if (source != nullptr) {
        std::memcpy(&value, source, sizeof value);
    }
    return value;
}

[[nodiscard]] std::byte* verified_target(std::uintptr_t rva,
                                         const std::byte* prefix,
                                         std::size_t prefixSize) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return nullptr;
    }
    std::byte* const candidate = image + rva;
    for (std::size_t index = 0; index < prefixSize; ++index) {
        if (candidate[index] != prefix[index]) {
            return nullptr;
        }
    }
    return candidate;
}

/** Guard each read against a component or table being released during a streaming transition. */
bool read_dialogue_memory(void*, std::uintptr_t address, std::span<std::byte> output) noexcept {
    SIZE_T copied{};
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address),
                             output.data(), output.size(), &copied) != FALSE
           && copied == output.size();
}

/** Native resolution dereferences both the global slot and its directory object. */
[[nodiscard]] std::uint32_t resolve_bank_handle(const std::byte* component,
                                                std::uint32_t& selfHandle,
                                                std::int64_t& refOffset) noexcept {
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const content::handles::Source source{
        image == 0 ? 0 : image+kHandleRegistryPointerRva, nullptr, read_dialogue_memory};
    const auto binding = omega_dialogue_bank::resolve(
        source, reinterpret_cast<std::uintptr_t>(component));
    selfHandle = binding.definition;
    refOffset = binding.offset;
    return binding.bank;
}

void log_record_zero(const char* stage,
                     const std::byte* component,
                     std::uint64_t extra) noexcept {
    const std::uint64_t root = read_value<std::uint64_t>(component + kAuthorityBaseOffset);
    const std::uint64_t first = read_value<std::uint64_t>(component + kRecordBaseOffset);
    const std::uint64_t time = read_value<std::uint64_t>(component + kRecordTimeOffset);
    const std::uint64_t ref = read_value<std::uint64_t>(component + kRecordRefOffset);
    const std::int32_t generation =
        read_value<std::int32_t>(component + kRecordGenerationOffset);
    const std::uint8_t mode = read_value<std::uint8_t>(component + kRecordModeOffset);
    const std::int32_t processed =
        read_value<std::int32_t>(component + kProcessedBaseOffset);
    std::uint32_t selfHandle = 0;
    std::int64_t refOffset = 0;
    const std::uint32_t bank = resolve_bank_handle(component, selfHandle, refOffset);
    std::array<char, 512> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_dialogue stage=%s component=%p rec0_root=0x%llX rec0_a=0x%llX "
        "rec0_time=0x%llX rec0_ref=0x%llX rec0_gen=%d rec0_mode=%u processed0=%d "
        "self=0x%08X off8=0x%llX bank=0x%08X n=%llu mutation=observe_only",
        stage,
        static_cast<const void*>(component),
        static_cast<unsigned long long>(root),
        static_cast<unsigned long long>(first),
        static_cast<unsigned long long>(time),
        static_cast<unsigned long long>(ref),
        generation,
        static_cast<unsigned>(mode),
        processed,
        selfHandle,
        static_cast<unsigned long long>(refOffset),
        bank,
        static_cast<unsigned long long>(extra));
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

[[nodiscard]] constexpr std::uint64_t fnv1a64(const char* text) noexcept {
    std::uint64_t hash = 0xCBF29CE484222325ULL;
    for (; text != nullptr && *text != '\0'; ++text) {
        hash ^= static_cast<std::uint8_t>(*text);
        hash *= 0x100000001B3ULL;
    }
    return hash;
}

/**
 * Counts one sighting of (reason,row,generation) and returns how many times it has now been seen,
 * or 0 when the memo ring is exhausted for this identity. The ring is tiny by design: the
 * diagnostics are for the handful of rows a run offers, not a stream.
 */
[[nodiscard]] std::uint32_t note_reject_sighting(std::uint64_t identity) noexcept {
    std::uint32_t sightings = 0;
    AcquireSRWLockExclusive(&g_rejectLock);
    bool found = false;
    for (auto& memo : g_rejectMemo) {
        if (memo.sightings != 0 && memo.identity == identity) {
            if (memo.sightings < UINT32_MAX) { ++memo.sightings; }
            sightings = memo.sightings;
            found = true;
            break;
        }
    }
    if (!found) {
        auto& memo = g_rejectMemo[g_rejectMemoNext % g_rejectMemo.size()];
        ++g_rejectMemoNext;
        memo = {identity, 1U};
        sightings = 1U;
    }
    ReleaseSRWLockExclusive(&g_rejectLock);
    return sightings;
}

/**
 * Bounded `ev=omega_dialogue stage=reject reason=...` line. `minimumSightings` lets a transient
 * state (a just-dispatched row whose receipt has not retired it yet) pass once before it counts.
 */
void log_reject(const char* reason,
                const std::byte* component,
                std::uint32_t row,
                std::uint32_t generation,
                std::uint64_t detail,
                std::uint32_t minimumSightings = 1U) noexcept {
    const std::uint64_t identity =
        fnv1a64(reason) ^ (std::uint64_t{row} << 40U) ^ std::uint64_t{generation};
    if (note_reject_sighting(identity) != minimumSightings) {
        return;
    }
    if (g_rejectLogs.fetch_add(1, std::memory_order_relaxed) >= kRejectLogCap) {
        return;
    }
    std::array<char, 256> line{};
    const int length = std::snprintf(
        line.data(), line.size(),
        "ev=omega_dialogue stage=reject reason=%s component=%p row=%u generation=%u "
        "detail=0x%llX applies=%u mutation=observe_only",
        reason, static_cast<const void*>(component), row, generation,
        static_cast<unsigned long long>(detail), g_applyCount.load(std::memory_order_relaxed));
    if (length > 0 && static_cast<std::size_t>(length) < line.size()) {
        core::log::write(core::log::Channel::client, core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/**
 * Audits every row the native authority buffer now holds. A row with a nonzero time is the
 * server's active offer; it can only dispatch when its mode is 2, the bank is the Omega bank and
 * the native processed generation is still older than the offered one.
 */
void audit_applied_rows(const std::byte* component) noexcept {
    std::uint32_t self = 0;
    std::int64_t refOffset = 0;
    const std::uint32_t bank = resolve_bank_handle(component, self, refOffset);
    for (std::size_t row = 0; row < kDialogueRowCount; ++row) {
        const std::size_t offset = row * kRecordStride;
        const auto generation =
            read_value<std::uint32_t>(component + kRecordGenerationOffset + offset);
        const auto time = read_value<std::uint64_t>(component + kRecordTimeOffset + offset);
        if (generation == 0 || time == 0) {
            continue;
        }
        const auto rowIndex = static_cast<std::uint32_t>(row);
        const auto mode = read_value<std::uint8_t>(component + kRecordModeOffset + offset);
        if (mode != 2U) {
            log_reject("record_mode", component, rowIndex, generation, mode);
            continue;
        }
        if (bank != kDialogueBankHandle) {
            log_reject("bank", component, rowIndex, generation, bank);
            continue;
        }
        const auto processed =
            read_value<std::uint32_t>(component + kProcessedBaseOffset + row * 4U);
        // Native +100A180 dispatches only when the generation CHANGED; a generation the
        // component already consumed is never retried. Seen twice = not the receipt window.
        if (processed >= generation) {
            log_reject("already_processed", component, rowIndex, generation, processed, 2U);
        }
    }
}

/** Logs a hook's return address as an image RVA so the invoking router can be decompiled. */
void log_caller_rva(const char* stage, const void* caller, const void* payload) noexcept {
    const auto* const image = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    const std::uint64_t rva =
        image != nullptr && caller != nullptr
            ? static_cast<std::uint64_t>(static_cast<const std::byte*>(caller) - image)
            : 0U;
    std::array<char, 160> line{};
    const int written = std::snprintf(line.data(), line.size(),
                                      "ev=omega_router stage=%s caller=+0x%llX payload=%p", stage,
                                      static_cast<unsigned long long>(rva), payload);
    if (written > 0) {
        core::log::write(core::log::Channel::client, core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

__declspec(noinline) void __fastcall dialogue_apply(std::byte* component,
                                                    const std::byte* packet) noexcept {
    static std::atomic_uint32_t s_callerLogs{0};
    if (s_callerLogs.fetch_add(1, std::memory_order_relaxed) < 3U) {
        log_caller_rva("dialogue_apply", _ReturnAddress(), packet);
    }
    const DialogueApply original = g_applyOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(component, packet);
    }
    const std::uint32_t count = g_applyCount.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (component != nullptr && (count <= 12U || (count & 15U) == 0U)) {
        log_record_zero("apply", component, count);
    }
    {
        const std::uint64_t now = GetTickCount64();
        const std::uint64_t previous = g_lastApplyTick.exchange(now, std::memory_order_acq_rel);
        if (g_applySilent.exchange(false, std::memory_order_acq_rel)) {
            std::array<char, 192> line{};
            const int length = std::snprintf(
                line.data(), line.size(),
                "ev=omega_dialogue stage=resume component=%p gap_ms=%llu n=%u mutation=observe_only",
                static_cast<void*>(component),
                static_cast<unsigned long long>(previous == 0 ? 0 : now - previous), count);
            if (length > 0 && static_cast<std::size_t>(length) < line.size()) {
                core::log::write(core::log::Channel::client, core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    if (component != nullptr) {
        audit_applied_rows(component);
        // Record which active row actually reached the native authority buffer. A server offer
        // alone does not prove delivery while bubbles are changing.
        static std::atomic_uint64_t lastDelivery{~0ULL};
        for (std::size_t row = 0; row < 34; ++row) {
            const auto offset = row*0x20U;
            const auto generation = read_value<std::uint32_t>(component+kRecordGenerationOffset+offset);
            if (generation == 0 || read_value<std::uint64_t>(component+kRecordTimeOffset+offset) == 0
                || read_value<std::uint8_t>(component+kRecordModeOffset+offset) != 2) { continue; }
            std::uint32_t self{};
            std::int64_t refOffset{};
            if (resolve_bank_handle(component,self,refOffset) != 0x80F1FD07U) { break; }
            const auto identity = (std::uint64_t{generation} << 8U) | row;
            if (lastDelivery.exchange(identity,std::memory_order_relaxed) == identity) { break; }
            std::array<char,160> line{};
            const int size = std::snprintf(line.data(),line.size(),
                "ev=omega_dialogue stage=delivered component=%p row=%u generation=%u",
                static_cast<void*>(component),static_cast<unsigned>(row),generation);
            if (size > 0) { core::log::write(core::log::Channel::client,core::log::Level::info,
                                           {line.data(),static_cast<std::size_t>(size)}); }
            break;
        }
    }
}

/** Slot descriptor for the sync-pool lookups: {registry key, slot type, slot index}. */
struct PoolSlotDescriptor {
    std::uint32_t key;
    std::uint8_t type;
    std::uint8_t pad;
    std::int16_t index;
};
static_assert(sizeof(PoolSlotDescriptor) == 8);
/** The Ghost dialogue sync-pool object: mission registry 82FB58B7, slot type 53, index 2. */
constexpr PoolSlotDescriptor kDialoguePoolSlot{0x82FB58B7U, 53U, 0U, 2};
/** Pool context fetch (+0x9FD040) and slot lookup (+0xA03560): the exact pair the object-block
 * processor (+0x4D7470) uses; the resolved pool-object handle lands at context+0x18. */
constexpr std::uintptr_t kPoolContextRva = 0x9FD040U;
constexpr std::uintptr_t kPoolLookupRva = 0xA03560U;
constexpr std::uintptr_t kObjectComponentLookupRva = 0x9EB6D0U;
constexpr std::uintptr_t kObjectRuntimeResolveRva = 0x9FEC30U;
constexpr std::uint32_t kTowerWatchRegistry = 0x9D8076E4U;
constexpr std::uint32_t kTowerWatchOpeningCue = 0x4FCECAB6U;
constexpr std::uint32_t kTowerWatchBreachCue = 0x432D2C95U;
constexpr std::uint32_t kTowerWatchPathCue = 0x432D2C96U;
constexpr std::uint32_t kSpawnerAuthoritySchema = 0x80807EC9U;
constexpr std::uint32_t kSceneAuthoritySchema = 0x8080626BU;
constexpr std::size_t kSpawnerRuntimeBytes = 0xC4U;
constexpr std::size_t kSceneRuntimeBytes = 0xD4U;
constexpr std::array<std::byte, 12> kPoolContextPrefix{
    std::byte{0x33}, std::byte{0xC0}, std::byte{0xC7}, std::byte{0x41},
    std::byte{0x18}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0x48}, std::byte{0x89}, std::byte{0x01}};
constexpr std::array<std::byte, 12> kPoolLookupPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x30}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xC2}, std::byte{0x4C}, std::byte{0x8D}, std::byte{0x44}};
using PoolContextFn = void(__fastcall*)(void*) noexcept;
using PoolLookupFn = std::uint8_t(__fastcall*)(void*, const PoolSlotDescriptor*) noexcept;
using ObjectComponentLookup = std::uint64_t*(__fastcall*)(std::uint64_t*,
                                                          std::int32_t) noexcept;
using ObjectRuntimeResolve = void*(__fastcall*)(void*, std::uint32_t) noexcept;

[[nodiscard]] const std::byte* resolve_object_datum(std::uint32_t handle,
                                                    std::uint8_t& stage) noexcept;

struct TowerWatchSlotDefinition final {
    const char* label{};
    PoolSlotDescriptor descriptor{};
    std::size_t runtimeBytes{};
};

constexpr std::array<TowerWatchSlotDefinition, 5U> kTowerWatchSlots{{
    {"scene_43_5", {kTowerWatchRegistry, 43U, 0U, 5}, kSceneRuntimeBytes},
    {"spawner_1_1", {kTowerWatchRegistry, 1U, 0U, 1}, kSpawnerRuntimeBytes},
    {"spawner_1_2", {kTowerWatchRegistry, 1U, 0U, 2}, kSpawnerRuntimeBytes},
    {"spawner_1_3", {kTowerWatchRegistry, 1U, 0U, 3}, kSpawnerRuntimeBytes},
    {"spawner_1_4", {kTowerWatchRegistry, 1U, 0U, 4}, kSpawnerRuntimeBytes},
}};

struct TowerWatchSlotSnapshot final {
    bool found{};
    std::uint8_t resolveStage{};
    std::uint32_t handle{0xFFFFFFFFU};
    const std::byte* object{};
    std::uint32_t schema{};
    std::uint32_t objectDefinition{};
    std::uint32_t bubble{0xFFFFFFFFU};
    std::uint8_t pending{0xFFU};
    std::uint8_t applied{0xFFU};
    const std::byte* authEntry{};
    const std::byte* senseEntry{};
    std::uint64_t authEntryHash{};
    std::uint64_t senseEntryHash{};
    std::uint32_t authGate98{};
    std::uint32_t authGateA0{};
    std::uint32_t senseGate98{};
    std::uint32_t senseGateA0{};
    std::uint32_t component{0xFFFFFFFFU};
    const std::byte* runtime{};
    bool runtimeReadable{};
    std::uint64_t runtimeHash{};
    std::array<std::byte, kSceneRuntimeBytes> runtimeState{};
    std::uint64_t fingerprint{};
};

using TowerWatchSnapshotSet = std::array<TowerWatchSlotSnapshot, kTowerWatchSlots.size()>;

[[nodiscard]] bool tower_watch_forced() noexcept {
    if (!state::activity::forced::override_active()) {
        return false;
    }
    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::size_t length = (std::min)(
        static_cast<std::size_t>(forced.packageNameLength), forced.packageName.size());
    return std::string_view(forced.packageName.data(), length) == "mission_towerfall";
}

[[nodiscard]] std::uint64_t hash_readable_bytes(const std::byte* source,
                                                std::size_t size) noexcept {
    if (!readable(source, size)) {
        return 0U;
    }
    std::uint64_t hash = 1469598103934665603ULL;
    for (std::size_t index = 0U; index < size; ++index) {
        hash ^= std::to_integer<std::uint8_t>(source[index]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

[[nodiscard]] TowerWatchSlotSnapshot capture_tower_watch_slot(
    const TowerWatchSlotDefinition& definition) noexcept {
    TowerWatchSlotSnapshot snapshot{};
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr
        || std::memcmp(image + kPoolContextRva,
                       kPoolContextPrefix.data(),
                       kPoolContextPrefix.size()) != 0
        || std::memcmp(image + kPoolLookupRva,
                       kPoolLookupPrefix.data(),
                       kPoolLookupPrefix.size()) != 0) {
        return snapshot;
    }

    std::array<std::byte, 0x40U> context{};
    __try {
        reinterpret_cast<PoolContextFn>(image + kPoolContextRva)(context.data());
        snapshot.found = reinterpret_cast<PoolLookupFn>(image + kPoolLookupRva)(
                             context.data(), &definition.descriptor)
                         != 0U;
        snapshot.handle = read_value<std::uint32_t>(context.data() + 0x18U);
        snapshot.authEntry = read_value<const std::byte*>(context.data());
        snapshot.senseEntry = read_value<const std::byte*>(context.data() + 0x08U);
        snapshot.object = snapshot.found
                              ? resolve_object_datum(snapshot.handle, snapshot.resolveStage)
                              : nullptr;
        if (snapshot.object != nullptr && readable(snapshot.object, 0x70U)) {
            snapshot.resolveStage = 8U;
            snapshot.schema = read_value<std::uint32_t>(snapshot.object + 0x0CU);
            snapshot.objectDefinition = read_value<std::uint32_t>(snapshot.object + 0x40U);
            snapshot.pending = read_value<std::uint8_t>(snapshot.object + 0x18U);
            snapshot.applied = read_value<std::uint8_t>(snapshot.object + 0x6EU);
            snapshot.bubble = read_value<std::uint32_t>(snapshot.object + 0x68U);

            std::uint64_t componentValue = 0U;
            const auto componentLookup = reinterpret_cast<ObjectComponentLookup>(
                image + kObjectComponentLookupRva);
            const auto runtimeResolve = reinterpret_cast<ObjectRuntimeResolve>(
                image + kObjectRuntimeResolveRva);
            (void)componentLookup(&componentValue,
                                  static_cast<std::int32_t>(snapshot.schema));
            snapshot.component = static_cast<std::uint32_t>(componentValue);
            snapshot.runtime = static_cast<const std::byte*>(runtimeResolve(
                const_cast<std::byte*>(snapshot.object), snapshot.component));
            snapshot.runtimeReadable = readable(snapshot.runtime, definition.runtimeBytes);
            if (snapshot.runtimeReadable) {
                std::memcpy(snapshot.runtimeState.data(),
                            snapshot.runtime,
                            definition.runtimeBytes);
                snapshot.runtimeHash = hash_readable_bytes(snapshot.runtime,
                                                           definition.runtimeBytes);
            }
        }
        if (readable(snapshot.authEntry, 0xB0U)) {
            snapshot.authEntryHash = hash_readable_bytes(snapshot.authEntry, 0xB0U);
            snapshot.authGate98 = read_value<std::uint32_t>(snapshot.authEntry + 0x98U);
            snapshot.authGateA0 = read_value<std::uint32_t>(snapshot.authEntry + 0xA0U);
        }
        if (readable(snapshot.senseEntry, 0xB0U)) {
            snapshot.senseEntryHash = hash_readable_bytes(snapshot.senseEntry, 0xB0U);
            snapshot.senseGate98 = read_value<std::uint32_t>(snapshot.senseEntry + 0x98U);
            snapshot.senseGateA0 = read_value<std::uint32_t>(snapshot.senseEntry + 0xA0U);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        snapshot.runtimeReadable = false;
    }

    snapshot.fingerprint = snapshot.runtimeHash ^ snapshot.authEntryHash
                           ^ (snapshot.senseEntryHash << 1U)
                           ^ (static_cast<std::uint64_t>(snapshot.handle) << 32U)
                           ^ snapshot.schema
                           ^ (static_cast<std::uint64_t>(snapshot.pending) << 8U)
                           ^ (static_cast<std::uint64_t>(snapshot.applied) << 16U);
    return snapshot;
}

[[nodiscard]] TowerWatchSnapshotSet capture_tower_watch_slots() noexcept {
    TowerWatchSnapshotSet snapshots{};
    for (std::size_t index = 0U; index < snapshots.size(); ++index) {
        snapshots[index] = capture_tower_watch_slot(kTowerWatchSlots[index]);
    }
    return snapshots;
}

template <typename Value>
[[nodiscard]] Value tower_watch_runtime_field(const TowerWatchSlotSnapshot& snapshot,
                                               std::size_t offset) noexcept {
    Value value{};
    if (snapshot.runtimeReadable
        && offset + sizeof value <= snapshot.runtimeState.size()) {
        std::memcpy(&value, snapshot.runtimeState.data() + offset, sizeof value);
    }
    return value;
}

void report_tower_watch_slots(const char* phase,
                              std::uint32_t cueBefore,
                              std::uint32_t cueAfter,
                              const TowerWatchSnapshotSet& snapshots,
                              bool changesOnly) noexcept {
    for (std::size_t index = 0U; index < snapshots.size(); ++index) {
        const TowerWatchSlotSnapshot& snapshot = snapshots[index];
        if (changesOnly
            && g_towerWatchSlotFingerprints[index].exchange(
                   snapshot.fingerprint, std::memory_order_acq_rel)
                   == snapshot.fingerprint) {
            continue;
        }
        const bool spawner = kTowerWatchSlots[index].descriptor.type == 1U;
        std::array<char, core::log::kLineCapacity> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=tower_watch_local_probe phase=%s cue_before=0x%08X cue_after=0x%08X "
            "slot=%s key=0x%08X type=%u index=%d found=%u handle=0x%08X resolve=%u "
            "object=%p schema=0x%08X schema_expected=0x%08X object_definition=0x%08X "
            "bubble=%u pending=%u applied=%u component=0x%08X runtime=%p readable=%u "
            "runtime_hash=0x%016llX auth_entry=%p auth_hash=0x%016llX auth_gates=%08X/%08X "
            "sense_entry=%p sense_hash=0x%016llX sense_gates=%08X/%08X "
            "requested=%u counts=%u,%u,%u,%u,%u,%u generation=%u "
            "target=%08X/%u/%u squad=%08X/%u/%u target_gen=%u active=%u mode=%u name=%08X "
            "r0=%08X r4=%08X r8=%08X rC=%08X r4C=%08X r90=%08X r94=%08X r98=%08X r9C=%08X "
            "mutation=observe_only",
            phase,
            cueBefore,
            cueAfter,
            kTowerWatchSlots[index].label,
            kTowerWatchSlots[index].descriptor.key,
            static_cast<unsigned>(kTowerWatchSlots[index].descriptor.type),
            static_cast<int>(kTowerWatchSlots[index].descriptor.index),
            snapshot.found ? 1U : 0U,
            snapshot.handle,
            static_cast<unsigned>(snapshot.resolveStage),
            static_cast<const void*>(snapshot.object),
            snapshot.schema,
            spawner ? kSpawnerAuthoritySchema : kSceneAuthoritySchema,
            snapshot.objectDefinition,
            snapshot.bubble,
            static_cast<unsigned>(snapshot.pending),
            static_cast<unsigned>(snapshot.applied),
            snapshot.component,
            static_cast<const void*>(snapshot.runtime),
            snapshot.runtimeReadable ? 1U : 0U,
            static_cast<unsigned long long>(snapshot.runtimeHash),
            static_cast<const void*>(snapshot.authEntry),
            static_cast<unsigned long long>(snapshot.authEntryHash),
            snapshot.authGate98,
            snapshot.authGateA0,
            static_cast<const void*>(snapshot.senseEntry),
            static_cast<unsigned long long>(snapshot.senseEntryHash),
            snapshot.senseGate98,
            snapshot.senseGateA0,
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x2CU),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x30U),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x34U),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x38U),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x3CU),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x40U),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x44U),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x7CU),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x90U),
            static_cast<unsigned>(tower_watch_runtime_field<std::uint8_t>(snapshot, 0x94U)),
            static_cast<unsigned>(tower_watch_runtime_field<std::uint16_t>(snapshot, 0x96U)),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x98U),
            static_cast<unsigned>(tower_watch_runtime_field<std::uint8_t>(snapshot, 0x9CU)),
            static_cast<unsigned>(tower_watch_runtime_field<std::uint16_t>(snapshot, 0x9EU)),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0xB8U),
            static_cast<unsigned>(tower_watch_runtime_field<std::uint8_t>(snapshot, 0xBCU)),
            static_cast<unsigned>(tower_watch_runtime_field<std::uint8_t>(snapshot, 0xBDU)),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0xC0U),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x00U),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x04U),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x08U),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x0CU),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x4CU),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x90U),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x94U),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x98U),
            tower_watch_runtime_field<std::uint32_t>(snapshot, 0x9CU));
        if (written > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(),
                              (std::min)(static_cast<std::size_t>(written),
                                         line.size() - 1U)});
        }
    }
}

/** Resolves an object-datum handle exactly as authority_publish_apply does.
 * The global names a registry root whose first pointer names the actual 64-byte bucket table;
 * treating the root itself as that table was the reason every Tower Watch probe stopped at stage
 * 5. The selected encoded row then receives the native relocation fixup
 * `row - (mask & *(row+8))`. Returns null on any bad intermediate. */
[[nodiscard]] const std::byte* resolve_object_datum(std::uint32_t handle,
                                                    std::uint8_t& stage) noexcept {
    stage = 1;
    if (handle == 0xFFFFFFFFU) {
        return nullptr;
    }
    stage = 2;
    const auto* const image = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr || !readable(image + kHandleRegistryPointerRva, 8U)) {
        return nullptr;
    }
    stage = 3;
    const auto* const directoryObject =
        read_value<const std::byte*>(image + kHandleRegistryPointerRva);
    if (!readable(directoryObject, 8U)) {
        return nullptr;
    }
    const auto* const registry = read_value<const std::byte*>(directoryObject);
    if (registry == nullptr) { return nullptr; }
    const std::int32_t shifted = static_cast<std::int32_t>(handle) >> 13;
    const std::uint64_t tableIndex =
        ((static_cast<std::uint64_t>(static_cast<std::uint32_t>(shifted)) | 0xFFC0000ULL) >> 0x12U)
        & static_cast<std::uint64_t>(static_cast<std::uint32_t>(shifted) & 0xFFFFU);
    const auto* const tableBase = registry + tableIndex * 0x40U;
    stage = 4;
    if (!readable(tableBase, 0x38U)) {
        return nullptr;
    }
    const std::int32_t stride = read_value<std::int32_t>(tableBase + 0x30U);
    const std::int32_t maskField = read_value<std::int32_t>(tableBase + 0x34U);
    const auto* const elements = read_value<const std::byte*>(tableBase + 8U);
    stage = 5;
    if (elements == nullptr || stride <= 0) {
        return nullptr;
    }
    const auto* const element =
        elements + static_cast<std::uint64_t>(handle & 0x1FFFU)
                       * static_cast<std::uint32_t>(stride);
    stage = 6;
    if (!readable(element, 0x10U)) {
        return nullptr;
    }
    const std::uint64_t reloc = static_cast<std::uint64_t>(static_cast<std::int64_t>(maskField))
                                & read_value<std::uint64_t>(element + 8U);
    stage = 7;
    return element - reloc;
}

/** Logs one slot's sync-pool object state {records, pending, bubble} when it changes. */
void sample_pool_slot(const char* label,
                      const PoolSlotDescriptor& descriptor,
                      std::atomic_uint64_t& lastState) noexcept {
    const auto* const image = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return;
    }
    const auto* const contextBytes = image + kPoolContextRva;
    const auto* const lookupBytes = image + kPoolLookupRva;
    if (std::memcmp(contextBytes, kPoolContextPrefix.data(), kPoolContextPrefix.size()) != 0
        || std::memcmp(lookupBytes, kPoolLookupPrefix.data(), kPoolLookupPrefix.size()) != 0) {
        return;
    }
    std::array<std::byte, 0x40> context{};
    std::uint32_t objectHandle = 0xFFFFFFFFU;
    std::uint8_t found = 0;
    std::int32_t recordA = -1;
    std::int32_t recordB = -1;
    std::uint8_t pending = 0xFF;
    std::uint8_t applied = 0xFF;
    std::uint8_t resolveStage = 0;
    std::uint32_t bubble = 0xFFFFFFFFU;
    __try {
        reinterpret_cast<PoolContextFn>(const_cast<std::byte*>(contextBytes))(context.data());
        found = reinterpret_cast<PoolLookupFn>(
            const_cast<std::byte*>(lookupBytes))(context.data(), &descriptor);
        objectHandle = read_value<std::uint32_t>(context.data() + 0x18U);
        const std::byte* const object =
            found != 0 ? resolve_object_datum(objectHandle, resolveStage) : nullptr;
        if (object != nullptr && readable(object, 0x70U)) {
            resolveStage = 8;
            recordB = read_value<std::int32_t>(object + 0x8U);
            recordA = read_value<std::int32_t>(object + 0xCU);
            pending = read_value<std::uint8_t>(object + 0x18U);
            applied = read_value<std::uint8_t>(object + 0x6EU);
            bubble = read_value<std::uint32_t>(object + 0x68U);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    std::uint64_t ctx0 = 0;
    std::uint64_t ctx1 = 0;
    std::uint64_t ctx2 = 0;
    std::memcpy(&ctx0, context.data(), sizeof ctx0);
    std::memcpy(&ctx1, context.data() + 8U, sizeof ctx1);
    std::memcpy(&ctx2, context.data() + 16U, sizeof ctx2);
    // ctx+0 / ctx+8 are the slot's pool ENTRY pointers (auth / sense). Their +0x98 dword
    // and +0xA0 dword are the walker's apply gates; nearby bytes carry the seed state.
    std::int32_t entA98 = -2;
    std::int32_t entAA0 = -2;
    std::int32_t entB98 = -2;
    std::int32_t entBA0 = -2;
    __try {
        const auto* const entryA = reinterpret_cast<const std::byte*>(ctx0);
        if (entryA != nullptr && readable(entryA + 0x90U, 0x20U)) {
            entA98 = read_value<std::int32_t>(entryA + 0x98U);
            entAA0 = read_value<std::int32_t>(entryA + 0xA0U);
        }
        const auto* const entryB = reinterpret_cast<const std::byte*>(ctx1);
        if (entryB != nullptr && readable(entryB + 0x90U, 0x20U)) {
            entB98 = read_value<std::int32_t>(entryB + 0x98U);
            entBA0 = read_value<std::int32_t>(entryB + 0xA0U);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    // ctx+8 is the live component itself (0000012155AB8110 in run 27456); a replaced or
    // destroyed component must produce a fresh line even when every flag still matches.
    const std::uint64_t entryState =
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(entA98)) << 32U)
        ^ static_cast<std::uint32_t>(entAA0) ^ (std::uint64_t{static_cast<std::uint32_t>(entB98)} << 16U)
        ^ (ctx1 * 0x9E3779B97F4A7C15ULL);
    const std::uint64_t state = entryState ^ (std::uint64_t{found} << 56U)
                                | (std::uint64_t{pending} << 48U)
                                | (std::uint64_t{applied} << 40U)
                                | (std::uint64_t{bubble & 0xFFU} << 32U)
                                | (static_cast<std::uint32_t>(recordA) & 0xFFFFU)
                                | ((static_cast<std::uint32_t>(recordB) & 0xFFFFU) << 16U);
    if (lastState.exchange(state, std::memory_order_relaxed) == state) {
        return;
    }
    std::array<char, 384> line{};
    const int written = std::snprintf(
        line.data(), line.size(),
        "ev=omega_pool slot=%s found=%u handle=0x%08X stage=%u recA=%d recB=%d pending=%u "
        "applied=%u bubble=%u ctx=%016llX,%016llX,%016llX ea98=0x%X eaA0=0x%X eb98=0x%X "
        "ebA0=0x%X",
        label, found, objectHandle, resolveStage, recordA, recordB, pending, applied, bubble,
        static_cast<unsigned long long>(ctx0), static_cast<unsigned long long>(ctx1),
        static_cast<unsigned long long>(ctx2), static_cast<std::uint32_t>(entA98),
        static_cast<std::uint32_t>(entAA0), static_cast<std::uint32_t>(entB98),
        static_cast<std::uint32_t>(entBA0));
    if (written > 0) {
        core::log::write(core::log::Channel::client, core::log::Level::info,
                         {line.data(), (static_cast<std::size_t>(written) < line.size())
                             ? static_cast<std::size_t>(written) : line.size()-1});
    }
}

__declspec(noinline) void __fastcall dialogue_scan(std::byte* component) noexcept {
    const std::uint64_t count = g_scanCount.fetch_add(1, std::memory_order_relaxed) + 1U;
    // The cue apply is synchronous, but local scene/spawner activation may be deferred to a
    // later component update. Sample the exact Tower Watch candidates periodically and emit only
    // when their pool or runtime fingerprint changes.
    if ((count & 0xFFU) == 1U && tower_watch_forced()) {
        report_tower_watch_slots("followup_change",
                                 0U,
                                 0U,
                                 capture_tower_watch_slots(),
                                 true);
    }
    // Sync-pool object sampler: interrogate the generator group's pool objects the same way
    // the object-block processor does, next to a healthy bubble-15 control slot. The seeded
    // predicate (+0x4D6530) blocks bubble 11's sweep while any of its objects keeps
    // pending(+0x18) set.
    if ((count & 0x1FFU) == 1U) {
        static std::atomic_uint64_t s_generatorPool{~0ULL};
        static std::atomic_uint64_t s_engagementPool{~0ULL};
        static std::atomic_uint64_t s_monitorPool{~0ULL};
        static std::atomic_uint64_t s_controlPool{~0ULL};
        static std::atomic_uint64_t s_dialoguePool{~0ULL};
        constexpr PoolSlotDescriptor generator{0x2763EC97U, 37U, 0U, 1};
        constexpr PoolSlotDescriptor engagement{0x2763EC97U, 70U, 0U, 0};
        constexpr PoolSlotDescriptor monitor{0x2763EC97U, 30U, 0U, 2};
        constexpr PoolSlotDescriptor control{0xD00142CFU, 30U, 0U, 24};
        sample_pool_slot("gen_37_1", generator, s_generatorPool);
        sample_pool_slot("gen_70_0", engagement, s_engagementPool);
        sample_pool_slot("gen_30_2", monitor, s_monitorPool);
        sample_pool_slot("ctl_30_24", control, s_controlPool);
        sample_pool_slot("dialogue_53_2", kDialoguePoolSlot, s_dialoguePool);
    }
    std::int32_t processedBefore = -1;
    if (component != nullptr) {
        const std::int32_t generation =
            read_value<std::int32_t>(component + kRecordGenerationOffset);
        const std::int32_t processed =
            read_value<std::int32_t>(component + kProcessedBaseOffset);
        processedBefore = processed;
        const std::uint64_t state =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(generation)) << 32U)
            | static_cast<std::uint32_t>(processed);
        if (g_lastScanState.exchange(state, std::memory_order_acq_rel) != state
            || count == 1U) {
            log_record_zero("scan", component, count);
        }
    }
    // Mission rows arrive through native 80804F77 apply; this scan only observes playback.
    const DialogueScan original = g_scanOriginal.load(std::memory_order_acquire);
    const auto adventureBefore=adventure_dialogue_observer::begin(component);
    if (original != nullptr) {
        original(component);
    }
    (void)adventure_dialogue_observer::finish(component,adventureBefore,original!=nullptr);
    if (component != nullptr && tower_watch_forced()) {
        const std::uint64_t time =
            read_value<std::uint64_t>(component + kRecordTimeOffset);
        const std::int32_t generation =
            read_value<std::int32_t>(component + kRecordGenerationOffset);
        const std::uint8_t mode =
            read_value<std::uint8_t>(component + kRecordModeOffset);
        const std::int32_t processedAfter =
            read_value<std::int32_t>(component + kProcessedBaseOffset);
        if (time != 0U && generation == 1 && mode == 2
            && processedBefore == 0 && processedAfter == 1) {
            const bool first =
                state::activity::mark_tower_watch_opening_dialogue_processed();
            std::array<char, 256> line{};
            const int written = std::snprintf(
                line.data(),
                line.size(),
                "ev=tower_watch_executor stage=dialogue_handoff result=%s "
                "record=0 generation=%d processed_before=%d processed_after=%d "
                "next=breach_cabal_beat",
                first ? "latched" : "duplicate",
                generation,
                processedBefore,
                processedAfter);
            if (written > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(),
                                  (std::min)(static_cast<std::size_t>(written),
                                             line.size() - 1U)});
            }
        }
    }
}

__declspec(noinline) void __fastcall dialogue_dispatch(std::byte* component,
                                                       std::int32_t index) noexcept {
    std::array<char, 192> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_dialogue stage=dispatch component=%p index=%d mutation=observe_only",
        static_cast<void*>(component),
        index);
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    const auto gatewayDispatchRun = state::activity::mission_run_generation();
    const DialogueDispatch original = g_dispatchOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(component, index);
        bool beyondDispatch{};
        if(component!=nullptr && index>=0 && index<55) {
            std::uint32_t self{};std::int64_t offset{};const auto bank=resolve_bank_handle(component,self,offset);
            if(bank==state::activity::vanilla::one_au::kBank) {
                beyondDispatch=true;
                const auto generation=read_value<std::uint32_t>(component+kRecordGenerationOffset+static_cast<std::size_t>(index)*0x20U);
                state::activity::vanilla::one_au::observe_submission(gatewayDispatchRun,self,offset,bank,static_cast<std::uint8_t>(index),generation);
            }
        }
        if(component!=nullptr && index>=0 && index<static_cast<std::int32_t>(std::size(state::activity::vanilla::homecoming::kDialogue))) {
            std::uint32_t self{};std::int64_t offset{};const auto bank=resolve_bank_handle(component,self,offset);
            if(bank==state::activity::vanilla::homecoming::kBank) {
                beyondDispatch=true;
                const auto generation=read_value<std::uint32_t>(component+kRecordGenerationOffset+static_cast<std::size_t>(index)*0x20U);
                state::activity::vanilla::homecoming::observe_submission(gatewayDispatchRun,self,offset,bank,static_cast<std::uint8_t>(index),generation);
            }
        }
        if(component!=nullptr && index>=0 && index<static_cast<std::int32_t>(std::size(state::activity::vanilla::adieu::kDialogue))) {
            std::uint32_t self{};std::int64_t offset{};const auto bank=resolve_bank_handle(component,self,offset);
            if(bank==state::activity::vanilla::adieu::kBank) {
                beyondDispatch=true;
                const auto generation=read_value<std::uint32_t>(component+kRecordGenerationOffset+static_cast<std::size_t>(index)*0x20U);
                state::activity::vanilla::adieu::observe_submission(gatewayDispatchRun,self,offset,bank,static_cast<std::uint8_t>(index),generation);
            }
        }
        if(component!=nullptr && index>=0 && index<49) {
            std::uint32_t self{};std::int64_t offset{};
            const auto bank=resolve_bank_handle(component,self,offset);
            if(bank==state::activity::beyond_infinity::kBank) {
                beyondDispatch=true;
                const auto generation=read_value<std::uint32_t>(component+kRecordGenerationOffset+static_cast<std::size_t>(index)*0x20U);
                state::activity::beyond_infinity::observe_submission(gatewayDispatchRun,self,offset,bank,static_cast<std::uint8_t>(index),generation);
            }
            if(bank==state::activity::deep_storage::kBank) {
                beyondDispatch=true;
                const auto generation=read_value<std::uint32_t>(component+kRecordGenerationOffset+static_cast<std::size_t>(index)*0x20U);
                state::activity::deep_storage::observe_submission(gatewayDispatchRun,self,offset,bank,static_cast<std::uint8_t>(index),generation);
            }
            if(bank==state::activity::hijacked::kBank) {
                beyondDispatch=true;
                const auto generation=read_value<std::uint32_t>(component+kRecordGenerationOffset+static_cast<std::size_t>(index)*0x20U);
                state::activity::hijacked::observe_submission(gatewayDispatchRun,self,offset,bank,static_cast<std::uint8_t>(index),generation);
            }
            if(bank==state::activity::newlight::launchpad::kBank) {
                beyondDispatch=true;
                const auto generation=read_value<std::uint32_t>(component+kRecordGenerationOffset+static_cast<std::size_t>(index)*0x20U);
                state::activity::newlight::launchpad::observe_submission(gatewayDispatchRun,self,offset,bank,static_cast<std::uint8_t>(index),generation);
            }
        }
        if (component != nullptr && index >= 0 && index < 34) {
            std::uint32_t self{};
            std::int64_t offset{};
            const auto bank = resolve_bank_handle(component, self, offset);
            const auto generation = read_value<std::uint32_t>(
                component + kRecordGenerationOffset + static_cast<std::size_t>(index)*0x20U);
            state::activity::gateway::observe_submission(gatewayDispatchRun,self,offset,bank,
                static_cast<std::uint8_t>(index),generation);
            state::activity::deadly_trial::observe_submission(gatewayDispatchRun,self,offset,bank,
                static_cast<std::uint8_t>(index),generation);
            state::activity::strike_bond::observe_submission(gatewayDispatchRun,self,offset,bank,static_cast<std::uint8_t>(index),generation);
            state::activity::strike_pact::observe_submission(gatewayDispatchRun,self,offset,bank,
                static_cast<std::uint8_t>(index),generation);
            if (bank != kDialogueBankHandle && bank != 0x80F1FC9EU && bank != 0x80F1F086U
                && !beyondDispatch && state::activity::strike_bond::native_run()==0
                && state::activity::strike_pact::native_run()==0) {
                // observe_submission() drops a foreign bank silently; say so once per row.
                log_reject("dispatch_bank", component, static_cast<std::uint32_t>(index),
                           generation, bank);
            }
            state::activity::omega_presentation::observe_submission(
                bank, static_cast<std::uint8_t>(index), generation);
        } else if (component != nullptr && !beyondDispatch) {
            log_reject("dispatch_index", component, static_cast<std::uint32_t>(index) & 0xFFU,
                       0U, static_cast<std::uint64_t>(static_cast<std::uint32_t>(index)));
        }
    }
}


void log_directive(const char* stage, const std::byte* component, std::uint64_t n) noexcept {
    const std::uint64_t headA = read_value<std::uint64_t>(component + 0x180U);
    const std::uint64_t headB = read_value<std::uint64_t>(component + 0x188U);
    const std::uint32_t key = read_value<std::uint32_t>(component + 0x190U);
    const std::int32_t discriminator = read_value<std::int32_t>(component + 0x194U);
    const std::int8_t lifecycle = read_value<std::int8_t>(component + 0x198U);
    const std::int32_t ring = read_value<std::int32_t>(component + 0x478U);
    const std::int32_t managerSlot = read_value<std::int32_t>(component + 0xB00U);
    const std::int32_t managerFlag = read_value<std::int32_t>(component + 0xB04U);
    const std::uint64_t time0 = read_value<std::uint64_t>(component + 0x1A8U);
    const std::uint64_t time1 = read_value<std::uint64_t>(component + 0x1B0U);
    const std::uint64_t time4 = read_value<std::uint64_t>(component + 0x1C8U);
    const std::uint32_t progressTail = read_value<std::uint32_t>(component + 0x1D0U);
    const std::int32_t value0 = read_value<std::int32_t>(component + 0x1D8U);
    const std::int32_t value1 = read_value<std::int32_t>(component + 0x1DCU);
    std::array<char, 384> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_directive stage=%s component=%p headA=0x%llX headB=0x%llX rec0_key=0x%08X "
        "rec0_disc=%d rec0_life=%d ring=%d mgr_slot=%d mgr_flag=%d t0=0x%llX t1=0x%llX "
        "t4=0x%llX ptail=0x%X v0=%d v1=%d n=%llu mutation=observe_only",
        stage,
        static_cast<const void*>(component),
        static_cast<unsigned long long>(headA),
        static_cast<unsigned long long>(headB),
        key,
        discriminator,
        static_cast<int>(lifecycle),
        ring,
        managerSlot,
        managerFlag,
        static_cast<unsigned long long>(time0),
        static_cast<unsigned long long>(time1),
        static_cast<unsigned long long>(time4),
        progressTail,
        value0,
        value1,
        static_cast<unsigned long long>(n));
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

void log_gate(const char* stage, const std::byte* component, std::uint64_t n) noexcept {
    const std::uint64_t channel0 = read_value<std::uint64_t>(component + 0x1C0U);
    const std::uint64_t channel1 = read_value<std::uint64_t>(component + 0x1C8U);
    const std::uint64_t channel2 = read_value<std::uint64_t>(component + 0x1D0U);
    // The tick only drives the bound 8080390E device: weak handle at +0x1F0, u32 handle
    // at +0x1F4 (-1 = unbound, committed channels go nowhere).
    const std::uint64_t deviceHandle = read_value<std::uint64_t>(component + 0x1F0U);
    std::array<char, 256> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_gate stage=%s component=%p ch0=0x%llX ch1=0x%llX ch2=0x%llX "
        "device=0x%llX n=%llu mutation=observe_only",
        stage,
        static_cast<const void*>(component),
        static_cast<unsigned long long>(channel0),
        static_cast<unsigned long long>(channel1),
        static_cast<unsigned long long>(channel2),
        static_cast<unsigned long long>(deviceHandle),
        static_cast<unsigned long long>(n));
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

__declspec(noinline) void __fastcall gate_apply(std::byte* component,
                                                const std::byte* packet) noexcept {
    const std::uint32_t count = g_gateCount.fetch_add(1, std::memory_order_relaxed) + 1U;
    // One-shot experiment: a few applies after arrival, drive the captured vex_wall device's
    // position channel through the native authority-checked setter (the recorder_device_test
    // precedent). Polarity 1.0 first; flip to 0.0 if the wall reads inverted.
    // DISABLED: firing the native setter from the apply thread froze mission entry; the
    // push belongs on the world thread. The vex_wall stash/logging stays for the rework.
    if (false && count >= 10U) {
        std::byte* const wall = g_vexWallDevice.load(std::memory_order_acquire);
        const DeviceSetter push = g_deviceCh0Original.load(std::memory_order_acquire);
        if (wall != nullptr && push != nullptr
            && g_vexWallPushState.exchange(1U, std::memory_order_acq_rel) == 0U) {
            const std::int32_t before = read_value<std::int32_t>(wall + 0x960U);
            push(wall, 1.F, 1, static_cast<std::uint32_t>(before + 1));
            const std::int32_t after = read_value<std::int32_t>(wall + 0x960U);
            std::array<char, 192> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=omega_device stage=vexwall_push device=%p value=1.0 rev_before=%d "
                "rev_after=%d mutation=native_setter_one_shot",
                static_cast<void*>(wall),
                before,
                after);
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    const DialogueApply original = g_gateOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(component, packet);
    }
    // Compare committed state per component AFTER the native apply. Alternating
    // an unchanged bound gate and an unchanged unbound gate is not a transition.
    // Log actual changes plus a ten-second per-component heartbeat, not two
    // synchronous log writes every authority refresh. Gameplay remains ungated.
    if (component != nullptr) {
        const GateTraceSample sample{reinterpret_cast<std::uintptr_t>(component),
            read_value<std::uint32_t>(component),read_value<std::uint32_t>(component+0x24U),
            {read_value<std::uint64_t>(component+0x1C0U),read_value<std::uint64_t>(component+0x1C8U),
             read_value<std::uint64_t>(component+0x1D0U),read_value<std::uint64_t>(component+0x1F0U)}};
        if(g_gateTrace.report(sample,GetTickCount64())) log_gate("apply_post",component,count);
    }
    state::activity::newlight::launchpad::observe_native_shutter_gate(component);
}

void log_device_setter(const char* channel,
                       const std::byte* device,
                       std::uint32_t value,
                       char snap,
                       std::uint32_t revision,
                       std::int32_t revBefore,
                       std::int32_t revAfter) noexcept {
    const std::uint32_t count = g_deviceLogCount.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (count > 24U && (count & 63U) != 0U) {
        return;
    }
    const std::uint16_t objectId = read_value<std::uint16_t>(device + 0x2CU);
    // +0x70 is the property binding the setter publishes through; -1 = store-only, no
    // presentation ever reaches the owner. +0x24 is the publish key it would use.
    const std::int32_t propertyBinding = read_value<std::int32_t>(device + 0x70U);
    const std::uint32_t publishKey = read_value<std::uint32_t>(device + 0x24U);
    std::uint32_t bit = 0xEEU;
    auto* const image = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    if (image != nullptr) {
        const std::uint32_t word = read_value<std::uint32_t>(
            image + kObjectAuthorityTableRva
            + (static_cast<std::size_t>(objectId & 0x1FFFU) >> 5U) * 4U);
        bit = (word >> (objectId & 0x1FU)) & 1U;
    }
    std::array<char, 256> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_device stage=%s device=%p object=0x%X auth_bit=%u value=0x%X snap=%d "
        "revision=%u rev960_before=%d rev960_after=%d binding=%d key=0x%X n=%u "
        "mutation=observe_only",
        channel,
        static_cast<const void*>(device),
        objectId,
        bit,
        value,
        static_cast<int>(snap),
        revision,
        revBefore,
        revAfter,
        propertyBinding,
        publishKey,
        count);
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

__declspec(noinline) void __fastcall device_channel0_setter(std::byte* device,
                                                            float value,
                                                            char snap,
                                                            std::uint32_t revision) noexcept {
    const std::int32_t before =
        device != nullptr ? read_value<std::int32_t>(device + 0x960U) : -2;
    const DeviceSetter original = g_deviceCh0Original.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(device, value, snap, revision);
    }
    const std::int32_t after =
        device != nullptr ? read_value<std::int32_t>(device + 0x960U) : -2;
    if (device != nullptr) {
        log_device_setter("ch0", device, std::bit_cast<std::uint32_t>(value), snap, revision, before, after);
    }
}

__declspec(noinline) void __fastcall device_channel1_setter(std::byte* device,
                                                            float value,
                                                            char snap,
                                                            std::uint32_t revision) noexcept {
    const std::int32_t before =
        device != nullptr ? read_value<std::int32_t>(device + 0x950U) : -2;
    const DeviceSetter original = g_deviceCh1Original.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(device, value, snap, revision);
    }
    const std::int32_t after =
        device != nullptr ? read_value<std::int32_t>(device + 0x950U) : -2;
    if (device != nullptr) {
        log_device_setter("ch1", device, std::bit_cast<std::uint32_t>(value), snap, revision, before, after);
    }
}

__declspec(noinline) std::uint64_t __fastcall device_configure(
    std::byte* device, const std::byte* config) noexcept {
    const std::uint32_t count =
        g_deviceConfigureCount.fetch_add(1, std::memory_order_relaxed) + 1U;
    const std::uint32_t nameHandle =
        config != nullptr ? read_value<std::uint32_t>(config) : 0xEEEEEEEEU;
    const std::uint32_t configWord1 =
        config != nullptr ? read_value<std::uint32_t>(config + 4) : 0U;
    const DeviceConfigure original = g_deviceConfigureOriginal.load(std::memory_order_acquire);
    std::uint64_t nativeResult = 0;
    if (original != nullptr) {
        nativeResult = original(device, config);
    }
    if (device == nullptr || (count > 48U && (count & 63U) != 0U)) {
        return nativeResult;
    }
    const std::int32_t binding = read_value<std::int32_t>(device + 0x70U);
    const std::uint32_t key = read_value<std::uint32_t>(device + 0x24U);
    std::array<char, 0x41> name{};
    for (std::size_t index = 0; index < 0x40U; ++index) {
        const char letter = static_cast<char>(std::to_integer<std::uint8_t>(device[0x30U + index]));
        if (letter == '\0') {
            break;
        }
        name[index] = (letter >= 0x20 && letter < 0x7F) ? letter : '?';
    }
    if (binding != -1 && std::memcmp(name.data(), "vex_wall", 9) == 0) {
        g_vexWallDevice.store(device, std::memory_order_release);
    }
    std::array<char, 288> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_device stage=configure device=%p name_handle=0x%08X word1=0x%X binding=%d "
        "key=0x%X name=\"%s\" n=%u mutation=observe_only",
        static_cast<void*>(device),
        nameHandle,
        configWord1,
        binding,
        key,
        name.data(),
        count);
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return nativeResult;
}

__declspec(noinline) void __fastcall directive_apply(std::byte* component,
                                                     const std::byte* packet) noexcept {
    const std::uint32_t count = g_directiveCount.fetch_add(1, std::memory_order_relaxed) + 1U;
    // Always log a record-state change (the armed body arriving), plus the first few applies.
    const std::uint64_t state =
        component != nullptr
            ? ((static_cast<std::uint64_t>(read_value<std::uint32_t>(component + 0x190U)) << 8U)
               | static_cast<std::uint8_t>(read_value<std::int8_t>(component + 0x198U)))
            : 0U;
    const bool changed =
        g_lastDirectiveState.exchange(state, std::memory_order_acq_rel) != state;
    const bool verbose = changed || count <= 4U || (count & 15U) == 0U;
    if (component != nullptr && verbose) {
        log_directive("apply_pre", component, count);
    }
    const bool inspectTowerWatch = component != nullptr && tower_watch_forced();
    const std::uint32_t cueBefore = inspectTowerWatch
                                        ? read_value<std::uint32_t>(component + 0x190U)
                                        : 0U;
    const TowerWatchSnapshotSet towerWatchBefore = inspectTowerWatch
                                                        ? capture_tower_watch_slots()
                                                        : TowerWatchSnapshotSet{};
    const DialogueApply original = hooking::await_original(g_directiveOriginal);
    const auto adventureBefore=adventure_cue_observer::begin(component,packet);
    if (original != nullptr) {
        original(component, packet);
        static_cast<void>(adventure_cue_observer::finish(component,adventureBefore));
    }
    const std::uint32_t cueAfter = inspectTowerWatch
                                       ? read_value<std::uint32_t>(component + 0x190U)
                                       : 0U;
    const bool authoredTowerWatchEdge = inspectTowerWatch && cueBefore != cueAfter
                                        && (cueAfter == kTowerWatchOpeningCue
                                            || cueAfter == kTowerWatchBreachCue
                                            || cueAfter == kTowerWatchPathCue);
    if (authoredTowerWatchEdge) {
        const TowerWatchSnapshotSet towerWatchAfter = capture_tower_watch_slots();
        report_tower_watch_slots("directive_pre",
                                 cueBefore,
                                 cueAfter,
                                 towerWatchBefore,
                                 false);
        report_tower_watch_slots("directive_post",
                                 cueBefore,
                                 cueAfter,
                                 towerWatchAfter,
                                 false);
        for (std::size_t index = 0U; index < towerWatchAfter.size(); ++index) {
            g_towerWatchSlotFingerprints[index].store(
                towerWatchAfter[index].fingerprint, std::memory_order_release);
        }
    }
    if (component != nullptr && verbose) {
        log_directive("apply_post", component, count);
    }
}

/** True only when [ptr, ptr+size) is entirely committed and readable; no fault on a bad resolve. */
[[nodiscard]] bool readable(const std::byte* ptr, std::size_t size) noexcept {
    if (ptr == nullptr) {
        return false;
    }
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(ptr, &info, sizeof info) == 0) {
        return false;
    }
    if (info.State != MEM_COMMIT) {
        return false;
    }
    const DWORD readMask = PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ
                           | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY;
    if ((info.Protect & readMask) == 0 || (info.Protect & PAGE_GUARD) != 0) {
        return false;
    }
    const std::byte* const regionEnd =
        reinterpret_cast<const std::byte*>(info.BaseAddress) + info.RegionSize;
    return ptr + size <= regionEnd;
}

/** Replicates the destructor's own handle->object resolve (registry walk + relocation fixup). */
struct HandleResolve {
    const std::byte* obj;
    std::int32_t esize;
};
[[nodiscard]] omega_teardown_native::Source teardown_source() noexcept {
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    return {image==0 ? 0 : image+kHandleRegistryPointerRva,nullptr,read_dialogue_memory};
}
[[nodiscard]] HandleResolve resolve_handle_value(std::uint32_t handle) noexcept {
    const auto object=omega_teardown_native::resolve(teardown_source(),handle);
    return {reinterpret_cast<const std::byte*>(object.address),object.stride};
}

[[nodiscard]] const std::byte* resolve_destructor_object(const std::uint32_t* component) noexcept {
    static std::atomic_uint32_t s_resolveLog{};
    const std::uint32_t n=s_resolveLog.fetch_add(1,std::memory_order_relaxed);
    omega_teardown_native::Object object{};
    std::uint32_t handle=UINT32_MAX;
    const auto address=omega_teardown_native::scene_object(teardown_source(),
        reinterpret_cast<std::uintptr_t>(component),object,handle);
    if(n<24U) {
        std::array<char,320> line{};
        const int length=std::snprintf(line.data(),line.size(),
            "ev=omega_teardown stage=resolve n=%u handle=0x%08X directory=%p registry=%p "
            "tidx=0x%X table=%p esize=%d element=%p object=%p",
            n,handle,reinterpret_cast<const void*>(object.directory),
            reinterpret_cast<const void*>(object.registry),object.tableIndex,
            reinterpret_cast<const void*>(object.table),object.stride,
            reinterpret_cast<const void*>(object.element),reinterpret_cast<const void*>(address));
        if(length>0 && static_cast<std::size_t>(length)<line.size()) {
            core::log::write(core::log::Channel::client,core::log::Level::info,
                {line.data(),static_cast<std::size_t>(length)});
        }
    }
    return reinterpret_cast<const std::byte*>(address);
}

[[nodiscard]] std::uint64_t forest_record_hash(const std::byte* record) noexcept {
    std::uint64_t hash = 0xCBF29CE484222325ULL;
    for (std::size_t index = 0; index < kForestRecordBytes; ++index) {
        hash = (hash ^ static_cast<std::uint64_t>(record[index])) * 0x100000001B3ULL;
    }
    return hash;
}

/** Logs one 0x5B4 record as chunked hex, budgeted so a chatty run cannot flood the log. */
void forest_dump_record(const char* which, const std::byte* record) noexcept {
    if (g_forestDumpBudget.load(std::memory_order_relaxed) == 0) {
        return;
    }
    g_forestDumpBudget.fetch_sub(1, std::memory_order_relaxed);
    constexpr std::size_t kChunk = 96;
    for (std::size_t offset = 0; offset < kForestRecordBytes; offset += kChunk) {
        const std::size_t count = (std::min)(kChunk, kForestRecordBytes - offset);
        std::array<char, 260> line{};
        int written = std::snprintf(line.data(), line.size(), "ev=forest dump=%s off=0x%03zX ",
                                    which, offset);
        for (std::size_t index = 0; index < count; ++index) {
            written += std::snprintf(line.data() + written,
                                     line.size() - static_cast<std::size_t>(written), "%02X",
                                     static_cast<unsigned>(record[offset + index]));
        }
        if (written > 0) {
            core::log::write(core::log::Channel::client, core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(written)});
        }
    }
}

__declspec(noinline) std::uint64_t __fastcall forest_create_hook(void* component) noexcept {
    log_caller_rva("forest_create", _ReturnAddress(), component);
    g_forestSensorPtr.store(component, std::memory_order_release);
    const ForestCreateFn original = g_forestCreateOriginal.load(std::memory_order_acquire);
    const std::uint64_t result = original != nullptr ? original(component) : 0;
    const std::uint32_t count = g_forestCreateCount.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (count <= 8U) {
        std::array<char, 128> line{};
        const int written = std::snprintf(line.data(), line.size(),
                                          "ev=forest stage=create component=%p n=%u",
                                          component, count);
        if (written > 0) {
            core::log::write(core::log::Channel::client, core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(written)});
        }
    }
    return result;
}

__declspec(noinline) std::uint64_t __fastcall forest_apply_hook(void* component,
                                                                void* payload) noexcept {
    log_caller_rva("forest_apply", _ReturnAddress(), payload);
    const ForestPairFn original = g_forestApplyOriginal.load(std::memory_order_acquire);
    const std::uint64_t result =
        original != nullptr ? original(component, payload) : 0;
    const std::uint32_t count = g_forestApplyCount.fetch_add(1, std::memory_order_relaxed) + 1U;
    const auto* record = static_cast<const std::byte*>(component) + kForestAuthorityOffset;
    if (component != nullptr && readable(record, kForestRecordBytes)) {
        const std::uint64_t hash = forest_record_hash(record);
        const std::uint64_t previous =
            g_forestAuthorityHash.exchange(hash, std::memory_order_acq_rel);
        if (hash != previous || count <= 2U) {
            std::array<char, 160> line{};
            const int written = std::snprintf(
                line.data(), line.size(),
                "ev=forest stage=authority_apply component=%p n=%u hash=0x%016llX changed=%u",
                component, count, static_cast<unsigned long long>(hash),
                hash != previous ? 1U : 0U);
            if (written > 0) {
                core::log::write(core::log::Channel::client, core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(written)});
            }
            if (hash != previous) {
                forest_dump_record("authority", record);
            }
        }
    }
    return result;
}

__declspec(noinline) std::uint64_t __fastcall forest_sense_hook(void* component,
                                                                void* payload) noexcept {
    const ForestPairFn original = g_forestSenseOriginal.load(std::memory_order_acquire);
    const std::uint64_t result =
        original != nullptr ? original(component, payload) : 0;
    const std::uint32_t count = g_forestSenseCount.fetch_add(1, std::memory_order_relaxed) + 1U;
    const auto* record = static_cast<const std::byte*>(component) + kForestSenseOffset;
    if (component != nullptr && readable(record, kForestRecordBytes)) {
        const std::uint64_t hash = forest_record_hash(record);
        const std::uint64_t previous =
            g_forestSenseHash.exchange(hash, std::memory_order_acq_rel);
        if (hash != previous || count <= 2U) {
            std::array<char, 160> line{};
            const int written = std::snprintf(
                line.data(), line.size(),
                "ev=forest stage=sense_export component=%p n=%u hash=0x%016llX changed=%u",
                component, count, static_cast<unsigned long long>(hash),
                hash != previous ? 1U : 0U);
            if (written > 0) {
                core::log::write(core::log::Channel::client, core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(written)});
            }
            if (hash != previous) {
                forest_dump_record("sense", record);
            }
        }
    }
    return result;
}

/** Applies the Forest menu page's explicit diagnostic overrides before each worker tick
 * because the wire's (empty) authority apply rewrites the record every push; identical
 * rewrites are free thanks to the worker's own change-detect. Field map: presence mask
 * @+0x2C {bit0 seed@+0x00, bit1 mode@+0x04, bit2 anchor block@+0x08, bit3 enable@+0x2D};
 * floats @+0x30/34 and ints @+0x38/3C use -1 sentinels and need no bits. */
void apply_forest_tuner(std::byte* record) noexcept {
    auto& dial = forest_tuner::state();
    std::uint8_t maskSet = 0;
    if (dial.writeSeed.load(std::memory_order_relaxed)) {
        const std::uint32_t seed =
            static_cast<std::uint32_t>(dial.seed.load(std::memory_order_relaxed));
        std::memcpy(record + 0x00U, &seed, 4U);
        maskSet |= 0x01U;
    }
    if (dial.writeMode.load(std::memory_order_relaxed)) {
        record[0x04U] =
            static_cast<std::byte>(dial.mode.load(std::memory_order_relaxed) & 0xFF);
        maskSet |= 0x02U;
    }
    bool wroteGroup = false;
    for (std::size_t index = 0; index < dial.groups.size(); ++index) {
        const forest_tuner::Group& source = dial.groups[index];
        if (!source.write.load(std::memory_order_relaxed)) {
            continue;
        }
        forest_tuner::write_group(record, index, source);
        wroteGroup = true;
    }
    if (wroteGroup) {
        maskSet |= 0x04U;
    }
    if (dial.writeFloats.load(std::memory_order_relaxed)) {
        const float f0 = dial.f0.load(std::memory_order_relaxed);
        const float f1 = dial.f1.load(std::memory_order_relaxed);
        std::memcpy(record + 0x30U, &f0, 4U);
        std::memcpy(record + 0x34U, &f1, 4U);
    }
    if (dial.writeInts.load(std::memory_order_relaxed)) {
        const int i0 = dial.i0.load(std::memory_order_relaxed);
        const int i1 = dial.i1.load(std::memory_order_relaxed);
        std::memcpy(record + 0x38U, &i0, 4U);
        std::memcpy(record + 0x3CU, &i1, 4U);
    }
    if (dial.enable.load(std::memory_order_relaxed)) {
        maskSet |= 0x08U;
        record[0x2DU] = std::byte{1};
    }
    if (maskSet != 0U) {
        record[0x2CU] = static_cast<std::byte>(read_value<std::uint8_t>(record + 0x2CU)
                                               | maskSet);
    }
    dial.applies.fetch_add(1, std::memory_order_relaxed);
}

#include "beyond_infinity_forest_runtime.inl"
#include "garden_world_forest_runtime.inl"

/** A cached sensor or configured Omega default does not identify the current world. */
[[nodiscard]] bool omega_forest_context() noexcept {
    const bool worldActive = state::activity::world_phase() != state::activity::WorldPhase::idle;
    if (!worldActive) {
        return false;
    }
    const auto activity = state::activity::newest_joined_activity();
    state::activity::destination::DestinationSelection selected{};
    if (!static_cast<bool>(activity) || !state::activity::destination::snapshot(activity, selected)
        || selected.packageNameLength > selected.packageName.size()) {
        return false;
    }
    return omega_forest::legacy_mutation_allowed(worldActive, true,
        {reinterpret_cast<const char*>(selected.packageName.data()), selected.packageNameLength});
}

/** Recipe, solver and owner-authority interventions also require the proven worker family. */
[[nodiscard]] bool omega_forest_worker(void* instance) noexcept {
    const auto* bytes = static_cast<const std::byte*>(instance);
    return omega_forest_context() && bytes != nullptr
           && readable(bytes, omega_forest::kWorkerPrefixSize)
           && omega_forest::matches({bytes, omega_forest::kWorkerPrefixSize}, "mission_scot");
}

/** Native generator authority owns recipe delivery. Gateway ownership remains a compatibility adapter. */
void prepare_omega_forest(void* instance) noexcept {
    if (!omega_forest_worker(instance)) { return; }
    auto* worker = static_cast<std::byte*>(instance);
    // FF4B20 skips replicated gateway creation without the WORKER OWNER'S authority bit.
    // Repair just this live owner's bit before native state4; never force gate position/progress.
    const auto owner = read_value<std::uint32_t>(worker + 0x2CU);
    const auto setter = g_forestOwnerAuthoritySetter.load(std::memory_order_acquire);
    const auto* image = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    if (owner == 0xFFFFFFFFU || setter == nullptr || image == nullptr) { return; }
    const auto* word = image + kObjectAuthorityTableRva + ((owner & 0x1FFFU) >> 5U) * 4U;
    const auto bit = 1U << (owner & 31U);
    if (readable(word, 4U) && (read_value<std::uint32_t>(word) & bit) == 0U) {
        setter(owner, 1U);
        static std::atomic_uint32_t reports{};
        if (reports.fetch_add(1U, std::memory_order_relaxed) < 8U) {
            std::array<char, 112> line{};
            const int length = std::snprintf(line.data(), line.size(),
                "ev=forest stage=omega_owner_authority owner=0x%08X result=local", owner);
            if (length > 0) {
                core::log::write(core::log::Channel::client, core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
}

// Shared native activities register their exact worker definition, palette and
// effective seed. Retain Dawn's existing owner-authority compatibility boundary
// across worker recreation; do not special-case missions, branches or actors.
void prepare_registered_forest(void* instance) noexcept {
    const auto owner=registered_native_forest_owner(instance);
    const auto setter=g_forestOwnerAuthoritySetter.load(std::memory_order_acquire);
    const auto* image=reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    if(owner==UINT32_MAX || !setter || !image)return;
    const auto authority=native_authority_bitmap::View::acquire(
        image+kObjectAuthorityTableRva,readable);
    if(!authority)return;
    const auto retain=[&](std::uint32_t entity) noexcept {
        if(!authority.missing(entity))return false;
        setter(entity,1U);return true;
    };
    const bool repaired=retain(owner);
    // FF9320 retires a gateway only while that gateway owns authority, then
    // drops its weak reference unconditionally. Cover the worker's exact live
    // children before the normal tick can queue that native cleanup.
    std::array<std::uint32_t,128U*17U> gates{};
    const auto count=registered_native_forest_gate_owners(instance,gates);
    unsigned repairedGates{};
    for(std::size_t i=0;i<count;++i)if(retain(gates[i]))++repairedGates;
    if(!repaired && !repairedGates)return;
    std::array<char,128> line{};
    const auto size=std::snprintf(line.data(),line.size(),
        "ev=forest stage=registered_owner_authority owner=%08X worker=%p gates=%u",owner,instance,repairedGates);
    if(size>0 && static_cast<std::size_t>(size)<line.size())
        core::log::write(core::log::Channel::client,core::log::Level::info,
            {line.data(),static_cast<std::size_t>(size)});
}

__declspec(noinline) std::uint64_t __fastcall forest_worker_create_hook(void* instance,
                                                                        void* defRef) noexcept {
    const ForestPairFn original = g_forestWorkerCreateOriginal.load(std::memory_order_acquire);
    const std::uint64_t result = original != nullptr ? original(instance, defRef) : 0;
    const std::uint32_t count =
        g_forestWorkerCreateCount.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (count <= 8U) {
        std::array<char, 128> line{};
        const int written = std::snprintf(line.data(), line.size(),
                                          "ev=forest stage=worker_create instance=%p n=%u ok=%llu",
                                          instance, count,
                                          static_cast<unsigned long long>(result));
        if (written > 0) {
            core::log::write(core::log::Channel::client, core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(written)});
        }
    }
    return result;
}

__declspec(noinline) std::uint64_t __fastcall forest_worker_tick_hook(void* instance,
                                                                      void* context) noexcept {
    prepare_omega_forest(instance);
    const bool beyondForest=beyond_forest_runtime::selected();
    if(beyondForest) { beyond_forest_runtime::prepare(instance); }
    garden_forest_runtime::prepare(instance);
    prepare_registered_forest(instance);
    const bool legacyMutation = omega_forest_context();
    // Omega-only diagnostic ignition; generic native forest workers keep their authority input.
    // The worker reads the sensor authority record at instance+0x180 through
    // presence-gated accessors (+0x2C mask: bit0 seed@+0x00, bit1 mode@+0x04, bit2 the 4-group
    // anchor block @+0x08 (see forest_tuner_record.h), bit3 enable@+0x2D, bits 4..6 ints
    // @+0x40/44/48; f32s @+0x30/34 and ints @+0x38/3C use -1 sentinels). Values come from
    // the Forest menu's shared diagnostic dial, applied before every tick; any change makes the worker's
    // change-detect rebuild the whole layout in-place â€” a live combination dial.
    void* const sensor = g_forestSensorPtr.load(std::memory_order_acquire);
    if (legacyMutation && sensor != nullptr && !beyondForest) {
        auto* const record = static_cast<std::byte*>(sensor) + kForestAuthorityOffset;
        if (readable(record, 0x60U)) {
            // Explicit developer UI overrides only; ordinary mission delivery is native.
            apply_forest_tuner(record);
        }
    }
    // Entry force-toggle: the dial hands an entry index; call the worker's own per-entry
    // activate (+0x10020A0, the host-sync branch's toggle) on the tick thread to learn which
    // entry is the stairs anchor piece.
    if (legacyMutation) {
        static std::array<std::atomic<void*>, forest_tuner::State::kWorkerSlots> s_workers{};
        int workerSlot = -1;
        for (std::size_t slot = 0; slot < s_workers.size() && workerSlot < 0; ++slot) {
            void* expected = s_workers[slot].load(std::memory_order_relaxed);
            if (expected == instance) {
                workerSlot = static_cast<int>(slot);
            } else if (expected == nullptr) {
                if (s_workers[slot].compare_exchange_strong(expected, instance,
                                                            std::memory_order_relaxed)) {
                    workerSlot = static_cast<int>(slot);
                }
            }
        }
        int entryCount = 0;
        if (instance != nullptr
            && readable(static_cast<const std::byte*>(instance) + 0x924U, 4U)) {
            entryCount =
                read_value<std::int32_t>(static_cast<const std::byte*>(instance) + 0x924U);
            if (workerSlot >= 0) {
                forest_tuner::state().entryCountPer[static_cast<std::size_t>(workerSlot)]
                    .store(entryCount, std::memory_order_relaxed);
            }
        }
        const int forced =
            workerSlot >= 0
                ? forest_tuner::state().forceEntryPer[static_cast<std::size_t>(workerSlot)]
                      .exchange(-1, std::memory_order_relaxed)
                : -1;
        // The native per-entry activate indexes raw tables with no bounds check; an index
        // past the worker's entry count corrupts memory (the 2026-08-27 crash at ~20).
        if (forced >= 0 && forced < entryCount && instance != nullptr) {
            // Entries the build skipped (unresolved piece defs) stay at state 0 and the
            // native activate wedges on them (the freeze at entry 18); only fire entries
            // the build initialized. Entry table: base = instance + qword@+0x858 + 0x858,
            // stride 0x38, state byte @+0x29.
            std::uint8_t entryState = 0;
            bool entryKnown = false;
            const auto* const instanceBytes = static_cast<const std::byte*>(instance);
            if (readable(instanceBytes + 0x858U, 8U)) {
                const std::int64_t tableOffset =
                    read_value<std::int64_t>(instanceBytes + 0x858U);
                const std::byte* const entry = instanceBytes + tableOffset + 0x858U
                                               + static_cast<std::int64_t>(forced) * 0x38;
                if (readable(entry, 0x38U)) {
                    entryState = read_value<std::uint8_t>(entry + 0x29U);
                    entryKnown = true;
                }
            }
            if (!entryKnown || entryState == 0U) {
                std::array<char, 112> skipLine{};
                const int skipWritten = std::snprintf(
                    skipLine.data(), skipLine.size(),
                    "ev=forest stage=entry_skip entry=%d worker=%d state=%u known=%u",
                    forced, workerSlot, entryState, entryKnown ? 1U : 0U);
                if (skipWritten > 0) {
                    core::log::write(core::log::Channel::client, core::log::Level::info,
                                     {skipLine.data(),
                                      static_cast<std::size_t>(skipWritten)});
                }
            } else {
            auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
            if (image != nullptr) {
                using ActivateEntryFn = void(__fastcall*)(void*, int) noexcept;
                __try {
                    reinterpret_cast<ActivateEntryFn>(image + 0x10020A0U)(instance, forced);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                }
                std::array<char, 96> line{};
                const int written = std::snprintf(
                    line.data(), line.size(),
                    "ev=forest stage=entry_force entry=%d worker=%d instance=%p", forced,
                    workerSlot, instance);
                if (written > 0) {
                    core::log::write(core::log::Channel::client, core::log::Level::info,
                                     {line.data(), static_cast<std::size_t>(written)});
                }
            }
            }
        }
    }
    // Authority verdict probe: the build (+0xFF8300) SKIPS network-replicated pieces (def+0x98
    // bit 4, filter +0x14E4720) when self-authority (+0xFFF500 -> +0x4E7F70 bubble-mask/
    // registry gate) is false â€” the missing first/last platforms are the replicated ones.
    {
        static std::atomic_uint32_t s_authorityProbes{0};
        const std::uint32_t probe = s_authorityProbes.fetch_add(1, std::memory_order_relaxed);
        if (probe < 3U || (probe & 0x1FFU) == 0U) {
            auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
            if (image != nullptr) {
                using SelfAuthorityFn = std::uint8_t(__fastcall*)(void*) noexcept;
                using StartActiveFn = std::uint8_t(__fastcall*)(void*) noexcept;
                std::uint8_t selfAuthority = 0xFF;
                std::uint8_t startActive = 0xFF;
                __try {
                    selfAuthority =
                        reinterpret_cast<SelfAuthorityFn>(image + 0xFFF500U)(instance);
                    startActive = reinterpret_cast<StartActiveFn>(image + 0xFFF4A0U)(instance);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                }
                std::array<char, 112> line{};
                const int written = std::snprintf(
                    line.data(), line.size(),
                    "ev=forest stage=authority_probe self=%u start_active=%u n=%u", selfAuthority,
                    startActive, probe);
                if (written > 0) {
                    core::log::write(core::log::Channel::client, core::log::Level::info,
                                     {line.data(), static_cast<std::size_t>(written)});
                }
            }
        }
    }
    const ForestPairFn original = g_forestWorkerTickOriginal.load(std::memory_order_acquire);
    const std::uint64_t result = original != nullptr ? original(instance, context) : 0;
    if (omega_forest_worker(instance)) {
        omega_enemy_forest::observe(instance,state::activity::mission_run_generation());
    }
    // Shared population admission uses the registered generator lease; it is
    // independent of Omega's legacy recipe/authority intervention scope.
    observe_native_generated_population(instance);
    const std::uint32_t count =
        g_forestWorkerTickCount.fetch_add(1, std::memory_order_relaxed) + 1U;
    const auto* stateByte = static_cast<const std::byte*>(instance) + kForestWorkerStateOffset;
    if (instance != nullptr && readable(stateByte, 1U)) {
        const std::uint32_t state = static_cast<std::uint32_t>(read_value<std::uint8_t>(stateByte));
        const std::uint32_t previous =
            g_forestWorkerLastState.exchange(state, std::memory_order_acq_rel);
        if (state != previous || count <= 2U) {
            std::array<char, 128> line{};
            const int written = std::snprintf(
                line.data(), line.size(),
                "ev=forest stage=worker_tick instance=%p n=%u state=%u prev_state=%u", instance,
                count, state, previous);
            if (written > 0) {
                core::log::write(core::log::Channel::client, core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(written)});
            }
        }
    }
    return result;
}

__declspec(noinline) std::uint64_t __fastcall seed_check_hook(void* poolRoot) noexcept {
    const SeedCheckFn original = g_seedCheckOriginal.load(std::memory_order_acquire);
    const std::uint64_t verdict = original != nullptr ? original(poolRoot) : 0;
    if (static_cast<std::uint8_t>(verdict) != 0U || poolRoot == nullptr
        || !omega_forest_context()) {
        return verdict;
    }
    std::uint32_t maskB = 0;
    __try {
        const auto* const container = static_cast<const std::byte*>(poolRoot)
                                      - kSeedCheckPoolOffset;
        if (!readable(container + kMaskBOffset, sizeof maskB)) {
            return verdict;
        }
        std::memcpy(&maskB, container + kMaskBOffset, sizeof maskB);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return verdict;
    }
    const auto scopedVerdict = omega_forest::legacy_seed_verdict(true, verdict, maskB);
    if (scopedVerdict == verdict) {
        return verdict;
    }
    const std::uint32_t count = g_seedForceCount.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (count <= 8U) {
        std::array<char, 112> line{};
        const int written = std::snprintf(
            line.data(), line.size(),
            "ev=omega_pool stage=seed_forced mask_b=0x%08X n=%u", maskB, count);
        if (written > 0) {
            core::log::write(core::log::Channel::client, core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(written)});
        }
    }
    return scopedVerdict;
}

__declspec(noinline) std::uint64_t __fastcall record_processor_hook(void* container,
                                                                    void* stream) noexcept {
    const RecordProcessorFn original = g_recordProcessorOriginal.load(std::memory_order_acquire);
    const std::uint64_t result = original != nullptr ? original(container, stream) : 0;
    const std::uint32_t call =
        g_recordProcessorCalls.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (static_cast<std::uint8_t>(result) == 0U) {
        const std::uint32_t fails =
            g_recordProcessorFails.fetch_add(1, std::memory_order_relaxed) + 1U;
        if (fails <= 24U) {
            std::array<char, 128> line{};
            const int written = std::snprintf(
                line.data(), line.size(),
                "ev=omega_pool stage=record_fail call=%u fails=%u result=0x%llX", call, fails,
                static_cast<unsigned long long>(result));
            if (written > 0) {
                core::log::write(core::log::Channel::client, core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(written)});
            }
        }
    }
    return result;
}

[[nodiscard]] std::uint32_t current_roster_bubble() noexcept {
    const auto context=g_bubbleContext.load(std::memory_order_acquire);
    const auto read=g_bubbleRead.load(std::memory_order_acquire);
    std::array<std::uint32_t,2> result{UINT32_MAX,UINT32_MAX};
    if(context!=nullptr && read!=nullptr) { (void)read(context(),result.data()); }
    return result[0];
}
void retirement_log(const char* reason,const state::activity::omega_ending::Token& token,
                    std::uint32_t owner,bool accepted) noexcept {
    if(!accepted && g_retirementRejects.fetch_add(1,std::memory_order_relaxed)>=16) { return; }
    std::array<char,256> line{};
    const int size=std::snprintf(line.data(),line.size(),
        "ev=omega_ending stage=native_retirement reason=%s run=%llu epoch=%llu actor=%08X generation=%u owner=%08X accepted=%u",
        reason,static_cast<unsigned long long>(token.run),static_cast<unsigned long long>(token.epoch),
        token.actor,token.generation,owner,accepted?1U:0U);
    if(size>0 && static_cast<std::size_t>(size)<line.size()) {
        core::log::write(core::log::Channel::client,core::log::Level::info,
            {line.data(),static_cast<std::size_t>(size)});
    }
}
/** Observe the actual cleanup transaction, never its publication. No locks are held; copied owner identities are
 * revalidated after the native callback. Original always runs once. */
__declspec(noinline) void __fastcall roster_apply_hook(void* context,const void* delta) noexcept {
    const hooking::CallGate::Scope scope(g_rosterGate);
    const auto original=hooking::await_original(g_rosterOriginal);
    namespace ending=state::activity::omega_ending;
    const auto gardenLease=scope.accepts_side_effects()?garden_ending_native::before(teardown_source(),
        reinterpret_cast<std::uintptr_t>(context),reinterpret_cast<std::uintptr_t>(delta)):garden_ending_native::Cleanup{};
    ending::Token token{};
    omega_teardown_native::Retirement lease{},launchpadLease{};
    namespace launchpad=state::activity::newlight::launchpad;
    state::activity::coo::Generation launchpadOwner{};
    std::uint8_t launchpadMovie{};
    if(scope.accepts_side_effects()) {
        const auto request=launchpad::request();
        if(request.frame.enabled && request.owner.valid()
            && request.frame.cinematic.retiring() && !request.frame.cinematic.gameplayRetired
            && (request.frame.cinematic.movie==1
                || (request.frame.cinematic.movie==2 && request.frame.cinematic.endingStarted))) {
            launchpadOwner=request.owner;
            launchpadMovie=request.frame.cinematic.movie;
            unsigned reason{};
            launchpadLease=launchpad_retirement_native::begin(teardown_source(),
                reinterpret_cast<std::uintptr_t>(context),reinterpret_cast<std::uintptr_t>(delta),launchpadMovie,&reason);
            // Ignore foreign owners. Record the first relevant native callback
            // so a rejected mirror can be distinguished from cleanup stalling.
            static std::atomic<unsigned> reports{};
            if(reason!=1 && reports.fetch_add(1,std::memory_order_relaxed)<8) {
                std::array<char,160> line{};std::snprintf(line.data(),line.size(),
                    "ev=launchpad stage=ending_retirement result=native_apply run=%llu owner=%u movie=%u guard=%u",
                    static_cast<unsigned long long>(launchpadOwner.run),launchpadOwner.value,launchpadMovie,reason);
                core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
            }
        }
        token=ending::retirement_request(state::activity::mission_run_generation());
        if(token.valid()) {
            lease=omega_teardown_native::begin_retirement(teardown_source(),
                reinterpret_cast<std::uintptr_t>(context),reinterpret_cast<std::uintptr_t>(delta),current_roster_bubble());
            if(!lease.valid()) { retirement_log("before_guard",token,UINT32_MAX,false); }
        }
    }
    {
        const graphics::hijacked_frame_timing::PostSpan timing(graphics::hijacked_frame_timing::Kind::roster_apply);
        original(context,delta);
    }
    if(scope.accepts_side_effects()) garden_ending_native::after(teardown_source(),gardenLease);
    if(scope.accepts_side_effects() && launchpadLease.valid()
        && state::activity::mission_run_generation()==launchpadOwner.run) {
        const auto current=launchpad::request();
        const auto remove=g_indexFreeOriginal.load(std::memory_order_acquire);
        const auto source=teardown_source();
        if(current.owner==launchpadOwner && current.frame.cinematic.retiring()
            && current.frame.cinematic.movie==launchpadMovie
            && !current.frame.cinematic.gameplayRetired) {
            const bool done=launchpad_retirement_native::finish(source,launchpadLease)
                || (remove && launchpad_retirement_native::retire_globals(source,launchpadLease,
                    [remove](std::uintptr_t list,std::uint32_t node) noexcept {
                        remove(reinterpret_cast<int*>(list),node);
                    }));
            if(done) {static_cast<void>(launchpad::observe_retirement(launchpadOwner));}
            else {
                std::int32_t count{-1};
                static_cast<void>(launchpad_retirement_native::read(source,launchpadLease.owner+0x28,count));
                std::array<char,160> line{};std::snprintf(line.data(),line.size(),
                    "ev=launchpad stage=ending_retirement result=cleanup_guard groups=%d owner=%u mirror=%u",
                    count,launchpad_retirement_native::same_owner(source,launchpadLease)?1U:0U,
                    launchpad_retirement_native::roster(source,launchpadLease.context+8,true)?1U:0U);
                core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
            }
        }
    }
    if(!scope.accepts_side_effects() || !lease.valid()) { return; }
    if(state::activity::mission_run_generation()!=token.run
        || ending::retirement_request(token.run)!=token
        || !omega_teardown_native::finish_retirement(teardown_source(),lease,current_roster_bubble())) {
        retirement_log("after_guard",token,lease.handle,false);return;
    }
    const bool accepted=ending::observe_retirement(token);
    retirement_log(accepted?"native_cleanup_complete":"token_changed",token,lease.handle,accepted);
}
[[nodiscard]] bool roster_idle() noexcept { return g_rosterGate.idle(); }

__declspec(noinline) void __fastcall index_free_guard(int* list, std::uint32_t index) noexcept {
    const IndexFree original = g_indexFreeOriginal.load(std::memory_order_acquire);
    // Resolve through both native directory levels, then require this exact full
    // datum in the native links that removal touches. No unlink or free is synthesized.
    const bool present=omega_teardown_native::can_unregister(teardown_source(),
        reinterpret_cast<std::uintptr_t>(list),index);
    if (!present) {
        const std::uint32_t n = g_indexFreeSkips.fetch_add(1, std::memory_order_relaxed) + 1U;
        if (n <= 64U) {
            std::array<char, 128> line{};
            const int len = std::snprintf(line.data(), line.size(),
                "ev=omega_teardown stage=indexfree_skip index=0x%08X n=%u mutation=skip_absent_unregister",
                index, n);
            if (len > 0) {
                core::log::write(core::log::Channel::client, core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(len)});
            }
        }
        return;
    }
    if (original != nullptr) {
        original(list, index);
    }
}

__declspec(noinline) void __fastcall scene_destructor(std::uint32_t* component) noexcept {
    const SceneDestructor original = g_destructorOriginal.load(std::memory_order_acquire);
    if (component == nullptr) {
        if (original != nullptr) {
            original(component);
        }
        return;
    }
    // Diagnostic only. The original destructor has always been forwarded here;
    // a failed diagnostic resolve must not introduce a new teardown bypass.
    const std::byte* const obj = resolve_destructor_object(component);
    if (obj == nullptr) {
        const std::uint32_t skips = g_destructorSkips.fetch_add(1, std::memory_order_relaxed) + 1U;
        if (skips <= 24U) {
            const auto* b = reinterpret_cast<const std::byte*>(component);
            const std::uint32_t f00 = read_value<std::uint32_t>(b + 0x00U);
            const std::uint32_t f24 = read_value<std::uint32_t>(b + 0x24U);
            const std::uint32_t f28 = read_value<std::uint32_t>(b + 0x28U);
            const std::uint32_t f2c = read_value<std::uint32_t>(b + 0x2CU);
            const HandleResolve r00 = resolve_handle_value(f00);
            const HandleResolve r24 = resolve_handle_value(f24);
            const HandleResolve r28 = resolve_handle_value(f28);
            const HandleResolve r2c = resolve_handle_value(f2c);
            std::array<char, 320> dl{};
            const int dn = std::snprintf(dl.data(), dl.size(),
                "ev=omega_teardown stage=fields f00=0x%08X(es%d) f24=0x%08X(es%d) "
                "f28=0x%08X(es%d) f2c=0x%08X(es%d) n=%u",
                f00, r00.esize, f24, r24.esize, f28, r28.esize, f2c, r2c.esize, skips);
            if (dn > 0) {
                core::log::write(core::log::Channel::client, core::log::Level::info,
                                 {dl.data(), static_cast<std::size_t>(dn)});
            }
        }
        if (skips <= 256U) {
            const std::uint32_t handle =
                readable(reinterpret_cast<const std::byte*>(component), 4U) ? component[0] : 0U;
            std::array<char, 176> line{};
            const int length = std::snprintf(
                line.data(), line.size(),
                "ev=omega_teardown stage=resolve_unavailable handle=0x%08X n=%u "
                "mutation=observe_only",
                handle, skips);
            if (length > 0) {
                core::log::write(core::log::Channel::client, core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    if (original != nullptr) {
        original(component);
    }
}

} // namespace

namespace forest_tuner {

State& state() noexcept {
    static State s_state;
    return s_state;
}

bool sensor_present() noexcept {
    return g_forestSensorPtr.load(std::memory_order_acquire) != nullptr;
}

} // namespace forest_tuner

bool install_omega_dialogue_dispatch_probe() noexcept {
    if (g_applyHandle.attached || g_scanHandle.attached || g_dispatchHandle.attached) {
        return true;
    }
    std::byte* const rosterTarget=verified_target(kRosterApplyRva,kRosterApplyPrefix.data(),kRosterApplyPrefix.size());
    std::byte* const bubbleContext=verified_target(0x4294C0U,kBubbleContextPrefix.data(),kBubbleContextPrefix.size());
    std::byte* const bubbleRead=verified_target(0x429BA0U,kBubbleReadPrefix.data(),kBubbleReadPrefix.size());
    std::byte* const applyTarget = verified_target(
        kDialogueApplyRva, kDialogueApplyPrefix.data(), kDialogueApplyPrefix.size());
    std::byte* const scanTarget = verified_target(
        kDialogueScanRva, kDialogueScanPrefix.data(), kDialogueScanPrefix.size());
    std::byte* const dispatchTarget = verified_target(kDialogueDispatchRva,
                                                      kDialogueDispatchPrefix.data(),
                                                      kDialogueDispatchPrefix.size());
    std::byte* const directiveTarget = verified_target(kDirectiveApplyRva,
                                                       kDirectiveApplyPrefix.data(),
                                                       kDirectiveApplyPrefix.size());
    // The gate applier compiles to the same 21-byte prologue as the dialogue applier.
    std::byte* const gateTarget = verified_target(kGateApplyRva,
                                                  kDialogueApplyPrefix.data(),
                                                  kDialogueApplyPrefix.size());
    std::byte* const forestCreateTarget = verified_target(kForestCreateRva,
                                                          kForestCreatePrefix.data(),
                                                          kForestCreatePrefix.size());
    std::byte* const forestApplyTarget = verified_target(kForestApplyRva,
                                                         kForestApplyPrefix.data(),
                                                         kForestApplyPrefix.size());
    std::byte* const forestSenseTarget = verified_target(kForestSenseRva,
                                                         kForestSensePrefix.data(),
                                                         kForestSensePrefix.size());
    std::byte* const forestWorkerCreateTarget =
        verified_target(kForestWorkerCreateRva,
                        kForestWorkerCreatePrefix.data(),
                        kForestWorkerCreatePrefix.size());
    std::byte* const forestWorkerTickTarget = verified_target(kForestWorkerTickRva,
                                                              kForestWorkerTickPrefix.data(),
                                                              kForestWorkerTickPrefix.size());
    std::byte* const forestOwnerAuthorityTarget = verified_target(kForestOwnerAuthoritySetterRva,
        kForestOwnerAuthoritySetterPrefix.data(), kForestOwnerAuthoritySetterPrefix.size());
    std::byte* const indexFreeTarget = verified_target(kIndexFreeRva,
                                                       kIndexFreePrefix.data(),
                                                       kIndexFreePrefix.size());
    std::byte* const recordProcessorTarget = verified_target(kRecordProcessorRva,
                                                             kRecordProcessorPrefix.data(),
                                                             kRecordProcessorPrefix.size());
    std::byte* const seedCheckTarget = verified_target(kSeedCheckRva,
                                                       kSeedCheckPrefix.data(),
                                                       kSeedCheckPrefix.size());
    std::byte* const destructorTarget = verified_target(kSceneDestructorRva,
                                                        kSceneDestructorPrefix.data(),
                                                        kSceneDestructorPrefix.size());
    std::byte* const deviceCh0Target = verified_target(kDeviceChannel0SetterRva,
                                                       kDeviceSetterPrefix.data(),
                                                       kDeviceSetterPrefix.size());
    std::byte* const deviceCh1Target = verified_target(kDeviceChannel1SetterRva,
                                                       kDeviceSetterPrefix.data(),
                                                       kDeviceSetterPrefix.size());
    std::byte* const deviceConfigureTarget = verified_target(kDeviceConfigureRva,
                                                             kDeviceConfigurePrefix.data(),
                                                             kDeviceConfigurePrefix.size());
    if (rosterTarget==nullptr || bubbleContext==nullptr || bubbleRead==nullptr
        || applyTarget == nullptr || scanTarget == nullptr || dispatchTarget == nullptr
        || directiveTarget == nullptr || gateTarget == nullptr
        || deviceCh0Target == nullptr || deviceCh1Target == nullptr
        || deviceConfigureTarget == nullptr || destructorTarget == nullptr
        || indexFreeTarget == nullptr || forestCreateTarget == nullptr
        || forestApplyTarget == nullptr || forestSenseTarget == nullptr
        || forestWorkerCreateTarget == nullptr || forestWorkerTickTarget == nullptr
        || forestOwnerAuthorityTarget == nullptr
        || recordProcessorTarget == nullptr || seedCheckTarget == nullptr) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=omega_dialogue stage=install result=fail reason=target");
        return false;
    }
    const hooking::detour::Spec applySpec{applyTarget,
                                          reinterpret_cast<void*>(&dialogue_apply)};
    const hooking::detour::Spec scanSpec{scanTarget,
                                         reinterpret_cast<void*>(&dialogue_scan)};
    const hooking::detour::Spec dispatchSpec{dispatchTarget,
                                             reinterpret_cast<void*>(&dialogue_dispatch)};
    const hooking::detour::Spec directiveSpec{directiveTarget,
                                              reinterpret_cast<void*>(&directive_apply)};
    const hooking::detour::Spec gateSpec{gateTarget,
                                         reinterpret_cast<void*>(&gate_apply)};
    const hooking::detour::Spec deviceCh0Spec{deviceCh0Target,
                                              reinterpret_cast<void*>(&device_channel0_setter)};
    const hooking::detour::Spec deviceCh1Spec{deviceCh1Target,
                                              reinterpret_cast<void*>(&device_channel1_setter)};
    const hooking::detour::Spec deviceConfigureSpec{
        deviceConfigureTarget, reinterpret_cast<void*>(&device_configure)};
    const hooking::detour::Spec destructorSpec{destructorTarget,
                                               reinterpret_cast<void*>(&scene_destructor)};
    const hooking::detour::Spec forestCreateSpec{forestCreateTarget,
                                                 reinterpret_cast<void*>(&forest_create_hook)};
    const hooking::detour::Spec forestApplySpec{forestApplyTarget,
                                                reinterpret_cast<void*>(&forest_apply_hook)};
    const hooking::detour::Spec forestSenseSpec{forestSenseTarget,
                                                reinterpret_cast<void*>(&forest_sense_hook)};
    const hooking::detour::Spec forestWorkerCreateSpec{
        forestWorkerCreateTarget, reinterpret_cast<void*>(&forest_worker_create_hook)};
    const hooking::detour::Spec forestWorkerTickSpec{
        forestWorkerTickTarget, reinterpret_cast<void*>(&forest_worker_tick_hook)};
    const hooking::detour::Spec indexFreeSpec{indexFreeTarget,
                                              reinterpret_cast<void*>(&index_free_guard)};
    const hooking::detour::Spec recordProcessorSpec{
        recordProcessorTarget, reinterpret_cast<void*>(&record_processor_hook)};
    const hooking::detour::Spec seedCheckSpec{seedCheckTarget,
                                              reinterpret_cast<void*>(&seed_check_hook)};
    const hooking::detour::Spec rosterSpec{rosterTarget,reinterpret_cast<void*>(&roster_apply_hook)};
    if (!hooking::detour::install(applySpec, g_applyHandle)
        || !hooking::detour::install(scanSpec, g_scanHandle)
        || !hooking::detour::install(dispatchSpec, g_dispatchHandle)
        || !hooking::detour::install(directiveSpec, g_directiveHandle)
        || !hooking::detour::install(gateSpec, g_gateHandle)
        || !hooking::detour::install(deviceCh0Spec, g_deviceCh0Handle)
        || !hooking::detour::install(deviceCh1Spec, g_deviceCh1Handle)
        || !hooking::detour::install(deviceConfigureSpec, g_deviceConfigureHandle)
        || !hooking::detour::install(destructorSpec, g_destructorHandle)
        || !hooking::detour::install(indexFreeSpec, g_indexFreeHandle)
        || !hooking::detour::install(forestCreateSpec, g_forestCreateHandle)
        || !hooking::detour::install(forestApplySpec, g_forestApplyHandle)
        || !hooking::detour::install(forestSenseSpec, g_forestSenseHandle)
        || !hooking::detour::install(forestWorkerCreateSpec, g_forestWorkerCreateHandle)
        || !hooking::detour::install(forestWorkerTickSpec, g_forestWorkerTickHandle)
        || !hooking::detour::install(recordProcessorSpec, g_recordProcessorHandle)
        || !hooking::detour::install(seedCheckSpec, g_seedCheckHandle)
        || !hooking::detour::install(rosterSpec,g_rosterHandle)) {
        if (g_applyHandle.attached) {
            (void)hooking::detour::uninstall(g_applyHandle);
        }
        if (g_scanHandle.attached) {
            (void)hooking::detour::uninstall(g_scanHandle);
        }
        if (g_dispatchHandle.attached) {
            (void)hooking::detour::uninstall(g_dispatchHandle);
        }
        if (g_directiveHandle.attached) {
            (void)hooking::detour::uninstall(g_directiveHandle);
        }
        if (g_gateHandle.attached) {
            (void)hooking::detour::uninstall(g_gateHandle);
        }
        if (g_deviceCh0Handle.attached) {
            (void)hooking::detour::uninstall(g_deviceCh0Handle);
        }
        if (g_deviceCh1Handle.attached) {
            (void)hooking::detour::uninstall(g_deviceCh1Handle);
        }
        if (g_deviceConfigureHandle.attached) {
            (void)hooking::detour::uninstall(g_deviceConfigureHandle);
        }
        if (g_destructorHandle.attached) {
            (void)hooking::detour::uninstall(g_destructorHandle);
        }
        if (g_indexFreeHandle.attached) {
            (void)hooking::detour::uninstall(g_indexFreeHandle);
        }
        if (g_forestCreateHandle.attached) {
            (void)hooking::detour::uninstall(g_forestCreateHandle);
        }
        if (g_forestApplyHandle.attached) {
            (void)hooking::detour::uninstall(g_forestApplyHandle);
        }
        if (g_forestSenseHandle.attached) {
            (void)hooking::detour::uninstall(g_forestSenseHandle);
        }
        if (g_forestWorkerCreateHandle.attached) {
            (void)hooking::detour::uninstall(g_forestWorkerCreateHandle);
        }
        if (g_forestWorkerTickHandle.attached) {
            (void)hooking::detour::uninstall(g_forestWorkerTickHandle);
        }
        if (g_recordProcessorHandle.attached) {
            (void)hooking::detour::uninstall(g_recordProcessorHandle);
        }
        if (g_seedCheckHandle.attached) {
            (void)hooking::detour::uninstall(g_seedCheckHandle);
        }
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=omega_dialogue stage=install result=fail reason=attach");
        return false;
    }
    hooking::publish_original(g_rosterOriginal,reinterpret_cast<RosterApply>(g_rosterHandle.original));
    g_bubbleContext.store(reinterpret_cast<BubbleContext>(bubbleContext),std::memory_order_release);
    g_bubbleRead.store(reinterpret_cast<BubbleRead>(bubbleRead),std::memory_order_release);
    g_rosterGate.accept();
    g_applyOriginal.store(reinterpret_cast<DialogueApply>(g_applyHandle.original),
                          std::memory_order_release);
    g_scanOriginal.store(reinterpret_cast<DialogueScan>(g_scanHandle.original),
                         std::memory_order_release);
    g_dispatchOriginal.store(reinterpret_cast<DialogueDispatch>(g_dispatchHandle.original),
                             std::memory_order_release);
    g_directiveOriginal.store(reinterpret_cast<DialogueApply>(g_directiveHandle.original),
                              std::memory_order_release);
    g_gateOriginal.store(reinterpret_cast<DialogueApply>(g_gateHandle.original),
                         std::memory_order_release);
    g_deviceCh0Original.store(reinterpret_cast<DeviceSetter>(g_deviceCh0Handle.original),
                              std::memory_order_release);
    g_deviceCh1Original.store(reinterpret_cast<DeviceSetter>(g_deviceCh1Handle.original),
                              std::memory_order_release);
    g_deviceConfigureOriginal.store(
        reinterpret_cast<DeviceConfigure>(g_deviceConfigureHandle.original),
        std::memory_order_release);
    g_destructorOriginal.store(reinterpret_cast<SceneDestructor>(g_destructorHandle.original),
                               std::memory_order_release);
    g_indexFreeOriginal.store(reinterpret_cast<IndexFree>(g_indexFreeHandle.original),
                              std::memory_order_release);
    g_forestCreateOriginal.store(
        reinterpret_cast<ForestCreateFn>(g_forestCreateHandle.original),
        std::memory_order_release);
    g_forestApplyOriginal.store(reinterpret_cast<ForestPairFn>(g_forestApplyHandle.original),
                                std::memory_order_release);
    g_forestSenseOriginal.store(reinterpret_cast<ForestPairFn>(g_forestSenseHandle.original),
                                std::memory_order_release);
    g_forestWorkerCreateOriginal.store(
        reinterpret_cast<ForestPairFn>(g_forestWorkerCreateHandle.original),
        std::memory_order_release);
    g_forestWorkerTickOriginal.store(
        reinterpret_cast<ForestPairFn>(g_forestWorkerTickHandle.original),
        std::memory_order_release);
    g_forestOwnerAuthoritySetter.store(
        reinterpret_cast<ForestOwnerAuthoritySetter>(forestOwnerAuthorityTarget),
        std::memory_order_release);
    g_recordProcessorOriginal.store(
        reinterpret_cast<RecordProcessorFn>(g_recordProcessorHandle.original),
        std::memory_order_release);
    g_seedCheckOriginal.store(reinterpret_cast<SeedCheckFn>(g_seedCheckHandle.original),
                              std::memory_order_release);
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     "ev=omega_dialogue stage=install result=ok mode=observe+omega_forest_recipe "
                     "targets=+1009B60,+100A180,+10097D0,+1009C00,+10699C0,+DF6BD0,+DF7120,+DF5070,+4E3B40,+FFE820,+10059A0,+FF2F80");
    return true;
}

void* omega_native_device_channel0() noexcept {
    return reinterpret_cast<void*>(g_deviceCh0Original.load(std::memory_order_acquire));
}

void uninstall_omega_dialogue_dispatch_probe() noexcept {
    g_rosterGate.quiesce();
    if(g_rosterHandle.attached) {
        const std::array<hooking::detour::ProtectedCodeEntry,3> protectedCode{{
            {reinterpret_cast<void*>(&roster_apply_hook)},
            {reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
            {reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}}};
        if(hooking::detour::uninstall(g_rosterHandle,protectedCode,roster_idle)
            !=hooking::detour::UninstallResult::removed) { return; }
    }
    g_rosterOriginal.store(nullptr,std::memory_order_release);
    g_bubbleContext.store(nullptr,std::memory_order_release);
    g_bubbleRead.store(nullptr,std::memory_order_release);
    g_retirementRejects.store(0,std::memory_order_release);

    if (g_applyHandle.attached) {
        (void)hooking::detour::uninstall(g_applyHandle);
    }
    if (g_scanHandle.attached) {
        (void)hooking::detour::uninstall(g_scanHandle);
    }
    if (g_dispatchHandle.attached) {
        (void)hooking::detour::uninstall(g_dispatchHandle);
    }
    if (g_directiveHandle.attached) {
        (void)hooking::detour::uninstall(g_directiveHandle);
    }
    if (g_gateHandle.attached) {
        (void)hooking::detour::uninstall(g_gateHandle);
    }
    if (g_deviceCh0Handle.attached) {
        (void)hooking::detour::uninstall(g_deviceCh0Handle);
    }
    if (g_deviceCh1Handle.attached) {
        (void)hooking::detour::uninstall(g_deviceCh1Handle);
    }
    if (g_deviceConfigureHandle.attached) {
        (void)hooking::detour::uninstall(g_deviceConfigureHandle);
    }
    if (g_destructorHandle.attached) {
        (void)hooking::detour::uninstall(g_destructorHandle);
    }
    if (g_indexFreeHandle.attached) {
        (void)hooking::detour::uninstall(g_indexFreeHandle);
    }
    if (g_forestCreateHandle.attached) {
        (void)hooking::detour::uninstall(g_forestCreateHandle);
    }
    if (g_forestApplyHandle.attached) {
        (void)hooking::detour::uninstall(g_forestApplyHandle);
    }
    if (g_forestSenseHandle.attached) {
        (void)hooking::detour::uninstall(g_forestSenseHandle);
    }
    if (g_forestWorkerCreateHandle.attached) {
        (void)hooking::detour::uninstall(g_forestWorkerCreateHandle);
    }
    if (g_forestWorkerTickHandle.attached) {
        (void)hooking::detour::uninstall(g_forestWorkerTickHandle);
    }
    if (g_recordProcessorHandle.attached) {
        (void)hooking::detour::uninstall(g_recordProcessorHandle);
    }
    if (g_seedCheckHandle.attached) {
        (void)hooking::detour::uninstall(g_seedCheckHandle);
    }
    g_applyOriginal.store(nullptr, std::memory_order_release);
    g_scanOriginal.store(nullptr, std::memory_order_release);
    g_dispatchOriginal.store(nullptr, std::memory_order_release);
    g_applyCount.store(0, std::memory_order_release);
    g_scanCount.store(0, std::memory_order_release);
    g_lastScanState.store(UINT64_MAX, std::memory_order_release);
    for (std::atomic_uint64_t& fingerprint : g_towerWatchSlotFingerprints) {
        fingerprint.store(UINT64_MAX, std::memory_order_release);
    }
    g_directiveOriginal.store(nullptr, std::memory_order_release);
    g_directiveCount.store(0, std::memory_order_release);
    g_lastDirectiveState.store(UINT64_MAX, std::memory_order_release);
    g_gateOriginal.store(nullptr, std::memory_order_release);
    g_gateCount.store(0, std::memory_order_release);
    g_gateTrace.clear();
    g_deviceCh0Original.store(nullptr, std::memory_order_release);
    g_deviceCh1Original.store(nullptr, std::memory_order_release);
    g_deviceLogCount.store(0, std::memory_order_release);
    g_deviceConfigureOriginal.store(nullptr, std::memory_order_release);
    g_deviceConfigureCount.store(0, std::memory_order_release);
    g_destructorOriginal.store(nullptr, std::memory_order_release);
    g_destructorCount.store(0, std::memory_order_release);
    g_destructorSkips.store(0, std::memory_order_release);
    g_indexFreeOriginal.store(nullptr, std::memory_order_release);
    g_indexFreeSkips.store(0, std::memory_order_release);
    g_forestCreateOriginal.store(nullptr, std::memory_order_release);
    g_forestApplyOriginal.store(nullptr, std::memory_order_release);
    g_forestSenseOriginal.store(nullptr, std::memory_order_release);
    g_forestCreateCount.store(0, std::memory_order_release);
    g_forestApplyCount.store(0, std::memory_order_release);
    g_forestSenseCount.store(0, std::memory_order_release);
    g_forestAuthorityHash.store(0, std::memory_order_release);
    g_forestSenseHash.store(0, std::memory_order_release);
    g_forestDumpBudget.store(12, std::memory_order_release);
    g_forestWorkerCreateOriginal.store(nullptr, std::memory_order_release);
    g_forestWorkerTickOriginal.store(nullptr, std::memory_order_release);
    g_forestOwnerAuthoritySetter.store(nullptr, std::memory_order_release);
    g_forestWorkerCreateCount.store(0, std::memory_order_release);
    g_forestWorkerTickCount.store(0, std::memory_order_release);
    g_forestWorkerLastState.store(0xFFFFFFFFU, std::memory_order_release);
    g_forestSensorPtr.store(nullptr, std::memory_order_release);
    g_recordProcessorOriginal.store(nullptr, std::memory_order_release);
    g_recordProcessorCalls.store(0, std::memory_order_release);
    g_recordProcessorFails.store(0, std::memory_order_release);
    g_seedCheckOriginal.store(nullptr, std::memory_order_release);
    g_seedForceCount.store(0, std::memory_order_release);
    g_vexWallDevice.store(nullptr, std::memory_order_release);
    g_vexWallPushState.store(0, std::memory_order_release);
}

} // namespace dawn::client::hooks::bootflow
