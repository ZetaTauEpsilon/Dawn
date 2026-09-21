#include "state/activity/vanilla/homecoming/entry.h"
#include "state/activity/vanilla/homecoming/continuation.h"
#include "state/activity/vanilla/homecoming/authority.h"
#include "state/activity/vanilla/homecoming/sense_adapter.h"
#include "state/activity/vanilla/homecoming/transit_rules.h"
#include "state/activity/vanilla/homecoming/entrance_native.h"
#include "state/activity/vanilla/homecoming/door_native.h"
#include "state/activity/vanilla/homecoming/ship_barrier.h"
#include "state/activity/vanilla/homecoming/console_scan.h"
#include "state/activity/vanilla/homecoming/registries.h"
#include "state/activity/vanilla/homecoming/prologue.h"
#include "state/activity/vanilla/homecoming/music.h"
#include "server/bap/encrypted/push/activity/vanilla/homecoming_roster.h"
#include "client/activity/campaign_openings.h"
#include "client/hooks/bootflow/gate_trace_cache.h"
#include "client/hooks/bootflow/opening_fade_scope.h"
#include "middleware/encoding/bit_writer.h"
#include "middleware/encoding/bit_reader.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <cstring>

namespace m=dawn::state::activity::vanilla::homecoming;
namespace coo=dawn::state::activity::coo;
namespace dawn::core::log { void write(Channel,Level,std::string_view) noexcept {} }
static unsigned checks{};
static void check(bool ok,const char* why) {
    ++checks;if(!ok) {std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}
}
#include "homecoming_fade_scope_tests.inl"
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
        check(m::expected(p)<=16,"expected admissions also fit the bounded actor ledger");
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
    const auto& military=m::mission().phases[static_cast<unsigned>(m::Section::military)].definition;
    std::uint32_t sightingDependencies{},sightingRequired{};
    for(std::size_t i=0;i<military.steps.size();++i) {
        const auto& step=military.steps[i];
        if(step.name=="command ship sighting") sightingDependencies=step.dependencies;
        if(step.name=="destroyer present" || step.name=="hangar window") sightingRequired|=1U<<i;
    }
    check(std::popcount(sightingRequired)==2 && (sightingDependencies&sightingRequired)==sightingRequired,
        "ship sighting joins the route sightline with actual ship presence, without a timer");
    check(m::kDialogue[72].sceneOwned,"Ikora performer exclusively owns the full Ghost/Zavala pickup exchange");
    std::array<unsigned,94> requests{};std::array<unsigned,2> wards{};
    for(std::size_t s=0;s<m::mission().phases.size();++s) {
        const auto& g=m::mission().phases[s].definition;
        check(!g.steps.empty() && g.steps.size()<=coo::Executor::kMaxSteps,"section graph fits the executor");
        for(const auto& step:g.steps) {
            check(step.commands.size()<=coo::Executor::kMaxCommands,"step command budget");
            for(const auto& c:step.commands) {
                if(c.operation==coo::Operation::dialogue) {
                    const auto row=m::dialogue_row(c.argument);
                    check(row<std::size(m::kDialogue) && !m::kDialogue[row].sceneOwned && m::kDialogue[row].durationMs,"dialogue commands name authored, natively timed cues");
                    ++requests[row];
                    if(s==static_cast<unsigned>(m::Section::plaza) || s==static_cast<unsigned>(m::Section::plazaWaves)) {
                        check(m::dialogue_delay(c.argument)==0,"plaza cues have no fixed pre-play delay");
                    }
                    if(row==37) check(step.name=="to the plaza","Red Legion conversation starts in the exit corridor, not at the ship reveal");
                    if(row==82) check(step.name=="stair impacts","Cayde status radio follows the actual stairwell milestone");
                    if(row==6) check(step.name=="Cabal purpose","opening explanation follows first-contact clearance");
                }
                if(s==static_cast<unsigned>(m::Section::plaza) || s==static_cast<unsigned>(m::Section::plazaWaves)) {
                    check(c.operation!=coo::Operation::eventAfter,"plaza does not add intermission padding to native scene/clearance waits");
                    if(c.operation==coo::Operation::device && c.asset==m::asset(m::kPlaza,4,18))
                        check(c.argument==0,"plaza only clears the standalone Ward; the native performer owns casting effects");
                    if(c.operation==coo::Operation::scene && c.asset==m::asset(m::kPlaza,43,4) && c.argument==0) {
                        ++wards[s-static_cast<unsigned>(m::Section::plaza)];
                    }
                    if(s==static_cast<unsigned>(m::Section::plaza) && c.asset==m::kDialogueAsset
                        && coo::is_observation(c.operation)) {check(c.argument!=51,"first assault cannot wait on the second assault's radio line");}
                }
                if(c.operation==coo::Operation::population) {check(c.asset==m::kModule && c.argument<std::size(m::kSpawnBatches)
                    && static_cast<std::size_t>(m::kSpawnBatches[c.argument].section)==s,"population commands request a checkpoint batch in its own section");}
                if(c.operation==coo::Operation::objective) {
                    bool known=false;for(const auto o:m::kObjectives) {known|=o==c.argument;}
                    check(known,"objective commands use authored directive events");
                }
                if(c.operation==coo::Operation::scene) {check(m::scene_index(c.asset)<std::size(m::kScenes),"scene commands name authored Scenes");}
            }
        }
    }
    check(requests[72]==0 && requests[53]==1 && requests[57]==1 && wards[0]==1 && wards[1]==1,
        "one owner for pickup exchange and one distinct shield warning per inter-wave barrage");
    check(requests[60]==0 && requests[61]==0,"delay-leaving reminders do not play when the player actually leaves");
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
    std::array<dawn::state::build_data::activities::Definition,268> rows{};
    rows[pro::kVideo].hash=pro::kVideoHash;rows[pro::kMission].hash=pro::kMissionHash;
    std::copy(m::kPackage.begin(),m::kPackage.end(),rows[pro::kMission].package.begin());
    check(pro::catalog_valid(rows),"the opening video precedes Homecoming in the installed catalog");
    rows[pro::kVideo].package[0]='x';check(!pro::catalog_valid(rows),"a named entry is not the opening video");
    pro::Sequence chain;chain.begin(100);
    check(chain.wanted()==pro::kVideo,"the Red War opening starts with the opening video activity");
    check(!chain.queued(pro::kMission,100) && chain.queued(pro::kVideo,100),"only the wanted activity can be queued");
    check(chain.wanted()<0 && chain.state().phase==pro::Phase::videoLoading,"the queued video waits for native playback");
    chain.video(39,pro::kVideo,false,true,110);
    check(chain.state().phase==pro::Phase::videoLoading,"a stale finished flag is not playback");
    chain.video(39,pro::kMission,true,false,110);
    check(chain.state().phase==pro::Phase::videoLoading,"another activity's playback is ignored");
    chain.video(39,pro::kVideo,true,false,120);
    check(chain.state().phase==pro::Phase::videoPlaying,"native playback is observed");
    chain.tick(120+500000);
    check(chain.state().phase==pro::Phase::videoPlaying,"the full-length opening does not time out");
    chain.video(39,pro::kVideo,false,true,150000);
    check(chain.state().phase==pro::Phase::videoReturning && chain.wanted()<0,"completion waits for the native chain");
    chain.video(29,-1,false,false,151000);chain.video(29,-1,false,false,160000);
    check(chain.wanted()<0,"a transient orbit step does not compete with the native chain");
    chain.selected(7,pro::kMission,0x12345678U,161000);
    check(chain.state().phase==pro::Phase::videoReturning,"a different scenario cannot bind the mission");
    chain.selected(0,pro::kMission,m::kScenario,161000);
    check(chain.state().phase==pro::Phase::videoReturning,"an absent run cannot bind the mission");
    chain.selected(7,pro::kMission,m::kScenario,161000);
    check(chain.state().phase==pro::Phase::missionLoading && chain.state().run==7 && chain.wanted()<0,"retail's mission selection is the arrival receipt");
    chain.video(29,-1,false,false,161000+pro::kOrbitGraceMs);
    check(chain.wanted()<0,"the loading mission is never relaunched");
    chain.complete();check(chain.state().phase==pro::Phase::complete,"arrival in Homecoming completes the chain");
    // Orbit after the video without the native chain: the adapter launches the mission.
    chain.begin(200);check(chain.queued(pro::kVideo,200),"the chain restarts from the video");
    chain.video(39,pro::kVideo,true,false,210);chain.video(39,pro::kVideo,false,true,220);
    chain.video(29,-1,false,false,230);chain.video(33,-1,false,false,240);chain.video(29,-1,false,false,250);
    chain.video(29,-1,false,false,250+pro::kOrbitGraceMs-1);
    check(chain.wanted()<0,"the orbit grace restarts with every step away from orbit");
    chain.video(29,-1,false,false,250+pro::kOrbitGraceMs);
    check(chain.wanted()==pro::kMission && chain.queued(pro::kMission,300000),"a lasting orbit step hands the mission to the launch adapter");
    chain.selected(9,pro::kMission,m::kScenario,300100);
    check(chain.state().phase==pro::Phase::missionLoading && chain.state().run==9,"the adapter's launch binds the same receipt");
    chain.begin(400);chain.tick(400+30000);check(chain.state().phase==pro::Phase::failed,"an unanswered video launch fails closed");
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
static void scene_source_and_counter_wire() {
    auto f=std::make_unique<m::Frame>();f->enabled=true;f->spawnGeneration=1;
    std::array<std::byte,2048> storage{};
    for(const auto& p:m::kSpawns) if(p.sceneOwned) for(unsigned phase=0;phase<8;++phase) {
        const auto a=m::asset(p.registry,1,p.source);auto& n=f->native[m::asset_index(a)];
        n.managed=true;n.active=(phase&1)!=0;n.sceneReleased=(phase&2)!=0;n.sourceCleared=(phase&4)!=0;n.generation=1;
        dawn::middleware::encoding::bits::Writer writer(storage);
        check(m::write_body(writer,*f,a.registry,1,a.slot),"scene source authority encodes");
        dawn::middleware::encoding::bits::Reader reader{std::span<const std::byte>(storage)};
        std::uint64_t mode{};check(reader.skip(writer.bit_count()-36) && reader.read(3,mode),"read exact final source-placement mode");
        check(mode==2,"scene ownership survives staging, entry, flag removal and death without creating an ordinary actor");
        dawn::middleware::encoding::bits::Reader countsReader{std::span<const std::byte>(storage)};
        std::uint64_t categories{},count{};
        check(countsReader.skip(m::tactical(p).registry?59:4) && countsReader.read(4,categories)
            && categories==p.categories,"scene request retains its authored category count");
        const auto expected=n.active && !n.sourceCleared?m::requests(p):std::array<std::uint8_t,4>{};
        for(unsigned i=0;i<p.categories;++i) {
            check(countsReader.read(32,count) && count==0x80000000ULL+expected[i],"dormant or killed scene cast never requests replacement population");
        }
    }
    f->presentation.published=f->presentation.active=true;
    for(const auto event:{m::kObjectives[7],m::kObjectives[11]}) for(unsigned count=0;count<=3;++count) {
        f->presentation.event=event;f->assaultsRepelled=static_cast<std::uint8_t>(count);
        for(unsigned i=0;i<3;++i) f->generatorDown[i]=i<count;
        dawn::middleware::encoding::bits::Writer writer(storage);
        check(m::write_body(writer,*f,m::kDirectiveAsset.registry,static_cast<std::uint8_t>(m::kDirectiveAsset.type),m::kDirectiveAsset.slot),"counted objective encodes");
        dawn::middleware::encoding::bits::Reader reader{std::span<const std::byte>(storage)};std::uint64_t current{},target{};
        dawn::middleware::encoding::bits::Reader variantReader{std::span<const std::byte>(storage)};std::uint64_t variant{};
        check(variantReader.skip(142) && variantReader.read(32,variant)
            && variant==0x80000000ULL+(event==m::kObjectives[11]?1U:0U),
            "turbines select authored counted variant 1; assaults retain counted variant 0");
        check(reader.skip(529) && reader.read(32,current) && reader.read(32,target),"read directive current and target fields");
        check(current==0x80000000ULL+count && target==0x80000003ULL,"assault and turbine HUD counters encode N of 3");
    }
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
    en::SweepGate gate;
    const m::EntranceRequest opening{owner,1,static_cast<std::uint8_t>(m::Section::underwatch)};
    check(!gate.due(opening,0x10000,0) && !en::repair(world,opening,[&] {return opening;}).rows,
        "opening sections with no entrance placements do not scan");
    unsigned scans{};
    for(std::uint64_t ms=0;ms<2000;++ms) {
        // Many devices per frame cannot multiply full-table scans.
        for(unsigned callback=0;callback<64;++callback) scans+=gate.due(ship,0x10000,ms)?1U:0U;
    }
    check(scans==4,"128000 unchanged device callbacks trigger only four fallback scans in two seconds");
    check(gate.due(ship,0x20000,1999),"a replacement entity table triggers fresh discovery immediately");
    check(gate.due(military,0x20000,1999),"a section transition triggers fresh discovery");
    auto replay=military;++replay.generation;
    check(gate.due(replay,0x20000,1999),"checkpoint generation changes invalidate the sweep schedule");
    ++replay.owner.run;
    check(gate.due(replay,0x20000,1999),"new runs do not inherit the old scan cooldown");
    check(gate.due(replay,0x20000,10),"clock rollback cannot indefinitely suppress fallback discovery");
    check(!gate.due({},0x20000,11) && gate.due(replay,0x20000,12),"inactive missions clear sweep state");
    world.local[2]=false;
    result=en::repair_callback(world,ship,2,[&] {return ship;});
    check(result.doors==1 && result.rows==1 && world.local[2],"a newly ticking door is repaired immediately without a sweep");
    check(!en::repair_callback(world,ship,2,[&] {return ship;}).doors,"already owned callbacks are idempotent");
    world.local[2]=false;
    check(!en::repair_callback(world,ship,0x2002,[&] {return ship;}).doors,"recycled callback handles cannot grant a replacement door");
    check(!en::repair_callback(world,ship,2,[&] {return military;}).doors,"changed request blocks a callback grant");
    check(!en::repair_callback(world,ship,5,[&] {return ship;}).doors,"unrelated doors are not granted by the fast path");
    struct RecycledWorld : World {
        unsigned reads{};
        bool row(std::size_t slot,en::Row& out) noexcept {
            if(!World::row(slot,out)) return false;
            if(++reads==2) out.entity+=0x2000;
            return true;
        }
    } recycled;
    recycled.rows=world.rows;
    check(!en::repair_callback(recycled,ship,2,[&] {return ship;}).doors && recycled.grants==0,
        "identity is rechecked immediately before a callback grant");
    struct UnstableWorld : World {bool stable() const noexcept {return false;}} unstable;
    unstable.rows=world.rows;
    check(!en::repair_callback(unstable,ship,2,[&] {return ship;}).doors,
        "a changing entity table blocks callback mutation");
    // A streaming door without a device tick is still found by fallback repair.
    result=en::repair(world,ship,[&] {return ship;});
    check(result.doors==1 && world.local[2],"fallback discovery retains repair of doors not yet receiving callbacks");
    const m::door_native::Row bazaar{0,5,5,0x80B505B2U,15,0x7B8AC80734FA725FULL},militaryGate{0,1,5,0x80F10442U,1,0x125D2DA09F611701ULL};
    check(m::door_native::placement(bazaar) && !m::door_native::placement(militaryGate),"the bazaar door placement is exact");
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
#include "homecoming_progression_tests.inl"
#include "homecoming_ship_barrier_tests.inl"
#include "homecoming_performance_tests.inl"
int main() {
    namespace openings=dawn::client::activity::mission_launch::openings;
    unsigned homecoming{};
    for(const auto& mission:openings::kMissions) if(mission.campaign==0 && openings::listed(mission) && mission.activity==266) {
        ++homecoming;check(mission.investmentHash==0x62D85FB3U && mission.destination.bubble==9 && mission.destination.sliceSet==72
            && !mission.destination.hasSpawnSetHash,"Homecoming launch is pinned to its installed identity and authored Underwatch arrival");
    }
    check(homecoming==1,"Red War lists Homecoming once");
    check(std::string_view(openings::kMissions[12].title)=="Exodus" && openings::mission_number(12)==2
        && openings::kMissions[12].activity==288 && openings::kMissions[12].investmentHash==0xB913ED3FU,
        "Exodus is mission 2 without changing the Adieu launch route");
    check(std::string_view(openings::kMissions[11].title)=="1AU" && openings::mission_number(11)==16
        && openings::kMissions[11].activity==281,"1AU remains its existing route but displays mission 16");
    check(openings::kDisplayOrder[0]==0 && openings::kDisplayOrder[1]==12 && openings::kDisplayOrder[2]==11,
        "launcher presents Homecoming, Exodus, then 1AU in campaign order");
    std::array<bool,openings::kMissions.size()> seen{};
    for(const auto index:openings::kDisplayOrder) {
        check(index<seen.size() && !seen[index],"display order contains every launch route exactly once");seen[index]=true;
        if(openings::kMissions[index].campaign==1) check(openings::mission_number(index)==index,"Osiris numbering unchanged");
        if(openings::kMissions[index].campaign==2) check(openings::mission_number(index)==index-8,"strike numbering unchanged");
    }
    check(openings::mission_number(openings::kMissions.size())==0,"invalid launcher index has no label");
    catalog();graphs();ship_barriers();performance_regressions();
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
    check(controller->frame().activeRow==1,"Ghost's first line is offered on the first gameplay update, with no seven-second delay");
    check(controller->frame().spawnCheckpoints.count()==1 && controller->frame().spawnCheckpoints[0],"only the hallway batch starts on landing");
    for(const auto slot:{6,8,9,20,22,23,28,30,32}) {
        check(!controller->frame().native[m::asset_index(m::asset(m::kUnderwatch,1,static_cast<std::uint16_t>(slot)))].active,
            "later hallway Cabal wait for their individual route triggers");
    }
    for(const auto slot:{10,18,17,64,14}) {
        check(!controller->frame().native[m::asset_index(m::asset(m::kUnderwatch,43,static_cast<std::uint16_t>(slot)))].active,
            "checkpoint cast preparation does not play hero scenes ahead of their triggers");
    }
    check(!controller->frame().native[m::asset_index(m::asset(m::kUnderwatch,1,39))].managed,
        "live-identified Shaxx doorway civilian is excluded from landing population");
    check(!controller->frame().native[m::asset_index(m::asset(m::kUnderwatch,1,15))].managed,
        "Shaxx is not spawned at landing");
    for(const auto member:m::kUnderwatchCast) {
        check(controller->frame().native[m::asset_index(m::asset(member.registry,1,member.slot))].active,"ambient crash-site cast remains present on landing");
    }
    // Cross only the first-contact trigger; later reinforcement batches stay dormant.
    for(const auto& v:m::kVolumes) if(v.asset==m::trigger_area(m::kUnderwatch,"pt_wall_explode")) {
        m::Point p{};for(const auto& q:v.vertices) {p.x+=q.x;p.y+=q.y;}
        p.x/=static_cast<float>(v.vertices.size());p.y/=static_cast<float>(v.vertices.size());p.z=(v.min.z+v.max.z)*.5F;
        controller->position(1,p);
    }
    check(controller->advance(1,51,true),"first contact trigger requests its local batch");
    check(controller->frame().spawnCheckpoints[static_cast<unsigned>(m::SpawnCheckpoint::firstContact)]
        && !controller->frame().spawnCheckpoints[static_cast<unsigned>(m::SpawnCheckpoint::centurionRush)]
        && !controller->frame().spawnCheckpoints[static_cast<unsigned>(m::SpawnCheckpoint::dropPod)],"opening checkpoints remain independent");
    const auto hallwayGeneration=controller->frame().native[m::asset_index(m::asset(m::kUnderwatch,1,8))].generation;
    const auto hallwayRevision=controller->frame().revision;
    for(unsigned repeat=0;repeat<20;++repeat) {check(controller->advance(1,51+repeat,true),"standing at a checkpoint is safe");}
    check(controller->frame().native[m::asset_index(m::asset(m::kUnderwatch,1,8))].generation==hallwayGeneration
        && controller->frame().revision==hallwayRevision,"repeated checkpoint updates do not request a second batch or re-arm actors");
    const auto backup=m::asset(m::kUnderwatch,1,8);
    for(unsigned i=0;i<m::expected(m::kSpawns[m::spawn_index(backup)]);++i) {
        const m::EnemyReceipt enemy{1,1000+i,2000,hallwayGeneration,8,m::kUnderwatch};
        check(controller->admitted(enemy) && controller->died(enemy),"triggered hallway Cabal can be cleared before the next route trigger");
    }
    check(controller->frame().native[m::asset_index(backup)].sourceCleared,"early hallway deaths remain terminal");
    for(unsigned repeat=0;repeat<10;++repeat) {static_cast<void>(controller->advance(1,80+repeat,true));}
    check(controller->frame().native[m::asset_index(backup)].sourceCleared
        && controller->frame().native[m::asset_index(backup)].generation==hallwayGeneration,"waiting at a checkpoint cannot resurrect a cleared batch member");
    check(controller->frame().section==static_cast<std::uint8_t>(m::Section::underwatch) && controller->frame().bubble==9,"gameplay starts in the Underwatch");
    check(controller->frame().presentation.published && controller->frame().presentation.event==m::kObjectives[0],"the first directive is published on arrival");
    check(controller->frame().musicSection==m::music_section::firstCabal && m::music::section(controller->frame())==m::music_section::firstCabal,"the first-contact trigger advances the score");
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
    check(controller->frame().spawnCheckpoints.none(),"reset clears all encounter checkpoint latches");
    check(controller->select(1,100) && controller->owner()!=owner,"replayed run gets a fresh authority generation");
    check(!controller->arrival(owner,1,110),"retired run receipts cannot activate new attempt");
    cinematics();dialogue_rows();centurion_spawn_regression();civilian_door_run_regression();scene_source_and_counter_wire();entrance();prologue_chain();progression();opening_fade_test::run();
    std::printf("PASS: %u Homecoming launch identity, catalog, section graph, Lua entry, authority width, cinematic, dialogue, entrance and receipt checks\n",checks);
    return 0;
}
