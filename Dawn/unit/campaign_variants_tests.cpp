#include "state/activity/strike_pact/controller.h"
#include "state/activity/strike_pact/authority.h"
#include "strike_pact_boss_damage_tests.h"
#include "state/activity/coo/campaign_dialogue.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>
namespace m=dawn::state::activity::strike_pact;
namespace coo=dawn::state::activity::coo;
static unsigned checks{};
static void check(bool ok,const char* message) {++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
struct Wire {
    std::vector<std::pair<std::uint64_t,unsigned>> fields;std::size_t bits{};
    bool write(std::uint64_t v,unsigned n){fields.push_back({v,n});bits+=n;return true;}
    std::size_t bit_count()const{return bits;}
};
static std::unique_ptr<coo::script::MissionDocument> document(const char* name) {
    std::ifstream file(std::filesystem::path(__FILE__).parent_path().parent_path()/"scripts"/name);
    std::ostringstream text;text<<file.rdbuf();check(file.good()||file.eof(),"read shipped script");
    std::string error;auto result=coo::script::MissionDocument::parse_lua(text.str(),m::kProfile,error);
    if(!result) std::fprintf(stderr,"Lua: %s\n",error.c_str());
    check(result && m::valid_document(result->views()),"both Tree documents are authorized");return result;
}
#include "strike_pact_phase_tests.h"
#include "campaign_ordering_test_helpers.h"
static void tree_ordering(const coo::script::Views& v) {
    namespace t=campaign_ordering;using coo::Operation;
    const auto& arrival=t::step(v,"opening","arrival");
    check(arrival.dependencies==0&&t::has(arrival,Operation::dialogue,1)&&t::has(arrival,Operation::objective,m::kApproachGateway),"survivors exchange starts with initial objective");
    check(t::has(t::step(v,"opening","defense"),Operation::dialogue,0)&&t::after(v,"opening","defense","gate"),"coordinates lock on gateway approach");
    const auto* route=v.table("campaign_route");check(route&&route->dialogue.size()==4,"four independent traversal cues");
    const coo::DialogueBinding expected[]{
        {{m::kLighthouse,m::kLighthouseTag,60,5},5,0},{{m::kForestArea,m::kForestAreaTag,60,7},4,0},
        {{m::kChaseEntry,m::kChaseEntryTag,60,2},10,0},{{m::kBossApproach,m::kBossApproachTag,60,4},19,0}};
    for(const auto& e:expected){bool found{};for(const auto& b:route->dialogue)found|=b.asset==e.asset&&b.row==e.row&&b.delayMs==0;check(found,"campaign cue uses its native transition trigger");}
    const auto& mount=t::step(v,"chase","mount_up");
    check(t::has(mount,Operation::objective,m::kMountUp)&&t::has(mount,Operation::dialogue,13),"Sparrow cue accompanies Mount Up");
    check(!t::has(t::step(v,"boss","begin"),Operation::objective,m::kDefeatThuun),"scene request cannot announce Minotaur death");
    check(t::after(v,"boss","minotaur_line","minotaur_spawn")&&t::has(t::step(v,"boss","minotaur_line"),Operation::dialogue,20),"Minotaur cue waits for native spawn");
    const auto& reveal=t::step(v,"boss","thuun_revealed");
    check(t::after(v,"boss","thuun_revealed","minotaur_death")&&t::has(reveal,Operation::objective,m::kDefeatThuun)&&t::has(reveal,Operation::dialogue,21),"Thuun cue and objective share native Minotaur death");
    check(t::after(v,"boss","retreat3_line","retreat3")&&t::has(t::step(v,"boss","retreat3_line"),Operation::dialogue,24),"map hurry cue belongs to second retreat");
    check(t::after(v,"boss","access","dead")&&t::has(t::step(v,"boss","access"),Operation::dialogue,27),"map completion cue waits for final boss death");
    check(v.dialogue.rows[0].durationMs==7756&&v.dialogue.rows[1].durationMs==9794&&v.dialogue.rows[27].durationMs==2341,"campaign queue uses recovered branch lengths");
    for(const auto row:{6,11,22,25})check(v.dialogue.rows[row].durationMs==0,"empty campaign branches cannot occupy the queue");
}

int main(){
    for (const auto& variant : dawn::state::activity::strikes::kVariants) {
        check(coo::campaign_dialogue::value(variant.activity) == std::uint8_t{1},
            "every standard and Nightfall variant selects strike dialogue");
    }
    strike_pact_phase_tests();
    check(strike_pact_boss_damage_contracts(),"Thuun native identity, health-floor and immunity contracts");
    namespace voice=coo::campaign_dialogue;
    check(voice::value(296)==2 && voice::value(298)==2,"both campaign banks select native campaign conditions");
    check(voice::value(230)==1 && voice::value(229)==1,"both strike banks retain native strike conditions");
    check(!voice::value(-1) && !voice::value(300),"flag policy is limited to the four authored variants");
    for (const auto prior:{-1,0,1,2}) {
        voice::Flags flags{}; flags.rows[flags.count++]={70,2,0};
        if (prior>=0) { flags.rows[flags.count++]={voice::kFlag,static_cast<std::uint8_t>(prior),0}; }
        voice::Lease lease; bool changed{};
        check(lease.apply(flags,296,changed)&&lease.active&&flags.rows[1].value==2,"campaign lease selects campaign flag");
        check(lease.apply(flags,296,changed)&&!changed,"unchanged campaign does not recommit each frame");
        flags.rows[flags.count++]={71,1,0};
        check(lease.apply(flags,229,changed)&&changed&&flags.rows[1].value==1,"campaign to strike switches branch");
        check(lease.apply(flags,298,changed)&&changed&&flags.rows[1].value==2,"strike to second campaign switches branch");
        check(lease.apply(flags,-1,changed)&&!lease.active,"orbit retires the flag lease");
        check(flags.count==static_cast<unsigned>(prior>=0?3:2)&&flags.rows[0].slot==70
            &&flags.rows[flags.count-1].slot==71,"release preserves unrelated live overrides");
        if(prior>=0) check(flags.rows[1].value==prior,"release restores pre-existing flag value");
        check(lease.apply(flags,-1,changed)&&!changed,"orbit does not keep writing investment state");
    }
    voice::Flags full{}; full.count=100;
    for(unsigned i=0;i<100;++i) full.rows[i]={static_cast<std::uint16_t>(i+100),1,0};
    voice::Lease fullLease;bool changed{};
    check(!fullLease.apply(full,296,changed)&&!fullLease.active&&full.count==100,"full override storage cannot overflow");
    full.count=101;check(!fullLease.apply(full,296,changed),"malformed native override count rejected");
    check(voice::requested(266) && !voice::requested(265) && !voice::requested(288),
        "veteran override is scoped to Homecoming gameplay, not its video or Adieu");
    for(const auto prior:{-1,0,1,2}) {
        voice::Flags flags{};flags.rows[flags.count++]={70,2,0};
        for(const auto flag:voice::kFlags) if(flag!=voice::kFlag && prior>=0) {
            flags.rows[flags.count++]={flag,static_cast<std::uint8_t>(prior),0};
        }
        voice::SelectionLease lease;
        check(lease.apply(flags,266,changed) && changed==(prior!=2) && lease.active,"Homecoming leases all veteran choices atomically");
        for(const auto flag:voice::kFlags) if(flag!=voice::kFlag) {
            unsigned count{};for(std::size_t i=0;i<flags.count;++i) if(flags.rows[i].slot==flag) {++count;check(flags.rows[i].value==2,"veteran condition is true");}
            check(count==1,"one override per veteran condition");
        }
        check(lease.apply(flags,266,changed) && !changed,"standing in Homecoming does not repeatedly commit veteran flags");
        flags.rows[flags.count++]={71,1,0};
        check(lease.apply(flags,296,changed) && lease.active,"campaign selection restores veteran flags and retains its own policy");
        for(const auto flag:voice::kFlags) if(flag!=voice::kFlag) {
            unsigned count{};for(std::size_t i=0;i<flags.count;++i) if(flags.rows[i].slot==flag) {++count;check(flags.rows[i].value==prior,"previous veteran state restored");}
            check(count==static_cast<unsigned>(prior>=0),"absent veteran overrides are removed on departure");
        }
        check(lease.apply(flags,-1,changed) && !lease.active,"orbit releases every dialogue policy lease");
        check(flags.rows[0].slot==70 && flags.rows[flags.count-1].slot==71,"dialogue leases preserve unrelated live investment changes");
    }
    voice::Flags crowded{};crowded.count=98;
    for(unsigned i=0;i<98;++i) crowded.rows[i]={static_cast<std::uint16_t>(i+100),1,0};
    voice::SelectionLease veteranLease;
    check(!veteranLease.apply(crowded,266,changed) && !veteranLease.active && !changed && crowded.count==98,
        "insufficient room for all veteran flags leaves the entire native record unchanged");

    for(const bool campaign:{false,true}){
        auto d=document(campaign?"mission_pact.lua":"strike_pact.lua");
        if(campaign)tree_ordering(d->views());
        else check(d->views().dialogue.rows[0].durationMs==22201,"strike dialogue duration unchanged");
        bool scan{},scanDone{},access{},closing{};
        for(const auto& g:d->views().graphs){
            check(g.definition.steps.size()<=32,"authored step budget");
            for(const auto& step:g.definition.steps){
                check(step.commands.size()<=8,"authored parallel command budget");
                for(const auto& c:step.commands){
                    scan|=c.asset==m::kMissionAsset&&c.argument==80;
                    scanDone|=c.asset==m::kMissionAsset&&c.argument==82;
                    access|=c.operation==coo::Operation::objective&&c.argument==m::kAccessMap;
                    closing|=c.operation==coo::Operation::observation&&c.asset==m::kDialogueAsset&&c.argument==29;
                }
            }
        }
        check(scan==campaign&&scanDone==campaign&&access==campaign&&closing==campaign,"only campaign waits for map scan and final dialogue");
        m::Controller controller;check(controller.select(d->views(),91),"select own activity variant");
        check(controller.frame().campaign==campaign,"variant retained in controller");
        check(!controller.scan_request().enabled(),"selection alone cannot enable map interaction");
        controller.reset();check(!controller.scan_request().enabled(),"reset clears scan authority");
        m::Frame f{};f.enabled=true;f.spawnGeneration=7;f.campaign=campaign;f.presentation.published=true;
        Wire objective;const auto root=campaign?0x0F0A7E94U:m::kRoot;
        check(m::write_body(objective,f,root,68,0)&&objective.bits==4802,"actual variant root publishes native objective");
        const auto linkBits=m::body_bits(f,0x547F6321U,65,0);check(linkBits==(campaign?65U:0U),"scan link is campaign-scoped");
        f.boss.prepared=true;f.boss.dead=true;
        Wire bossExit;check(m::write_body(bossExit,f,m::kBoss,23,m::kBossRooms.back().exit),"final gate authority exists");
        // Keep the final room uncleared: the campaign objective must own access even
        // when optional adds remain. Other gates and strike clear rules stay intact.
        f.scan.armed=true;Wire mapExit;
        check(m::write_body(mapExit,f,m::kBoss,23,m::kBossRooms.back().exit),"map gate publishes");
        check((mapExit.fields!=bossExit.fields)==campaign,"only campaign scan opens uncleared final gate");
        Wire room2;check(m::write_body(room2,f,m::kBoss,23,m::kBossRooms[1].exit)&&room2.fields==bossExit.fields,"scan does not alter earlier room gates");
        f.scan.complete=true;Wire retainedExit;
        check(m::write_body(retainedExit,f,m::kBoss,23,m::kBossRooms.back().exit)&&retainedExit.fields==mapExit.fields,"scan completion retains open gate");
        f.scan={};
        if(campaign){
            Wire disabled;check(m::write_body(disabled,f,0x547F6321U,65,0)&&disabled.fields[1].first==0,"scan disabled before boss ending");
            f.scan.armed=true;Wire armed;check(m::write_body(armed,f,0x547F6321U,65,0)&&armed.bits==65&&armed.fields[0].first==0x80000008U&&armed.fields[1].first==1,"authored native scan generation and activation");
            f.scan.complete=true;Wire done;check(m::write_body(done,f,0x547F6321U,65,0)&&done.fields[1].first==0,"completed scan cannot be repeated");
        }
    }
    coo::CampaignScan scan;coo::ScanPlayback started{8,2,1,1.F,4.F},finished{8,2,0,4.F,4.F};
    check(!scan.observe(8,71,81,started,true),"unarmed scan rejected");scan.armed=true;
    check(!scan.observe(8,71,81,finished,true),"finish without prior native start rejected");
    check(!scan.observe(9,71,81,started,true),"stale generation rejected");
    check(!scan.observe(8,71,81,started,false),"start requires attached Ghost");
    check(scan.observe(8,71,81,started,true),"valid native start binds salted controller");
    check(!scan.observe(8,71,82,finished,true),"recycled handle rejected");
    check(!scan.observe(8,72,81,finished,true),"other controller rejected");
    check(scan.observe(8,71,81,finished,false)&&scan.complete,"matching native completion accepted");
    check(!scan.observe(8,71,81,finished,true),"duplicate completion ignored");
    for(const auto duration:{10000U,30000U}){
        coo::LifecycleService lifecycle;check(lifecycle.begin(91),"new completion owner");
        check(lifecycle.complete_timed(lifecycle.owner(),1000,duration),"start requested variant timer");
        check(!lifecycle.advance(1000+duration-1)&&lifecycle.activity_state()==6,"cannot finish early");
        check(lifecycle.advance(1000+duration)&&lifecycle.activity_state()==7,"native transition at exact deadline");
        Wire timer;check(coo::native_clock::countdown(timer,true,0,duration)&&timer.bits==386&&timer.fields[2].first==coo::native_clock::ticks(duration),"wire duration matches lifecycle deadline");
    }
    std::printf("PASS: %u campaign/strike documents, scan ownership, native roots and completion timer checks\n",checks);
}
