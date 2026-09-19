#include "state/activity/vanilla/homecoming/entry.h"
#include "state/activity/vanilla/homecoming/authority.h"
#include "state/activity/vanilla/homecoming/sense_adapter.h"
#include "state/activity/vanilla/homecoming/transit_rules.h"
#include "state/activity/vanilla/homecoming/entrance_native.h"
#include "state/activity/vanilla/homecoming/door_native.h"
#include "state/activity/vanilla/homecoming/console_scan.h"
#include "state/activity/vanilla/homecoming/registries.h"
#include "state/activity/vanilla/homecoming/prologue.h"
#include "state/activity/vanilla/homecoming/music.h"
#include "server/bap/encrypted/push/activity/vanilla/homecoming_roster.h"
#include "client/activity/campaign_openings.h"
#include "middleware/encoding/bit_writer.h"
#include "middleware/encoding/bit_reader.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>

namespace m=dawn::state::activity::vanilla::homecoming;
namespace coo=dawn::state::activity::coo;
namespace dawn::core::log { void write(Channel,Level,std::string_view) noexcept {} }
static unsigned checks{};
static void check(bool ok,const char* why) {
    ++checks;if(!ok) {std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}
}
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
    for(const auto& movie:m::kMovies) {
        const auto expected=m::body_bits(frame,movie.registry,6,0);
        if(!expected) continue;
        dawn::middleware::encoding::bits::Writer writer(storage);
        check(m::write_body(writer,frame,movie.registry,6,0) && writer.bit_count()==expected,"cinematic owner body width");
    }
}
static void catalog() {
    check(m::kScenario==0x80B500BCU && m::kRoot==0x664128F4U && m::kBank==0x80C2AF61U,"scenario identity");
    check(std::size(m::kDialogue)==94 && std::size(m::kObjectives)==16 && m::kSpawns.size()==168,"catalog sizes from the Towerfall extraction");
    check(m::registries::required(m::kScenario,0x80B50746U,m::kPlaza,1ULL<<6),"plaza registry is admitted through the catalog hook");
    check(!m::registries::required(m::kScenario,0x80B50746U,m::kPlaza,1ULL<<5) && !m::registries::required(0,0x80B50746U,m::kPlaza,1ULL<<6),"catalog hook is exact");
    namespace roster=dawn::server::bap::encrypted::push::activity::homecoming_roster;
    for(const auto& g:m::kGroups) {
        for(const auto& s:g.slots) {check(m::asset_index(s.asset)<std::size(m::kAssets),"every group slot is a catalog asset");}
        // A required group must resolve from the scenario cache: an ordinary cache ordinal or the
        // mission catalog hook. Groups without either (plaza areas) must stay out of the roster.
        check(!roster::required(g) || g.topLevel || g.hint!=0 || m::registries::required(m::kScenario,g.tag,g.key,1ULL<<g.bubble),
            "every required roster group has a scenario cache record or the catalog hook");
    }
    check(roster::required(m::kGroups[std::size(m::kGroups)-2]) && !roster::required(m::kGroups[std::size(m::kGroups)-1]),"the plaza registry is admitted and the plaza areas registry is not");
    for(std::size_t i=0;i<m::kSpawns.size();++i) {
        const auto& p=m::kSpawns[i];
        check(m::spawn_index(m::asset(p.registry,1,p.source))==i,"spawn rows index by source asset");
        check(p.categories>=1 && p.categories<=4,"declared native categories");
        check(m::expected(p)<=63,"expected admissions fit the native request width");
    }
    for(const auto& cohort:m::kCohorts) {
        for(const auto member:cohort.members) {
            const auto n=m::spawn_index(m::asset(member.registry,1,member.slot));
            check(n<m::kSpawns.size(),"cohort members are authored sources");
        }
    }
    for(const auto& nav:m::kNavigation) {check(nav.asset.type==47 && nav.asset.registry && nav.asset.registry!=0x811C9DC5U,"navigation points are authored scenario points");}
    check(m::use_subscription(m::kReviveInteract) && m::asset_index(m::kConsoleLink)<std::size(m::kAssets) && m::kBazaarDoor.type==23,"native receipts bind to authored assets");
    for(const auto& scene:m::kScenes) {
        const auto sources=m::scene_sources(scene);
        for(const auto& c:sources.span()) {check((c.type==1 || c.type==4) && m::asset_index(m::asset(c.registry,c.type,c.slot))<std::size(m::kAssets),"scene reference arrays carry catalog sources and objects, never type-48 markers");}
        check(m::asset_index(scene.asset)<std::size(m::kAssets),"every Scene is a catalog asset");
        for(const auto key:scene.inputs) {check(key!=0 && key!=UINT32_MAX,"scene events are authored input hashes");}
    }
}
static void graphs() {
    check(m::mission().valid(),"all nine section graphs are valid");
    for(std::size_t s=0;s<m::mission().phases.size();++s) {
        const auto& g=m::mission().phases[s].definition;
        check(!g.steps.empty() && g.steps.size()<=coo::Executor::kMaxSteps,"section graph fits the executor");
        for(const auto& step:g.steps) {
            check(step.commands.size()<=coo::Executor::kMaxCommands,"step command budget");
            for(const auto& c:step.commands) {
                if(c.operation==coo::Operation::dialogue) {
                    const auto row=m::dialogue_row(c.argument);
                    check(row<std::size(m::kDialogue) && !m::kDialogue[row].sceneOwned && m::kDialogue[row].durationMs,"dialogue commands name authored, natively timed cues");
                }
                if(c.operation==coo::Operation::population) {check(c.asset==m::kModule && c.argument<std::size(m::kCohorts),"population commands request cohorts");}
                if(c.operation==coo::Operation::objective) {
                    bool known=false;for(const auto o:m::kObjectives) {known|=o==c.argument;}
                    check(known,"objective commands use authored directive events");
                }
                if(c.operation==coo::Operation::scene) {check(m::scene_index(c.asset)<std::size(m::kScenes),"scene commands name authored Scenes");}
            }
        }
    }
    unsigned finishes{};
    for(std::size_t s=0;s+1<m::mission().phases.size();++s) {
        for(const auto& step:m::mission().phases[s].definition.steps) for(const auto& c:step.commands) {
            if(c.operation==coo::Operation::mechanic && c.asset==m::kModule && c.argument==static_cast<std::uint32_t>(m::Mechanic::finishSection)) {++finishes;}
        }
    }
    check(finishes==m::mission().phases.size()-1,"every section except the escape hands over to the next");
    bool complete=false;
    for(const auto& step:m::mission().phases.back().definition.steps) for(const auto& c:step.commands) {complete|=c.operation==coo::Operation::complete;}
    check(complete,"the escape section completes the mission");
}
static void cinematics() {
    namespace cine=m::cinematics;
    cine::Sequence sequence;const coo::Generation owner{7,1},stale{6,1};
    sequence.begin(owner,100);
    check(!sequence.state().masking_opening(101),"selection alone does not mask gameplay");
    check(!sequence.fly_in_complete(stale) && sequence.fly_in_complete(owner),"only current arrival arms opening mask");
    check(sequence.state().masking_opening(102),"opening mask held until playback");
    check(m::transit::destination(sequence.state().route()).region==17,"intro plays in bubble 2 region 17");
    check(!sequence.arrival(stale,1,103) && sequence.arrival(owner,1,103),"arrival is scoped to current mission");
    check(!sequence.incident(stale,5239,cine::kMovies[0].registry,6,0,1,210),"stale playback rejected");
    check(!sequence.incident(owner,5239,0,6,0,1,210),"foreign cinematic rejected");
    check(sequence.incident(owner,5239,cine::kMovies[0].registry,6,0,1,210),"intro started");
    check(!sequence.state().masking_opening(211),"playback releases opening mask");
    check(sequence.incident(owner,3338,cine::kMovies[0].registry,6,0,2,220),"skip uses accepted source");
    check(sequence.incident(owner,1685,cine::kMovies[0].registry,6,0,3,230),"native completion accepted");
    check(sequence.state().route()==4 && m::transit::destination(4).region==72,"intro returns to the Underwatch region 72");
    check(sequence.arrival(owner,4,240) && sequence.state().phase==cine::Phase::gameplay,"gameplay begins on the Underwatch arrival");
    check(!sequence.finish_gameplay(owner,250),"the outro cannot start before the pickup");
    check(sequence.begin_pickup(owner,250) && sequence.state().route()==2 && m::transit::destination(2).region==65,"the pickup leaves for region 65");
    check(sequence.arrival(owner,2,260) && sequence.incident(owner,5239,cine::kMovies[1].registry,6,0,4,270)
        && sequence.incident(owner,1685,cine::kMovies[1].registry,6,0,5,280),"pickup plays and completes");
    check(sequence.state().route()==5 && m::transit::destination(5).region==64 && sequence.arrival(owner,5,290)
        && sequence.state().phase==cine::Phase::gameplay,"the pickup lands on the command ship region 64");
    check(!sequence.begin_pickup(owner,300),"the pickup plays once");
    check(sequence.finish_gameplay(owner,300) && sequence.state().route()==3 && m::transit::destination(3).region==9,"the outro plays in bubble 1 region 9");
    check(sequence.state().ending(),"the outro retires the roster");
    check(sequence.arrival(owner,3,310) && sequence.incident(owner,5239,cine::kMovies[2].registry,6,0,6,320)
        && sequence.incident(owner,1685,cine::kMovies[2].registry,6,0,7,330) && sequence.state().phase==cine::Phase::complete,"outro completes the mission");
    check(!m::transit::destination(0).valid() && !m::transit::destination(6).valid(),"only authored routes request transit");
    sequence.begin(owner,0);sequence.fly_in_complete(owner);sequence.advance(90000);
    check(sequence.state().phase==cine::Phase::failed,"an unanswered opening fails closed");
}
static void prologue_chain() {
    namespace pro=m::prologue;
    pro::Sequence chain;chain.begin(100);
    check(chain.wanted()==pro::kTowerCinematic,"the Red War opening starts with the Tower cinematic activity");
    check(!chain.queued(pro::kMission,100) && chain.queued(pro::kTowerCinematic,100),"only the wanted activity can be queued");
    chain.selected(7,266,m::kScenario,110);
    check(!chain.frame(7).enabled,"the mission scenario cannot bind the cinematic");
    chain.selected(7,pro::kTowerCinematic,pro::kScenario,110);
    check(chain.frame(7).enabled && !chain.frame(8).enabled && chain.frame(7).revision==1,"the loaded Tower cinematic scenario binds the run");
    check(pro::matches(chain.frame(7),0x32DDAD77U,6,0) && !pro::matches(chain.frame(7),0x32DDAD77U,6,1),"the cinematic owner body matches the approach movie");
    const dawn::state::activity::ActivityInstanceKey instance{0x9EAA300100200001ULL,{7}};
    auto authority=chain.project(instance,7,42,{},120);
    check(authority.publish && authority.host.state==1 && authority.host.sliceSetIndex==pro::kSliceSet,"arrival requests the authored cinematic slice");
    pro::native::Observation arrived{};arrived.hasTeleport=true;arrived.local={3,authority.host.token,pro::kSliceSet,pro::kSliceHash};arrived.hasRegion=true;arrived.currentRegion=pro::kSliceSet;
    authority=chain.project(instance,7,42,arrived,130);
    check(chain.state().phase==pro::Phase::offered && chain.frame(7).play,"reaching the cinematic slice offers playback");
    check(!chain.incident(7,{5239,0x964D8F24U,1,6,0},140),"the mission intro cannot answer the Tower cinematic");
    check(chain.incident(7,{5239,0x32DDAD77U,1,6,0},140) && chain.state().phase==pro::Phase::playing,"native playback start is accepted");
    check(chain.incident(7,{1685,0x32DDAD77U,1,6,0},150) && chain.state().phase==pro::Phase::missionRequested,"native completion requests the mission");
    check(chain.wanted()<0,"the mission waits for the teleport release");
    pro::native::Observation released{};released.hasTeleport=true;released.local={0,authority.host.token,pro::kSliceSet,pro::kSliceHash};
    chain.project(instance,7,42,released,160);
    check(chain.wanted()==pro::kMission && chain.queued(pro::kMission,160) && chain.frame(7).enabled,"the released teleport queues Homecoming while the owner stays published");
    chain.complete();check(chain.state().phase==pro::Phase::complete && !chain.frame(7).enabled,"arrival in Homecoming completes the chain");
    chain.begin(200);chain.tick(300000);check(chain.state().phase==pro::Phase::failed,"an unanswered cinematic launch fails closed");
}
static void dialogue_rows() {
    coo::DialogueService<std::size(m::kDialogue)> service;
    struct Presentation {std::uint8_t activeRow{coo::kNoDialogue};std::uint32_t objective{};std::array<std::uint32_t,std::size(m::kDialogue)> generations{};} presentation;
    std::uint32_t revision{};
    std::uint8_t last=0;
    for(std::uint8_t row=static_cast<std::uint8_t>(std::size(m::kDialogue));row-->0;) {if(m::kDialogue[row].durationMs && !m::kDialogue[row].sceneOwned) {last=row;break;}}
    check(last>=64,"Homecoming banks more than sixty-four cues");
    service.enqueue(m::kDialoguePolicy,last,0,0,0,revision);
    service.enqueue(m::kDialoguePolicy,last,0,0,0,revision);
    service.advance(m::kDialoguePolicy,5,1,false,presentation,revision);
    check(presentation.activeRow==last,"rows above sixty-three are requested once and offered");
    check(service.submitted(m::kDialoguePolicy,m::kBank,last,presentation.generations[last],2,presentation,revision) && presentation.activeRow==coo::kNoDialogue,"native submission acknowledges the offered row");
    service.advance(m::kDialoguePolicy,5,3,false,presentation,revision);
    check(presentation.activeRow==coo::kNoDialogue,"a duplicate request does not replay the row");
}
static void entrance() {
    namespace en=m::entrance_native;
    struct World {
        std::array<en::Row,8> rows{};std::array<bool,8> local{};unsigned grants{};
        bool stable() const noexcept {return true;}
        bool row(std::size_t slot,en::Row& out) const noexcept {if(slot>=rows.size()) return false;out=rows[slot];return true;}
        bool owned(std::uint32_t entity,bool& out) const noexcept {out=local[entity&7];return true;}
        void grant(std::uint32_t entity) noexcept {local[entity&7]=true;++grants;}
    } world;
    world.rows[1]={0,1,5,0x80F10442U,1,0x125D2DA09F611701ULL};
    world.rows[2]={0,2,5,0x80C3B4B7U,25,0xD8FFC2F1F8979C65ULL};
    world.rows[3]={0,3,5,0x80C3B4B7U,21,0};
    world.rows[4]={4,4,5,0x80F10442U,0,0x690A3243C7E62499ULL};
    world.rows[5]={0,5,5,0x80B505B2U,15,0x7B8AC80734FA725FULL};
    const coo::Generation owner{3,1};
    m::EntranceRequest military{owner,1,static_cast<std::uint8_t>(m::Section::military)};
    auto result=en::repair(world,military,[&] {return military;});
    check(result.doors==1 && world.local[1] && !world.local[2],"the military gates are granted before the ship");
    m::EntranceRequest ship{owner,1,static_cast<std::uint8_t>(m::Section::ship)};
    result=en::repair(world,ship,[&] {return ship;});
    check(result.doors==2 && world.local[2] && world.local[3] && !world.local[4] && !world.local[5],"ship doors, contact doors and the console are granted once; retired rows and the bazaar door are not");
    result=en::repair(world,ship,[&] {return m::EntranceRequest{};});
    check(result.doors==0,"a changed request cancels the grant");
    check(!en::repair(world,{},[&] {return ship;}).doors,"an idle request grants nothing");
    const m::door_native::Row bazaar{0,5,5,0x80B505B2U,15,0x7B8AC80734FA725FULL},gate{0,1,5,0x80F10442U,1,0x125D2DA09F611701ULL};
    check(m::door_native::placement(bazaar) && !m::door_native::placement(gate),"the bazaar door placement is exact");
}
static void receipts(m::Controller& controller) {
    const auto owner=controller.owner();
    const auto& first=m::kSpawns[0];
    const m::EnemyReceipt foreign{owner.run+1,1,2,owner.value,first.source,first.registry};
    check(!controller.admitted(foreign),"receipts from another run are rejected");
    check(!controller.died(foreign),"deaths from another run are rejected");
    check(!controller.ghost(owner,m::kConsoleLink,{true,0.5F,static_cast<std::int32_t>(controller.frame().spawnGeneration+1U)}),"ghost progress is ignored while the console is not armed");
    check(!controller.use(owner,m::kReviveInteract,{}),"revive use requires an armed prompt");
}
int main() {
    namespace openings=dawn::client::activity::mission_launch::openings;
    unsigned homecoming{};
    for(const auto& mission:openings::kMissions) if(mission.campaign==0 && openings::listed(mission) && mission.activity==266) {
        ++homecoming;check(mission.investmentHash==0x62D85FB3U && mission.destination.bubble==9 && mission.destination.sliceSet==72
            && !mission.destination.hasSpawnSetHash,"Homecoming launch is pinned to its installed identity and authored Underwatch arrival");
    }
    check(homecoming==1,"Red War lists Homecoming once");
    catalog();graphs();
    std::string error;
    auto document=coo::script::MissionDocument::read(std::filesystem::path(__FILE__).parent_path().parent_path()/"scripts/homecoming.lua",m::kEntryProfile,error);
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
    check(controller->arrival(owner,4,40),"controller reaches the Underwatch landing");
    if(!entry.update(*controller,1,50,true).enabled) {
        const auto d=controller->diagnostics();
        std::fprintf(stderr,"executor phase=%u failure=%u active=%08X complete=%08X fault=%u\n",static_cast<unsigned>(d.phase),static_cast<unsigned>(d.failure),d.active,d.complete,controller->frame().fault?1U:0U);
        const auto& g=controller->graph().definition;
        for(std::size_t i=0;i<g.steps.size();++i) {const auto st=controller->step_state(i);std::fprintf(stderr,"step %zu %.*s phase=%u\n",i,static_cast<int>(g.steps[i].name.size()),g.steps[i].name.data(),static_cast<unsigned>(st.phase));}
        check(false,"Lua composition advances gameplay");
    }
    check(!controller->frame().fault,"crash-site graph has no rejected commands");
    check(controller->frame().section==static_cast<std::uint8_t>(m::Section::underwatch) && controller->frame().bubble==9,"gameplay starts in the Underwatch");
    check(controller->frame().presentation.published && controller->frame().presentation.event==m::kObjectives[0],"the first directive is published on arrival");
    check(controller->frame().musicSection==m::music_section::underwatchRuins && m::music::section(controller->frame())==m::music_section::underwatchRuins,"the Underwatch score starts on landing");
    wire(controller->frame());
    receipts(*controller);
    auto frame=std::make_unique<m::Frame>(controller->frame());
    frame->presentation.published=frame->presentation.active=true;frame->presentation.event=m::kObjectives[13];
    frame->consoleArmed=true;frame->section=static_cast<std::uint8_t>(m::Section::plaza);frame->activeRow=0;frame->generations[0]=1;
    for(auto& state:frame->native) {state.managed=state.active=state.prepared=true;state.generation=frame->spawnGeneration;state.sequenceRevision=1;}
    for(auto& scene:frame->scenes) {scene.count=1;scene.events[0]=0x6F51AC66U;}
    wire(*frame);
    for(auto& state:frame->native) {state.retired=true;state.bound=false;}
    wire(*frame);
    check(m::console_scan_request(owner,*frame).enabled()==false,"the console scan waits for the command ship section");
    frame->section=static_cast<std::uint8_t>(m::Section::ship);
    check(m::console_scan_request(owner,*frame).generation==frame->spawnGeneration+1U,"the console scan request carries the Ghost link generation");
    controller->reset();check(!controller->frame().enabled && !controller->owner().valid(),"reset retires mission authority");
    check(controller->select(1,100) && controller->owner()!=owner,"replayed run gets a fresh authority generation");
    check(!controller->arrival(owner,1,110),"retired run receipts cannot activate new attempt");
    cinematics();dialogue_rows();entrance();prologue_chain();
    std::printf("PASS: %u Homecoming launch identity, catalog, section graph, Lua entry, authority width, cinematic, dialogue, entrance and receipt checks\n",checks);
    return 0;
}
