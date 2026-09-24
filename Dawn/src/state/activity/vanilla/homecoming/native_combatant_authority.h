#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "../../coo/native_combatant_authority.h"

namespace dawn::state::activity::vanilla::homecoming::native_combatant {

/** Optional native type-3 tactical group and zero-based authored row. */
using TacticalGroup = coo::native_combatant::TacticalGroup;

/** Reflected 80807EC9 source authority. The native spawner still owns template
 * selection, placement, request queuing, actor creation and AI initialization.
 * Counts are cumulative loose requests per native category, not members. Homecoming
 * sources declare up to four authored categories; the request array is written for
 * every declared category, which is the 1AU two/three-category encoding extended. */
struct Source final {
    std::uint32_t registry{};
    std::uint32_t generation{};
    std::uint16_t ruleSlot{};
    std::uint8_t categories{1};
    std::array<std::uint8_t,4> requested{};
    TacticalGroup tactical{};
    bool hasRule{true};
    bool memberOwned{};
    bool reserve{};
    std::uint32_t objectiveRevision{};
    bool authoredPlacement{};
    bool overrideRule{};
};
inline constexpr std::size_t kSourceBits = 641;
[[nodiscard]] constexpr std::size_t source_bits(std::uint8_t categories) noexcept {
    return kSourceBits + 32U * (categories > 1 ? categories - 1U : 0U);
}

template<class Writer>
[[nodiscard]] bool write_source(Writer& writer,const Source& source) noexcept {
    if(source.registry==0 || source.registry==0x811C9DC5U
        || source.generation==0 || source.generation>0x7FFFFFFFU || source.objectiveRevision>0x7FFFFFFFU
        || source.categories==0 || source.categories>4
        || (source.overrideRule && !source.hasRule) || (source.hasRule && source.ruleSlot>0x7FFFU)) { return false; }
    unsigned total{};
    for(std::size_t i=0;i<4;++i) {
        if(source.requested[i]>63 || (i>=source.categories && source.requested[i]!=0)) { return false; }
        total+=source.requested[i];
    }
    if(total>63) { return false; }
    const auto& tactical=source.tactical;
    const bool assigned=tactical.registry!=0;
    if(assigned ? (tactical.registry==0 || tactical.registry==0x811C9DC5U
                   || tactical.slot>0x7FFFU || tactical.row < -1 || tactical.row>=24)
                : (tactical.row!=-1 || tactical.registry!=0 || tactical.slot!=0)) { return false; }
    if(source.memberOwned && (total || assigned)) { return false; }
    const auto begin=writer.bit_count();
    const auto absent=[&writer]() noexcept {
        return writer.write(1,1) && writer.write(0x811C9DC5U,32)
            && writer.write(0,7) && writer.write(32767U,16);
    };
    bool ok=(assigned
        ? (writer.write(1,1) && writer.write(tactical.registry,32)
            && writer.write(4,7) && writer.write(32768U+tactical.slot,16))
        : absent()) && absent()
        && writer.write(1,1) && writer.write(0,3)
        && writer.write(1,1) && writer.write(source.categories,4);
    for(std::size_t i=0;ok && i<source.categories;++i) { ok=writer.write(0x80000000U+source.requested[i],32); }
    ok=ok && writer.write(1,1) && writer.write(0,4)
        // Variant 0 is required. -1 would index before six native template arrays.
        // Name tier 0 selects the authored display name; other overrides are absent.
        && writer.write(1,1) && writer.write(1,3)
        && writer.write(1,2) && writer.write(0,8)
        && writer.write(1,1) && writer.write(source.generation,31)
        && writer.write(1,1) && writer.write(0,32)
        && writer.write(1,1) && writer.write(source.memberOwned?0x811C9DC5U:0U,32)
        && absent() && absent()
        && (source.hasRule && (!source.authoredPlacement || source.overrideRule) ? (writer.write(1,1) && writer.write(source.registry,32)
            && writer.write(67,7) && writer.write(32768U+source.ruleSlot,16)) : absent())
        && absent()
        && writer.write(1,1) && writer.write(source.objectiveRevision,31)
        && writer.write(1,1) && writer.write(0,31)
        && writer.write(1,1) && writer.write(source.memberOwned?1U:0U,6)
        && writer.write(1,1) && writer.write(source.memberOwned?1U:assigned?static_cast<std::uint32_t>(tactical.row)+1U:0U,5)
        && writer.write(1,1) && writer.write(source.generation,31)
        && writer.write(source.memberOwned?1U:2U,2) && writer.write(source.reserve?4U:source.authoredPlacement?3U:1U,3)
        && writer.write(1,1) && writer.write(0x811C9DC5U,32);
    return ok && writer.bit_count()-begin==source_bits(source.categories);
}

} // namespace dawn::state::activity::vanilla::homecoming::native_combatant
