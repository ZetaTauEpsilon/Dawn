#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include "../strike_variants.h"

namespace dawn::state::activity::coo::campaign_dialogue {
// The shared 80F1FFB6/80F1FEC2 banks test unlock 51289EB0. Its dense
// 81319321 flag-table index is 4; native evaluated flags use 1=false, 2=true.
inline constexpr std::uint16_t kFlag = 4;
// Homecoming conditions from 80C2AF61 -> 81319324 -> 81319321:
// 63533000 -> 2024 (City/Tower), D12EC6F2 -> 2027 (Amanda's tools),
// EAB8395B -> 2028 (Saladin's Young Wolf). Lease evaluated overrides;
// do not change the character's durable Destiny 1 accomplishments.
inline constexpr std::array<std::uint16_t,4> kFlags{kFlag,2024,2027,2028};
[[nodiscard]] constexpr std::optional<std::uint8_t> value(std::int16_t activity,std::uint16_t flag=kFlag) noexcept {
    if(flag!=kFlag) {
        if(activity==266 && (flag==2024 || flag==2027 || flag==2028)) {return std::uint8_t{2};}
        return std::nullopt;
    }
    if (activity == 296 || activity == 298) { return std::uint8_t{2}; }
    if (strikes::find(activity)) { return std::uint8_t{1}; }
    return std::nullopt;
}
[[nodiscard]] constexpr bool requested(std::int16_t activity) noexcept {
    for(const auto flag:kFlags) {if(value(activity,flag)) {return true;}}return false;
}
struct Flag { std::uint16_t slot{}; std::uint8_t value{}, padding{}; };
struct Flags { std::uint32_t count{}; std::array<Flag,100> rows{}; };
static_assert(sizeof(Flag)==4 && sizeof(Flags)==0x194);
// One game-thread lease. Restore only our flag; preserve unrelated live overrides.
struct Lease {
    bool active{};
    std::optional<std::uint8_t> original{};
    [[nodiscard]] bool apply(Flags& flags, std::int16_t activity, bool& changed,std::uint16_t flag=kFlag) noexcept {
        changed=false;
        if (flags.count>flags.rows.size()) { return false; }
        std::size_t found=flags.count;
        for (std::size_t i=0;i<flags.count;++i) {
            if (flags.rows[i].slot!=flag) { continue; }
            if (found!=flags.count || flags.rows[i].value>2) { return false; }
            found=i;
        }
        auto desired=value(activity,flag);
        if (!desired && !active) { return true; }
        if (desired && !active) {
            if (found==flags.count && flags.count==flags.rows.size()) { return false; }
            original=found==flags.count ? std::nullopt : std::optional{flags.rows[found].value};
        }
        if (!desired) { desired=original; }
        if (desired) {
            if (found==flags.count) {
                if (flags.count==flags.rows.size()) { return false; }
                flags.rows[flags.count++]={flag,*desired,0}; changed=true;
            } else if (flags.rows[found].value!=*desired) {
                flags.rows[found].value=*desired; changed=true;
            }
        } else if (found!=flags.count) {
            for (auto i=found+1;i<flags.count;++i) { flags.rows[i-1]=flags.rows[i]; }
            flags.rows[--flags.count]={}; changed=true;
        }
        active=value(activity,flag).has_value();
        if (!active) { original.reset(); }
        return true;
    }
};
// Apply the whole selection atomically: a full or malformed native override
// array must not leave half of the veteran branches changed.
struct SelectionLease {
    std::array<Lease,kFlags.size()> leases{};bool active{};
    [[nodiscard]] bool apply(Flags& flags,std::int16_t activity,bool& changed) noexcept {
        auto next=flags;auto candidate=leases;changed=false;bool any{},leased{};
        for(std::size_t i=0;i<kFlags.size();++i) {
            bool edit{};if(!candidate[i].apply(next,activity,edit,kFlags[i])) {return false;}
            any|=edit;leased|=candidate[i].active;
        }
        flags=next;leases=candidate;active=leased;changed=any;return true;
    }
};
}
