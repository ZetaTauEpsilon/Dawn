#pragma once
#include <algorithm>
#include "../../../../../../state/activity/vanilla/homecoming/native_catalog.h"
#include "../../../../../../state/activity/vanilla/homecoming/cinematics.h"
#include "../../../../../../state/build_data/scenarios/definition.h"
#include "../../../../../../middleware/bap/activity_message/sensor_auth_update.h"
namespace dawn::server::bap::encrypted::push::activity::homecoming_roster {
namespace native=state::activity::vanilla::homecoming;
namespace layouts=state::build_data::scenarios;
namespace wire=middleware::bap::activity_message::sensor_auth_update;
inline constexpr std::string_view kPackage="mission_towerfall";
inline constexpr std::uint8_t kBubbleCount=10;
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
// Preserve the existing wire capacity. C++ owns trigger volumes by position
// sampling; groups holding only navigation points, trigger callbacks or
// volumes have no authority to publish. Sources, named members, tactical groups,
// sequences, objects, devices, Scenes and the console link remain registered.
inline constexpr bool required(const native::Group& group) noexcept {
    if(group.topLevel) {return true;}
    for(const auto& slot:group.slots) {switch(slot.asset.type) {case 1:case 2:case 3:case 4:case 5:case 23:case 43:case 65:return true;}}return false;
}
inline constexpr std::size_t kRequiredGroups=[] { std::size_t n{};for(const auto& g:native::kGroups) { if(required(g)) { ++n; } }return n; }();
static_assert(kRequiredGroups+2+3<=wire::kGroupCapacity); // roots and all three cinematic owners
inline constexpr std::uint64_t kRootBubbles=0x3FFULL;
template<class FindGroup>
[[nodiscard]] bool prepare_layout(layouts::Definition& layout,FindGroup find) noexcept {
    if(layout.tag!=native::kScenario || layout.nameLength!=kPackage.size()
        || std::string_view(layout.name.data(),layout.nameLength)!=kPackage
        || layout.bubbleCount!=kBubbleCount) { return false; }
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
// Groups resolve by registry key and object tag: the plaza registry has no
// ordinary cache ordinal and enters the cache through the mission catalog hook.
template<class Storage,class FindGroup>
[[nodiscard]] bool admit(const layouts::Definition& layout,Storage& storage,wire::Roster& roster,FindGroup find,std::uint32_t* failedKey=nullptr) noexcept {
    if(failedKey) { *failedKey=0; }
    if(layout.tag!=native::kScenario || layout.nameLength>layout.name.size()
        || std::string_view(layout.name.data(),layout.nameLength)!=kPackage
        || layout.bubbleCount!=kBubbleCount
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
        if(!find(expected.key,expected.tag,group) || !matches(group,expected)) { return false; }
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
// Stable key ordinals throughout the intro, gameplay, the pickup and the outro.
// Each cinematic owner is the last key of its bubble block; presence retires a
// source without dropping its old ordinal from the native removal walk.
inline constexpr auto kPresence=[] {
    std::array<std::array<std::array<std::uint8_t,wire::kBubbleKeyCapacity>,2>,2> masks{};
    for(std::size_t regular=0;regular<2;++regular) for(std::size_t tail=0;tail<2;++tail) {
        auto& mask=masks[regular][tail];mask.fill(static_cast<std::uint8_t>(regular));mask.back()=static_cast<std::uint8_t>(tail);
    }
    return masks;
}();
template<class Storage>
bool movies(Storage& storage,wire::Roster& roster,const native::cinematics::State& state) noexcept {
    namespace cine=native::cinematics;
    std::array<std::size_t,std::size(native::kMovies)> movieBlocks{};
    for(std::size_t m=0;m<std::size(native::kMovies);++m) {
        const auto& movie=native::kMovies[m];
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
        movieBlocks[m]=block;
    }
    // Two cinematic owners never share a bubble block; each stays the last key of its block.
    for(std::size_t m=0;m<std::size(native::kMovies);++m) {for(std::size_t n=0;n<m;++n) {if(movieBlocks[m]==movieBlocks[n]) {return false;}}}
    const std::size_t regular=state.ending()?0:1;
    for(std::size_t i=0;i<roster.bubbleSubBlocks.size();++i) {
        auto& block=storage.rosterSubBlocks[i];const auto count=block.keys.size();if(count>wire::kBubbleKeyCapacity) {return false;}
        std::size_t tail=regular;
        for(std::size_t m=0;m<std::size(native::kMovies);++m) {
            if(movieBlocks[m]==i) {tail=state.selected() && state.movie==m?1:0;}
        }
        block.presence=std::span(kPresence[regular][tail]).last(count);
    }
    return true;
}
}
