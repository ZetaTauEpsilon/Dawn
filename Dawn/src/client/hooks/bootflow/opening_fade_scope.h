#pragma once
#include "../../../state/activity/destination/definition.h"
#include "../../../state/activity/forced/prelaunch_profile.h"
#include <array>
#include <cstdint>
#include <string_view>

namespace dawn::client::hooks::bootflow::opening_fade_scope {
enum class Owner : std::uint8_t { none,launchpad,oneAu,homecoming };
inline Owner owner(const state::activity::destination::DestinationSelection& selection) noexcept {
    namespace profiles=state::activity::forced::prelaunch;
    if(selection.packageNameLength>selection.packageName.size()) return Owner::none;
    const std::string_view package{reinterpret_cast<const char*>(selection.packageName.data()),selection.packageNameLength};
    constexpr std::array profilesByOwner{profiles::kLaunchpad,profiles::kOneAu,profiles::kTowerfall};
    for(std::size_t i=0;i<profilesByOwner.size();++i)
        if(selection.activityIndex==profilesByOwner[i].activity && package==profilesByOwner[i].package)
            return static_cast<Owner>(i+1);
    return Owner::none;
}
struct Source {
    void (*arrival)() noexcept;
    bool (*mask)(std::uint64_t) noexcept;
};
// Retained/prepared controllers can share the current run during teardown.
// Only the joined destination may receive the arrival or own the screen mask.
inline bool wanted(Owner owner,const std::array<Source,3>& sources,std::uint64_t now,bool arrival=false) noexcept {
    const auto index=static_cast<std::size_t>(owner);
    if(!index || index>sources.size()) return false;
    const auto& source=sources[index-1];
    if(arrival && source.arrival) source.arrival();
    return source.mask && source.mask(now);
}
}
