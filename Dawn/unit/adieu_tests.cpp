#include "state/activity/vanilla/adieu/presentation.h"
#include "state/activity/vanilla/adieu/authority.h"
#include "state/activity/vanilla/adieu/transit_rules.h"
#include "state/activity/vanilla/adieu/starting_loadout.h"
#include "middleware/encoding/bit_writer.h"
#include "middleware/encoding/bit_reader.h"
#include "client/activity/mission_launch_options.h"
#include <cstdio>
#include <cstdlib>

namespace ad=dawn::state::activity::vanilla::adieu;
namespace coo=dawn::state::activity::coo;
static unsigned checks{};
static void require(bool value,const char* label) {
    ++checks;if(!value) {std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);}
}
static void starting_loadout_tests() {
    namespace start=ad::starting_loadout;
    using Item=dawn::state::account::inventory::Item;
    using Slot=dawn::state::account::inventory::EquipmentSlot;
    static dawn::state::CharacterState character{},original{};
    const auto item=[](std::uint64_t id,std::uint32_t hash) {
        Item value{};value.instanceSoid=id;value.definitionHash=hash;value.quantity=1;value.mutationSerial=1;
        value.sockets.policy=dawn::state::account::inventory::SocketPolicy::nativeDefaults;return value;
    };
    const auto classify=[](std::uint32_t hash) {
        if(hash==100 || hash==101 || hash==102 || hash==1195725819 || hash==start::kSidearm) return start::ItemKind::weapon;
        return hash==200?start::ItemKind::other:start::ItemKind::unknown;
    };
    for(std::size_t i=0;i<character.equipment.slots.size();++i)
        character.equipment.slots[i]=item(10+i,i<3?static_cast<std::uint32_t>(100+i):200U);
    character.inventory.count=5;
    character.inventory.values[0]=item(1000,100);
    character.inventory.values[1]=item(1001,200);
    character.inventory.values[2]=item(1002,101);character.inventory.values[2].postmaster=true;
    character.inventory.values[3]=item(1003,200);character.inventory.values[3].postmaster=true;
    character.inventory.values[4]=item(1004,start::kSidearm);
    original=character;
    const auto sidearm=item(9999,start::kSidearm);
    character.inventory.values[4].definitionHash=999;
    const auto unresolved=character;
    require(!start::prepare(character,sidearm,classify) && character==unresolved,
        "unknown carried item leaves the entire starting loadout unchanged");
    character=original;
    auto wrong=sidearm;wrong.definitionHash=2362471601U;
    require(!start::prepare(character,wrong,classify) && character==original,"the previously used same-name sidearm variant is rejected");
    require(start::kSidearm==53159281U,"starting sidearm matches the user's captured equipped definition");
    start::Once once;
    require(!once.apply(0,[] {return true;}),"no inventory reset without a selected run");
    require(!once.apply(42,[] {return false;}) && !once.applied(42),"failed startup remains retryable");
    require(once.apply(42,[&] {return start::prepare(character,sidearm,classify);}),"starting loadout commits once");
    require(character.equipment.slots[static_cast<std::size_t>(Slot::kinetic)]==sidearm
        && !character.equipment.slots[static_cast<std::size_t>(Slot::energy)]
        && !character.equipment.slots[static_cast<std::size_t>(Slot::heavy)],"only Traveler's Chosen Damaged is equipped");
    for(std::size_t i=3;i<character.equipment.slots.size();++i)
        require(character.equipment.slots[i]==original.equipment.slots[i],"armor, Ghost, subclass and travel gear survive weapon reset");
    require(character.inventory.count==2 && character.inventory.values[0]==original.inventory.values[1]
        && character.inventory.values[1]==original.inventory.values[3],"all carried weapons removed; other inventory and recovery items retained");
    require(std::all_of(character.inventory.values.begin()+2,character.inventory.values.end(),[](const Item& value) {return value==Item{};}),
        "removed weapon rows cannot remain in the inventory tail");
    character.equipment.slots[static_cast<std::size_t>(Slot::energy)]=item(10000,1195725819);
    character.inventory.values[character.inventory.count++]=item(10001,102);
    const auto collected=character;
    unsigned retries{};
    for(unsigned i=0;i<10;++i) require(!once.apply(42,[&] {++retries;return start::prepare(character,sidearm,classify);}),
        "same run never resets on another poll, transition, or respawn");
    require(retries==0 && character==collected,"mission pickups remain equipped and in inventory");
    require(once.apply(43,[&] {return start::prepare(character,sidearm,classify);})
        && !character.equipment.slots[static_cast<std::size_t>(Slot::energy)] && character.inventory.count==2,
        "a new Adieu run resets the starting weapons again");
}
int main() {
    starting_loadout_tests();
    namespace launch=dawn::client::activity::mission_launch;
    // Installed Adieu scenario: four bubbles, four states in the Journey bubble.
    static dawn::state::build_data::scenarios::Definition openingLayout{};
    constexpr auto opening=dawn::state::activity::forced::profiles::kAdieuOpening;
    openingLayout.name=opening.packageName;openingLayout.nameLength=opening.packageNameLength;
    openingLayout.tag=ad::kScenario;openingLayout.bubbleCount=4;openingLayout.bubbleStateCounts[3]=4;
    require(launch::validate_manual(opening,openingLayout,{})==launch::ManualError::none,
        "Adieu title-card opening accepted without a forced spawn");
    auto invalidOpening=opening;invalidOpening.hasSpawnSetHash=true;
    require(launch::validate_manual(invalidOpening,openingLayout,{})==launch::ManualError::spawn,
        "absent spawn sentinel cannot be advertised as a forced spawn");
    ad::Presentation p;coo::LifecycleService life;
    require(life.begin(42,128),"reserve owner");const auto first=life.owner();
    require(p.begin(first),"begin presentation");
    const auto scene=ad::asset(ad::kMain,43,102);
    require(p.request_scene(first,scene),"request finding Ghost");
    require(p.bits(scene)==0,"unbound object cast cannot publish a selector");
    require(!p.input(first,scene,0x3DCFB316),"native emitted event cannot be an input");
    require(!p.input(first,scene,123),"undeclared input rejected");
    require(p.input(first,scene,0x86685186),"collapse event declared");
    const auto revision=p.scene(scene)->revision;
    require(p.input(first,scene,0x86685186) && p.scene(scene)->revision==revision,"input idempotency");
    ad::scene_wire::Output receipt{};receipt.delta=true;receipt.state=1;
    receipt.generationWire=first.value^0x80000000U;receipt.hasSourceRevision=true;receipt.sourceRevision=revision;
    receipt.completed=true;
    require(!p.observe(first,scene,receipt) && !p.scene(scene)->started,"echo cannot establish playback");
    ad::PlaybackReceipt playing{{first,scene,first.value},7,8};
    require(!p.playback(playing),"selector cannot start before object cast is ready");
    require(p.arm(first,scene),"object readiness arms selector");
    const ad::PlaybackRequest completionRequest{first,scene,first.value,7,8};
    const auto terminal=ad::completed_playback(completionRequest,first.value,1,UINT32_MAX);
    require(terminal.completed && !p.playback(terminal),"native completion cannot establish unobserved playback");
    require(!ad::completed_playback(completionRequest,first.value+1,1,UINT32_MAX).completed,
        "native completion rejects an older scene generation");
    require(!ad::completed_playback(completionRequest,first.value,0,UINT32_MAX).completed
        && !ad::completed_playback(completionRequest,first.value,1,7).completed,
        "selector disappearance or a live selector alone cannot establish completion");
    require(!ad::completed_playback({first,scene,first.value},first.value,1,UINT32_MAX).completed,
        "completed scene needs its previously observed selector identity");
    require(p.bits(scene)==coo::native_scene::cast_bits(ad::Presentation::source_cast(ad::kScenes[ad::Presentation::scene_index(scene)]).count),
        "early trigger is withheld until selector startup");
    require(p.playback(playing),"qualified selector starts scene");
    {
        auto finished=p;auto staleTerminal=terminal;staleTerminal.serial++;
        require(!finished.playback(staleTerminal),"stale selector completion cannot finish the current scene");
        require(finished.playback(terminal) && finished.scene(scene)->completed,
            "native completed component survives destruction of the known selector");
        require(finished.stop_scene(first,scene) && !finished.playback(terminal),
            "late completion cannot restart a retired scene");
    }
    require(p.scene(scene)->revision==revision+1,"startup publishes queued input on a new revision");
    receipt.sourceRevision=p.scene(scene)->revision;
    auto stale=playing;stale.serial=9;
    require(!p.playback(stale),"selector serial replacement rejected");
    receipt.eventCount=2;receipt.events[0]=0x86685186;receipt.events[1]=0x3DCFB316;
    require(p.observe(first,scene,receipt),"qualified scene output");
    require(!p.output(scene,0x86685186) && p.output(scene,0x3DCFB316),"input echo is not output");
    require(!p.scene(scene)->performanceFinished,"scene terminal flag is not performance completion");
    playing.speech.set(ad::kReunionDialogueRow);
    require(p.playback(playing) && !p.scene(scene)->speechFinished[ad::kReunionDialogueRow],
        "speech startup does not establish dialogue completion");
    playing.speechFinished.set(ad::kReunionDialogueRow);
    require(p.playback(playing) && p.scene(scene)->speechFinished[ad::kReunionDialogueRow],
        "native finished speech action is retained independently of animation");
    playing.speechFinished.reset();
    require(p.playback(playing) && p.scene(scene)->speechFinished[ad::kReunionDialogueRow],
        "dialogue completion survives later partial receipts");
    require(!p.effect(first,17,true) && !p.effect(first,10,true),"Ghost targets cannot become player targets");
    require(p.effect(first,0,true),"wounded player effect");
    require(!p.music(first,7) && p.music(first,2),"music selection range");
    std::array<std::byte,4096> buffer{};
    const auto verify=[&](coo::Asset asset) {
        dawn::middleware::encoding::bits::Writer writer(buffer);
        require(p.write(writer,asset),"native writer accepts body");
        require(writer.bit_count()==p.bits(asset),"native width matches body");
    };
    verify(scene);verify(ad::asset(ad::kMain,26,0));verify(ad::kMusicAsset);
    require(!p.ghost_effect(first,18,true),"Ghost effects cannot target the player healing slot");
    require(p.ghost_effect(first,17,true),"Ghost healing effect");
    verify(ad::asset(ad::kMain,26,17));verify(ad::asset(ad::kMain,34,246));
    {
        dawn::middleware::encoding::bits::Writer writer(buffer);
        require(p.write(writer,ad::asset(ad::kMain,34,246)),"Ghost collection encodes");
        dawn::middleware::encoding::bits::Reader reader(buffer);std::uint64_t value{};
        require(reader.read(4,value) && value==1 && reader.read(1,value) && value==1
            && reader.read(32,value) && value==0x80809579U && reader.read(2,value) && value==1
            && reader.read(32,value) && value==ad::kMain && reader.read(7,value) && value==5
            && reader.read(16,value) && value==32768U+118U,"Ghost selector resolves only the authored Ghost object");
    }
    {
        std::array<std::byte,128> firstBytes{},repeatBytes{};
        dawn::middleware::encoding::bits::Writer initial(firstBytes),repeat(repeatBytes);
        require(p.write(initial,ad::asset(ad::kMain,26,17)) && p.ghost_effect(first,17,true)
            && p.write(repeat,ad::asset(ad::kMain,26,17)) && firstBytes==repeatBytes,
            "repeated native cue does not restart the Ghost effect");
        dawn::middleware::encoding::bits::Reader reader(firstBytes);std::uint64_t value{};
        require(reader.read(1,value) && value==1 && reader.read(1,value) && value==0 && reader.skip(128)
            && reader.read(32,value) && value==ad::kMain && reader.read(7,value) && value==35
            && reader.read(16,value) && value==32768U+246U && reader.read(1,value) && value==0,
            "Ghost effect is a one-shot linked collection without a player selector");
    }
    require(p.stop_scene(first,scene),"stop");
    require(!p.playback(playing) && !p.observe(first,scene,receipt),"stopped scene ignores delayed receipts");
    verify(scene);
    life.reset();require(life.begin(42,128),"reuse run allocates new owner");
    require(life.owner()!=first && p.begin(life.owner()),"reset presentation");
    require(!p.request_scene(first,scene) && !p.playback(playing),"old run incarnation rejected");
    ad::Controller controller;
    require(controller.select(100,10),"select mission");const auto owner=controller.owner();
    std::uint64_t now=20;
    const auto musicIs=[&](std::uint8_t candidate) {
        std::array<std::byte,1024> bytes{};
        dawn::middleware::encoding::bits::Writer writer(bytes);
        require(controller.frame().native.write(writer,ad::kMusicAsset),"music authority remains published");
        dawn::middleware::encoding::bits::Reader reader(bytes);
        for(unsigned i=0;i<4;++i) {
            std::uint64_t word{};require(reader.read(32,word) && word==(i==0?1U<<candidate:0U),"expected native music candidate");
        }
    };
    const auto movie=[&](std::uint8_t index,bool skip=false) {
        require(controller.arrival(owner,index+1,++now),"movie destination arrived");
        require(!controller.cinematic(owner,{1685,ad::kMovies[(index+1)%3].registry,1,6,0},++now),"foreign movie rejected");
        require(controller.cinematic(owner,{5239,ad::kMovies[index].registry,1,6,0},++now),"native movie starts");
        if(skip) {
            require(controller.cinematic(owner,{3338,ad::kMovies[index].registry,3,6,0},++now),"skip requests native stop");
            require(!controller.frame().finished,"skip alone cannot complete mission");
        }
        require(controller.cinematic(owner,{1685,ad::kMovies[index].registry,2,6,0},++now),"native movie ends");
        if(index==0) require(controller.arrival(owner,4,++now),"gameplay destination arrived");
    };
    movie(0);
    musicIs(1);
    require(controller.advance(owner.run,100,true),"start activity clock");
    require(controller.advance(owner.run,1100,true) && controller.frame().gameplayClockTicks==coo::native_activity_ticks(1000),
        "native clock advances during traversal");
    const auto clock=controller.frame().gameplayClockTicks;
    require(controller.advance(owner.run,1000,true) && controller.frame().gameplayClockTicks==clock,"native clock never moves backwards");
    now=1100;
    const auto visit=[&](std::string_view name) {
        for(const auto& v:ad::kVolumes) if(v.name==name) {
            ad::Point center{};for(const auto pnt:v.polygon) {center.x+=pnt.x;center.y+=pnt.y;}
            center.x/=static_cast<float>(v.polygon.size());center.y/=static_cast<float>(v.polygon.size());center.z=(v.min.z+v.max.z)*.5F;
            require(ad::contains(v,center),"fixture inside native volume");controller.position(owner,center);return;
        }
        require(false,"volume exists");
    };
    const auto bindCast=[&](coo::Asset scene) {
        const auto& binding=ad::kScenes[ad::Presentation::scene_index(scene)];
        for(const auto object:binding.cast) if(object.type==4) {
            const auto i=ad::object_index(object);
            if(controller.frame().objects[i].phase==coo::ObjectPhase::prepare)
                require(controller.prepared(owner,object),"scene object preparation");
            require(controller.object({{owner.run,controller.frame().objects[i].generation},object,
                static_cast<std::uint32_t>(7000+i),static_cast<std::uint32_t>(8000+i)}),"scene object readiness");
        }
    };
    const auto beforeExit=controller;
    visit("tv_goto_climb");require(controller.frame().cinematic.route()==0,"city exit requires healing");
    controller=beforeExit;
    visit("tv_scene_rooftop");
    const auto rooftop=ad::asset(ad::kMain,43,94);
    const auto* march=controller.frame().native.scene(rooftop);
    require(march->count==2 && march->inputs[0]==0xDD6D986FU && march->inputs[1]==0x5A9CAA14U,
        "rooftop creates the lead Cabal and releases his separate native movement gate");
    visit("tv_scene_rooftop");
    require(controller.frame().native.scene(rooftop)->count==2,"patrol cues publish once on repeated volume visits");
    visit("tv_ghost_finding");visit("tv_player_collapse");
    bindCast(ad::kFinding);
    auto ghost=ad::PlaybackReceipt{{owner,ad::kFinding,owner.value},77,88};
    require(controller.playback(ghost,++now),"Ghost playback");
    require(!controller.frame().healed,"running Ghost does not complete healing");
    const auto output=[&](ad::PlaybackReceipt& r,std::uint32_t event) {
        const auto& binding=ad::kScenes[ad::Presentation::scene_index(r.request.scene)];
        for(std::size_t i=0;i<binding.events.size();++i) if(binding.events[i]==event) {r.outputs.set(i);return;}
        require(false,"native output fixture exists");
    };
    now+=10000;output(ghost,0x3DCFB316);
    require(controller.playback(ghost,now),"native Ghost healing cue");
    const auto healStart=controller.frame().sequenceStartTicks[5];
    require(healStart>0 && healStart==controller.frame().gameplayClockTicks,"late healing captures current activity time");
    {
        dawn::middleware::encoding::bits::Writer writer(buffer);
        require(ad::write_body(writer,controller.frame(),ad::kMain,5,133),"healing sequence authority");
        dawn::middleware::encoding::bits::Reader reader(buffer);std::uint64_t value{};
        require(reader.read(64,value) && value==healStart && reader.read(64,value) && value==UINT64_MAX
            && reader.read(8,value) && value==1,"healing starts at its cue with no premature end time");
    }
    require(controller.playback(ghost,++now) && controller.frame().sequenceStartTicks[5]==healStart,
        "repeated healing receipt does not restart the animation");
    auto leaving=controller;
    output(ghost,0xB01280FE);
    require(controller.playback(ghost,++now) && !controller.frame().ghostRetired,"transmat cue retains Ghost through the child performance");
    require(controller.frame().native.bits(ad::asset(ad::kMain,26,20))==186,"transmat effect is published to Ghost");
    ghost.performanceFinished=true;require(controller.playback(ghost,++now),"natural healing performance");
    require(controller.frame().healed,"healing unlocks escape");
    require(!controller.frame().ghostRetired && !controller.frame().native.scene(ad::kFinding)->stopped
        && controller.frame().objects[ad::object_index(ad::asset(ad::kMain,4,118))].create,
        "revive animation completion preserves Ghost and his continuing dialogue");
    const auto waitingForSpeech=controller;
    visit("tv_dlg_city_exit");visit("tv_goto_climb");
    require(controller.advance(owner.run,now+60000,true) && !controller.frame().ghostRetired
        && controller.frame().cinematic.route()==0 && controller.frame().activeRow==coo::kNoDialogue,
        "elapsed time, queued city speech and an early exit cannot interrupt unobserved reunion speech");
    controller=waitingForSpeech;
    ghost.speech.set(ad::kReunionDialogueRow);
    require(controller.playback(ghost,++now),"final reunion dialogue starts");
    const auto voiceEnd=now+ad::kDialogue[ad::kReunionDialogueRow].durationMs+250;
    const auto speaking=controller;
    auto staleDialogue=ghost;staleDialogue.serial++;staleDialogue.speechFinished.set(ad::kReunionDialogueRow);
    require(!controller.playback(staleDialogue,now+1),"foreign selector cannot finish reunion dialogue");
    require(controller.advance(owner.run,voiceEnd+5000,true) && controller.frame().ghostRetired,
        "native departure cue and full voice duration survive a missed final speech-node callback");
    controller=speaking;
    auto priorLine=ghost;priorLine.speechFinished.set(15);
    require(controller.playback(priorLine,++now) && !controller.frame().ghostRetired,
        "completion of an earlier line cannot retire Ghost");
    ghost.speechFinished.set(ad::kReunionDialogueRow);
    require(controller.playback(ghost,++now) && !controller.frame().ghostRetired,
        "early native action completion still reserves the final audio window");
    require(controller.playback(ghost,voiceEnd-1) && !controller.frame().ghostRetired,
        "repeated dialogue receipts neither cut audio short nor extend its window");
    const auto beforeVoiceEnd=controller;
    visit("tv_goto_climb");
    require(controller.frame().cinematic.route()==0 && !controller.frame().ghostRetired,
        "section cleanup cannot bypass an active reunion voice window");
    require(controller.advance(owner.run,voiceEnd,true) && controller.frame().ghostRetired
        && controller.frame().cinematic.route()==5,"early city exit resumes after Ghost finishes speaking");
    controller=beforeVoiceEnd;now=voiceEnd;
    require(controller.advance(owner.run,now,true),"final reunion audio window ends");
    require(controller.frame().ghostRetired && controller.frame().native.scene(ad::kFinding)->stopped
        && !controller.frame().objects[ad::object_index(ad::asset(ad::kMain,4,118))].create,
        "completed revive and final speech retire Ghost without requiring the parent return cue");
    {
        auto captured=waitingForSpeech;
        auto nativeVoice=ghost;nativeVoice.speech.reset();nativeVoice.speechFinished.reset();
        for(const auto row:{9U,11U,14U,15U}) {nativeVoice.speech.set(row);nativeVoice.speechFinished.set(row);}
        require(captured.playback(nativeVoice,now) && !captured.frame().ghostRetired,
            "captured CA00 speech mask protects actual reunion lines without inventing row 16");
        const auto actualVoiceEnd=now+ad::kDialogue[14].durationMs+250;
        require(captured.advance(owner.run,actualVoiceEnd-1,true) && !captured.frame().ghostRetired,
            "actual spoken-line duration is protected until its final millisecond");
        require(captured.advance(owner.run,actualVoiceEnd,true) && captured.frame().ghostRetired
            && !captured.frame().native.scene(ad::kFinding)->speech[16],
            "captured City stall clears without waiting for the dormant dialogue row");
    }
    {
        // PID 60008: native Scene complete=1, selector=FFFFFFFF, controller
        // outputs=0C (no departure), speech=CA00, finishedSpeech=4A00.
        const auto saved=controller;controller=leaving;
        auto nativeVoice=ghost;nativeVoice.outputs=0x0C;nativeVoice.speech=0xCA00;
        nativeVoice.speechFinished=0x4A00;nativeVoice.completed=false;
        require(controller.playback(nativeVoice,now) && controller.frame().healed,
            "replay native self-destruction case without a final event or speech callback");
        visit("tv_goto_climb");
        const auto pending=controller;
        const auto actualVoiceEnd=now+(std::max)(ad::kDialogue[14].durationMs,ad::kDialogue[15].durationMs)+250;
        const auto completion=ad::completed_playback(
            {owner,ad::kFinding,owner.value,nativeVoice.selector,nativeVoice.serial},owner.value,1,UINT32_MAX);
        require(controller.playback(completion,actualVoiceEnd-1) && !controller.frame().ghostRetired
            && controller.frame().cinematic.route()==0,"scene destruction does not cut the remaining voice window short");
        require(controller.advance(owner.run,actualVoiceEnd,true) && controller.frame().ghostRetired
            && controller.frame().cinematic.route()==5,"native completion releases the City exit at the voice boundary");
        controller=pending;
        require(controller.advance(owner.run,actualVoiceEnd+60000,true) && !controller.frame().ghostRetired,
            "elapsed time still cannot invent scene completion or a departure cue");
        auto foreign=completion;foreign.request.owner.run++;
        require(!controller.playback(foreign,actualVoiceEnd+60000),"a different run cannot complete the reunion");
        require(controller.playback(completion,actualVoiceEnd+60001) && controller.frame().ghostRetired
            && controller.frame().cinematic.route()==5,"late observation of actual completion clears the captured stall");
        controller=saved;
    }
    const auto retirementGeneration=[&](std::uint16_t slot) {
        dawn::middleware::encoding::bits::Writer writer(buffer);
        require(ad::write_body(writer,controller.frame(),ad::kMain,4,slot),"Ghost object retirement authority");
        dawn::middleware::encoding::bits::Reader reader(buffer);std::uint64_t generation{},active{};
        require(reader.read(32,generation) && reader.skip(32) && reader.read(1,active) && active==0,
            "retired deferred Ghost source cannot spawn again");
        return static_cast<std::uint32_t>(generation)^0x80000000U;
    };
    for(const auto slot:{std::uint16_t{118},std::uint16_t{119}}) {
        if(!controller.frame().managedObjects[ad::object_index(ad::asset(ad::kMain,4,slot))]) {
            require(ad::body_bits(controller.frame(),ad::kMain,4,slot)==0,"unused Ghost object remains unpublished");
            continue;
        }
        const auto before=leaving.frame().objects[ad::object_index(ad::asset(ad::kMain,4,slot))].generation;
        require(retirementGeneration(slot)>before,"deferred Ghost retirement crosses native 9F19F0's strict generation gate");
    }
    const auto retiredGeneration=retirementGeneration(118);
    require(!controller.object({{owner.run,retiredGeneration-1},ad::asset(ad::kMain,4,118),7000,8000}),
        "old native creation receipt cannot revive a retired Ghost");
    output(ghost,0xAFCD176B);
    require(!controller.playback(ghost,++now),"late reunion receipt cannot recreate Ghost");
    visit("tv_ghost_exit_1");visit("tv_ghost_exit_2");visit("tv_ghost_exit_3");
    require(!controller.frame().fault && controller.frame().ghostRetired,
        "backtracking through search triggers after the reunion cannot resurrect Ghost or fault the mission");
    require(retirementGeneration(118)==retiredGeneration,"repeated cleanup uses one stable retirement generation");
    {
        dawn::middleware::encoding::bits::Writer writer(buffer);
        require(ad::write_body(writer,controller.frame(),ad::kMain,5,133),"retired healing sequence remains published");
        dawn::middleware::encoding::bits::Reader reader(buffer);std::uint64_t value{};
        require(reader.skip(128) && reader.read(8,value) && value==255,"Ghost cleanup explicitly withdraws the healing sequence");
    }
    for(const auto slot:{90U,91U}) {
        dawn::middleware::encoding::bits::Writer writer(buffer);
        require(ad::body_bits(controller.frame(),ad::kMain,1,static_cast<std::uint16_t>(slot))==coo::native_combatant::kSourceBits
            && ad::write_body(writer,controller.frame(),ad::kMain,1,static_cast<std::uint16_t>(slot)),"Ghost source retirement has full native authority");
        std::array<std::byte,4096> expected{};dawn::middleware::encoding::bits::Writer retired(expected);
        coo::native_combatant::Source source{};source.registry=ad::kMain;source.generation=owner.value+1;
        source.hasRule=false;source.retireOwned=true;
        require(coo::native_combatant::write_source(retired,source) && buffer==expected,
            "Ghost source retirement advances generation and destroys retained scene entities");
    }
    visit("tv_goto_climb");
    require(retirementGeneration(118)==retiredGeneration,"section cleanup preserves the Ghost retirement generation");
    const auto completed=controller;controller=leaving;
    ghost.outputs.reset();
    require(controller.playback(ghost,++now) && controller.frame().healed && !controller.frame().ghostRetired,
        "first observation of finished speech still protects its full audio window");
    now+=ad::kDialogue[ad::kReunionDialogueRow].durationMs+250;
    require(controller.advance(owner.run,now,true) && !controller.frame().ghostRetired,
        "elapsed voice duration cannot fabricate a missing native departure cue");
    output(ghost,ad::kReunionDepartureCue);
    require(controller.playback(ghost,++now) && controller.frame().ghostRetired,
        "authored departure retires Ghost after his actual voice window without the unused parent return cue");
    visit("tv_goto_climb");
    require(controller.frame().ghostRetired,"City departure retains Ghost retirement");
    controller=completed;
    require(controller.frame().cinematic.route()==5 && !controller.frame().cinematic.selected(),"city exit goes directly to outskirts, never Hawthorne");
    musicIs(1);
    require(controller.arrival(owner,5,++now),"outskirts arrival");
    musicIs(2);
    require(controller.frame().native.scene(ad::kFinding)->stopped,"city performance retired at exit");
    const auto falcon=ad::asset(ad::kMain,43,96);
    visit("tv_falcon_climb");
    require(controller.frame().native.bits(falcon)==0,"falcon trigger cannot run before actor binds");
    bindCast(falcon);
    require(!controller.frame().native.scene(falcon)->started,"object readiness is not falcon playback");
    require(controller.playback({{owner,falcon,owner.value},900,901},++now),"falcon selector startup releases flight cue");
    require(controller.frame().native.scene(falcon)->count==1,"early falcon flight cue preserved");
    visit("tv_goto_mountain");require(controller.frame().cinematic.route()==0,"camp exit requires pickup and encounter");
    require(controller.prepared(owner,ad::kPickup),"pickup cleared before creation");
    const auto generation=controller.frame().objects[ad::object_index(ad::kPickup)].generation;
    coo::ObjectReceipt object{{owner.run,generation},ad::kPickup,50,60,70};
    require(controller.object(object),"pickup native object bound");
    dawn::middleware::bap::activity_message::object_sense::Output used{};
    used.generation=static_cast<std::int32_t>(generation);used.alive=used.present=used.hasUse=used.used=true;
    require(controller.use(owner,ad::kPickup,used),"accepted native hold");
    require(controller.frame().stage==ad::Stage::outskirts,"use is not inventory completion");
    require(controller.granted(controller.grant_request(),123),"committed equipped weapon");
    const auto fight=[&](std::size_t first) {
        for(std::size_t i=first;i<first+12;++i) {
            const auto& source=ad::kCombat[i];
            ad::EnemyReceipt enemy{owner.run,static_cast<std::uint32_t>(1000+i),static_cast<std::uint32_t>(2000+i),owner.value,source.source,source.registry};
            auto forged=enemy;forged.generation++;
            require(!controller.admitted(forged),"old or foreign spawn generation rejected");
            require(controller.admitted(enemy),"native actor admission");
            require(controller.readiness(enemy,{true,true,true,true,1,source.registry,source.tactical,0}),"native combat readiness");
            require(controller.died(enemy),"qualified native death");
            require(!controller.died(enemy),"duplicate death rejected");
        }
    };
    fight(0);
    require(controller.frame().cinematic.route()==6 && !controller.frame().cinematic.selected(),"camp exit goes directly to Twilight Gap, never Ghaul");
    musicIs(2);
    require(controller.arrival(owner,6,++now),"Twilight Gap arrival");
    musicIs(3);
    require(controller.frame().native.scene(ad::asset(ad::kMain,43,98))->requested,"third falcon staged before trigger");
    require(controller.frame().native.scene(ad::asset(ad::kMain,43,99))->requested,"final falcon staged before trigger");
    visit("tv_beast_spawn");
    auto escaped=controller;
    for(const auto name:{"tv_goto_haw","tv_goto_arrival"}) {
        for(const auto& v:ad::kVolumes) if(v.name==name) {
            ad::Point point{};for(const auto vertex:v.polygon) {point.x+=vertex.x;point.y+=vertex.y;}
            point.x/=static_cast<float>(v.polygon.size());point.y/=static_cast<float>(v.polygon.size());point.z=(v.min.z+v.max.z)*.5F;
            escaped.position(owner,point);
        }
    }
    require(escaped.frame().stage==ad::Stage::ending && escaped.frame().cinematic.route()==2 && !escaped.frame().finished,
        "authored physical exit reaches rescue even with surviving bowl enemies");
    require(!escaped.frame().combat[12].cleared,"physical escape does not forge enemy deaths");
    visit("tv_goto_arrival");require(!controller.frame().finished,"second encounter cannot be skipped");
    fight(12);
    require(!controller.frame().finished && controller.frame().cinematic.route()==2,"endpoint starts Hawthorne instead of mission completion");
    require(controller.frame().stage==ad::Stage::ending,"ending begins at final cliff");
    musicIs(0);
    movie(1,true);
    require(!controller.frame().finished && controller.frame().cinematic.route()==3,"Hawthorne ends into Ghaul");
    movie(2);
    require(controller.frame().finished,"Ghaul native end completes mission");
    const auto f=controller.frame();
    for(const auto& binding:ad::kBindings) {
        const auto a=binding.asset;const auto bits=ad::body_bits(f,a.registry,static_cast<std::uint8_t>(a.type),a.slot);
        if(!bits) continue;
        std::array<std::byte,8192> bytes{};dawn::middleware::encoding::bits::Writer writer(bytes);
        require(ad::write_body(writer,f,a.registry,static_cast<std::uint8_t>(a.type),a.slot),"mission authority writer");
        require(writer.bit_count()==bits,"mission authority width");
    }
    ad::Controller timeout;require(timeout.select(101,1),"timeout fixture");
    require(timeout.advance(101,1000000,true) && timeout.frame().fault && !timeout.frame().finished,"movie timeout never completes mission");
    require(ad::transit::destination(4).spawnSet!=ad::transit::destination(5).spawnSet
        && ad::transit::destination(5).spawnSet!=ad::transit::destination(6).spawnSet,"three distinct native arrival points");
    ad::cinematics::Sequence ordering;ordering.begin({101,1},1);
    require(!ordering.finish_gameplay({101,1},2),"rescue forbidden before gameplay");
    require(ordering.arrival({101,1},1,3) && ordering.incident({101,1},5239,ad::kMovies[0].registry,6,0,1,4)
        && ordering.incident({101,1},1685,ad::kMovies[0].registry,6,0,2,5) && ordering.arrival({101,1},4,6),"intro lifecycle fixture");
    require(!ordering.transition({101,1},2,7) && !ordering.finish_gameplay({101,1},7),"cannot jump over outskirts");
    require(ordering.transition({101,1},1,8) && ordering.arrival({101,1},5,9)
        && ordering.transition({101,1},2,10) && ordering.arrival({101,1},6,11),"traversal needs no cinematic receipt");
    require(ordering.finish_gameplay({101,1},12) && ordering.arrival({101,1},2,13)
        && ordering.incident({101,1},5239,ad::kMovies[1].registry,6,0,3,14),"Hawthorne playback fixture");
    ordering.advance(1000000);
    require(ordering.state().phase==ad::cinematics::Phase::failed,"missing rescue end fails, never completes");
    std::printf("Adieu: %u checks passed\n",checks);return 0;
}
