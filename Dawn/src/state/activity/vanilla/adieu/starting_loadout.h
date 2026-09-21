#pragma once
#include "../../../account/account_state.h"
#include <algorithm>

namespace dawn::state::activity::vanilla::adieu::starting_loadout {
// User-selected kinetic read from the active character on 2026-09-20.
// Installed row 2509: 81319EAD, display 8132973F. Multiple variants share
// the name Traveler's Chosen (Damaged); retain this exact item identity.
inline constexpr std::uint32_t kSidearm=53159281U;
enum class ItemKind { unknown,weapon,other };

// Resolve every carried item before replacing the character. An unavailable
// definition must leave the existing loadout intact so startup can retry.
template<class Classify>
bool prepare(CharacterState& character,const account::inventory::Item& sidearm,Classify classify) noexcept {
    if(character.inventory.count>character.inventory.values.size() || !sidearm.instanceSoid
        || sidearm.definitionHash!=kSidearm || sidearm.quantity!=1 || sidearm.postmaster) return false;
    auto after=character;
    std::size_t kept{};
    for(std::size_t i=0;i<character.inventory.count;++i) {
        const auto& item=character.inventory.values[i];
        const auto kind=classify(item.definitionHash);
        if(kind==ItemKind::unknown) return false;
        if(kind==ItemKind::other) after.inventory.values[kept++]=item;
    }
    std::fill(after.inventory.values.begin()+kept,after.inventory.values.end(),account::inventory::Item{});
    after.inventory.count=kept;
    using Slot=account::inventory::EquipmentSlot;
    after.equipment.slots[static_cast<std::size_t>(Slot::kinetic)]=sidearm;
    after.equipment.slots[static_cast<std::size_t>(Slot::energy)].reset();
    after.equipment.slots[static_cast<std::size_t>(Slot::heavy)].reset();
    character=after;
    return true;
}

// Region changes, respawns and inventory refreshes share a run. Only a new
// mission run may reset the loadout again; a failed commit remains retryable.
class Once final {
public:
    bool applied(std::uint64_t run) const noexcept {return run && run==run_;}
    template<class Commit> bool apply(std::uint64_t run,Commit commit) noexcept {
        if(!run || applied(run) || !commit()) return false;
        run_=run;return true;
    }
private:
    std::uint64_t run_{};
};
}
