#include "mission.h"
#include <bit>
#include <algorithm>
namespace dawn::state::activity::vanilla::homecoming {
namespace {
using coo::CommandSpec;using coo::Operation;using coo::Wait;
CommandSpec objective(std::size_t i) {return {Operation::objective,kDirectiveAsset,kObjectives[i],Wait::requested};}
// Speech is queued at once; the authored window of the previous cue paces playback. A delay
// holds the request itself, which is how the retail beat timing is reproduced.
CommandSpec speech(std::uint8_t row,std::uint32_t delayMs=0) {return {Operation::dialogue,kDialogueAsset,row|(delayMs<<8),Wait::requested};}
CommandSpec spoken(std::uint8_t row) {return {Operation::observation,kDialogueAsset,row,Wait::observed};}
CommandSpec visit(std::uint32_t r,std::string_view trigger) {return {Operation::observation,trigger_area(r,trigger),0,Wait::observed};}
CommandSpec area(std::uint32_t r,std::string_view volume) {return {Operation::observation,volume_named(r,volume),0,Wait::observed};}
CommandSpec reached(Milestone m) {return {Operation::observation,kModule,static_cast<std::uint32_t>(m),Wait::observed};}
CommandSpec after(std::uint32_t ms) {return {Operation::eventAfter,kModule,ms,Wait::observed};}
CommandSpec device(std::uint32_t r,std::uint16_t slot,float position) {return {Operation::device,asset(r,23,slot),std::bit_cast<std::uint32_t>(position),Wait::requested};}
CommandSpec posed(std::uint32_t r,std::uint16_t slot,float position) {return {Operation::observation,asset(r,23,slot),std::bit_cast<std::uint32_t>(position),Wait::observed};}
CommandSpec object(std::uint32_t r,std::uint16_t slot,bool active=true) {return {Operation::device,asset(r,4,slot),active?1U:0U,Wait::requested};}
CommandSpec present(std::uint32_t r,std::uint16_t slot) {return {Operation::observation,asset(r,4,slot),static_cast<std::uint32_t>(ObjectEvent::present),Wait::observed};}
CommandSpec used(std::uint32_t r,std::uint16_t slot) {return {Operation::observation,asset(r,4,slot),static_cast<std::uint32_t>(ObjectEvent::used),Wait::observed};}
CommandSpec sequence(std::uint32_t r,std::uint16_t slot) {return {Operation::device,asset(r,5,slot),1,Wait::requested};}
CommandSpec bind(std::uint32_t r,std::uint16_t slot) {return {Operation::device,asset(r,2,slot),1,Wait::requested};}
CommandSpec ghost(std::uint32_t r,std::uint16_t slot,bool active) {return {Operation::device,asset(r,65,slot),active?1U:0U,Wait::requested};}
CommandSpec scene(std::uint32_t r,std::uint16_t slot) {return {Operation::scene,asset(r,43,slot),0,Wait::requested};}
CommandSpec sceneEvent(std::uint32_t r,std::uint16_t slot,std::uint32_t key) {return {Operation::scene,asset(r,43,slot),key,Wait::requested};}
CommandSpec control(Mechanic value) {return {Operation::mechanic,kModule,static_cast<std::uint32_t>(value),Wait::requested};}
CommandSpec mechanic(coo::Asset a,Mechanic value) {return {Operation::mechanic,a,static_cast<std::uint32_t>(value),Wait::requested};}
CommandSpec spawn(Cohort c) {return {Operation::population,kModule,static_cast<std::uint32_t>(c),Wait::requested};}
CommandSpec cleared(Cohort c) {return {Operation::observation,kModule,0x100U+static_cast<std::uint32_t>(c),Wait::observed};}
CommandSpec finish() {return control(Mechanic::finishSection);}
// Authored graph inputs (package evidence; see the audit's scene reviews).
inline constexpr std::uint32_t kEntry=0x6F51AC66U,kBreachEntry=0xAE7CC69CU,kIkoraMove=0x84FFD4F6U,kIkoraBlast=0x0B78A21AU,
    kZavalaArrive=0xB8C5C0A5U,kZavalaArriveB=0x1BED1ED3U,kZavalaWard=0xC021F76CU,kZavalaDeath=0xC021F76FU;
}
void Graph::name(std::string_view label) noexcept {
    definition={label,coo::Schema::otherMissions,{},{}};
    first_=storage_?storage_->stepCount:0;
}
std::uint32_t Graph::add(std::string_view label,std::uint32_t dependencies,std::initializer_list<coo::CommandSpec> commands) noexcept {
    const auto size=definition.steps.size();
    if(!storage_ || definition.schema==coo::Schema::unspecified
        || size>=coo::Executor::kMaxSteps || commands.size()==0 || commands.size()>coo::Executor::kMaxCommands
        || storage_->stepCount!=first_+size || storage_->stepCount>=storage_->steps.size()
        || commands.size()>storage_->commands.size()-storage_->commandCount) {
        definition.schema=coo::Schema::unspecified;return 0;
    }
    auto target=std::span(storage_->commands).subspan(storage_->commandCount,commands.size());
    std::copy(commands.begin(),commands.end(),target.begin());storage_->commandCount+=commands.size();
    storage_->steps[storage_->stepCount++]={label,dependencies,target};
    definition.steps=std::span(storage_->steps).subspan(first_,size+1);return 1U<<size;
}
Mission::Mission() noexcept {
    for(auto& phase:phases) {phase.storage_=&storage;}
    // Underwatch: the native cinematic lands the player at the crash site. Cue 0 belongs to
    // the opening movie; cue 1 follows the retail walkthrough seven seconds after landing.
    auto& underwatch=phases[static_cast<std::size_t>(Section::underwatch)];underwatch.name("Homecoming / Underwatch");
    auto arrival=underwatch.add("crash site",0,{objective(0),spawn(Cohort::underwatchCast),device(kUnderwatch,108,0.F),
        object(kUnderwatch,71),object(kUnderwatch,72),object(kUnderwatch,81),speech(1,7000)});
    auto a=underwatch.add("wall approach",arrival,{visit(kUnderwatch,"pt_wall_explode")});
    auto b=underwatch.add("first contact scene",a,{scene(kUnderwatch,10),spawn(Cohort::breachBackup),speech(4),speech(5,1500),speech(6,4000)});
    auto c=underwatch.add("first contact staged",b,{after(500)});
    auto breach=underwatch.add("wall breach",c,{sceneEvent(kUnderwatch,10,kBreachEntry),device(kUnderwatch,108,1.F)});
    a=underwatch.add("centurion approach",arrival,{visit(kUnderwatch,"pt_centurion_intro")});
    b=underwatch.add("centurion scene",a,{scene(kUnderwatch,18),bind(kUnderwatch,21)});
    c=underwatch.add("centurion staged",b,{after(500)});
    auto centurion=underwatch.add("centurion entry",c,{sceneEvent(kUnderwatch,18,kEntry)});
    a=underwatch.add("corridor reinforcement area",arrival,{visit(kUnderwatch,"pt_centurion_intro_reinforce")});
    auto rush=underwatch.add("corridor rush",a,{spawn(Cohort::centurionRush)});
    a=underwatch.add("drop pod area",arrival,{visit(kUnderwatch,"pt_drop_pod")});
    auto pod=underwatch.add("drop pod backup",a,{spawn(Cohort::centurionBackup)});
    a=underwatch.add("civilian run",arrival,{visit(kUnderwatch,"pt_civ_run_b")});
    underwatch.add("evacuation line",a,{speech(7)});
    a=underwatch.add("hero moment area",arrival,{visit(kUnderwatch,"pt_hero_moment")});
    underwatch.add("hero moment",a,{spawn(Cohort::heroMoment)});
    auto clear=underwatch.add("approach clear",breach|centurion|rush|pod,{cleared(Cohort::breachApproach)});
    a=underwatch.add("Cayde near",clear,{reached(Milestone::caydeNear)});
    b=underwatch.add("Cayde settle",a,{after(1500)});
    // Reserve Cayde, the three Legionaries and the aiming target, bind the named cells, then
    // activate with the seven authored participants and deliver the entry gate input.
    c=underwatch.add("Cayde scene",b,{scene(kUnderwatch,17),bind(kUnderwatch,29),bind(kUnderwatch,31),bind(kUnderwatch,33)});
    a=underwatch.add("Cayde staged",c,{after(500)});
    b=underwatch.add("Cayde entry",a,{sceneEvent(kUnderwatch,17,kEntry)});
    c=underwatch.add("Cayde handoff",b,{after(500)});
    // The collision blocker is released only after the entry input is staged (v22).
    auto cayde=underwatch.add("collision handoff",c,{object(kUnderwatch,81,false)});
    a=underwatch.add("Shaxx near",cayde,{reached(Milestone::shaxxNear)});
    b=underwatch.add("Shaxx scene",a,{scene(kUnderwatch,14)});
    c=underwatch.add("Shaxx staged",b,{after(500)});
    underwatch.add("armory",c,{sceneEvent(kUnderwatch,14,kEntry),objective(3),device(kUnderwatch,117,1.F),finish()});

    auto& armory=phases[static_cast<std::size_t>(Section::armory)];armory.name("Homecoming / Armory");
    a=armory.add("post gun area",0,{visit(kUnderwatch,"pt_postgun")});
    armory.add("post gun",a,{speech(30),spawn(Cohort::postGun)});
    a=armory.add("weapon taken",0,{visit(kUnderwatchExit,"pt_weapon_complete")});
    b=armory.add("find Zavala",a,{speech(28),objective(2)});
    c=armory.add("military entrance",b,{visit(kUnderwatchExit,"pt_goto_military")});
    armory.add("military",c,{finish()});

    // Military: both Amanda gates are held closed until their authored areas are observed
    // alive and then cleared; every open waits for the actual applied device position.
    auto& military=phases[static_cast<std::size_t>(Section::military)];military.name("Homecoming / Military");
    arrival=military.add("hangar route",0,{objective(4),device(kMilitary,40,0.F),device(kMilitary,41,0.F)});
    a=military.add("hangar spawn area",arrival,{visit(kMilitary,"pt_hangar_spawn")});
    b=military.add("overlook encounter",a,{object(kMilitary,2),spawn(Cohort::overlookStart),spawn(Cohort::friendlies),spawn(Cohort::fakeFight),spawn(Cohort::frames)});
    c=military.add("drop pod present",b,{present(kMilitary,2)});
    a=military.add("hallway destruction",c,{spawn(Cohort::hallwayDestruction)});
    b=military.add("first gate cleared",a,{cleared(Cohort::gateStartClear)});
    c=military.add("first gate opens",b,{device(kMilitary,40,1.F)});
    a=military.add("first gate open",c,{posed(kMilitary,40,1.F)});
    b=military.add("corridor squads",a,{spawn(Cohort::corridor)});
    c=military.add("second gate cleared",b,{cleared(Cohort::gateHangarClear)});
    a=military.add("second gate opens",c,{device(kMilitary,41,1.F)});
    b=military.add("second gate open",a,{posed(kMilitary,41,1.F)});
    c=military.add("destroyer",b,{object(kMilitary,0)});
    a=military.add("destroyer present",c,{present(kMilitary,0)});
    military.add("destroyer missiles",a,{object(kMilitary,48),object(kMilitary,49),object(kMilitary,50),object(kMilitary,51),object(kMilitary,52),object(kMilitary,53)});
    a=military.add("hangar combat area",arrival,{visit(kMilitary,"pt_hangar_combat")});
    military.add("hangar floor",a,{spawn(Cohort::hangarFloor),speech(35)});
    a=military.add("hangar window",arrival,{visit(kMilitaryRoute,"pt_dialogue_hangar_window")});
    military.add("command ship sighting",a,{speech(34)});
    a=military.add("plaza route",arrival,{visit(kMilitaryRoute,"pt_goto_plaza")});
    military.add("red legion",a,{speech(37),speech(38),speech(39),finish()});

    // Plaza: Zavala's authored scenes own his actor; the waves, barrage, Ward and revival
    // follow the retail pacing with five-second intermissions and a twelve-second barrage.
    auto& plaza=phases[static_cast<std::size_t>(Section::plaza)];plaza.name("Homecoming / Plaza");
    arrival=plaza.add("plaza arrival",0,{objective(5),object(kPlaza,18,false),object(kPlaza,44,false),object(kPlaza,24,false),
        object(kPlaza,25,false),object(kPlaza,46,false),object(kPlaza,47,false),object(kPlazaProps,0)});
    plaza.add("skybox ships A",arrival,{object(kPlazaProps,5),object(kPlazaProps,4),object(kPlazaProps,9),object(kPlazaProps,10),object(kPlazaProps,11),object(kPlazaProps,12)});
    plaza.add("skybox ships B",arrival,{object(kPlazaProps,13),object(kPlazaProps,14),object(kPlazaProps,15),object(kPlazaProps,6),object(kPlazaProps,7),object(kPlazaProps,8)});
    a=plaza.add("Zavala arrival scene",arrival,{scene(kPlaza,2),bind(kPlaza,7),spawn(Cohort::plazaInit)});
    b=plaza.add("Zavala arrival staged",a,{after(500)});
    auto zavala=plaza.add("Zavala arrival entry",b,{sceneEvent(kPlaza,2,kZavalaArrive),sceneEvent(kPlaza,2,kZavalaArriveB)});
    a=plaza.add("Traveler sighting area",arrival,{area(kPlazaRoute,"tv_dialog_military_11")});
    plaza.add("Traveler sighting",a,{speech(40)});
    a=plaza.add("meet Zavala area",arrival,{visit(kPlaza,"pt_plaza_zavala_meet")});
    b=plaza.add("meet Zavala",a,{object(kPlazaProps,0,false),object(kPlaza,8),speech(50)});
    c=plaza.add("Zavala combat",b|zavala,{mechanic(asset(kPlaza,43,2),Mechanic::stopScene),scene(kPlaza,3)});
    a=plaza.add("opening Cabal cleared",c,{cleared(Cohort::plazaInit)});
    b=plaza.add("first assault intermission",a,{after(5000)});
    c=plaza.add("first assault",b,{speech(51),speech(52),object(kPlaza,45),spawn(Cohort::wave1),object(kPlazaProps,16),object(kPlazaProps,17),device(kPlazaProps,19,1.F),device(kPlazaProps,20,1.F)});
    a=plaza.add("first assault cleared",c,{cleared(Cohort::wave1)});
    b=plaza.add("barrage intermission",a,{after(5000)});
    c=plaza.add("barrage",b,{speech(53),mechanic(asset(kPlaza,43,3),Mechanic::stopScene),scene(kPlaza,4),object(kPlaza,44,false),object(kPlaza,46,false),object(kPlaza,47,false),mechanic(asset(kPlaza,23,48),Mechanic::powerOff)});
    a=plaza.add("Ward staged",c,{after(500)});
    b=plaza.add("Ward of Dawn",a,{sceneEvent(kPlaza,4,kZavalaWard),object(kPlaza,18)});
    c=plaza.add("barrage held",b,{after(12000)});
    a=plaza.add("barrage strike",c,{object(kPlaza,18,false),object(kPlaza,44),speech(54),mechanic(asset(kPlaza,43,4),Mechanic::stopScene),scene(kPlaza,5)});
    b=plaza.add("death staged",a,{after(500)});
    c=plaza.add("death entry",b,{sceneEvent(kPlaza,5,kZavalaDeath)});
    a=plaza.add("Zavala struck",c,{after(10000)});
    b=plaza.add("Zavala down",a,{speech(47),mechanic(asset(kPlaza,43,5),Mechanic::stopScene),mechanic(asset(kPlaza,2,7),Mechanic::retireMember),
        object(kPlaza,44,false),object(kPlaza,25),device(kPlaza,26,1.F),mechanic(kReviveInteract,Mechanic::arm)});
    c=plaza.add("Zavala revived",b,{used(kPlaza,24)});
    a=plaza.add("revival",c,{mechanic(kReviveInteract,Mechanic::disarm),device(kPlaza,26,0.F),object(kPlaza,25,false),scene(kPlaza,2),bind(kPlaza,7)});
    b=plaza.add("revival staged",a,{after(500)});
    c=plaza.add("revival entry",b,{sceneEvent(kPlaza,2,kZavalaArrive)});
    a=plaza.add("revival settle",c,{after(3000)});
    plaza.add("second assault approaches",a,{finish()});

    auto& waves=phases[static_cast<std::size_t>(Section::plazaWaves)];waves.name("Homecoming / Plaza assaults");
    arrival=waves.add("Zavala combat resumes",0,{mechanic(asset(kPlaza,43,2),Mechanic::stopScene),scene(kPlaza,3)});
    a=waves.add("second assault intermission",arrival,{after(5000)});
    b=waves.add("second assault",a,{speech(55),spawn(Cohort::wave2),object(kPlazaProps,18),object(kPlazaProps,22),object(kPlazaProps,23),object(kPlazaProps,24)});
    c=waves.add("second carriers",a,{device(kPlazaProps,21,1.F),device(kPlazaProps,31,1.F),device(kPlazaProps,32,1.F),device(kPlazaProps,33,1.F)});
    a=waves.add("second assault cleared",b|c,{cleared(Cohort::wave2)});
    b=waves.add("third assault intermission",a,{after(5000)});
    c=waves.add("third assault",b,{speech(58),spawn(Cohort::wave3),object(kPlazaProps,25),object(kPlazaProps,26),object(kPlazaProps,27),object(kPlazaProps,28),object(kPlazaProps,29),object(kPlazaProps,30)});
    a=waves.add("third carriers",b,{device(kPlazaProps,34,1.F),device(kPlazaProps,35,1.F),device(kPlazaProps,36,1.F),device(kPlazaProps,37,1.F),device(kPlazaProps,38,1.F),device(kPlazaProps,39,1.F)});
    b=waves.add("third assault cleared",c|a,{cleared(Cohort::wave3)});
    c=waves.add("plaza held",b,{after(5000)});
    a=waves.add("shuttles away",c,{mechanic(asset(kPlaza,43,3),Mechanic::stopScene),object(kPlaza,45,false),speech(59),objective(9)});
    b=waves.add("plaza exit",a,{visit(kPlazaRoute,"pt_goto_boulevard")});
    waves.add("boulevard",b,{speech(60),speech(61),finish()});

    // Boulevard: Ikora's scene is prepared on arrival; her entry input follows the authored
    // start trigger, and the Nova blast is released after the recovered entry window.
    auto& boulevard=phases[static_cast<std::size_t>(Section::boulevard)];boulevard.name("Homecoming / Boulevard");
    arrival=boulevard.add("boulevard arrival",0,{objective(8),device(kBoulevard,0,1.F),scene(kBoulevard,16)});
    a=boulevard.add("Ikora staged",arrival,{after(500)});
    auto ikora=boulevard.add("Cabal movement",a,{sceneEvent(kBoulevard,16,kIkoraMove)});
    a=boulevard.add("Ikora start area",arrival,{visit(kBoulevard,"pt_start_ikora")});
    b=boulevard.add("Ikora entry",a|ikora,{sceneEvent(kBoulevard,16,kEntry)});
    c=boulevard.add("entry window",b,{after(2000)});
    auto blast=boulevard.add("Nova blast",c,{object(kBoulevard,23),sceneEvent(kBoulevard,16,kIkoraBlast)});
    a=boulevard.add("Speaker report",blast,{after(12000)});
    boulevard.add("Zavala sends Holliday",a,{speech(72)});
    a=boulevard.add("Speaker route",arrival,{visit(kBoulevardRoute,"pt_goto_speaker")});
    boulevard.add("Speaker close",a,{speech(64)});
    a=boulevard.add("bazaar sighting",arrival,{area(kBoulevardRoute,"tv_bazaar")});
    boulevard.add("flamethrowers",a,{speech(74)});
    a=boulevard.add("bazaar entry",arrival,{visit(kBoulevard,"pt_bazaar")});
    auto w1=boulevard.add("bazaar first wave",a,{spawn(Cohort::bazaarStart),speech(75),objective(10)});
    a=boulevard.add("bazaar middle",arrival,{visit(kBoulevard,"pt_bazaar_mid")});
    auto w2=boulevard.add("bazaar second wave",a,{spawn(Cohort::bazaarMid)});
    a=boulevard.add("bazaar far",arrival,{visit(kBoulevard,"pt_bazaar_far")});
    auto w3=boulevard.add("bazaar third wave",a,{spawn(Cohort::bazaarFar)});
    a=boulevard.add("bazaar end",arrival,{visit(kBoulevard,"pt_bazaar_2")});
    auto w4=boulevard.add("bazaar final wave",a,{spawn(Cohort::bazaarEnd)});
    a=boulevard.add("Ikora's Cabal cleared",blast,{cleared(Cohort::blasted)});
    b=boulevard.add("bazaar door opens",a,{device(kBoulevard,9,1.F)});
    c=boulevard.add("bazaar door open",b,{posed(kBoulevard,9,1.F)});
    boulevard.add("bazaar breach",c,{sequence(kBoulevard,33),object(kBoulevard,25,false),object(kBoulevard,27,false),control(Mechanic::releaseDoor)});
    a=boulevard.add("bazaar cleared",w1|w2|w3|w4,{cleared(Cohort::bazaarAll)});
    b=boulevard.add("Hawk",a,{object(kBoulevard,12)});
    c=boulevard.add("Hawk present",b,{present(kBoulevard,12)});
    a=boulevard.add("Holliday arrives",c,{speech(76)});
    b=boulevard.add("Holliday spoken",a,{spoken(76)});
    c=boulevard.add("pickup area",b,{visit(kBoulevardRoute,"pt_goto_sky_battle")});
    boulevard.add("pickup",c,{control(Mechanic::pickup),finish()});

    // Command ship: the console room is populated before the scan; console doors follow the
    // matching scan generation and wait for application before later encounters.
    auto& ship=phases[static_cast<std::size_t>(Section::ship)];ship.name("Homecoming / Command ship");
    arrival=ship.add("ship arrival",0,{objective(13),speech(77),spawn(Cohort::pods),ghost(kShip,166,true)});
    a=ship.add("console scanned",arrival,{reached(Milestone::consoleScanned)});
    ship.add("console doors",a,{ghost(kShip,166,false),device(kShip,167,1.F),device(kShip,56,1.F),device(kShip,57,1.F),objective(14),speech(79)});
    a=ship.add("Hawk area",arrival,{visit(kShip,"pt_hawk")});
    ship.add("Hawk flyby",a,{object(kShip,165),device(kShip,164,1.F)});
    a=ship.add("pod launch area",arrival,{visit(kShip,"pt_drop_pod_launch")});
    ship.add("pod launch",a,{object(kShip,150),device(kShip,151,1.F)});
    a=ship.add("pod door A",arrival,{visit(kShip,"pt_pods_door_a")});
    ship.add("start impacts",a,{object(kShip,78),object(kShip,80),object(kShip,82),object(kShip,84),object(kShip,86),object(kShip,88)});
    ship.add("start impact devices",a,{device(kShip,77,.5F),device(kShip,79,.5F),device(kShip,81,.5F),device(kShip,83,.5F),device(kShip,85,.5F),device(kShip,87,.5F)});
    a=ship.add("damaged area",arrival,{visit(kShip,"pt_damaged")});
    ship.add("damaged squad",a,{spawn(Cohort::damaged)});
    a=ship.add("damaged hall",arrival,{visit(kShip,"pt_damaged_hall")});
    ship.add("hall squads",a,{spawn(Cohort::hall),object(kShip,90),object(kShip,92),object(kShip,94),object(kShip,96),object(kShip,98),object(kShip,100)});
    ship.add("hall impact devices",a,{device(kShip,89,.5F),device(kShip,91,.5F),device(kShip,93,.5F),device(kShip,95,.5F),device(kShip,97,.5F),device(kShip,99,.5F)});
    a=ship.add("stairs",arrival,{visit(kShip,"pt_damaged_hall_stairs")});
    ship.add("stair squads",a,{spawn(Cohort::stairs),object(kShip,102),object(kShip,104),object(kShip,106),object(kShip,108),object(kShip,110)});
    ship.add("stair impact devices",a,{device(kShip,101,.5F),device(kShip,103,.5F),device(kShip,105,.5F),device(kShip,107,.5F),device(kShip,109,.5F)});
    a=ship.add("stairs door area",arrival,{visit(kShip,"pt_damaged_hall_stairs_door")});
    ship.add("stairs door",a,{device(kShip,58,1.F)});
    a=ship.add("deck start",arrival,{visit(kShip,"pt_deck_start")});
    ship.add("deck front squads",a,{spawn(Cohort::deckA),object(kShip,112),object(kShip,114),object(kShip,116),object(kShip,118),object(kShip,120),object(kShip,122),object(kShip,124)});
    ship.add("deck impacts",a,{object(kShip,126),object(kShip,128),object(kShip,130),device(kShip,111,.5F),device(kShip,113,.5F),device(kShip,115,.5F),device(kShip,117,.5F),device(kShip,119,.5F)});
    ship.add("deck impact devices",a,{device(kShip,121,.5F),device(kShip,123,.5F),device(kShip,125,.5F),device(kShip,127,.5F),device(kShip,129,.5F)});
    a=ship.add("deck middle",arrival,{visit(kShip,"pt_deck_mid")});
    ship.add("deck middle squads",a,{spawn(Cohort::deckB)});
    a=ship.add("hardpoint",arrival,{visit(kShip,"pt_deck_hardpoint")});
    ship.add("hardpoint squads",a,{spawn(Cohort::hardpoints)});
    a=ship.add("deck boss area",arrival,{visit(kShip,"pt_deck_boss")});
    ship.add("deck boss",a,{finish()});

    auto& generator=phases[static_cast<std::size_t>(Section::generator)];generator.name("Homecoming / Shield generator");
    arrival=generator.add("boss announce",0,{spawn(Cohort::boss),sequence(kShip,174)});
    a=generator.add("airlock area",arrival,{visit(kShip,"pt_airlock")});
    generator.add("airlock squads",a,{spawn(Cohort::airlock)});
    a=generator.add("matrix door area",arrival,{visit(kShip,"pt_matrix_door")});
    generator.add("matrix squads",a,{spawn(Cohort::matrix)});
    a=generator.add("engine room upper",arrival,{visit(kShip,"pt_engine_room_upper")});
    generator.add("upper squads",a,{spawn(Cohort::engineUpper)});
    auto approach=generator.add("engine room lower",arrival,{visit(kShip,"pt_engine_room_lower")});
    generator.add("lower squads",approach,{spawn(Cohort::engineLower),speech(84)});
    a=generator.add("boss cleared",arrival,{cleared(Cohort::boss)});
    b=generator.add("generator entrance opens",a|approach,{device(kShip,59,1.F)});
    c=generator.add("generator entrance open",b,{posed(kShip,59,1.F)});
    auto chamber=generator.add("generator chamber",c,{visit(kShipRoute,"pt_destroy_battleship")});
    generator.add("overload",chamber,{objective(11),speech(86),spawn(Cohort::generatorGuards),object(kShip,142),object(kShip,144),object(kShip,146),object(kShip,148),object(kShip,149)});
    auto turbines=generator.add("turbines",chamber,{device(kShip,143,.1F),device(kShip,145,.1F),device(kShip,147,.1F),device(kShip,61,.1F),device(kShip,62,.1F),device(kShip,64,.1F),device(kShip,63,1.F),device(kShip,159,1.F)});
    generator.add("turbine lights",chamber,{device(kShip,160,1.F),device(kShip,161,1.F),device(kShip,162,1.F),device(kShip,65,1.F),device(kShip,66,1.F),device(kShip,67,1.F)});
    a=generator.add("turbine A destroyed",turbines,{reached(Milestone::generatorA)});
    auto killA=generator.add("turbine A dark",a,{device(kShip,160,0.F)});
    a=generator.add("turbine B destroyed",turbines,{reached(Milestone::generatorB)});
    auto killB=generator.add("turbine B dark",a,{device(kShip,161,0.F)});
    a=generator.add("turbine C destroyed",turbines,{reached(Milestone::generatorC)});
    auto killC=generator.add("turbine C dark",a,{device(kShip,162,0.F)});
    a=generator.add("shutdown",killA|killB|killC,{device(kShip,61,.2F),device(kShip,62,.2F),device(kShip,64,.2F),device(kShip,63,0.F),device(kShip,159,0.F),object(kShip,68),object(kShip,69),object(kShip,70)});
    b=generator.add("escape opens",a,{object(kShip,76),device(kShip,163,1.F),device(kShip,60,1.F),objective(12),speech(91),speech(92),spawn(Cohort::escape)});
    c=generator.add("escape open",b,{posed(kShip,60,1.F)});
    generator.add("escape",c,{finish()});

    // Escape: the four authored explosion scenes and the finale own their timing; leaving
    // the terminal volume ends the escape (v34) and hands the ending to the native outro.
    auto& escape=phases[static_cast<std::size_t>(Section::escape)];escape.name("Homecoming / Escape");
    a=escape.add("explosion A area",0,{visit(kShip,"pt_escape_explosion_a")});
    escape.add("explosion A",a,{scene(kShip,137)});
    a=escape.add("explosion B area",0,{visit(kShip,"pt_escape_explosion_b")});
    escape.add("explosion B",a,{scene(kShip,138)});
    a=escape.add("explosion C area",0,{visit(kShip,"pt_escape_explosion_c")});
    escape.add("explosion C",a,{scene(kShip,139)});
    a=escape.add("explosion D area",0,{visit(kShip,"pt_escape_explosion_d")});
    escape.add("explosion D",a,{scene(kShip,140)});
    a=escape.add("hangar reached",0,{visit(kShipRoute,"pt_goto_end")});
    b=escape.add("finale",a,{scene(kShip,141)});
    c=escape.add("finale plays",b,{after(1500)});
    escape.add("departure",c,{mechanic(asset(kShip,43,137),Mechanic::stopScene),mechanic(asset(kShip,43,138),Mechanic::stopScene),
        mechanic(asset(kShip,43,139),Mechanic::stopScene),mechanic(asset(kShip,43,140),Mechanic::stopScene),mechanic(asset(kShip,43,141),Mechanic::stopScene),
        {Operation::complete,kModule,6,Wait::requested}});
}
bool Mission::valid() const noexcept {
    const auto valid=[](const Graph& graph) {
        if(!coo::Executor::valid(graph.definition)) {return false;}
        for(const auto& step:graph.definition.steps) for(const auto& command:step.commands) {
            if(!command.asset.registry || (!command.asset.definition && command.asset!=kModule)) {return false;}
            if(command.operation==coo::Operation::dialogue) {
                const auto row=dialogue_row(command.argument);
                if(row>=std::size(kDialogue) || !kDialogue[row].durationMs || kDialogue[row].sceneOwned) {return false;}
            }
            if(command.operation==coo::Operation::observation && command.asset==kDialogueAsset
                && (command.argument>=std::size(kDialogue) || !kDialogue[command.argument].durationMs)) {return false;}
            if(command.operation==coo::Operation::scene) {
                bool found=false;
                for(const auto& scene:kScenes) {
                    if(scene.asset!=command.asset) {continue;}found=true;
                    if(command.argument) {
                        bool input=false;for(const auto key:scene.inputs) {input|=key==command.argument;}
                        if(!input) {return false;}
                    }
                }
                if(!found) {return false;}
            }
        }
        return true;
    };
    return std::all_of(phases.begin(),phases.end(),valid);
}
const Mission& mission() noexcept {static const Mission value;return value;}
}
