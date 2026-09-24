#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "fixtures/towerfall_initial_sense_snapshot.h"
#include "fixtures/omega_native_sense_captures.h"
#include "fixtures/omega_native_sense_expectations.h"
#include "fixtures/omega_native_sense_boundaries.h"
#include "middleware/bap/activity_message/sense_update.h"
#include "state/build_data/scenarios/cue_graph_manifest.h"
#include "state/build_data/scenarios/cue_table_decoder.h"

namespace {

namespace sense = dawn::middleware::bap::activity_message::sense_update;
namespace fixtures = dawn::unit::fixtures;
namespace scenarios = dawn::state::build_data::scenarios;

int g_failures{};

void check(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }
    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    ++g_failures;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

[[nodiscard]] std::vector<std::byte> bytes(std::string_view hex) {
    std::vector<std::byte> output;
    output.reserve(hex.size() / 2U);
    const auto nibble = [](char value) -> unsigned {
        return value >= '0' && value <= '9' ? static_cast<unsigned>(value - '0')
             : value >= 'A' && value <= 'F' ? static_cast<unsigned>(value - 'A' + 10)
                                            : 0U;
    };
    for (std::size_t index = 0; index + 1U < hex.size(); index += 2U) {
        output.push_back(static_cast<std::byte>((nibble(hex[index]) << 4U)
                                                | nibble(hex[index + 1U])));
    }
    return output;
}

template <typename Value>
void put(std::vector<std::byte>& output, std::size_t offset, Value value) {
    const auto source = std::as_bytes(std::span{&value, 1U});
    std::copy(source.begin(), source.end(), output.begin() + static_cast<std::ptrdiff_t>(offset));
}

[[nodiscard]] std::vector<std::byte> cue_table_fixture() {
    std::vector<std::byte> output(0xA4U);
    put<std::uint64_t>(output, 0x00, output.size());
    put<std::uint64_t>(output, 0x08, 1U);
    put<std::int64_t>(output, 0x10, 0x10);
    put<std::uint32_t>(output, 0x1C, 0x80809FBDU);
    put<std::uint64_t>(output, 0x20, 1U);
    put<std::uint32_t>(output, 0x28, scenarios::kCueTableRecordClass);

    put<std::uint32_t>(output, 0x30, 0x11111111U);
    put<std::uint32_t>(output, 0x34, scenarios::kCueTableAbsentHash);
    put<std::uint64_t>(output, 0x38, 1U);
    put<std::uint64_t>(output, 0x40, 1U);
    put<std::int64_t>(output, 0x48, 0x28);
    put<std::uint64_t>(output, 0x50, 0x10000U);
    put<std::uint32_t>(output, 0x6C, 0x80809FBDU);
    put<std::uint64_t>(output, 0x70, 1U);
    put<std::uint32_t>(output, 0x78, scenarios::kCueTableValueClass);
    for (std::size_t index = 0; index < scenarios::kCueTableHashCount; ++index) {
        put<std::uint32_t>(output,
                           0x80 + index * 8U,
                           scenarios::kCueTableHashClass);
        put<std::uint32_t>(output,
                           0x84 + index * 8U,
                           index < 2U ? 0x22222222U + static_cast<std::uint32_t>(index)
                                      : scenarios::kCueTableAbsentHash);
    }
    put<std::uint32_t>(output, 0xA0, 1U);
    return output;
}

void decodes_validated_cue_table() {
    std::vector<std::byte> fixture = cue_table_fixture();
    scenarios::CueTable table{};
    CHECK(scenarios::decode_cue_table(fixture, table));
    CHECK(table.recordCount == 1U);
    CHECK(table.valueCount == 1U);
    CHECK(table.records[0].key == 0x11111111U);
    CHECK(table.records[0].domain == scenarios::kCueTableAbsentHash);
    CHECK(table.records[0].enabled == 1U);
    CHECK(table.records[0].flags == 0x10000U);
    CHECK(table.records[0].firstValue == 0U);
    CHECK(table.records[0].valueCount == 1U);
    CHECK(table.values[0].hashes[0].value == 0x22222222U);
    CHECK(table.values[0].hashes[1].value == 0x22222223U);
    CHECK(table.values[0].hashes[2].value == scenarios::kCueTableAbsentHash);
    CHECK(table.values[0].flags == 1U);

    put<std::uint32_t>(fixture, 0x80, 0x80800000U);
    CHECK(!scenarios::decode_cue_table(fixture, table));
}

constexpr std::string_view kTwoGroupPacket =
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF3A002859E0000026FD00142CF3F002920000000200000000"
    "0000000BA002859E7E006240000000400000000000000100";

constexpr std::string_view kForestInitialPacket =
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF24EC7D92E00000E612763EC974D000300000000807F8000000000BFC0000000005FE0000000002FF000000000111BF800000BF8000007FFFFFFF7FFFFFFF7FFFFFFF7FFFFFFF7FFFFFFF000000000100FF00000000017F8000000000BFC0000000005FE0000000002017F0000017F000000FFFFFFFEFFFFFFFEFFFFFFFEFFFFFFFEFFFFFFFE0000000000000000000000000000000000000000000000000000000000000000000000000000000404000400000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000040";

constexpr std::string_view kForestProgressPacket =
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF24EC7D92E00000E612763EC974D000300000000807F8000000000BFC0000000005FE0000000002FF000000000111BF800000BF8000007FFFFFFF7FFFFFFF7FFFFFFF7FFFFFFF7FFFFFFF000000000100FF00000000017F8000000000BFC0000000005FE0000000002017F0000017F000000FFFFFFFEFFFFFFFEFFFFFFFEFFFFFFFEFFFFFFFE0000000000040004000400000000000000000000000000000000000000000000000000000000040404040400000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000180";

constexpr std::string_view kActivationDeltaPacket =
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF33B00EDC80000014B9D8076E40900E9A0000000000000000"
    "000000000200";

constexpr std::string_view kInferredRootDeltaPacket =
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF33B00EDC80000014B9D8076E48B00E9A0000000000000000"
    "000000000200";

void parses_two_native_monitor_bodies() {
    const std::vector<std::byte> packet = bytes(kTwoGroupPacket);
    auto updateStorage=std::make_unique<sense::SenseUpdate>(); auto& update=*updateStorage;
    std::size_t consumed = 0;
    CHECK(packet.size() == 64U);
    CHECK(sense::parse_sense_update(packet, update, consumed));
    CHECK(consumed == 508U);
    CHECK(update.paddingBits == 4U);
    CHECK(update.groupCount == 1U);
    CHECK(update.objectCount == 2U);
    CHECK(update.groups[0].registryKey == 0xD00142CFU);
    CHECK(update.groups[0].bodyBits == 311U);
    CHECK(update.objects[0].slotType == 30U);
    CHECK(update.objects[0].slotIndex == 20U);
    CHECK(update.objects[0].bodyBits == 99U);
    CHECK(update.objects[0].bodyThird == 0U && update.objects[0].bodyFourth == 0U);
    CHECK(update.objects[1].registryKey == 0xD00142CFU);
    CHECK(update.objects[1].slotType == 30U);
    CHECK(update.objects[1].slotIndex == 24U);
    CHECK(update.objects[1].bodyBits == 99U);
    CHECK(update.groups[0].objectCount == 2U);
}

[[nodiscard]] std::uint64_t raw_bits(std::span<const std::byte> packet,
                                    std::size_t start, std::size_t width) {
    std::uint64_t value = 0;
    for (std::size_t bit = start; bit < start + width; ++bit)
        value = (value << 1U) | ((std::to_integer<unsigned>(packet[bit / 8U]) >> (7U-bit%8U)) & 1U);
    return value;
}

void parses_complete_native_omega_corpus() {
    for (const auto& expected : fixtures::kOmegaSenseExpectedPackets) {
        const auto packet = bytes(fixtures::omega_native_sense_capture(static_cast<std::uint16_t>(expected.packet)));
        auto updateStorage=std::make_unique<sense::SenseUpdate>(); auto& update=*updateStorage;
        std::size_t consumed{};
        const bool parsed = sense::parse_sense_update(packet, update, consumed);
        if (!parsed) std::cerr << "native capture packet " << expected.packet << " rejected\n";
        CHECK(parsed);
        if (!parsed) continue;
        CHECK(consumed == expected.consumed);
        CHECK(update.paddingBits == expected.padding);
        CHECK(update.groupCount == expected.groupCount);
        CHECK(update.objectCount == expected.objectCount);
        for (std::size_t group = 0; group < expected.groupCount; ++group) {
            const auto& wanted = fixtures::kOmegaSenseExpectedGroups[expected.firstGroup + group];
            const auto& actual = update.groups[group];
            CHECK(actual.registryKey == wanted.registry);
            CHECK(actual.bodyBits == wanted.bits);
            CHECK(actual.firstObject == wanted.firstObject - expected.firstObject);
            CHECK(actual.objectCount == wanted.objectCount);
            std::size_t accounted = 1U; // Explicit native object-list terminator.
            for (std::size_t index = actual.firstObject; index < actual.firstObject + actual.objectCount; ++index)
                accounted += 56U + update.objects[index].bodyBits;
            CHECK(accounted == wanted.bits);
            CHECK(raw_bits(packet, wanted.end - 1U, 1) == 0U);
            auto malformed = packet;
            const auto endBit = wanted.end - 1U;
            malformed[endBit / 8U] |= static_cast<std::byte>(1U << (7U-endBit%8U));
            auto rejectedStorage=std::make_unique<sense::SenseUpdate>(); auto& rejected=*rejectedStorage; std::size_t rejectedBits{};
            CHECK(!sense::parse_sense_update(malformed, rejected, rejectedBits));
            CHECK(rejectedBits == 0U && rejected.objectCount == 0U);
        }
        for (std::size_t index = 0; index < expected.objectCount; ++index) {
            const auto& wanted = fixtures::kOmegaSenseExpectedObjects[expected.firstObject + index];
            const auto& actual = update.objects[index];
            CHECK(actual.registryKey == wanted.registry);
            CHECK(actual.slotType == wanted.type && actual.slotIndex == wanted.index);
            CHECK(actual.bodyBits == wanted.bits);
            CHECK(actual.hasNativeSchema == (wanted.schema != 0U));
            if (wanted.schema != 0U) {
                CHECK(!actual.inferredBodyWidth);
                CHECK(actual.nativeSchema == wanted.schema);
                CHECK(actual.nativeRevision == wanted.revision);
                CHECK(actual.hasRootDelta == (wanted.root != 0U));
                CHECK(raw_bits(packet, wanted.end - 32U, 32) == wanted.revision);
            }
            const std::array words{actual.bodyFirst,actual.bodySecond,actual.bodyThird,actual.bodyFourth};
            for (std::size_t word = 0; word < words.size(); ++word) {
                const auto offset = word * 64U;
                const auto width = offset < wanted.bits ? (std::min)(64U, wanted.bits - static_cast<unsigned>(offset)) : 0U;
                CHECK(words[word] == raw_bits(packet, wanted.start + offset, width));
            }
            if (wanted.type == 43U) {
                CHECK(actual.hasSceneOutput);
                CHECK(actual.sceneOutput.revision == wanted.revision);
            }
        }
        if (expected.packet == 39U) {
            CHECK(packet.size() == 429U && update.groupCount == 2U && update.objectCount == 22U);
            CHECK(update.groups[0].registryKey == 0x95FB2E01U && update.groups[0].objectCount == 2U);
            CHECK(update.groups[1].registryKey == 0xF4D0E0B2U && update.groups[1].objectCount == 20U);
        }
        for (std::size_t size = 0; size < packet.size(); ++size) {
            auto truncatedStorage=std::make_unique<sense::SenseUpdate>(); auto& truncated=*truncatedStorage; std::size_t truncatedBits{};
            CHECK(!sense::parse_sense_update(std::span(packet).first(size), truncated, truncatedBits));
            CHECK(truncatedBits == 0U && truncated.objectCount == 0U);
        }
    }
}

void parses_reflected_native_schema_boundaries() {
    for (const auto& fixture : fixtures::kOmegaNativeSenseBoundaries) {
        const auto packet = bytes(fixture.hex);
        auto updateStorage=std::make_unique<sense::SenseUpdate>(); auto& update=*updateStorage; std::size_t consumed{};
        const bool parsed = sense::parse_sense_update(packet, update, consumed);
        if (parsed != fixture.valid) std::cerr << "native boundary " << fixture.name << " result differs\n";
        CHECK(parsed == fixture.valid);
        if (!fixture.valid) {
            CHECK(consumed == 0U && update.objectCount == 0U);
            continue;
        }
        if (!parsed) continue;
        CHECK(update.groupCount == 1U && update.objectCount == 2U);
        const auto& first = update.objects[0];
        CHECK(first.slotType == fixture.type && first.slotIndex == 0U);
        CHECK(first.bodyBits == fixture.bits);
        CHECK(first.hasNativeSchema && !first.inferredBodyWidth);
        CHECK(first.hasRootDelta == fixture.root);
        CHECK(first.nativeRevision == 7U);
        const auto& following = update.objects[1];
        CHECK(following.slotType == 43U && following.slotIndex == 9U);
        CHECK(following.bodyBits == 33U && following.hasSceneOutput);
        CHECK(!following.hasRootDelta && following.nativeRevision == 9U);
        if (!fixture.root) CHECK(first.bodyBits == 33U);
        if (fixture.type == 43U && fixture.root) {
            CHECK(first.sceneOutput.eventCount == 32U);
            CHECK(first.sceneOutput.events.front() == UINT32_MAX);
            CHECK(first.sceneOutput.events.back() == UINT32_MAX);
        }
        for (std::size_t size = 0; size < packet.size(); ++size) {
            auto truncatedStorage=std::make_unique<sense::SenseUpdate>(); auto& truncated=*truncatedStorage; std::size_t truncatedBits{};
            CHECK(!sense::parse_sense_update(std::span(packet).first(size), truncated, truncatedBits));
            CHECK(truncatedBits == 0U && truncated.objectCount == 0U);
        }
    }
}

void parses_forest_generator_state() {
    const std::vector<std::byte> packet = bytes(kForestInitialPacket);
    auto updateStorage=std::make_unique<sense::SenseUpdate>(); auto& update=*updateStorage;
    std::size_t consumed = 0;
    CHECK(packet.size() == 255U);
    CHECK(sense::parse_sense_update(packet, update, consumed));
    CHECK(consumed == 2037U);
    CHECK(update.paddingBits == 3U);
    CHECK(update.groupCount == 1U);
    CHECK(update.groups[0].registryKey == 0x2763EC97U);
    CHECK(update.groups[0].bodyBits == 1840U);
    CHECK(update.objectCount == 1U);
    const sense::SenseObject& object = update.objects[0];
    CHECK(object.slotType == 37U);
    CHECK(object.slotIndex == 1U);
    CHECK(object.bodyBits == 1783U);
    CHECK(object.hasForestGeneratorState);
    CHECK(object.forestActive32 == 0U);
    CHECK(object.forestActive64 == ((1ULL << 2U) | (1ULL << 3U) | (1ULL << 5U)));
    CHECK(object.forestRevision == 1U);
}

void parses_forest_progress_and_rejects_truncation() {
    std::vector<std::byte> packet = bytes(kForestProgressPacket);
    auto updateStorage=std::make_unique<sense::SenseUpdate>(); auto& update=*updateStorage;
    std::size_t consumed = 0;
    CHECK(packet.size() == 255U);
    CHECK(sense::parse_sense_update(packet, update, consumed));
    const sense::SenseObject& object = update.objects[0];
    CHECK(object.forestActive32 == ((1U << 0U) | (1U << 2U) | (1U << 4U)));
    CHECK(object.forestActive64
          == ((1ULL << 1U) | (1ULL << 2U) | (1ULL << 3U) | (1ULL << 4U)
              | (1ULL << 5U)));
    CHECK(object.forestRevision == 6U);

    packet.pop_back();
    CHECK(!sense::parse_sense_update(packet, update, consumed));
    CHECK(consumed == 0U);
}

void preserves_108_bit_opaque_activation_delta() {
    const std::vector<std::byte> packet = bytes(kActivationDeltaPacket);
    auto updateStorage=std::make_unique<sense::SenseUpdate>(); auto& update=*updateStorage;
    std::size_t consumed = 0;
    CHECK(packet.size() == 46U);
    CHECK(sense::parse_sense_update(packet, update, consumed));
    CHECK(consumed == 362U);
    CHECK(update.paddingBits == 6U);
    CHECK(update.groupCount == 1U);
    CHECK(update.groups[0].registryKey == 0x9D8076E4U);
    CHECK(update.groups[0].bodyBits == 165U);
    CHECK(update.objectCount == 1U);
    const sense::SenseObject& object = update.objects[0];
    CHECK(object.slotType == 3U);
    CHECK(object.slotIndex == 116U);
    CHECK(object.bodyBits == 108U);
    CHECK(object.bodyFirst == 0xD000000000000000ULL);
    CHECK(object.bodySecond == 0x1ULL);
    CHECK(object.inferredBodyWidth);
    CHECK(!object.hasNativeSchema);
}

void bounds_unknown_root_body_by_group_envelope() {
    const std::vector<std::byte> packet = bytes(kInferredRootDeltaPacket);
    auto updateStorage=std::make_unique<sense::SenseUpdate>(); auto& update=*updateStorage;
    std::size_t consumed = 0;
    CHECK(sense::parse_sense_update(packet, update, consumed));
    CHECK(update.objectCount == 1U);
    const sense::SenseObject& object = update.objects[0];
    CHECK(object.slotType == 68U);
    CHECK(object.slotIndex == 116U);
    CHECK(object.bodyBits == 108U);
    CHECK(object.inferredBodyWidth);
}

void parses_complete_towerfall_initial_snapshot() {
    const std::vector<std::byte> packet = bytes(fixtures::kTowerfallInitialSenseSnapshot);
    auto updateStorage=std::make_unique<sense::SenseUpdate>(); auto& update=*updateStorage;
    std::size_t consumed = 0;
    CHECK(packet.size() == 1900U);
    CHECK(sense::parse_sense_update(packet, update, consumed));
    CHECK(consumed == 15200U);
    CHECK(update.paddingBits == 0U);
    CHECK(update.hasRosterAcknowledgement);
    CHECK(update.topLevelRosterCount == 2U);
    CHECK(update.bubbleBlockCount == 3U);
    CHECK(update.rosterEntryCount == 8U);
    CHECK(update.rosterEntries[0].registryKey == 0x4786C0E0U);
    CHECK(update.rosterEntries[1].registryKey == 0x664128F4U);
    CHECK(update.rosterEntries[1].bubble == -1);
    CHECK(update.rosterEntries[1].active);
    CHECK(update.rosterEntries[1].state == 0x83U);

    CHECK(update.groupCount == 2U);
    CHECK(update.objectCount == 98U);
    CHECK(update.groups[0].registryKey == 0x49624A73U);
    CHECK(update.groups[0].bodyBits == 111U);
    CHECK(update.groups[0].firstObject == 0U);
    CHECK(update.groups[0].objectCount == 1U);
    CHECK(update.groups[1].registryKey == 0x9D8076E4U);
    CHECK(update.groups[1].bodyBits == 13780U);
    CHECK(update.groups[1].firstObject == 1U);
    CHECK(update.groups[1].objectCount == 97U);

    std::size_t monitor54 = 0;
    std::size_t spawner92 = 0;
    std::size_t spawner124 = 0;
    std::size_t spawner156 = 0;
    std::size_t scene75 = 0;
    std::size_t member46 = 0;
    std::size_t mission167 = 0;
    for (std::size_t index = 0; index < update.objectCount; ++index) {
        const sense::SenseObject& object = update.objects[index];
        if (object.slotType == 70U && object.bodyBits == 54U) {
            ++monitor54;
        } else if (object.slotType == 1U && object.bodyBits == 92U) {
            ++spawner92;
            CHECK(!object.inferredBodyWidth);
        } else if (object.slotType == 1U && object.bodyBits == 124U) {
            ++spawner124;
            CHECK(!object.inferredBodyWidth);
        } else if (object.slotType == 1U && object.bodyBits == 156U) {
            ++spawner156;
            CHECK(!object.inferredBodyWidth);
        } else if (object.slotType == 43U && object.bodyBits == 75U) {
            ++scene75;
        } else if (object.slotType == 2U && object.bodyBits == 46U) {
            ++member46;
            CHECK(!object.inferredBodyWidth);
            CHECK(object.hasNativeSchema && object.nativeSchema == 0x80807DA2U);
        } else if (object.slotType == 23U && object.bodyBits == 167U) {
            ++mission167;
        } else {
            CHECK(false);
        }
    }
    CHECK(monitor54 == 2U);
    CHECK(spawner92 == 54U);
    CHECK(spawner124 == 3U);
    CHECK(spawner156 == 1U);
    CHECK(scene75 == 32U);
    CHECK(member46 == 5U);
    CHECK(mission167 == 1U);
}

void maps_complete_towerfall_snapshot_by_stable_node_identity() {
    const std::vector<std::byte> packet = bytes(fixtures::kTowerfallInitialSenseSnapshot);
    auto updateStorage=std::make_unique<sense::SenseUpdate>(); auto& update=*updateStorage;
    std::size_t consumed = 0;
    CHECK(sense::parse_sense_update(packet, update, consumed));

    scenarios::Definition definition{};
    definition.rosterGroupCount = static_cast<std::uint8_t>(update.groupCount);
    std::array<scenarios::RosterGroup, 2> groups{};
    for (std::size_t groupOrdinal = 0; groupOrdinal < update.groupCount; ++groupOrdinal) {
        definition.rosterGroups[groupOrdinal] = static_cast<std::uint16_t>(groupOrdinal);
        const sense::SenseGroup& observedGroup = update.groups[groupOrdinal];
        scenarios::RosterGroup& group = groups[groupOrdinal];
        group.registryKey = observedGroup.registryKey;
        group.objectTag = 0x80000000U + static_cast<std::uint32_t>(groupOrdinal);
        group.slotCount = observedGroup.objectCount;
        for (std::size_t slot = 0; slot < observedGroup.objectCount; ++slot) {
            const sense::SenseObject& object = update.objects[observedGroup.firstObject + slot];
            group.slotTypes[slot] = object.slotType;
            group.slotIndices[slot] = object.slotIndex;
            group.componentClasses[slot] = 0x80800001U;
            group.senseSchemas[slot] = 0x80800002U;
            group.authSchemas[slot] = 0x80800003U;
            group.slotFlags[slot] = scenarios::kSlotSenseFlag | scenarios::kSlotAuthFlag;
        }
    }
    const scenarios::ObservationMappingReport mapped =
        scenarios::map_observations(definition, groups, update);
    CHECK(mapped.observedObjects == 98U);
    CHECK(mapped.mappedObjects == 98U);
    CHECK(mapped.missingObjects == 0U);
    CHECK(mapped.ambiguousObjects == 0U);

    ++groups[1].slotIndices[0];
    const scenarios::ObservationMappingReport missing =
        scenarios::map_observations(definition, groups, update);
    CHECK(missing.mappedObjects == 97U);
    CHECK(missing.missingObjects == 1U);
    CHECK(missing.ambiguousObjects == 0U);
}

#include "partial_roster_cases.h"
} // namespace

int main() {
    partial_roster::run();
    decodes_validated_cue_table();
    parses_two_native_monitor_bodies();
    parses_complete_native_omega_corpus();
    parses_reflected_native_schema_boundaries();
    parses_forest_generator_state();
    parses_forest_progress_and_rejects_truncation();
    preserves_108_bit_opaque_activation_delta();
    bounds_unknown_root_body_by_group_envelope();
    parses_complete_towerfall_initial_snapshot();
    maps_complete_towerfall_snapshot_by_stable_node_identity();
    if (g_failures != 0) {
        std::cerr << g_failures << " activity sense parser check(s) failed\n";
        return 1;
    }
    std::cout << "all activity sense parser checks passed\n";
    return 0;
}
