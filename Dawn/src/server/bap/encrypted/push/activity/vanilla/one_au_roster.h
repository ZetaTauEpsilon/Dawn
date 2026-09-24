#pragma once
#include <algorithm>
#include "../../../../../../state/activity/vanilla/one_au/native_catalog.h"
#include "../../../../../../state/activity/vanilla/one_au/cinematics.h"
#include "../../../../../../state/build_data/scenarios/definition.h"
#include "../../../../../../middleware/bap/activity_message/sensor_auth_update.h"
namespace dawn::server::bap::encrypted::push::activity::one_au_roster {
namespace native=state::activity::vanilla::one_au;
namespace layouts=state::build_data::scenarios;
namespace wire=middleware::bap::activity_message::sensor_auth_update;
[[nodiscard]] inline bool matches(const layouts::RosterGroup& actual,const native::Group& expected) noexcept {
    if(actual.registryKey!=expected.key || actual.objectTag!=expected.tag
        || actual.slotCount!=expected.slots.size() || actual.slotCount>actual.slotTypes.size()) { return false; }
    for(const auto& s:expected.slots) {
        unsigned count{};
        for(std::size_t i=0;i<actual.slotCount;++i) {
            if(actual.slotIndices[i]!=s.asset.slot) { continue; }
            if(actual.slotTypes[i]!=s.asset.type || actual.slotFlags[i]!=native::slot_flags(s) || actual.descriptorTags[i]!=s.asset.definition
                || actual.descriptorOffsets[i]!=s.offset || actual.componentClasses[i]!=s.component
                || actual.senseSchemas[i]!=s.sense || actual.authSchemas[i]!=s.authority) { return false; }
            ++count;
        }
        if(count!=1) { return false; }
    }
    return true;
}
// Preserve the existing wire capacity. C++ owns directive triggers; groups
// containing only type-31 trigger callbacks have no authority to publish.
// Native Scene cast, objects, generators and shared services remain registered.
inline constexpr bool required(const native::Group& group) noexcept {
    if(group.topLevel) {return true;}
    for(const auto& slot:group.slots) {switch(slot.asset.type) {case 1:case 4:case 23:case 26:case 43:case 65:return true;}}return false;
}
inline constexpr std::size_t kRequiredGroups=[] { std::size_t n{};for(const auto& g:native::kGroups) { if(required(g)) { ++n; } }return n; }();
static_assert(kRequiredGroups+2+3<=wire::kGroupCapacity); // roots and all three bookends
inline constexpr std::uint64_t kRootBubbles=0x1E1ULL;
template<class FindGroup>
[[nodiscard]] bool prepare_layout(layouts::Definition& layout,FindGroup find) noexcept {
    if(layout.tag!=native::kScenario || layout.nameLength!=13
        || std::string_view(layout.name.data(),layout.nameLength)!="mission_ember"
        || layout.bubbleCount!=9) { return false; }
    const native::Group* root{};
    for(const auto& group:native::kGroups) { if(group.key==native::kRoot) { if(root) { return false; }root=&group; } }
    if(!root) { return false; }
    layouts::RosterGroup verified{};std::size_t index=root->hint;
    bool resolved=find(index,verified) && matches(verified,*root);
    for(std::size_t i=0;!resolved && i<layouts::kRosterGroupCapacity;++i) {
        if(!find(i,verified)) { break; }
        if(matches(verified,*root)) { index=i;resolved=true; }
    }
    if(!resolved || index>=layouts::kRosterGroupCapacity) { return false; }
    layout.authoredGroupCounts={};layout.authoredGroups={};
    for(std::size_t bubble=0;bubble<layout.bubbleCount;++bubble) {
        if((kRootBubbles&(1ULL<<bubble))==0) { continue; }
        layout.authoredGroupCounts[bubble]=1;
        layout.authoredGroups[bubble][0]=static_cast<std::uint16_t>(index);
    }
    return true;
}
[[nodiscard]] inline bool wire_matches(const wire::Group& actual,const native::Group& expected) noexcept {
    if(actual.key!=expected.key || actual.slotTypes.size()!=expected.slots.size()
        || actual.slotFlags.size()!=expected.slots.size() || actual.slotIndices.size()!=expected.slots.size()) { return false; }
    for(const auto& s:expected.slots) {
        unsigned count{};
        for(std::size_t i=0;i<actual.slotTypes.size();++i) {
            if(actual.slotIndices[i]==s.asset.slot) {
                if(actual.slotTypes[i]!=s.asset.type || actual.slotFlags[i]!=native::slot_flags(s)) { return false; }
                ++count;
            }
        }
        if(count!=1) { return false; }
    }
    return true;
}
// The caller discards the entire scratch snapshot on failure. No cache mutation,
// no pointers to temporary storage, and no group changes at encounter boundaries.
template<class Storage,class FindGroup>
[[nodiscard]] bool admit(const layouts::Definition& layout,Storage& storage,wire::Roster& roster,FindGroup find,std::uint32_t* failedKey=nullptr) noexcept {
    if(failedKey) { *failedKey=0; }
    if(layout.tag!=native::kScenario || layout.nameLength>layout.name.size()
        || std::string_view(layout.name.data(),layout.nameLength)!="mission_ember"
        || layout.bubbleCount!=9
        || roster.groupCount>roster.groups.size() || roster.topLevelGroupCount>roster.groupCount) { return false; }
    for(const auto& expected:native::kGroups) {
        if(!required(expected)) { continue; }
        if(failedKey) { *failedKey=expected.key; }
        std::size_t foundIndex=roster.groupCount;unsigned count{};
        for(std::size_t i=0;i<roster.groupCount;++i) {
            if(roster.groups[i].key==expected.key) { foundIndex=i;++count; }
        }
        if(count>1) { return false; }
        // All top-level services must already come from the normal native layout.
        if(expected.topLevel && (count!=1 || foundIndex>=roster.topLevelGroupCount)) { return false; }
        if(!expected.topLevel && count && foundIndex<roster.topLevelGroupCount) { return false; }
        if(roster.groupCount>=storage.rosterGroups.size()) { return false; }
        // For an existing group use a temporary; don't overwrite scratch backing
        // another published group's spans. The temporary never escapes.
        layouts::RosterGroup check{};
        auto& group=count?check:storage.rosterGroups[roster.groupCount];
        bool resolved=find(expected.hint,group) && matches(group,expected);
        for(std::size_t i=0;!resolved && i<layouts::kRosterGroupCapacity;++i) {
            if(!find(i,group)) { break; }
            resolved=matches(group,expected);
        }
        if(!resolved) { return false; }
        std::size_t block=roster.bubbleSubBlocks.size();unsigned keys{},blocks{};
        for(std::size_t i=0;i<roster.bubbleSubBlocks.size();++i) {
            const auto& b=roster.bubbleSubBlocks[i];
            if(b.bubble==expected.bubble) { block=i;++blocks; }
            for(const auto key:b.keys) {
                if(key==expected.key) { if(expected.topLevel || b.bubble!=expected.bubble) { return false; } ++keys; }
            }
        }
        if(blocks>1 || keys>1 || (count && keys!=(expected.topLevel?0U:1U)) || (!count && keys)) { return false; }
        if(count) { if(!wire_matches(roster.groups[foundIndex],expected)) { return false; }continue; }
        const auto before=block<roster.bubbleSubBlocks.size()?roster.bubbleSubBlocks[block].keys.size():0;
        if(roster.groupCount>=roster.groups.size() || block>=storage.rosterSubBlocks.size()
            || block>=storage.rosterSubBlockKeys.size() || before>=storage.rosterSubBlockKeys[block].size()) { return false; }
        auto& keyStorage=storage.rosterSubBlockKeys[block];
        if(before && roster.bubbleSubBlocks[block].keys.data()!=keyStorage.data()) {
            std::copy(roster.bubbleSubBlocks[block].keys.begin(),roster.bubbleSubBlocks[block].keys.end(),keyStorage.begin());
        }
        const auto blockCount=roster.bubbleSubBlocks.size()+(block==roster.bubbleSubBlocks.size()?1:0);
        keyStorage[before]=expected.key;storage.rosterSubBlocks[block]={expected.bubble,std::span(keyStorage).first(before+1)};
        roster.groups[roster.groupCount++]={group.registryKey,std::span(group.slotTypes).first(group.slotCount),
            std::span(group.slotFlags).first(group.slotCount),std::span(group.slotIndices).first(group.slotCount)};
        roster.bubbleSubBlocks=std::span(storage.rosterSubBlocks).first(blockCount);
    }
    if(failedKey) { *failedKey=0; }
    return true;
}
// Stable key ordinals throughout intro, gameplay and both ending states. Presence
// retires a source without dropping its old ordinal from the native removal walk.
inline constexpr auto kMoviePresence=[] {
    std::array<std::array<std::array<std::uint8_t,wire::kBubbleKeyCapacity>,4>,2> masks{};
    for(std::size_t regular=0;regular<2;++regular) for(std::size_t tail=0;tail<4;++tail) {
        auto& mask=masks[regular][tail];mask.fill(static_cast<std::uint8_t>(regular));
        mask[mask.size()-2]=static_cast<std::uint8_t>((tail>>1)&1);mask.back()=static_cast<std::uint8_t>(tail&1);
    }
    return masks;
}();
inline constexpr std::array<std::uint8_t,wire::kBubbleKeyCapacity> kRetired{};
template<class Storage>
bool movies(Storage& storage,wire::Roster& roster,const native::cinematics::State& state) noexcept {
    namespace cine=native::cinematics;
    for(const auto& movie:cine::kMovies) {
        if(roster.groupCount>=roster.groups.size() || roster.groupCount>=storage.rosterGroups.size()) {return false;}
        for(std::size_t i=0;i<roster.groupCount;++i) {if(roster.groups[i].key==movie.registry) {return false;}}
        auto& group=storage.rosterGroups[roster.groupCount];group={};group.registryKey=movie.registry;group.objectTag=movie.object;
        group.slotCount=1;group.slotTypes[0]=6;group.slotFlags[0]=2;group.slotIndices[0]=0;
        group.descriptorTags[0]=movie.definition;group.descriptorOffsets[0]=0x2E8;
        group.componentClasses[0]=0x80804F06;group.senseSchemas[0]=UINT32_MAX;group.authSchemas[0]=0x80804F08;
        roster.groups[roster.groupCount++]={group.registryKey,std::span(group.slotTypes).first(1),std::span(group.slotFlags).first(1),std::span(group.slotIndices).first(1)};
        std::size_t block=roster.bubbleSubBlocks.size();
        for(std::size_t i=0;i<roster.bubbleSubBlocks.size();++i) {if(roster.bubbleSubBlocks[i].bubble==movie.bubble) {block=i;}}
        if(block>=storage.rosterSubBlocks.size()) {return false;}
        const auto count=block<roster.bubbleSubBlocks.size()?roster.bubbleSubBlocks[block].keys.size():0;
        auto& keys=storage.rosterSubBlockKeys[block];if(count>=keys.size()) {return false;}
        if(count && roster.bubbleSubBlocks[block].keys.data()!=keys.data()) {std::copy(roster.bubbleSubBlocks[block].keys.begin(),roster.bubbleSubBlocks[block].keys.end(),keys.begin());}
        keys[count]=movie.registry;storage.rosterSubBlocks[block]={movie.bubble,std::span(keys).first(count+1)};
        roster.bubbleSubBlocks=std::span(storage.rosterSubBlocks).first((std::max)(roster.bubbleSubBlocks.size(),block+1));
    }
    const std::size_t regular=state.ending()?0:1;
    const bool selected=state.phase==cine::Phase::preparing || state.phase==cine::Phase::offered
        || state.phase==cine::Phase::playing || state.phase==cine::Phase::stopping;
    for(std::size_t i=0;i<roster.bubbleSubBlocks.size();++i) {
        auto& block=storage.rosterSubBlocks[i];const auto count=block.keys.size();if(count>wire::kBubbleKeyCapacity) {return false;}
        if(block.bubble==0) {
            const std::size_t tail=selected && state.movie==1?2:selected && state.movie==2?1:0;
            block.presence=std::span(kMoviePresence[regular][tail]).last(count);
        } else if(block.bubble==6) {
            const std::size_t tail=(regular?2U:0U)+(selected && state.movie==0?1U:0U);
            block.presence=std::span(kMoviePresence[regular][tail]).last(count);
        } else if(!regular) {block.presence=std::span(kRetired).first(count);}
    }
    return true;
}
}
