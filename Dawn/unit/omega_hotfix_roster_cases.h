#pragma once
#include "server/bap/encrypted/push/activity/omega_lair_roster.h"

void omega_hotfix_roster_checks() {
    namespace lair=dawn::server::bap::encrypted::push::activity::omega_lair;
    namespace catalog=dawn::state::activity::omega_lair_full_roster;
    auto rows=std::make_unique<std::vector<layouts::RosterGroup>>();
    const auto add=[&](const auto& expected) {
        layouts::RosterGroup row{};
        row.registryKey=expected.key;row.objectTag=expected.tag;
        row.slotCount=static_cast<std::uint16_t>(expected.slots.size());
        for(std::size_t i=0;i<expected.slots.size();++i) {
            row.slotTypes[i]=expected.slots[i].type;
            row.slotFlags[i]=expected.slots[i].flags;
            row.slotIndices[i]=expected.slots[i].index;
            row.descriptorTags[i]=expected.tag;row.componentClasses[i]=0x80800001U;
            row.senseSchemas[i]=(row.slotFlags[i]&1U)?0x80800002U:UINT32_MAX;
            row.authSchemas[i]=(row.slotFlags[i]&2U)?0x80800003U:UINT32_MAX;
        }
        rows->push_back(row);
        row.objectTag^=1U;rows->insert(rows->begin(),row);
    };
    for(const auto& expected:catalog::kCombatGroups)add(expected);
    constexpr std::array<catalog::Slot,1> intro{{{6,2,22}}};
    constexpr std::array<catalog::Slot,3> boss{{{1,2,0},{2,2,1},{66,0,57}}};
    std::array<catalog::Slot,lair::kCrownDescriptorCount> crown{};
    for(std::size_t i=0;i<lair::kCrownSlotTypes.size();++i)
        crown[i]={lair::kCrownSlotTypes[i],static_cast<std::uint8_t>(((i>=40 && i<=44)||i>=46)?2:3),static_cast<std::uint16_t>(i)};
    for(std::size_t i=0;i<lair::kCrownRuleSlots.size();++i)
        crown[lair::kCrownSlotTypes.size()+i]={66,0,lair::kCrownRuleSlots[i]};
    add(catalog::Group{0xF4D0E0B2U,0x80F47979U,14,1076,intro});
    add(catalog::Group{0x95FB2E01U,0x80F475EEU,14,1071,boss});
    add(catalog::Group{0x0040BF06U,0x80F476C6U,14,1072,crown});
    for(unsigned i=0;i<1000;++i) {
        auto decoy=std::make_unique<layouts::RosterGroup>(rows->front());
        decoy->registryKey=0x10000000U+i;rows->insert(rows->begin(),*decoy);
    }
    layouts::Definition layout{};layout.tag=lair::kScenarioTag;
    constexpr std::string_view name="mission_scot";
    std::copy(name.begin(),name.end(),layout.name.begin());
    layout.nameLength=static_cast<std::uint8_t>(name.size());
    layout.bubbleCount=15;layout.bubbleHashes[lair::kBubble]=lair::kBubbleHash;
    CHECK(layouts::replace({},*rows));
    auto storage=std::make_unique<Storage>();auto roster=std::make_unique<wire::Roster>();
    unsigned copies{};
    const auto find=[&](std::uint32_t key,std::uint32_t tag,layouts::RosterGroup& out) {
        ++copies;return layouts::group_by_key(key,tag,out);
    };
    for(const auto& expected:catalog::kCombatGroups) {
        CHECK(lair::admit_full(layout,*storage,*roster,find,expected)==lair::Admission::added);
        CHECK(lair::full_descriptors(roster->groups[roster->groupCount-1],expected));
        CHECK(lair::admit_full(layout,*storage,*roster,find,expected)==lair::Admission::present);
    }
    CHECK(lair::admit(layout,*storage,*roster,find)==lair::Admission::added);
    CHECK(lair::admit(layout,*storage,*roster,find,true)==lair::Admission::added);
    CHECK(lair::admit_crown(layout,*storage,*roster,find)==lair::Admission::added);
    CHECK(copies==7);CHECK(roster->groupCount==7);
    const auto& expected=catalog::kCombatGroups.front();
    const auto good=std::find_if(rows->begin(),rows->end(),[&](const auto& r){return r.registryKey==expected.key && r.objectTag==expected.tag;});
    CHECK(good!=rows->end());
    const auto ordinal=static_cast<std::size_t>(good-rows->begin());
    const auto original=std::make_unique<layouts::RosterGroup>(*good);
    for(unsigned bad=0;bad<4;++bad) {
        storage=std::make_unique<Storage>();roster=std::make_unique<wire::Roster>();(*rows)[ordinal]=*original;
        if(bad==0)(*rows)[ordinal].objectTag^=1U;
        if(bad==1)(*rows)[ordinal].slotTypes[0]^=1U;
        if(bad==2) {
            auto& row=(*rows)[ordinal];row.slotFlags[0]^=1U;
            row.senseSchemas[0]=(row.slotFlags[0]&1U)?0x80800002U:UINT32_MAX;
        }
        if(bad==3)rows->push_back(*original);
        CHECK(layouts::replace({},*rows));
        CHECK(lair::admit_full(layout,*storage,*roster,find,expected)==lair::Admission::missingGroup);
        CHECK(roster->groupCount==0);CHECK(roster->bubbleSubBlocks.empty());
        if(bad==3)rows->pop_back();
    }
    (*rows)[ordinal]=*original;CHECK(layouts::replace({},*rows));
    storage=std::make_unique<Storage>();roster=std::make_unique<wire::Roster>();roster->groupCount=roster->groups.size();
    const auto before=copies;
    CHECK(lair::admit_full(layout,*storage,*roster,find,expected)==lair::Admission::noCapacity);
    CHECK(copies==before);
    layouts::clear();
    std::printf("Omega hotfix: seven identity lookups, one record per group; reordered catalog, collisions, invalid descriptors and capacity guards passed\n");
}
