#include "middleware/bap/activity_message/sensor_auth_update.h"
#include "server/bap/encrypted/activity_message/membership/activity_membership_route.h"
#include "state/activity/membership/transactions/internal.h"
#include "middleware/encoding/bit_writer.h"
#include "middleware/encoding/bit_reader.h"
#include "middleware/bap/activity_message/replicate_membership.h"
#include "server/bap/encrypted/push/activity/native_activity_publisher.h"
#include "server/bap/encrypted/push/activity/native_roster_lifetime_projection.h"
#include "server/runtime/activity/mercury_definition.h"
#include <memory>
#include <vector>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>

namespace auth=dawn::middleware::bap::activity_message::sensor_auth_update;
namespace native=dawn::middleware::bap::activity_message::native;
namespace bits=dawn::middleware::encoding::bits;
namespace {
unsigned checks{};
void check(bool value,int line) { ++checks;if(!value){std::printf("FAIL %d\n",line);std::exit(1);} }
#define CHECK(value) check((value),__LINE__)
constexpr std::array<std::uint32_t,6> kKeys{0x2571C34D,0x74337EDD,0x564C6ECE,0x2763EC97,0x2749BAAE,0xC8229B2B};
constexpr std::array<std::uint16_t,6> kSlots{0,0,2,0,4,106};
constexpr std::array<std::uint8_t,6> kTypes{1,4,42,37,23,70};
struct Fixture {
    std::array<std::uint32_t,6> keys{kKeys};
    std::array<std::uint8_t,6> types{kTypes},flags{3,3,2,3,3,3};
    std::array<std::uint8_t,6> presence{1,1,1,1,1,1},states{0x83,0x83,0x83,0x83,0x83,0x83};
    std::array<auth::BubbleSubBlock,2> blocks{};
    auth::Snapshot snapshot{};
    Fixture() {
        blocks[0]={15,keys,presence,states};
        snapshot.hasRegion=true;snapshot.region=120;snapshot.lifetime=6;snapshot.stateSequence=3;
        snapshot.roster.groupCount=6;snapshot.roster.bubbleSubBlocks=std::span(blocks).first(1);
        for(std::size_t i=0;i<6;++i)
            snapshot.roster.groups[i]={keys[i],std::span(types).subspan(i,1),std::span(flags).subspan(i,1),std::span(kSlots).subspan(i,1)};
        snapshot.populations.count=1;
        snapshot.populations.entries[0]={{kKeys[0],1,8,1,{kKeys[0],2,0}},kSlots[0],15};
        snapshot.placements.count=1;snapshot.placements.entries[0]={kKeys[1],kSlots[1],15};
        snapshot.placements.entries[0].generation=1;
        snapshot.animations.count=1;snapshot.animations.entries[0]={kKeys[2],kSlots[2],15,{0x010B0F07,native::npc_animation::kEmptyHash,1}};
        snapshot.generators.count=1;snapshot.generators.entries[0]={kKeys[3],kSlots[3],15};
        snapshot.generators.entries[0].state.primary.seed=0x11223344;
        snapshot.generators.entries[0].state.primary.enabled=true;
        snapshot.devices.count=1;snapshot.devices.entries[0]={kKeys[4],kSlots[4],15};
        snapshot.devices.entries[0].state.position={0.75F,1,false};
        snapshot.engagements.count=1;
        auto& engagement=snapshot.engagements.entries[0];
        engagement.registry=kKeys[5];engagement.slot=kSlots[5];engagement.scope=15;engagement.generation=1;
        engagement.collection=native::engagement::Collection::activePlayers;
    }
    Fixture(const Fixture&)=delete;
};
struct Packet {std::array<std::byte,16384> bytes{};std::size_t length{};};
Packet encode(auth::Snapshot value,bool archive) {
    value.archiveOmega=archive;Packet result;
    CHECK(auth::encode_sensor_auth_update(value,result.bytes,result.length));
    CHECK(result.length>0);return result;
}
void reject(auth::Snapshot value) {
    for(bool archive:{false,true}) {
        value.archiveOmega=archive;Packet result;result.bytes.fill(std::byte{0xA5});result.length=999;
        CHECK(!auth::encode_sensor_auth_update(value,result.bytes,result.length));
        CHECK(result.length==0);
        CHECK(std::all_of(result.bytes.begin(),result.bytes.end(),[](auto b){return b==std::byte{0xA5};}));
    }
}
void batches(const auth::Snapshot& s,bool expected) {
    CHECK(native::population::valid(s.populations,s.roster,s.region)==expected);
    CHECK(native::placement::valid(s.placements,s.roster,s.region)==expected);
    CHECK(native::npc_animation::valid(s.animations,s.roster,s.region)==expected);
    CHECK(native::forest_generator::valid(s.generators,s.roster,s.region)==expected);
    CHECK(native::world_device::valid(s.devices,s.roster,s.region)==expected);
    CHECK(native::engagement::valid(s.engagements,s.roster,s.region)==expected);
}
struct Body {std::array<std::byte,2048> bytes{};std::size_t bits{};};
Body body(const auth::Snapshot& s,std::size_t index,bool archive) {
    Body result;bits::Writer writer(result.bytes);
    CHECK(archive?auth::write_auth_body(writer,s,kKeys[index],kTypes[index],kSlots[index],false)
                 :auth::legacy_write_auth_body(writer,s,kKeys[index],kTypes[index],kSlots[index],false));
    result.bits=writer.bit_count();
    const auto measured=archive?auth::auth_body_bits(s,kKeys[index],kTypes[index],kSlots[index],false)
                               :auth::legacy_auth_body_bits(s,kKeys[index],kTypes[index],kSlots[index],false);
    CHECK(result.bits==measured && result.bits>0);return result;
}
void scope_transition() {
    Fixture f;
    for(bool archive:{false,true}) {
        f.snapshot.region=120;
        const auto initial=encode(f.snapshot,archive);
        std::array<Body,6> initialBodies{};
        for(std::size_t i=0;i<6;++i)initialBodies[i]=body(f.snapshot,i,archive);
        for(auto region:{120U,128U,120U}) {
            f.snapshot.region=region;batches(f.snapshot,true);
            const auto packet=encode(f.snapshot,archive);
            // No participation slot in this fixture: only publication scope
            // changed, so the entire encoded six-body message stays identical.
            CHECK(packet.length==initial.length && packet.bytes==initial.bytes);
            for(std::size_t i=0;i<6;++i) {
                const auto actual=body(f.snapshot,i,archive);
                CHECK(actual.bits==initialBodies[i].bits && actual.bytes==initialBodies[i].bytes);
            }
        }
        auto empty=f.snapshot;empty.populations={};empty.placements={};empty.animations={};
        empty.generators={};empty.devices={};empty.engagements={};
        const auto omitted=encode(empty,archive);
        CHECK(omitted.length<initial.length && omitted.bytes!=initial.bytes);
    }
    // Legacy packets stay byte-identical at their current bubble, and retain
    // their previous strict rejection when that current bubble changes.
    const auto retained=encode(f.snapshot,false);
    f.blocks[0].states={};f.blocks[0].presence={};
    batches(f.snapshot,true);const auto legacy=encode(f.snapshot,false);
    CHECK(legacy.length==retained.length && legacy.bytes==retained.bytes);
    f.snapshot.region=128;batches(f.snapshot,false);reject(f.snapshot);
    f.blocks[0].presence=f.presence;batches(f.snapshot,false);reject(f.snapshot);
    f.blocks[0].states=f.states;f.blocks[0].presence={};batches(f.snapshot,false);reject(f.snapshot);
}
void invalid_metadata() {
    for(unsigned variant=0;variant<15;++variant) {
        Fixture f;f.snapshot.region=128;
        switch(variant) {
        case 0:f.presence[0]=0;break;
        case 1:f.presence[0]=2;break;
        case 2:f.states[0]=0x7F;break;
        case 3:f.blocks[0].states=std::span(f.states).first(5);break;
        case 4:f.blocks[0].presence=std::span(f.presence).first(5);break;
        case 5:f.blocks[0].bubble=14;break;
        case 6:f.keys[0]=0x12345678;break;
        case 7:f.keys[1]=f.keys[0];break;
        case 8:f.blocks[1]={16,std::span(f.keys).first(1),std::span(f.presence).first(1),std::span(f.states).first(1)};
            f.snapshot.roster.bubbleSubBlocks=f.blocks;break;
        case 9:f.blocks[0].bubble=64;break;
        case 10:f.snapshot.region=512;break;
        case 11:f.snapshot.region=UINT32_MAX;break;
        case 12:f.snapshot.region=511;break;
        case 13:f.snapshot.region=121;break;
        default:f.snapshot.roster.bubbleSubBlocks={};break;
        }
        CHECK(!native::population::valid(f.snapshot.populations,f.snapshot.roster,f.snapshot.region));
        reject(f.snapshot);
    }
    for(auto state:{0x80U,0xFFU}) {
        Fixture f;f.states.fill(static_cast<std::uint8_t>(state));f.snapshot.region=128;
        batches(f.snapshot,true);(void)encode(f.snapshot,false);(void)encode(f.snapshot,true);
    }
    // A tombstone is denied in the current bubble too, even with full states.
    Fixture removed;removed.presence[0]=0;reject(removed.snapshot);
}
void descriptor_contracts() {
    for(std::size_t index=0;index<6;++index) {
        Fixture f;f.snapshot.region=128;
        f.types[index]=127;reject(f.snapshot);f.types[index]=kTypes[index];
        f.flags[index]=0;reject(f.snapshot);f.flags[index]=index==2?2:3;
        f.snapshot.roster.groups[6]=f.snapshot.roster.groups[index];f.snapshot.roster.groupCount=7;
        reject(f.snapshot);
    }
    for(unsigned index=0;index<6;++index) {
        Fixture f;f.snapshot.region=128;
        switch(index) {
        case 0:f.snapshot.populations.entries[1]=f.snapshot.populations.entries[0];f.snapshot.populations.count=2;break;
        case 1:f.snapshot.placements.entries[1]=f.snapshot.placements.entries[0];f.snapshot.placements.count=2;break;
        case 2:f.snapshot.animations.entries[1]=f.snapshot.animations.entries[0];f.snapshot.animations.count=2;break;
        case 3:f.snapshot.generators.entries[1]=f.snapshot.generators.entries[0];f.snapshot.generators.count=2;break;
        case 4:f.snapshot.devices.entries[1]=f.snapshot.devices.entries[0];f.snapshot.devices.count=2;break;
        default:f.snapshot.engagements.entries[1]=f.snapshot.engagements.entries[0];f.snapshot.engagements.count=2;break;
        }
        reject(f.snapshot);
    }
    Fixture f;f.snapshot.region=128;f.snapshot.roster.topLevelGroupCount=6;
    // Type70 retains its independent exclusion of top-level group admission.
    CHECK(!native::engagement::valid(f.snapshot.engagements,f.snapshot.roster,128));reject(f.snapshot);
}
}
namespace publisher_cases {
namespace bap=dawn::server::bap;
namespace publisher=bap::encrypted::push::activity::native_publisher;
namespace life=bap::encrypted::push::activity::roster_lifetime;
namespace runtime=dawn::server::runtime::activity;
using Owner=dawn::state::activity::ActivityInstanceKey;
constexpr Owner creator{0x9EAA300100200003ULL,{1}},child{0x9EAA300100200004ULL,{1}};
constexpr bap::RegionLineage creatorLineage{creator,creator,bap::RegionLineageKind::ownedActivity};
constexpr bap::RegionLineage childLineage{child,creator,bap::RegionLineageKind::groupDerivedBorrow};
constexpr std::uint32_t playerGroup=0x4786C0E0,genericGroup=0x12345678;
const auto& profile=runtime::mercury::kActivity;
struct Storage {
    std::array<auth::BubbleSubBlock,64> rosterSubBlocks{};
    std::array<std::array<std::uint32_t,auth::kGroupCapacity>,64> rosterSubBlockKeys{};
};
struct Publication {
    Storage storage{};
    std::array<std::vector<std::uint8_t>,auth::kGroupCapacity> types,flags;
    std::array<std::vector<std::uint16_t>,auth::kGroupCapacity> slots;
    auth::Snapshot snapshot{};
    void group(std::uint32_t key,std::span<const runtime::registry::Slot> definitions) {
        for(std::size_t i=0;i<snapshot.roster.groupCount;++i)if(snapshot.roster.groups[i].key==key)return;
        const auto index=snapshot.roster.groupCount++;CHECK(index<auth::kGroupCapacity);
        for(const auto& slot:definitions){types[index].push_back(slot.type);flags[index].push_back(slot.flags());slots[index].push_back(slot.index);}
        snapshot.roster.groups[index]={key,types[index],flags[index],slots[index]};
    }
    Publication() {
        constexpr std::array<runtime::registry::Slot,1> player{{{0,13,1,UINT32_MAX,1,1}}};
        constexpr std::array<runtime::registry::Slot,1> generic{{{0,30,1,1,1,1}}};
        group(playerGroup,player);snapshot.roster.topLevelGroupCount=1;snapshot.roster.playerKeyGroup=playerGroup;
        // These are the actual production descriptor identities and slots, including
        // disabled optional registrations and an authored Adventure overlay.
        for(const auto& definition:profile.registries)group(definition.key,definition.slots);
        for(const auto& binding:profile.optionalRegistries)group(binding.registry->key,binding.registry->slots);
        const auto& overlay=*profile.adventureOpenings.front().overlay;
        group(overlay.root.key,overlay.root.slots);group(overlay.local.key,overlay.local.slots);
        group(genericGroup,generic);
        std::size_t count{};
        for(std::size_t i=1;i<snapshot.roster.groupCount;++i)
            storage.rosterSubBlockKeys[0][count++]=snapshot.roster.groups[i].key;
        storage.rosterSubBlocks[0]={15,std::span(storage.rosterSubBlockKeys[0]).first(count)};
        snapshot.roster.bubbleSubBlocks=std::span(storage.rosterSubBlocks).first(1);
        snapshot.hasRegion=true;snapshot.region=120;snapshot.lifetime=6;snapshot.stateSequence=3;
        snapshot.playerKey=0x1122334455667788ULL;
    }
};
std::uint64_t take(bits::Reader& reader,std::uint8_t width) {
    std::uint64_t value{};CHECK(reader.read(width,value));return value;
}
struct Decoded {unsigned policyKeys{},policyGroups{},sources{},players{},generic{};};
Decoded decode(const Packet& packet) {
    bits::Reader reader(std::span(packet.bytes).first(packet.length));Decoded result;
    CHECK(reader.skip(auth::kLatchBitWithoutGrant));CHECK(take(reader,1)==1);
    CHECK(take(reader,3)==7);
    const auto top=take(reader,9);CHECK(top<=256);
    for(std::size_t i=0;i<top;++i)result.policyKeys+=publisher::owns_key(profile,static_cast<std::uint32_t>(take(reader,32)));
    CHECK(take(reader,1)==1);CHECK(reader.skip(256));CHECK(take(reader,1)==1);CHECK(take(reader,9)==top);CHECK(reader.skip(top*8));
    if(take(reader,1)) {
        const auto blocks=take(reader,7);CHECK(blocks<=64);
        for(std::size_t b=0;b<blocks;++b) {
            CHECK(take(reader,1)==1);CHECK(take(reader,32)>=0x80000000U);CHECK(take(reader,2)==3);
            const auto count=take(reader,7);CHECK(count<=96);
            for(std::size_t i=0;i<count;++i)result.policyKeys+=publisher::owns_key(profile,static_cast<std::uint32_t>(take(reader,32)));
            CHECK(take(reader,1)==1);CHECK(reader.skip(96));CHECK(take(reader,1)==1);CHECK(take(reader,7)==count);CHECK(reader.skip(count*8));
        }
    }
    while(take(reader,1)) {
        const auto key=static_cast<std::uint32_t>(take(reader,32));CHECK(take(reader,32)==0);
        result.policyGroups+=publisher::owns_key(profile,key);
        result.generic+=key==genericGroup;
        while(take(reader,1)) {
            CHECK(take(reader,32)==key);const auto type=take(reader,7)-1;const auto slot=take(reader,16)-32768;
            const auto width=take(reader,32);CHECK(width<=reader.remaining_bits());
            result.players+=key==playerGroup && type==13;
            if(type==1 && ((key==0x74337EDD && slot==1)||(key==0x564C6ECE && slot==0))) {
                auto body=reader;CHECK(take(body,2)==3); // reset + present
                CHECK(body.skip(121));CHECK(take(body,32)==0x80000001U); // cumulative target one
                CHECK(body.skip(19));CHECK(take(body,1)==1);CHECK(take(body,31)==1); // original generation
                ++result.sources;
            }
            CHECK(reader.skip(width));
        }
    }
    CHECK(take(reader,1)==0);
    CHECK(reader.remaining_bits()<8);while(reader.remaining_bits())CHECK(take(reader,1)==0);
    return result;
}
void round_trip() {
    runtime::population::Service service;CHECK(service.begin(creator,profile.populations,7));
    CHECK(service.request({creator,1,1,0x74337EDD,1,1,7},15)==runtime::population::Result::accepted);
    CHECK(service.request({creator,2,2,0x564C6ECE,0,1,7},15)==runtime::population::Result::accepted);
    auto creatorState=std::make_unique<life::State>(),childState=std::make_unique<life::State>();
    unsigned creatorSends{},childSends{};
    // Independent child warmup starts later, then both receive repeated full
    // refreshes during the exact Lighthouse/lost-sector/Lighthouse round trip.
    for(auto region:{120U,120U,120U,120U,128U,128U,120U,120U,120U}) {
        unsigned sources{};
        for(bool derived:{false,true}) {
            auto publication=std::make_unique<Publication>();auto& value=publication->snapshot;
            const auto owner=derived?child:creator;const auto lineage=derived?childLineage:creatorLineage;
            auto& prior=derived?*childState:*creatorState;auto& sends=derived?childSends:creatorSends;
            value.region=region;value.stateSequence=static_cast<std::uint8_t>(sends<3?sends+1:3);
            const auto selected=publisher::prepare(profile,owner,lineage,prior,publication->storage,value.roster);
            CHECK(selected==(derived?publisher::Role::derived:publisher::Role::creator));
            if(selected==publisher::Role::creator)value.populations=service.project(profile.bubble);
            const life::Identity identity{owner.sessionId,owner.incarnation.value,UINT64_MAX,UINT64_MAX,0x80F4696A};
            CHECK(life::prepare(prior,identity,region/8,static_cast<std::uint8_t>(0x80+value.stateSequence),sends<3,value.roster,prior)==life::Result::ready);
            CHECK(life::project(prior,value.roster,publication->storage.rosterSubBlocks));
            const auto packet=encode(value,false);const auto decoded=decode(packet);
            CHECK(decoded.players==1 && decoded.generic==1);
            CHECK(decoded.sources==(derived?0U:2U));sources+=decoded.sources;
            if(derived) {
                CHECK(decoded.policyKeys==0 && decoded.policyGroups==0);
                CHECK(value.roster.groupCount==2 && value.roster.topLevelGroupCount==1);
                CHECK(value.roster.playerKeyGroup==playerGroup && value.populations.count==0);
            } else {
                CHECK(decoded.policyKeys==decoded.policyGroups && decoded.policyKeys>profile.registries.size());
                CHECK(value.populations.entries[0].source.generation==1 && value.populations.entries[0].source.looseRequested==1);
            }
            // Identical refreshes cannot change topology or encoded request counts.
            const auto refresh=encode(value,false);CHECK(refresh.bytes==packet.bytes && refresh.length==packet.length);
            if(!derived || creatorSends>1)++sends;
        }
        CHECK(sources==2); // One pond source and one Vance source across both contexts.
    }
    CHECK(service.revision()==3 && service.last_request()==2);
}
void rejected_lineages_and_aliases() {
    const std::array<bap::RegionLineage,7> invalid{{{},
        {child,creator,bap::RegionLineageKind::ownedActivity},
        {child,child,bap::RegionLineageKind::groupDerivedBorrow},
        {creator,creator,bap::RegionLineageKind::ownedActivity},
        {child,{},bap::RegionLineageKind::groupDerivedBorrow},
        {child,creator,static_cast<bap::RegionLineageKind>(99)},
        {{child.sessionId,{2}},creator,bap::RegionLineageKind::groupDerivedBorrow}}};
    for(const auto& lineage:invalid) {
        auto p=std::make_unique<Publication>();const auto before=encode(p->snapshot,false);
        CHECK(publisher::prepare(profile,child,lineage,{},p->storage,p->snapshot.roster)==publisher::Role::invalid);
        CHECK(encode(p->snapshot,false).bytes==before.bytes);
    }
    auto contaminated=std::make_unique<life::State>();auto p=std::make_unique<Publication>();
    const life::Identity identity{child.sessionId,1,0,0,0x80F4696A};
    CHECK(life::prepare({},identity,15,0x83,true,p->snapshot.roster,*contaminated)==life::Result::ready);
    CHECK(publisher::prepare(profile,child,childLineage,*contaminated,p->storage,p->snapshot.roster)==publisher::Role::invalid);
    // Real scratch uses aliased spans. Removing an entire leading block and
    // compacting a mixed middle block must preserve the later generic keys.
    p=std::make_unique<Publication>();auto& storage=p->storage;
    storage.rosterSubBlockKeys[0][0]=0x74337EDD;
    storage.rosterSubBlockKeys[1][0]=0x564C6ECE;storage.rosterSubBlockKeys[1][1]=genericGroup;
    storage.rosterSubBlockKeys[2][0]=0x55667788;
    storage.rosterSubBlocks[0]={15,std::span(storage.rosterSubBlockKeys[0]).first(1)};
    storage.rosterSubBlocks[1]={16,std::span(storage.rosterSubBlockKeys[1]).first(2)};
    storage.rosterSubBlocks[2]={17,std::span(storage.rosterSubBlockKeys[2]).first(1)};
    p->snapshot.roster.bubbleSubBlocks=std::span(storage.rosterSubBlocks).first(3);
    CHECK(publisher::prepare(profile,child,childLineage,{},storage,p->snapshot.roster)==publisher::Role::derived);
    CHECK(p->snapshot.roster.bubbleSubBlocks.size()==2);
    CHECK(storage.rosterSubBlocks[0].bubble==16 && storage.rosterSubBlocks[0].keys.size()==1 && storage.rosterSubBlocks[0].keys[0]==genericGroup);
    CHECK(storage.rosterSubBlocks[1].bubble==17 && storage.rosterSubBlocks[1].keys[0]==0x55667788);
    p=std::make_unique<Publication>();p->snapshot.roster.playerKeyGroup=0x74337EDD;
    CHECK(publisher::prepare(profile,child,childLineage,{},p->storage,p->snapshot.roster)==publisher::Role::invalid);
}
void run(){round_trip();rejected_lineages_and_aliases();}
}

void open_world_held_region_route() {
    namespace route=dawn::server::bap::encrypted::activity_message::membership;
    namespace state=dawn::state::activity::membership;
    namespace client=dawn::middleware::bap::activity_message::client_authoritative_data;
    // Exact two-leg travel semantics seen in the Mercury reentry log: after a
    // swap, the first leg names the loaded bubble and the second names outgoing.
    for(const std::string_view destination:{"mercury_freeroam","eden_freeroam","fleet_freeroam",
            "polaris_freeroam","planet_x_freeroam","tangled_shore_freeroam",
            "dreaming_city_freeroam","strike_pact"}) {
        state::MembershipState current{};
        const auto apply=[&](int held,int outgoing,std::uint8_t token,bool hasToken,int expected) {
            client::ClientAuthoritativeData parsed{};
            parsed.hasCurrentRegion=true;parsed.currentRegion={held,held==120?0xA83A9175U:0x3CB36B81U,true};
            parsed.hasRegion=true;parsed.region={outgoing,outgoing==120?0xD49C610EU:0xC9782776U,true};
            parsed.hasTransitionToken=hasToken;parsed.transitionToken=token;
            const auto update=route::make_authoritative(parsed,destination);
            CHECK(update.hasCurrentRegion && update.currentRegion.index==held);
            CHECK(update.hasRegion && update.region.index==outgoing);
            current=state::transactions::merge(current,update);
            CHECK(current.region.index==expected && current.currentRegion.index==held);
            const auto repeated=state::transactions::merge(current,update);
            CHECK(state::transactions::equal(current,repeated));
        };
        apply(120,120,1,true,120);
        apply(120,128,2,true,128); // Prefetch does not masquerade as held arrival.
        apply(128,120,0,false,128); // PRV128 loaded; outgoing120 must not rewind it.
        apply(120,128,0,false,120); // PUB120 loaded on return; outgoing128 stays inactive.
        apply(120,128,2,true,120); // Duplicate old transition token cannot revive outgoing.
        // A later genuine transition can prefetch again while held stays120.
        apply(120,128,3,true,128);
        apply(128,120,0,false,128);
        apply(120,128,0,false,120);
    }
    // The opt-in is exact. Existing Lua missions keep their prior second-leg policy.
    for(const std::string_view destination:{"mission_scot","adventure_whisk",
            "adventure_vod","mission_abs","infinite_abyss","mercury_freeroam_extra",""}) {
        client::ClientAuthoritativeData parsed{};
        parsed.hasRegion=true;parsed.region={128,0xC9782776U,true};
        parsed.hasCurrentRegion=true;parsed.currentRegion={120,0xA83A9175U,true};
        const auto update=route::make_authoritative(parsed,destination);
        CHECK(!update.hasCurrentRegion);
        const auto current=state::transactions::merge({},update);
        CHECK(current.region.index==128 && current.currentRegion.index<0);
    }
    namespace publisher=dawn::server::bap::encrypted::push::activity::native_publisher;
    // Titan launch evidence: region56 remained the exact held/current Rig
    // report while publication prefetched region16. Patrol runtime must stay in
    // bubble7 until the client confirms the crossing.
    CHECK(publisher::runtime_region(16,56)==56);
    CHECK(publisher::runtime_region(16,-1)==16);
}

int main() {
    {
        namespace route=dawn::server::bap::encrypted::activity_message::membership;
        namespace state=dawn::state::activity::membership;
        namespace client=dawn::middleware::bap::activity_message::client_authoritative_data;
        namespace wire=dawn::middleware::bap::activity_message::replicate_membership;
        client::ClientAuthoritativeData parsed{};
        parsed.currentLeg={8,0x12345678,64,1,2,true};parsed.pendingLeg={7,0xFEDCBA98,56,0,1,true};
        parsed.hasRegion=true;parsed.region={56,0xFEDCBA98,true};
        parsed.hasCurrentRegion=true;parsed.currentRegion={64,0x12345678,true};
        const auto update=route::make_authoritative(parsed,"mission_ember");
        CHECK(!route::retains_held_region("mission_ember") && !update.hasCurrentRegion);
        CHECK(update.currentLeg.present && update.pendingLeg.present);
        const auto merged=state::transactions::merge({},update);
        CHECK(merged.region.index==56 && merged.currentLeg.sliceSetHash==0x12345678 && merged.pendingLeg.publicState==0);
        const auto sparse=state::transactions::merge(merged,route::make_authoritative({},"mission_ember"));
        CHECK(sparse.currentLeg==merged.currentLeg && sparse.pendingLeg==merged.pendingLeg);
        const auto snapshot=state::transactions::make_snapshot(sparse,{},7);
        CHECK(snapshot.currentLeg==merged.currentLeg && snapshot.pendingLeg==merged.pendingLeg);
        wire::MembershipSnapshot packet{};packet.revision=snapshot.revision;packet.epoch=snapshot.epoch;packet.localAmbassador=true;
        const auto leg=[](auto v){return wire::RegionLeg{v.sliceSetIndex,v.sliceSetHash,v.regionIndex,v.publicState,v.auxState,v.present};};
        packet.currentLeg=leg(snapshot.currentLeg);packet.pendingLeg=leg(snapshot.pendingLeg);
        std::array<std::byte,wire::kMaximumEncodedSize> data{};std::size_t written{};
        CHECK(wire::encode_replicate_membership(packet,data,written));
        CHECK(written==wire::encoded_size(packet) && wire::synchronization_bits(packet)==165);
        bits::Reader reader(std::span(data).first(written));std::uint64_t value{};
        // Native full local identity ends at bit 732, then D4 and its two legs.
        CHECK(reader.skip(732) && reader.read(1,value) && value==1);
        for(const auto v:{packet.currentLeg,packet.pendingLeg}) {
            CHECK(reader.read(1,value) && value==1);
            CHECK(reader.read(10,value) && value==static_cast<unsigned>(v.sliceSetIndex+1));
            CHECK(reader.read(32,value) && value==v.sliceSetHash);
            CHECK(reader.read(32,value) && value==0x80000000U+static_cast<std::uint32_t>(v.regionIndex));
            CHECK(reader.read(2,value) && value==static_cast<unsigned>(v.publicState+1));
            CHECK(reader.read(2,value) && value==static_cast<unsigned>(v.auxState+1));
            CHECK(reader.read(2,value) && value==0);
        }
        const auto legacy=route::make_authoritative(parsed,"mission_launchpad");
        CHECK(legacy.currentLeg==update.currentLeg && legacy.pendingLeg==update.pendingLeg && legacy.hasCurrentRegion);
        for(std::string_view name:{"mission_ember_extra","mission_scot",""}) {
            const auto other=route::make_authoritative(parsed,name);CHECK(!other.currentLeg.present && !other.pendingLeg.present);
        }
    }
    open_world_held_region_route();
    scope_transition();invalid_metadata();descriptor_contracts();publisher_cases::run();
    std::printf("PASS %u retained authority scope/actual encoder checks\n",checks);
}
