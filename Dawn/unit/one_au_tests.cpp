#include "state/activity/vanilla/one_au/entry.h"
#include "state/activity/vanilla/one_au/authority.h"
#include "state/activity/vanilla/one_au/sense_adapter.h"
#include "state/activity/vanilla/one_au/transit_rules.h"
#include "client/activity/campaign_openings.h"
#include "middleware/encoding/bit_writer.h"
#include "middleware/encoding/bit_reader.h"
#include "middleware/bap/activity_message/native_sense.h"
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace m=dawn::state::activity::vanilla::one_au;
namespace coo=dawn::state::activity::coo;
namespace sense=dawn::middleware::bap::activity_message;
static unsigned checks{};
static void check(bool ok,const char* why) {
    ++checks;if(!ok) {std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}
}
#include "one_au_bridge_tests.h"
#include "one_au_entrance_tests.h"
#include "one_au_callback_tests.h"
#include "one_au_pipe_tests.h"
static void wire(const m::Frame& frame) {
    std::array<std::byte,16384> storage{};
    for(const auto& asset:m::kAssets) {
        const auto a=asset.asset;const auto expected=m::body_bits(frame,a.registry,static_cast<std::uint8_t>(a.type),a.slot);
        if(!expected) continue;
        dawn::middleware::encoding::bits::Writer writer(storage);
        if(!m::write_body(writer,frame,a.registry,static_cast<std::uint8_t>(a.type),a.slot) || writer.bit_count()!=expected) {
            std::fprintf(stderr,"asset=%08X/%u/%u expected=%zu actual=%zu\n",a.registry,static_cast<std::uint8_t>(a.type),a.slot,expected,writer.bit_count());
            check(false,"mission authority body matches its published bit count");
        }
        check(true,"mission authority width");
    }
}
static void cinematics() {
    namespace cine=m::cinematics;
    cine::Sequence sequence;const coo::Generation owner{7,1},stale{6,1};
    sequence.begin(owner,100);
    check(!sequence.state().masking_opening(101),"selection alone does not mask gameplay");
    check(!sequence.fly_in_complete(stale) && sequence.fly_in_complete(owner),"only current arrival arms opening mask");
    check(sequence.state().masking_opening(102),"opening mask held until playback");
    check(!sequence.arrival(stale,1,103) && sequence.arrival(owner,1,103),"arrival is scoped to current mission");
    for(unsigned movie=0;movie<3;++movie) {
        if(movie) check(sequence.arrival(owner,static_cast<std::uint8_t>(movie+1),200+movie),"ending arrival");
        check(!sequence.incident(stale,5239,cine::kMovies[movie].registry,6,0,1,210),"stale playback rejected");
        check(!sequence.incident(owner,5239,0,6,0,1,210),"foreign cinematic rejected");
        check(sequence.incident(owner,5239,cine::kMovies[movie].registry,6,0,1,210),"cinematic started");
        check(!sequence.state().masking_opening(211),"playback releases opening mask");
        check(sequence.incident(owner,3338,cine::kMovies[movie].registry,6,0,2,220),"skip uses accepted source, not unstable incident value");
        check(sequence.incident(owner,1685,cine::kMovies[movie].registry,6,0,3,230),"native completion accepted");
        if(!movie) {
            check(sequence.state().route()==4 && sequence.arrival(owner,4,240),"opening returns to Starboard Landing");
            check(sequence.finish_gameplay(owner,250),"completed gameplay starts first ending");
        }
    }
    check(sequence.state().phase==cine::Phase::complete,"both ending cinematics complete");
    sequence.begin(owner,0);sequence.fly_in_complete(owner);sequence.advance(90000);
    check(sequence.state().phase==cine::Phase::failed && !sequence.state().masking_opening(90000),"timeout releases the opening mask");
}
static void recovery() {
    namespace r=m::recovery;
    r::State state{};state.phase=r::Phase::requested;
    check(!r::project(state,{0,8,0}) && state.host.state==1 && state.host.token==9,"wipe publishes a fresh spawn token");
    check(!r::project(state,{4,8,0}),"stale spawn acknowledgement cannot reset checkpoint");
    check(r::project(state,{2,9,0}),"matching spawn request restores checkpoint");
    state.reset=true;check(!r::project(state,{4,9,0}) && state.host.state==4,"restored checkpoint releases native spawn gate");
    check(!r::project(state,{0,9,0}) && !state.active(),"matching release completes wipe handshake");
    check(r::checkpoint(static_cast<std::uint8_t>(m::Section::escape)).spawn==0x45920385U,"escape retries at its own checkpoint");
}
static void receipts() {
    sense::squad_sense::Output source{};source.hasRevision=true;source.revision=13;
    source.costMask=5;source.cost[0]=127;source.cost[2]=9;
    const auto mapped=m::source_output(source);
    check(mapped.present==2 && mapped.counters[1]==13 && mapped.costMask==5 && mapped.costs[2]==9,
        "production squad parser preserves task revision, cost mask and costs");
    check(m::source_output({}).present==0,"absent squad revision stays absent");
    sense::combatant_sense::Output actor{};actor.spawnRevision=7;actor.hasSpawnRevision=true;
    actor.programRevision=2;actor.programState=1;actor.hasProgramRevision=actor.hasProgramState=true;
    actor.deliveryRevision=4;actor.hasDeliveryRevision=true;actor.deliveryState=0;actor.detached=true;
    const auto delta=m::actor_output(actor);
    check(delta.generation==7 && delta.programRevision==2 && delta.programState==1
        && delta.deliveryRevision==4 && delta.deliveryState==0 && delta.dead && delta.hasGeneration,
        "production actor parser preserves transport receipts");
    check(!m::actor_output({}).hasGeneration,"absent actor revision stays absent");
}
static void no_wipes() {
    namespace bits=dawn::middleware::encoding::bits;
    for(unsigned section=0;section<static_cast<unsigned>(m::Section::count);++section) {
        for(const bool restricted:{false,true}) {
            auto controller=std::make_unique<m::Controller>();
            check(controller->select(91,0),"no-wipe fixture selects mission");
            const auto owner=controller->owner();const auto movie=m::cinematics::kMovies[0];
            check(controller->arrival(owner,1,10)
                && controller->cinematic(owner,{5239,movie.registry,1,6,0},20)
                && controller->cinematic(owner,{1685,movie.registry,2,6,0},30)
                && controller->arrival(owner,4,40),"no-wipe fixture reaches gameplay");
            // Model a later encounter without bypassing the actual death,
            // update, spawn-handshake or authority publication functions.
            auto& frame=const_cast<m::Frame&>(controller->frame());
            frame.section=static_cast<std::uint8_t>(section);frame.restricted=restricted;
            frame.interactions[0].completed=true;frame.native[0].generation=777;
            controller->life(owner,0xA0002001U,true,100);
            for(const auto time:{std::uint64_t{101},std::uint64_t{5000},std::uint64_t{65000}}) {
                controller->life(owner,0xA0002001U,false,time);
                check(controller->advance(91,time,true),"mission advances after individual death");
                check(!frame.recovery.active() && frame.wipeRemaining==0,
                    "death and escape timeout never request a wipe");
                check(frame.section==section && frame.spawnGeneration==owner.value
                    && frame.interactions[0].completed && frame.native[0].generation==777,
                    "death preserves encounter progress and native generations");
                const auto spawn=controller->spawn({0,29,0x12345678},time);
                check(spawn.state==0 && spawn.token==29 && spawn.value==0x12345678,
                    "individual death does not initiate checkpoint spawn handshake");
                std::array<std::byte,64> data{};bits::Writer writer(data);
                check(m::write_body(writer,frame,0x4786C0E0U,35,1) && writer.bit_count()==359,
                    "darkness authority retains native wire shape");
                bits::Reader reader(data);std::uint64_t value{};
                check(reader.read(1,value) && value==(restricted?1U:0U),
                    "darkness presentation remains enabled for restricted encounters");
                check(reader.skip(3) && reader.read(3,value) && value==0,
                    "darkness authority publishes no wipe countdown or activation");
            }
            controller->life(owner,0xA0002002U,true,65001);
            controller->life(owner,0xA0002002U,false,65002);
            check(!frame.recovery.active(),"a respawned player can die again without a wipe");
            if(section==static_cast<unsigned>(m::Section::escape)) {
                check(frame.escapeClock && frame.escapeElapsed>=60000,
                    "escape timer continues presentation after expiry without recovery");
            }
        }
    }
}
static void ghost_packets() {
    namespace bits=dawn::middleware::encoding::bits;
    for (const bool root : {false,true}) {
        std::array<std::byte,16> storage{};
        bits::Writer writer(storage);
        check(writer.write(root,1),"ghost root fixture");
        if(root) check(writer.write(1,1) && writer.write(std::bit_cast<std::uint32_t>(0.75F),32)
            && writer.write(0x80000007U,32),"ghost receipt fixture");
        check(writer.write(11,32) && writer.write(0x6B,8),"ghost revision and following field fixture");
        bits::Reader reader(storage);sense::native_sense::Output output{};std::size_t width{};
        check(sense::native_sense::read(reader,65,output,width),"ghost receipt decodes with production reader");
        check(output.schema==0x80804D3EU && output.root==root && output.revision==11
            && width==(root?98U:33U),"ghost root and empty receipts consume exact wire widths");
        if(root) check(output.ghost.active && output.ghost.progress==0.75F && output.ghost.generation==7,
            "ghost progress and signed authority generation survive decoding");
        std::uint64_t following{};check(reader.read(8,following) && following==0x6B,
            "ghost receipt leaves the next field intact");
        bits::Reader shortReader(std::span<const std::byte>(storage).first(root?12:4));
        check(!sense::native_sense::read(shortReader,65,output,width),"truncated ghost receipts are rejected");
    }
    for (float invalid : {-1.F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        std::array<std::byte,16> storage{};bits::Writer writer(storage);
        check(writer.write(1,1) && writer.write(std::bit_cast<std::uint32_t>(invalid),32)
            && writer.write(0x80000000U,32),"invalid ghost fixture");
        bits::Reader reader(storage);sense::ghost_sense::Output output{true,0.5F,3};
        check(!sense::ghost_sense::read(reader,output) && output.progress==0.5F && output.generation==3,
            "invalid ghost progress is rejected without publishing partial output");
    }
}
int main() {
    namespace openings=dawn::client::activity::mission_launch::openings;
    unsigned redWar{},oneAu{};
    for(const auto& mission:openings::kMissions) if(mission.campaign==0 && openings::listed(mission)) {
        ++redWar;if(mission.activity!=281) continue;
        ++oneAu;check(mission.investmentHash==0x38F926B2U
            && mission.destination.bubble==8 && mission.destination.sliceSet==64
            && mission.destination.spawnSetHash==0x2EA8FB98U,"1AU launch is pinned to its installed identity and opening");
    }
    check(redWar==3 && oneAu==1,"Red War exposes Homecoming, Exodus and 1AU");
    check(m::mission().valid(),"all mission graphs and checkpoint graphs are valid");
    std::string error;
    auto document=coo::script::MissionDocument::read(std::filesystem::path(__FILE__).parent_path().parent_path()/"scripts/one_au.lua",m::kEntryProfile,error);
    if(!document) std::fprintf(stderr,"Lua: %s\n",error.c_str());
    check(document && m::valid_document(document->views()),"shipped Lua entry authorizes the native module");
    auto controller=std::make_unique<m::Controller>();m::Entry entry;
    check(entry.select(document->views(),*controller,1,0),"Lua entry starts the native controller");
    check(controller->frame().enabled && !controller->frame().finished,"selected mission awaits actual playthrough");
    wire(controller->frame());
    const auto owner=controller->owner();
    check(controller->arrival(owner,1,10),"native opening arrival");
    const auto movie=m::cinematics::kMovies[0];
    check(controller->cinematic(owner,{5239,movie.registry,1,6,0},20),"controller accepts native opening start");
    check(controller->cinematic(owner,{1685,movie.registry,2,6,0},30),"controller accepts native opening end");
    check(controller->arrival(owner,4,40),"controller reaches gameplay landing");
    check(entry.update(*controller,1,50,true).enabled,"Lua composition advances gameplay");
    check(!controller->frame().fault,"opening graph has no rejected commands");
    wire(controller->frame());
    auto frame=std::make_unique<m::Frame>(controller->frame());
    frame->presentation.published=frame->presentation.active=true;frame->presentation.event=m::kObjectives[0];
    frame->escapeClock=true;frame->escapeElapsed=30000;frame->gameplayClockTicks=coo::native_activity_ticks(45000);
    for(auto& state:frame->native) {state.managed=state.active=state.prepared=true;state.generation=frame->spawnGeneration;state.sequenceRevision=1;}
    wire(*frame);
    controller->reset();check(!controller->frame().enabled && !controller->owner().valid(),"reset retires mission authority");
    check(controller->select(1,100) && controller->owner()!=owner,"replayed run gets a fresh authority generation");
    check(!controller->arrival(owner,1,110),"retired run receipts cannot activate new attempt");
    cinematics();recovery();no_wipes();receipts();ghost_packets();bridge_test::run();entrance_test::run();entrance_callback_test::run();pipe_encounter();
    m::VentCycle cycle;
    check(cycle.sample(0)==m::VentCycle::Phase::idle && cycle.sample(250)==m::VentCycle::Phase::charging
        && cycle.sample(10000)==m::VentCycle::Phase::windup && cycle.sample(13000)==m::VentCycle::Phase::surging
        && cycle.sample(20500)==m::VentCycle::Phase::cooldown && cycle.sample(30250)==m::VentCycle::Phase::charging,
        "reactor vent and laser cycle stays synchronized across repeats");
    std::printf("PASS: 1AU %u checks; launch, Lua entry, opening, authority widths, stale receipts, endings, checkpoints and reactor cycle\n",checks);
}
