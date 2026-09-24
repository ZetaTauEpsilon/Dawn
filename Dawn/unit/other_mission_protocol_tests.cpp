#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#include "middleware/bap/activity_message/sensor_auth_update.h"
#include "middleware/encoding/bit_reader.h"
#include "state/activity/strike_bond/authority.h"

namespace wire = dawn::middleware::bap::activity_message::sensor_auth_update;
namespace bits = dawn::middleware::encoding::bits;

void check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

#ifdef OMEGA_PORT_LOCAL
// Native 3CA310 calls 351070 with an eight-byte destination. That reader copies
// the raw MSB-first bit stream into bytes, then the caller loads a little-endian
// qword. Decode those bytes explicitly instead of mirroring Writer::write(64).
std::uint64_t native_raw_clock(std::span<const std::byte> packet, std::size_t firstBit) {
    std::uint64_t ticks{};
    for (std::size_t byte = 0; byte < 8; ++byte) {
        std::uint8_t raw{};
        for (std::size_t bit = 0; bit < 8; ++bit) {
            const auto position = firstBit + byte * 8 + bit;
            const auto source = std::to_integer<unsigned>(packet[position / 8]);
            raw = static_cast<std::uint8_t>((raw << 1) | ((source >> (7 - position % 8)) & 1U));
        }
        ticks |= std::uint64_t{raw} << (byte * 8);
    }
    return ticks;
}
void check_gameplay_clock_transport() {
    for (const bool grant : {false, true}) {
        wire::Snapshot snapshot{};
        snapshot.lifetime = 3;
        snapshot.patchEpoch = {0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL};
        snapshot.hasGrant = grant;
        snapshot.grant = {11, 7};
        std::array<std::byte, 4096> baseline{}, packet{};
        std::size_t baselineSize{}, size{};
        check(wire::encode_sensor_auth_update(snapshot, baseline, baselineSize),
            "default clock packet encodes");
        const std::size_t clockBit = wire::kLatchBitWithoutGrant - wire::kActivityTokenWidth
            + (grant ? wire::kBubbleBlockBits : 0U);
        check(native_raw_clock(baseline, clockBit) == 0, "default native clock remains zero");
        for (const auto ticks : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{4712400},
                                std::uint64_t{0x0123456789ABCDEFULL},
                                std::uint64_t{0x8000000000000000ULL}, UINT64_MAX}) {
            snapshot.gameplayClockTicks = ticks;
            check(wire::encode_sensor_auth_update(snapshot, packet, size) && size == baselineSize,
                "native clock does not change packet width");
            check(native_raw_clock(packet, clockBit) == ticks,
                "native raw clock decodes little-endian after optional grant");
            bits::Reader decoded(packet);
            std::uint64_t value{};
            check(decoded.skip(clockBit + 64) && decoded.read(1, value) && value == 1,
                "clock preserves following enable latch");
            bits::Reader before(baseline), after(packet);
            for (std::size_t bit = 0; bit < size * 8; ++bit) {
                std::uint64_t a{}, b{};
                check(before.read(1, a) && after.read(1, b), "compare complete clock packet");
                if (bit < clockBit || bit >= clockBit + 64) {
                    check(a == b, "clock changes no epoch, grant, roster or padding bit");
                }
            }
        }
        snapshot.archiveOmega = true;
        snapshot.gameplayClockTicks = 0;
        check(wire::encode_sensor_auth_update(snapshot, baseline, baselineSize),
            "archive baseline clock packet encodes");
        snapshot.gameplayClockTicks = UINT64_MAX;
        check(wire::encode_sensor_auth_update(snapshot, packet, size)
            && size == baselineSize && packet == baseline,
            "archive Omega ignores non-archive gameplay clock field");
    }
}
void check_one_au_respawns() {
    for(const bool oneAu:{false,true}) for(const bool darkness:{false,true})
        for(const bool otherRestriction:{false,true}) {
            wire::Snapshot snapshot{};snapshot.one_au.enabled=oneAu;
            snapshot.one_au.restricted=darkness;snapshot.nativeRespawnRestricted=otherRestriction;
            std::array<std::byte,4096> packet{};bits::Writer writer(packet);
            check(wire::write_auth_body(writer,snapshot,0x4786C0E0U,13,0,true),
                "participation body encodes respawn policy");
            const bool hasDelay=oneAu || otherRestriction;
            bits::Reader reader(packet);std::uint64_t value{};
            // The last 42 bits are fixed participation fields after the delay.
            check(reader.skip(writer.bit_count()-42-(hasDelay?16:0)-1)
                && reader.read(1,value) && value==(hasDelay?1U:0U),
                "optional revive delay presence retains wire alignment");
            if(hasDelay) check(reader.read(16,value) && value==(oneAu?0x4200U:0x4F80U),
                "1AU always allows three-second respawns; other restricted missions retain thirty seconds");
            check(reader.read(1,value) && value==0 && reader.read(1,value) && value==0
                && reader.read(8,value) && value==128 && reader.read(32,value) && value==0x80000000U,
                "respawn change leaves following participation fields unchanged");
        }
}
#endif

int main() {
#ifdef OMEGA_PORT_LOCAL
    check_gameplay_clock_transport();
    check_one_au_respawns();
#endif
    // A Tower Watch publication must retain its original single dialogue record
    // and target-free directive even if unrelated Omega fields are populated.
    wire::Snapshot snapshot{};
    snapshot.publishAuthoredCueTransition = true;
    snapshot.authoredCueRegistry = 0x12345678U;
    snapshot.authoredDialogueRecord = 4;
    snapshot.authoredDirectiveEvent = 0xAABBCCDDU;
    snapshot.omegaTunnelDialogue = snapshot.omegaVistaDialogue = snapshot.omegaExitDialogue = true;
    snapshot.omegaLairDialogueRequestedMask = (1U << 12) | (1U << 13);
    snapshot.omegaLairDialoguePendingRow = 13;
    snapshot.omegaWaypointRegistry = 0x95FB2E01U;
    snapshot.omegaWaypointIndex = 13;
    std::array<std::byte, 4096> buffer{};
    bits::Writer dialogue(buffer);
    check(wire::write_auth_body(dialogue, snapshot, snapshot.authoredCueRegistry, 53, 2, false),
        "Tower Watch dialogue must encode");
    check(dialogue.bit_count() == 19831, "Tower Watch must retain one active dialogue row");
    bits::Writer directive(buffer);
    check(wire::write_auth_body(directive, snapshot, snapshot.authoredCueRegistry, 68, 0, false),
        "Tower Watch directive must encode");
    check(directive.bit_count() == 4802, "Tower Watch directive width must be unchanged");
    bits::Reader reader(buffer);
    std::uint64_t value{};
    check(reader.skip(717) && reader.read(32, value) && value == 0x811C9DC5U,
        "Omega waypoints must not enter Tower Watch's directive");

    snapshot = {};
    snapshot.publishAuthoredSceneSelector = true;
    snapshot.authoredSceneRegistry = 0x9D8076E4U;
    snapshot.authoredSceneType = 43;
    snapshot.authoredSceneIndex = 5;
    snapshot.authoredSceneSelector = 0x80B82771U;
    snapshot.authoredSceneEntryRegistry = 0x9D8076E4U;
    snapshot.authoredSceneEntryType = 2;
    snapshot.authoredSceneEntryIndex = 6;
    bits::Writer scene(buffer);
    check(wire::write_auth_body(scene, snapshot, snapshot.authoredSceneRegistry, 43, 5, false),
        "Tower Watch authored Scene must encode");
    bits::Reader sceneReader(buffer);
    check(sceneReader.read(32, value) && value == 0x00B82771U,
        "The existing signed Scene selector must keep its wire bias");
    // Exercise the complete shared writer, not just the mission-local serializer.
    namespace garden=dawn::state::activity::strike_bond;
    snapshot={};snapshot.strike_bond.enabled=true;snapshot.strike_bond.spawnGeneration=128;
    for(const bool restricted:{true,false}) {
        snapshot.strike_bond.restricted=restricted;
        bits::Writer director(buffer);
        check(wire::write_auth_body(director,snapshot,0x4786C0E0U,35,1,false) && director.bit_count()==359,
            "Garden respawn director size and write paths agree");
        bits::Reader state(buffer);check(state.read(1,value) && value==(restricted?1U:0U),"Garden restriction writes explicit on/off");
        bits::Writer lifetime(buffer);
        check(wire::write_auth_body(lifetime,snapshot,0x4786C0E0U,17,3,false),"Garden lifetime filter writes");
        bits::Reader filter(buffer);check(filter.skip(72) && filter.read(32,value) && value==(restricted?0x80000011U:0x80000000U),
            "Garden restriction targets authored Spire bubble 17");
    }
    for(const auto& golem:garden::kGolems) {
        const auto lens=garden::kLenses[golem.lens].source;
        auto& native=snapshot.strike_bond.native[garden::asset_index(lens)];native.managed=native.active=true;
        bits::Writer effect(buffer);
        check(wire::write_auth_body(effect,snapshot,golem.registry,26,golem.tether,false) && effect.bit_count()==186,
            "native Garden shield body is routed through shared codec");
        bits::Reader armed(buffer);check(armed.skip(1) && armed.read(1,value) && value==0,"live cube holds Minotaur shield");
        snapshot.strike_bond.lensDestroyed.set(golem.lens);
        bits::Writer off(buffer);
        check(wire::write_auth_body(off,snapshot,golem.registry,26,golem.tether,false),"destroyed Garden cube updates effect");
        bits::Reader disabled(buffer);check(disabled.skip(1) && disabled.read(1,value) && value==1,"real cube death disables only linked shield");
        bits::Writer collection(buffer);
        check(wire::write_auth_body(collection,snapshot,golem.registry,34,golem.collection,false) && collection.bit_count()==94,
            "Garden collection selector is a separate native body");
    }
    std::puts("PASS: native clock transport, Tower Watch isolation, Garden shields and respawn protocol");
}
