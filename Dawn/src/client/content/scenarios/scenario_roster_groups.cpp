#include <array>
#include <algorithm>
#include <cstdio>

#include "../../../core/logging/log.h"
#include "../../../core/settings/settings.h"
#include "../../../middleware/content/packages/tables/roster_intersection.h"
#include "../../../middleware/content/packages/tables/scenario_reader.h"
#include "../../../middleware/content/packages/tables/slot_descriptor_reader.h"
#include "../../../state/build_data/scenarios/omega_schema_catalog.h"
#include "../../../state/activity/coo/mercury_registries.h"
#include "../../../state/activity/coo/mercury_ambient_primary_owner.h"
#include "../../../state/activity/coo/mercury_public_event_registries.h"
#include "../../../state/activity/vendors/catalog.h"
#include "../../../state/activity/coo/open_world_catalog.h"
#include "../../../state/activity/coo/lost_sector_group_catalog.h"
#include "../../../state/activity/coo/edz_moon_lost_sector_group_catalog.h"
#include "../../../state/activity/vanilla/homecoming/registries.h"
#include "../../../state/activity/vanilla/adieu/registries.h"
#include "internal.h"
#include "campaign_shared_groups.h"

namespace dawn::client::content::scenarios {
namespace {

namespace tables = middleware::content::packages::tables;

[[nodiscard]] const char* disposition_name(ResolveDisposition disposition) noexcept {
    switch (disposition) {
    case ResolveDisposition::resolved:
        return "resolved";
    case ResolveDisposition::notRelevant:
        return "not_relevant";
    case ResolveDisposition::objectReadFailed:
        return "object_read_failed";
    case ResolveDisposition::objectKeyMissing:
        return "object_key_missing";
    case ResolveDisposition::bubbleLayoutInvalid:
        return "bubble_layout_invalid";
    case ResolveDisposition::slotsAbsent:
        return "slots_absent";
    case ResolveDisposition::slotCapacity:
        return "slot_capacity";
    case ResolveDisposition::descriptorWalkFailed:
        return "descriptor_walk_failed";
    case ResolveDisposition::slotFillFailed:
        return "slot_fill_failed";
    case ResolveDisposition::catalogCapacity:
        return "catalog_capacity";
    case ResolveDisposition::none:
    default:
        return "none";
    }
}

void trace_towerfall_resolution(const ResolveContext& context,
                                const ResolvedObject& output) noexcept {
    if (context.scenarioTag != kTowerfallScenarioTag) {
        return;
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=towerfall_extract stage=resolve slice=%u registry=%u object=0x%08X package=0x%04X key=0x%08X mask=0x%016llX root_cues=0x%02X group=%u ordinary=%u root=%u local=%u rejected=%u outcome=%s declared=%llu collected=%zu slot_overflow=%u",
        static_cast<unsigned>(context.slice),
        static_cast<unsigned>(context.registry),
        static_cast<unsigned>(output.objectTag),
        static_cast<unsigned>(tables::package_of(output.objectTag)),
        static_cast<unsigned>(output.registryKey),
        static_cast<unsigned long long>(output.explicitSliceMask),
        static_cast<unsigned>(output.authoredRootCueMask),
        static_cast<unsigned>(output.group),
        output.ordinary ? 1U : 0U,
        output.scenarioRoot ? 1U : 0U,
        output.selectedLocal ? 1U : 0U,
        output.authoredRejected ? 1U : 0U,
        disposition_name(output.disposition),
        static_cast<unsigned long long>(output.declaredSlotCount),
        output.collectedSlotCount,
        output.slotsOverflowed ? 1U : 0U);
    if (written > 0) {
        core::log::write(core::log::Channel::state,
                         output.authoredRejected ? core::log::Level::warn
                                                 : core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

/**
 * Exact mission_scot opening-object keys from the marker-working roster. These tags are unique to
 * Omega, so admitting them through the ordinary intersection cannot affect another destination.
 */
constexpr std::array<std::uint32_t, 4> kForcedAuthoredKeys = {
    0x82FB58B7U, 0xD00142CFU, 0xBA5F26EFU, 0xF7A6CE7FU};

[[nodiscard]] bool forced_authored_key(std::uint32_t key) noexcept {
    if (!core::settings::get().client.rosterForceAuthored) {
        return false;
    }
    for (const std::uint32_t forced : kForcedAuthoredKeys) {
        if (forced == key) {
            return true;
        }
    }
    return false;
}

constexpr std::size_t kChainDepthLimit = 8;
/** Leaves the old measured ordinary catalog available even if authored discovery grows. */
constexpr std::size_t kOrdinaryGroupReserve = 128;
static_assert(kOrdinaryGroupReserve < layouts::kRosterGroupCapacity);

bool collect_slot(void* context, const tables::SlotDescriptor& descriptor) noexcept {
    record_slot(*static_cast<RosterStorage*>(context), descriptor);
    return true;
}

[[nodiscard]] bool follow_handle(const reader::Source& source,
                                 reader::Scratch& scratch,
                                 RosterStorage& storage,
                                 std::uint32_t handle,
                                 std::uint32_t registryKey) noexcept {
    std::uint32_t tag = handle;
    for (std::size_t depth = 0; depth < kChainDepthLimit; ++depth) {
        std::uint32_t classId = 0;
        ++storage.reads;
        if (!reader::read_tag(source, scratch, tag, storage.chain, classId)) {
            // ObjectBubble handles also name host-only objects. Those tags are not guaranteed to
            // materialize in the client package reader, and the working traversal treated an
            // unread handle as a non-descriptor rather than rejecting its whole roster group.
            return true;
        }
        if (classId == tables::kPlacedObjectClass) {
            return tables::visit_slot_descriptors(
                storage.chain, tag, registryKey, &collect_slot, &storage);
        }
        if (classId != tables::kSlotIndirectClass && classId != tables::kSlotRedirectClass) {
            // A handle may terminate in a host-only/non-descriptor object. That is a successful
            // walk with no client slot, not evidence that a package read failed.
            return true;
        }
        std::uint32_t next = 0;
        if (!tables::next_descriptor_tag(storage.chain, classId, next)) {
            return true;
        }
        tag = next;
    }
    // A chain that does not reach a client descriptor contributes no client slot. Other handles
    // on the same object can still provide the complete descriptor-backed layout.
    return true;
}

[[nodiscard]] bool collect_descriptors(const reader::Source& source,
                                       reader::Scratch& scratch,
                                       RosterStorage& storage,
                                       std::span<const std::byte> objectBlob,
                                       std::uint32_t registryKey) noexcept {
    tables::Array bubbles{};
    if (!tables::object_bubbles(objectBlob, bubbles)) {
        return true;
    }
    for (std::uint64_t index = 0; index < bubbles.count; ++index) {
        tables::ObjectBubble bubble{};
        if (!tables::object_bubble_at(objectBlob, bubbles, index, bubble)) {
            return false;
        }
        for (std::uint64_t slot = 0; slot < bubble.handleCount; ++slot) {
            std::uint32_t handle = 0;
            if (!tables::object_placed_handle_at(objectBlob, bubble, slot, handle)) {
                return false;
            }
            if (!follow_handle(source, scratch, storage, handle, registryKey)) {
                return false;
            }
        }
    }
    return true;
}

/** @return Explicit non-global ObjectBubble indices declared by one object. */
[[nodiscard]] bool explicit_slice_mask(std::span<const std::byte> objectBlob,
                                       std::uint64_t& mask) noexcept {
    mask = 0;
    tables::Array bubbles{};
    if (!tables::object_bubbles(objectBlob, bubbles)) {
        // ObjectBubble is optional. Ordinary roster objects commonly omit the field entirely;
        // absence means the object declares no explicit slice, not that its package read failed.
        return true;
    }
    for (std::uint64_t index = 0; index < bubbles.count; ++index) {
        tables::ObjectBubble bubble{};
        if (!tables::object_bubble_at(objectBlob, bubbles, index, bubble)) {
            return false;
        }
        if (bubble.bubbleIndex >= 0
            && static_cast<std::uint64_t>(bubble.bubbleIndex) < layouts::kBubbleCapacity) {
            mask |= std::uint64_t{1} << static_cast<std::uint32_t>(bubble.bubbleIndex);
        }
    }
    return true;
}

/**
 * Reads the primary-registry control slots that identify an authored mission-global object.
 * Omega's proven root carries all three; type 68 is the unambiguous directive/objective root cue.
 */
[[nodiscard]] bool authored_root_cue_mask(std::span<const std::byte> objectBlob,
                                          std::uint8_t& mask) noexcept {
    constexpr std::uint32_t kDialogueSlot = 11;
    constexpr std::uint32_t kMusicSlot = 53;
    constexpr std::uint32_t kDirectiveSlot = 68;
    mask = 0;
    tables::Array slots{};
    if (!tables::object_slots(objectBlob, slots)) {
        return true;
    }
    for (std::uint64_t index = 0; index < slots.count; ++index) {
        tables::Slot slot{};
        if (!tables::object_slot_at(objectBlob, slots, index, slot)) {
            return false;
        }
        if (slot.type == kDialogueSlot) {
            mask |= 0x01U;
        } else if (slot.type == kMusicSlot) {
            mask |= 0x02U;
        } else if (slot.type == kDirectiveSlot) {
            mask |= 0x04U;
        }
    }
    return true;
}

void classify(const ObjectMemo& memo,
              std::uint32_t objectTag,
              const ResolveContext& context,
              bool& scenarioRoot,
              bool& selectedLocal) noexcept {
    // The measured Omega groups predate the authored-overlay model and must retain their proven
    // destination intersection/mask. Do not publish a second authored-overlay copy of them.
    if (forced_authored_key(memo.registryKey)) {
        scenarioRoot = false;
        selectedLocal = false;
        return;
    }
    const bool legacyHashRoot =
        context.scenarioHash != 0 && memo.registryKey == context.scenarioHash;
    const bool directiveRoot =
        tables::package_of(objectTag) == context.scenarioPackage
        && context.scenarioTag != 0x80F47522U
        && (memo.authoredRootCueMask & 0x04U) != 0;
    scenarioRoot = context.registry == 0 && (legacyHashRoot || directiveRoot);
    selectedLocal =
        context.registry == 2
        && tables::package_of(objectTag) == context.scenarioPackage
        && (memo.explicitSliceMask & (std::uint64_t{1} << context.slice)) != 0;
    selectedLocal = selectedLocal || campaign_shared::selected(context.scenarioTag,objectTag,
        memo.registryKey,context.registry,context.slice,memo.explicitSliceMask);
}

[[nodiscard]] bool potentially_authored(const ResolveContext& context,
                                        std::uint32_t objectTag) noexcept {
    return context.registry == 0
           || (context.registry == 2
               && (tables::package_of(objectTag) == context.scenarioPackage
                   || campaign_shared::candidate(context.scenarioTag,objectTag)));
}

void reject_authored_read(RosterStorage& storage,
                          const ResolveContext& context,
                          std::uint32_t objectTag,
                          ResolvedObject& output) noexcept {
    if (potentially_authored(context, objectTag)) {
        output.authoredRejected = true;
        ++storage.authoredReadFailures;
    }
}

[[nodiscard]] std::size_t memo_slot(const RosterStorage& storage, std::uint32_t tag) noexcept {
    std::size_t probe = tag % kObjectMemoCapacity;
    for (std::size_t step = 0; step < kObjectMemoCapacity; ++step) {
        if (storage.memo[probe].tag == 0 || storage.memo[probe].tag == tag) {
            return probe;
        }
        probe = (probe + 1) % kObjectMemoCapacity;
    }
    return kObjectMemoCapacity;
}

} // namespace

std::uint8_t measured_omega_key_bit(std::uint32_t key) noexcept {
    if (!core::settings::get().client.rosterForceAuthored) {
        return 0;
    }
    for (std::size_t index = 0; index < kForcedAuthoredKeys.size(); ++index) {
        if (kForcedAuthoredKeys[index] == key) {
            return static_cast<std::uint8_t>(1U << index);
        }
    }
    return 0;
}

bool resolve_object(const reader::Source& source,
                    reader::Scratch& scratch,
                    RosterStorage& storage,
                    std::uint32_t objectTag,
                    const ResolveContext& context,
                    ResolvedObject& output) noexcept {
    output = {};
    output.objectTag = objectTag;
    const std::size_t slot = memo_slot(storage, objectTag);
    if (slot == kObjectMemoCapacity) {
        return false;
    }
    ObjectMemo& memo = storage.memo[slot];
    bool objectLoaded = false;
    if (memo.tag != objectTag) {
        ObjectMemo inspected{};
        inspected.tag = objectTag;
        inspected.group = kNotARosterGroup;
        ++storage.reads;
        if (!reader::read_tag(source, scratch, objectTag, storage.object)) {
            reject_authored_read(storage, context, objectTag, output);
            output.disposition = ResolveDisposition::objectReadFailed;
            trace_towerfall_resolution(context, output);
            return true;
        }
        objectLoaded = true;
        if (!tables::object_key(storage.object, inspected.registryKey)
            || inspected.registryKey == 0) {
            reject_authored_read(storage, context, objectTag, output);
            output.disposition = ResolveDisposition::objectKeyMissing;
            trace_towerfall_resolution(context, output);
            return true;
        }
        inspected.carriesRosterSlot = tables::carries_roster_slot(storage.object)
                                       || forced_authored_key(inspected.registryKey);
        if (!authored_root_cue_mask(storage.object, inspected.authoredRootCueMask)) {
            reject_authored_read(storage, context, objectTag, output);
            output.registryKey = inspected.registryKey;
            output.disposition = ResolveDisposition::slotFillFailed;
            trace_towerfall_resolution(context, output);
            return true;
        }
        if (!explicit_slice_mask(storage.object, inspected.explicitSliceMask)) {
            reject_authored_read(storage, context, objectTag, output);
            output.registryKey = inspected.registryKey;
            output.disposition = ResolveDisposition::bubbleLayoutInvalid;
            trace_towerfall_resolution(context, output);
            return true;
        }
        memo = inspected;
    }

    output.registryKey = memo.registryKey;
    output.explicitSliceMask = memo.explicitSliceMask;
    output.authoredRootCueMask = memo.authoredRootCueMask;

    bool scenarioRoot = false;
    bool selectedLocal = false;
    classify(memo, objectTag, context, scenarioRoot, selectedLocal);
    output.scenarioRoot = scenarioRoot;
    output.selectedLocal = selectedLocal;
    const bool requiredCatalog=std::any_of(
        state::activity::coo::mercury::kRegistries.begin(),
        state::activity::coo::mercury::kRegistries.end(),[&](const auto& definition) noexcept {
            return state::activity::coo::registry::required(definition,context.scenarioTag,
                objectTag,memo.registryKey,memo.explicitSliceMask);
        }) || state::activity::coo::mercury::ambient::primary_owner::required(
            context.scenarioTag,objectTag,memo.registryKey,memo.explicitSliceMask)
        // The optional Crossroads profile needs its complete package catalog even
        // when this encounter is discovered outside a selected local registry.
        // Server admission still owns whether that catalog enters the wire roster.
        || state::activity::coo::mercury::public_events::required(
            context.scenarioTag,objectTag,memo.registryKey,memo.explicitSliceMask)
        || state::activity::coo::open_world::required(
            context.scenarioTag,objectTag,memo.registryKey,memo.explicitSliceMask)
        || state::activity::vanilla::homecoming::registries::required(
              context.scenarioTag,objectTag,memo.registryKey,memo.explicitSliceMask)
        || state::activity::vanilla::adieu::registries::required(
              context.scenarioTag,objectTag,memo.registryKey,memo.explicitSliceMask)
        || state::activity::vendors::required(
            context.scenarioTag,objectTag,memo.registryKey,memo.explicitSliceMask)
        || state::activity::coo::lost_sector::required(
            context.scenarioTag,objectTag,memo.registryKey,memo.explicitSliceMask)
        || state::activity::coo::edz_moon_lost_sector_groups::required(
            context.scenarioTag,objectTag,memo.registryKey,memo.explicitSliceMask);
    if (memo.group != kNotARosterGroup) {
        output.group = memo.group;
        output.ordinary = memo.carriesRosterSlot && memo.completeLayout;
        output.disposition = ResolveDisposition::resolved;
        trace_towerfall_resolution(context, output);
        return true;
    }
    if (!memo.carriesRosterSlot && !scenarioRoot && !selectedLocal && !requiredCatalog) {
        output.disposition = ResolveDisposition::notRelevant;
        trace_towerfall_resolution(context, output);
        return true;
    }
    if (!objectLoaded) {
        ++storage.reads;
        if (!reader::read_tag(source, scratch, objectTag, storage.object)) {
            if (scenarioRoot || selectedLocal) {
                output.authoredRejected = true;
                ++storage.authoredReadFailures;
            }
            output.disposition = ResolveDisposition::objectReadFailed;
            trace_towerfall_resolution(context, output);
            return true;
        }
    }

    tables::Array declared{};
    if (!tables::object_slots(storage.object, declared) || declared.count == 0) {
        output.disposition = ResolveDisposition::slotsAbsent;
        trace_towerfall_resolution(context, output);
        return true;
    }
    output.declaredSlotCount = declared.count;
    if (declared.count > layouts::kRosterSlotCapacity) {
        ++storage.unresolvedGroups;
        if (scenarioRoot || selectedLocal) {
            output.authoredRejected = true;
        }
        output.disposition = ResolveDisposition::slotCapacity;
        trace_towerfall_resolution(context, output);
        return true;
    }
    layouts::RosterGroup candidate{};
    candidate.registryKey = memo.registryKey;
    storage.slotCount = 0;
    storage.slotsOverflowed = false;
    const bool descriptorsComplete =
        collect_descriptors(source, scratch, storage, storage.object, candidate.registryKey);
    output.collectedSlotCount = storage.slotCount;
    output.slotsOverflowed = storage.slotsOverflowed;
    const bool allowPartial =
        scenarioRoot || selectedLocal || requiredCatalog || forced_authored_key(memo.registryKey);
    if (!descriptorsComplete) {
        ++storage.unresolvedGroups;
        if (allowPartial) {
            output.authoredRejected = true;
            ++storage.authoredReadFailures;
        }
        output.disposition = ResolveDisposition::descriptorWalkFailed;
        trace_towerfall_resolution(context, output);
        return true;
    }
    if (!fill_slots(storage, storage.object, declared, candidate, allowPartial)) {
        output.collectedSlotCount = storage.slotCount;
        output.slotsOverflowed = storage.slotsOverflowed;
        ++storage.unresolvedGroups;
        if (allowPartial && (storage.slotsOverflowed || storage.slotCount != 0)) {
            output.authoredRejected = true;
        }
        output.disposition = ResolveDisposition::slotFillFailed;
        trace_towerfall_resolution(context, output);
        return true;
    }
    output.collectedSlotCount = storage.slotCount;
    output.slotsOverflowed = storage.slotsOverflowed;
    // Descriptor-backed slots intentionally omit host-only declarations. Requiring the two counts
    // to match rejects every installed ordinary group even when all client descriptors resolved.
    const bool completeLayout = true;
    candidate.objectTag = objectTag;
    for (std::size_t index = 0; index < storage.slotCount; ++index) {
        const SlotRecord& slotRecord = storage.slots[index];
        state::build_data::scenarios::record_omega_schema({candidate.registryKey,
                                                            candidate.objectTag,
                                                            slotRecord.componentClass,
                                                            slotRecord.senseSchema,
                                                            slotRecord.authSchema,
                                                            slotRecord.index,
                                                            slotRecord.type});
    }
    for (std::size_t index = 0; index < storage.groupCount; ++index) {
        if (same_group_layout(storage.groups[index], candidate)) {
            memo.group = static_cast<std::uint16_t>(index);
            memo.completeLayout = completeLayout;
            output.group = memo.group;
            output.ordinary = memo.carriesRosterSlot && completeLayout;
            output.disposition = ResolveDisposition::resolved;
            trace_towerfall_resolution(context, output);
            return true;
        }
    }
    const bool ordinaryLayout = memo.carriesRosterSlot && completeLayout;
    const std::size_t authoredLimit = layouts::kRosterGroupCapacity - kOrdinaryGroupReserve;
    if (storage.groupCount == layouts::kRosterGroupCapacity
        || (!ordinaryLayout && storage.groupCount >= authoredLimit)) {
        if (scenarioRoot || selectedLocal || requiredCatalog) {
            output.authoredRejected = true;
            ++storage.catalogOverflows;
        }
        if(requiredCatalog) {
            std::array<char,384> line{};
            const auto written=std::snprintf(line.data(),line.size(),
                "ev=required_registry_extract result=failed reason=catalog_capacity scenario=%08X object=%08X key=%08X groups=%zu authored_limit=%zu capacity=%zu",
                context.scenarioTag,objectTag,memo.registryKey,storage.groupCount,authoredLimit,
                layouts::kRosterGroupCapacity);
            if(written>0 && static_cast<std::size_t>(written)<line.size())
                core::log::write(core::log::Channel::state,core::log::Level::error,
                    {line.data(),static_cast<std::size_t>(written)});
        }
        output.disposition = ResolveDisposition::catalogCapacity;
        trace_towerfall_resolution(context, output);
        return true;
    }
    storage.groups[storage.groupCount] = candidate;
    memo.group = static_cast<std::uint16_t>(storage.groupCount);
    memo.completeLayout = completeLayout;
    output.group = memo.group;
    output.ordinary = ordinaryLayout;
    output.disposition = ResolveDisposition::resolved;
    ++storage.groupCount;
    trace_towerfall_resolution(context, output);
    return true;
}

} // namespace dawn::client::content::scenarios
