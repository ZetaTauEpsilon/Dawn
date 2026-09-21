#include "controller.h"
#include "lighting.h"
#include "navigation.h"
#include <bit>
#include <cmath>

namespace dawn::state::activity::newlight::launchpad {
void Controller::reset() noexcept {
    executor_.cancel(*this);lifecycle_.reset();clock_.reset();run_=now_=0;
    started_=phaseFinished_=hasPosition_=false;enteredRegions_=0;previous_={};objectives_={};dialogue_={};
    objects_={};population_={};seen_.reset();submitted_.reset();voiceEnd_={};cinematics_={};frame_={};
    reinforcementAt_={};nextReinforcement_=0;
}
bool Controller::select(std::uint64_t run,std::uint64_t now) noexcept {
    if(run && run_==run) {return true;}reset();
    if(!run || !mission().valid() || !lifecycle_.begin(run,256) || !objects_.begin(owner(),kObjects)) {return false;}
    run_=run;frame_.spawnGeneration=owner().value;frame_.enabled=true;
    cinematics_.begin(owner(),now);frame_.cinematic=cinematics_.state();
    for(std::size_t i=0;i<kSpawns.size();++i) {
        const auto& p=kSpawns[i];const auto t=p.tactical;
        population_.policy(i,{true,p.scripted?coo::EnemyIntent::idleReveal:coo::EnemyIntent::combat,t.registry,t.slot,t.row});
    }
    return true;
}
bool Controller::fly_in_complete(coo::Generation gen,std::uint64_t now) noexcept {
    if(!frame_.enabled || !cinematics_.fly_in_complete(gen,now)) {return false;}
    frame_.cinematic=cinematics_.state();++frame_.revision;return true;
}
bool Controller::arrival(coo::Generation gen,std::uint8_t route,std::uint64_t now) noexcept {
    if(!cinematics_.arrival(gen,route,now)) {return false;}
    frame_.cinematic=cinematics_.state();hasPosition_=false;++frame_.revision;return true;
}
bool Controller::retired(coo::Generation gen) noexcept {
    if(!frame_.enabled || !cinematics_.retired(gen)) {return false;}
    frame_.cinematic=cinematics_.state();++frame_.revision;return true;
}
bool Controller::cinematic(coo::Generation gen,const cinematics::Incident& e,std::uint64_t now) noexcept {
    if(gen!=owner() || !frame_.enabled) {return false;}
    if(e.registry==kKetch && e.type==6 && e.slot==0 && e.runtime!=UINT64_MAX && frame_.ketch) {
        if(e.target==5239 && !frame_.ketchStarted) {frame_.ketchStarted=true;++frame_.revision;return true;}
        if(e.target==1685) {
            frame_.ketch=false;++frame_.ketchRevision;++frame_.revision;
            return request(kFlare,false);
        }
        return false;
    }
    if(!cinematics_.incident(gen,e.target,e.registry,e.type,e.slot,e.runtime,now)) {return false;}
    frame_.cinematic=cinematics_.state();++frame_.revision;return true;
}
void Controller::position(std::uint64_t run,Point point) noexcept {
    if(run!=run_ || !frame_.enabled || frame_.finished || frame_.cinematic.phase!=cinematics::Phase::gameplay
        || !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {return;}
    for(std::size_t i=0;i<std::size(kVolumes);++i) {
        if(contains(kVolumes[i],point) || (hasPosition_ && crosses(kVolumes[i],previous_,point))) {seen_.set(i);}
        if(kVolumes[i].asset==volume(kKetch,5) && frame_.flareStarted && !contains(kVolumes[i],point)
            && frame_.native[asset_index(kFlare)].desired) {static_cast<void>(request(kFlare,false));}
    }
    // The broad authored approach is 30-55m away. Keep the short scurry until
    // the player can see its authored origin; sweeping also handles sprinting.
    if(frame_.section==2 && !frame_.firstVandalNear && near_first_vandal(hasPosition_?previous_:point,point)) {
        frame_.firstVandalNear=true;++frame_.revision;
    }
    previous_=point;hasPosition_=true;
}
bool Controller::region(coo::Generation gen,std::int32_t region) noexcept {
    // Region residency starts well before the outdoor reveal. It is not proof
    // that the player left the Wall or completed the Divide encounter.
    if(gen!=owner() || !frame_.enabled || frame_.finished
        || frame_.cinematic.phase!=cinematics::Phase::gameplay) {return false;}
    const auto bit=region==0 && frame_.section==0?1U:0U;
    if(!bit || (enteredRegions_&bit)) {return false;}
    enteredRegions_|=static_cast<std::uint8_t>(bit);hasPosition_=false;++frame_.revision;return true;
}
bool Controller::request(coo::Asset a,bool active,float position) noexcept {
    const auto i=asset_index(a);if(i==std::size(kAssets) || !std::isfinite(position)) {return false;}
    auto& s=frame_.native[i];
    if(a.type!=5 && s.managed && s.desired==active && s.position==position) {return true;}
    if(a.type==5) {if(s.sequenceRevision==254) {return false;}++s.sequenceRevision;s.sequenceStartTicks=frame_.gameplayClockTicks;}
    s.snap=!s.managed;
    if(!s.managed) {s.managed=true;s.generation=frame_.spawnGeneration;s.prepared=a.type!=4;}
    else if(a.type==23) {
        if(!lifecycle_.reserve_through(owner(),s.generation+1)) {return false;}++s.generation;
    }
    s.desired=active;s.position=position;s.acknowledged=false;
    if(a.type==4) {
        const auto o=object_index(a);if(o==kObjects.size()) {return false;}
        if(!active) {
            const auto next=objects_.state(o).generation+1;
            if(!lifecycle_.reserve_through(owner(),next) || !objects_.rearm(owner(),o,next)) {return false;}
            objects_.retire(o);
        }
        const auto state=objects_.state(o);if(active && state.phase==coo::ObjectPhase::retired) {return false;}
        s.generation=state.generation;s.prepared=state.phase>=coo::ObjectPhase::create;s.active=active && state.create;
    } else {s.active=active;}
    if(a.type==1 && active) {const auto n=spawn_index(a);if(n==kSpawns.size()) {return false;}population_.enable(n);}
    ++frame_.revision;return true;
}
bool Controller::cohort(Cohort id) noexcept {
    const auto i=static_cast<std::size_t>(id);if(i>=std::size(kCohorts)) {return false;}
    for(const auto m:kCohorts[i].members) {if(!request(asset(m.registry,1,m.slot),true)) {return false;}}
    return true;
}
bool Controller::cleared(Cohort id) const noexcept {
    const auto i=static_cast<std::size_t>(id);if(i>=std::size(kCohorts)) {return false;}
    for(const auto m:kCohorts[i].members) {
        const auto n=spawn_index(asset(m.registry,1,m.slot));
        if(n==kSpawns.size() || !population_.cleared(n,kSpawns[n].count)) {return false;}
    }
    return true;
}
bool Controller::prepared(coo::Generation gen,coo::Asset a) noexcept {
    const auto i=asset_index(a),o=object_index(a);
    if(!frame_.enabled || gen!=owner() || i==std::size(kAssets) || o==kObjects.size()) {return false;}
    auto& s=frame_.native[i];if(!s.managed || !s.desired || s.prepared || !objects_.prepared(gen,o)) {return false;}
    const auto state=objects_.state(o);s.generation=state.generation;s.prepared=true;s.active=state.create;++frame_.revision;return true;
}
bool Controller::object(const coo::ObjectReceipt& r) noexcept {
    const auto i=asset_index(r.source),o=object_index(r.source),p=pickup_index(r.source);
    if(!frame_.enabled || !r.valid() || i==std::size(kAssets) || o==kObjects.size()
        || !frame_.native[i].active || r.owner.run!=run_ || r.owner.value!=frame_.native[i].generation) {return false;}
    const auto prior=objects_.owner(o);
    if(prior.valid() && (prior.entity!=r.entity || prior.serial!=r.serial)) {
        // The reveal is a single mission event. Streaming must never recreate
        // its emitter and replay the flares when the player backtracks.
        if(r.source==kFlare && frame_.flareStarted) {static_cast<void>(request(kFlare,false));return false;}
        auto& s=frame_.native[i];const auto next=s.generation+1;
        if(!lifecycle_.reserve_through(owner(),next) || !objects_.rearm(owner(),o,next)) {frame_.fault=true;return false;}
        s.generation=next;s.active=s.prepared=s.acknowledged=false;
        if(p<std::size(kPickups) && !frame_.pickups[p].granted) {
            // A collected reward keeps its lease across streaming until delivery.
            // Unopened caches can bind their newly streamed entity.
            auto& pickup=frame_.pickups[p];
            if(!pickup.used) {pickup.binding={};}
        }
        ++frame_.revision;return false;
    }
    const bool changed=objects_.observe(o,r,true,0.F,1);if(objects_.owner(o)!=r) {return false;}
    if(r.source==kFlare) {frame_.flareStarted=true;}
    if(p<std::size(kPickups) && !frame_.pickups[p].used) {frame_.pickups[p].binding=r;}
    if(changed) {frame_.native[i].acknowledged=true;++frame_.revision;}return changed;
}
bool Controller::device(coo::Generation gen,coo::Asset a,const middleware::bap::activity_message::device_sense::Output& d) noexcept {
    const auto i=asset_index(a);if(gen!=owner() || !frame_.enabled || a.type!=23 || i==std::size(kAssets)) {return false;}
    for(unsigned n=0;n<3;++n) {
        if((d.present&(1U<<(n*2))) && !std::isfinite(d.values[n])) {return false;}
        if((d.present&(2U<<(n*2))) && (d.revisions[n]<0 || d.revisions[n]>=32766)) {return false;}
    }
    auto& s=frame_.native[i];
    for(unsigned n=0;n<3;++n) if(d.present&(2U<<(n*2))) {
        s.deviceVersions[n]=(std::max)(s.deviceVersions[n],d.revisions[n]);s.deviceSeen|=static_cast<std::uint8_t>(1U<<n);
    }
    if(!s.managed || s.deviceSynchronized || s.deviceSeen!=7) {return false;}
    const auto next=(std::max)(s.generation,static_cast<std::uint32_t>(*std::max_element(s.deviceVersions.begin(),s.deviceVersions.end()))+1U);
    if(!lifecycle_.reserve_through(owner(),next)) {frame_.fault=true;return false;}
    s.generation=next;s.deviceSynchronized=true;++frame_.revision;return true;
}
bool Controller::lights(coo::Generation gen,const coo::ObjectReceipt& binding) noexcept {
    const auto a=lighting::kSource;
    if(gen!=owner() || !frame_.enabled || !frame_.lightRequested || frame_.light || !frame_.ghost.atLights
        || binding.source!=a || !binding.valid() || objects_.owner(object_index(a))!=binding) {return false;}
    frame_.light=true;frame_.ghost.release_return();++frame_.revision;return true;
}
void Controller::actor(coo::Generation gen,coo::Asset a,const middleware::bap::activity_message::combatant_sense::Output& d) noexcept {
    if(gen==owner() && frame_.enabled && a==transport::kPilot) {frame_.skiff.observe(d);}
    if(gen==owner() && frame_.enabled && a==ghost::kActor) {frame_.ghost.observe(d);}
    // A Type-2 program cursor acknowledges scheduling, not entrance playback.
    // Ambush cues are released by the native sequence observer instead.
}
void Controller::ghost_sample(coo::Generation gen,const EnemyReceipt& actor,ghost::Sample sample) noexcept {
    if(gen!=owner() || !frame_.enabled || !actor.valid() || actor!=frame_.ghostActor) {return;}
    const auto before=frame_.ghost.phase;const auto node=frame_.ghost.node;
    frame_.ghost.observe(sample);
    if(before!=frame_.ghost.phase || node!=frame_.ghost.node) {++frame_.revision;}
}
bool Controller::passenger(coo::Generation gen,coo::Asset a,const middleware::bap::activity_message::native_sense::Passenger& d) noexcept {
    if(gen!=owner() || !frame_.enabled || a!=transport::kPassenger
        || !frame_.skiff.passenger(d.revision,d.registry,d.type,d.slot)) {return false;}
    ++frame_.revision;return true;
}
bool Controller::source(coo::Generation gen,coo::Asset a,const middleware::bap::activity_message::squad_sense::Output& d) noexcept {
    const auto i=spawn_index(a),n=asset_index(a);
    if(gen!=owner() || !frame_.enabled || i==kSpawns.size() || n==std::size(kAssets)
        || !frame_.native[n].active || frame_.native[n].sourceCleared || !kSpawns[i].tactical.registry) {return false;}
    auto& state=frame_.tactics[i];
    if(d.hasRevision) {
        if(d.revision!=frame_.native[n].generation) {return false;}
        if(state.revision!=d.revision) {state={};state.revision=d.revision;}
    }
    if(state.revision!=frame_.native[n].generation) {return false;}
    for(unsigned k=0;k<24;++k) if(d.costMask&(1U<<k)) {state.costs[k]=d.cost[k];state.known|=1U<<k;}
    std::int8_t best=-1;unsigned cost=127;
    for(unsigned k=0;k<24;++k) if((state.known&(1U<<k)) && state.costs[k]<cost) {best=static_cast<std::int8_t>(k);cost=state.costs[k];}
    if(best>=0 && state.group>=0 && (state.known&(1U<<state.group)) && state.costs[state.group]==cost) {best=state.group;}
    if(best==state.group) {return false;}state.group=best;const auto t=kSpawns[i].tactical;
    population_.policy(i,{true,coo::EnemyIntent::combat,t.registry,t.slot,best});++frame_.revision;return true;
}
bool Controller::use(coo::Generation gen,coo::Asset a,const middleware::bap::activity_message::object_sense::Output& d) noexcept {
    const auto p=pickup_index(a);if(gen!=owner() || !frame_.enabled || p==std::size(kPickups)
        || frame_.cinematic.phase!=cinematics::Phase::gameplay || !d.hasUse || !d.used || !d.alive || !d.present) {return false;}
    auto& s=frame_.pickups[p];
    if(!s.armed || s.used || !s.binding.valid() || d.generation!=static_cast<std::int32_t>(s.binding.owner.value)) {return false;}
    s.used=true;++frame_.revision;return true;
}
GrantRequest Controller::grant_request() const noexcept {
    if(!frame_.enabled || frame_.finished) {return {};}
    for(std::size_t i=0;i<std::size(kPickups);++i) {
        const auto& p=frame_.pickups[i];if(p.requested && !p.granted && p.binding.valid()) {return {owner(),p.binding,static_cast<std::uint8_t>(i),p.seed};}
    }return {};
}
bool Controller::cache_looted(coo::Generation gen,const coo::ObjectReceipt& binding) noexcept {
    const auto p=pickup_index(binding.source);
    if(p<1 || p>=std::size(kPickups) || frame_.pickups[p].binding!=binding || !binding.valid()) {return false;}
    middleware::bap::activity_message::object_sense::Output use{};
    use.hasUse=use.used=use.alive=use.present=true;use.generation=static_cast<std::int32_t>(binding.owner.value);
    if(!this->use(gen,binding.source,use)) {return false;}
    // This optional reward remains available after the assault objective ends.
    if(binding.source==kWalkerCache) {frame_.pickups[p].requested=true;}
    return true;
}
bool Controller::granted(const GrantRequest& r,std::uint64_t item) noexcept {
    if(!item || item==UINT64_MAX || r.owner!=owner() || r.pickup>=std::size(kPickups)) {return false;}
    auto& s=frame_.pickups[r.pickup];
    if(!s.requested || !s.used || s.granted || s.binding!=r.binding || s.seed!=r.seed) {return false;}
    s.granted=true;s.item=item;++frame_.revision;return true;
}
bool Controller::admitted(const EnemyReceipt& r) noexcept {
    const auto a=asset(r.registry,1,r.source);const auto i=spawn_index(a),n=asset_index(a);
    if(!frame_.enabled || !r.valid() || r.run!=run_ || i==kSpawns.size() || n==std::size(kAssets)) {return false;}
    auto& s=frame_.native[n];if(!s.active || s.sourceCleared || r.generation!=s.generation) {return false;}
    if(s.sourceOwner!=UINT32_MAX && s.sourceOwner!=r.owner) {
        if(!lifecycle_.reserve_through(owner(),s.generation+1) || !population_.restart(i)) {frame_.fault=true;return false;}
        ++s.generation;s.sourceOwner=r.owner;frame_.tactics[i]={};
        if(a==ghost::kSource) {
            frame_.ghostActor={};frame_.ghost.node=UINT8_MAX;frame_.ghost.receipt={};
            frame_.ghost.publication.program.generation=s.generation;
        }
        ++frame_.revision;return false;
    }
    const auto accepted=population_.admit(kPopulation,r,run_,s.generation);
    if(accepted==coo::Admission::accepted) {
        s.sourceOwner=r.owner;
        if(a==ghost::kSource) {frame_.ghostActor=r;}
        if(a==asset(kBreach,1,29)) {frame_.firstVandal=r;frame_.firstVandalStarted=false;}
        if(a.registry==kBreach) for(std::size_t j=0;j<std::size(kAmbushCues);++j) {
            if(a.slot==kAmbushCues[j].source) {frame_.ambushActors[j]=r;}
        }
    }
    if(accepted==coo::Admission::overflow) {frame_.fault=true;}
    return accepted==coo::Admission::accepted;
}
bool Controller::entrance(coo::Generation gen,const EnemyReceipt& actor) noexcept {
    if(gen!=owner() || !frame_.enabled || !actor.valid() || actor.registry!=kBreach
        || frame_.section!=2 || frame_.cinematic.phase!=cinematics::Phase::gameplay) {return false;}
    std::uint16_t sound{};
    if(actor==frame_.firstVandal && !frame_.firstVandalStarted) {sound=77;}
    for(std::size_t i=0;i<std::size(kAmbushCues);++i) {
        if(actor==frame_.ambushActors[i]) {sound=kAmbushCues[i].sound;}
    }
    if(!sound) {return false;}
    const auto& source=frame_.native[asset_index(asset(kBreach,1,actor.source))];
    const auto cue=asset(kBreach,5,sound);
    if(!source.active || source.sourceCleared || source.generation!=actor.generation
        || source.sourceOwner!=actor.owner || frame_.native[asset_index(cue)].active
        || !request(cue,true)) {return false;}
    if(sound==77) {frame_.firstVandalStarted=true;}
    ++frame_.revision;return true;
}
bool Controller::died(const EnemyReceipt& r) noexcept {
    const auto a=asset(r.registry,1,r.source);const auto i=spawn_index(a),n=asset_index(a);
    if(!frame_.enabled || i==kSpawns.size() || n==std::size(kAssets)) {return false;}
    auto& s=frame_.native[n];if(r.owner!=s.sourceOwner || !population_.died(r,run_,s.generation)) {return false;}
    if(a==transport::kTank && !frame_.pickups[3].armed) {
        auto& reward=frame_.pickups[3];reward.armed=true;reward.seed=now_^run_;
        if(!request(kWalkerCache,true)) {frame_.fault=true;return false;}
    }
    if(a.registry==kDivide && (a==transport::kTank || (a.slot>=6 && a.slot<=9))
        && frame_.assault && !frame_.shipFound && frame_.assaultDefeated<kAssaultTarget) {++frame_.assaultDefeated;++frame_.revision;}
    if(population_.cleared(i,kSpawns[i].count)) {
        s.sourceCleared=true;++frame_.revision;
        if(a.registry==kDivide && a.slot>=6 && a.slot<=8) {reinforcementAt_[a.slot-6]=now_+20000;}
    }return true;
}
void Controller::reinforce() noexcept {
    if(frame_.section!=3 || !frame_.assault || frame_.shipFound || frame_.assaultDefeated>=kAssaultTarget
        || now_<nextReinforcement_) {return;}
    for(std::size_t n=0;n<reinforcementAt_.size();++n) {
        if(!reinforcementAt_[n] || now_<reinforcementAt_[n]) {continue;}
        const auto a=asset(kDivide,1,static_cast<std::uint16_t>(n+6));auto& s=frame_.native[asset_index(a)];const auto i=spawn_index(a);
        if(!s.active || !s.sourceCleared || !population_.cleared(i,kSpawns[i].count)) {continue;}
        if(!lifecycle_.reserve_through(owner(),s.generation+1) || !population_.restart(i)) {frame_.fault=true;return;}
        ++s.generation;s.sourceCleared=false;s.sourceOwner=UINT32_MAX;frame_.tactics[i]={};
        reinforcementAt_[n]=0;nextReinforcement_=now_+8000;++frame_.revision;return;
    }
}
bool Controller::submitted(std::uint64_t run,std::uint32_t bank,std::uint8_t row,std::uint32_t generation,std::uint64_t now) noexcept {
    if(run!=run_ || !dialogue_.submitted(kDialoguePolicy,bank,row,generation,now,frame_,frame_.revision)) {return false;}
    submitted_.set(row);voiceEnd_[row]=now+kDialogue[row].durationMs;return true;
}
bool Controller::tower_arrived(coo::Generation gen) noexcept {
    if(gen!=owner() || !frame_.towerRequested || !lifecycle_.complete(gen)) {return false;}
    frame_.finished=true;frame_.completion=lifecycle_.publication();++frame_.revision;return true;
}
coo::MarkerTarget Controller::marker(std::uint32_t event) const noexcept {
    return navigation::marker(event);
}
bool Controller::publish(const coo::Command& c) noexcept {
    const auto& g=graph().definition;
    if(c.token.run!=run_ || c.token.step>=g.steps.size() || c.token.command>=g.steps[c.token.step].commands.size()
        || c.token!=executor_.token(c.token.step,c.token.command) || c.schema!=g.schema) {return false;}
    const auto& s=c.spec;
    switch(s.operation) {
    case coo::Operation::objective:dialogue_.objective(kDialoguePolicy,s.argument,frame_,frame_.revision);objectives_.set(s.argument,marker(s.argument));return true;
    case coo::Operation::dialogue:dialogue_.enqueue(kDialoguePolicy,static_cast<std::uint8_t>(s.argument),now_,0,frame_.section,frame_.revision);return true;
    case coo::Operation::population:return s.asset==kModule && cohort(static_cast<Cohort>(s.argument));
    case coo::Operation::device:return request(s.asset,s.asset.type==23 || s.argument!=0,s.asset.type==23?std::bit_cast<float>(s.argument):0.F);
    case coo::Operation::observation:return true;
    case coo::Operation::mechanic:
        if(s.asset==kModule) switch(static_cast<Mechanic>(s.argument)) {
        case Mechanic::next:phaseFinished_=true;return true;
        case Mechanic::light:
            if(!frame_.ghost.atLights || !request(lighting::kSource,true)) {return false;}
            frame_.lightRequested=true;++frame_.revision;return true;
        case Mechanic::shutter:
            return frame_.light && frame_.ghost.ready() && request(asset(kBreach,23,75),true,1.F);
        case Mechanic::ghostLights:
            if(!request(ghost::kSource,true)) {return false;}
            frame_.ghost.begin(frame_.native[asset_index(ghost::kSource)].generation);++frame_.revision;return true;
        case Mechanic::ghostRifle:frame_.ghost.rifle();++frame_.revision;return true;
        case Mechanic::ghostDismiss:frame_.ghost.dismiss();++frame_.revision;return true;
        case Mechanic::rifle:case Mechanic::shotgun:case Mechanic::rocket: {
            const auto i=s.argument-static_cast<std::uint32_t>(Mechanic::rifle);auto& p=frame_.pickups[i];
            if(!p.used || !p.binding.valid()) {return false;}p.requested=true;++frame_.revision;return true;
        }
        case Mechanic::ketch:
            frame_.ketch=true;frame_.ketchRevision=frame_.spawnGeneration+1;++frame_.revision;return true;
        case Mechanic::assault:
            frame_.assault=true;
            if(!request(transport::kCarrier,true) || !cohort(Cohort::walker)) {return false;}
            frame_.skiff.begin(frame_.native[asset_index(transport::kCarrier)].generation);++frame_.revision;return true;
        case Mechanic::scanShip:frame_.shipFound=true;++frame_.revision;return true;
        case Mechanic::finish:
            if(!cinematics_.finish_gameplay(owner(),now_)) {return false;}
            frame_.skiff.retire();dialogue_.silence(frame_,frame_.revision);objectives_.clear();frame_.cinematic=cinematics_.state();return true;
        default:return false;
        }
        if(s.argument==static_cast<std::uint32_t>(Mechanic::arm)) {
            const auto i=pickup_index(s.asset);if(i==std::size(kPickups)) {return false;}
            frame_.pickups[i].armed=true;++frame_.revision;return request(s.asset,true);
        }
        return false;
    default:return false;
    }
}
bool Controller::observed(const coo::CommandSpec& s) const noexcept {
    if(s.asset==volume(kExterior,11) && (enteredRegions_&1U)) {return true;}
    if(s.asset==kModule) {
        if(s.argument>=0x100 && s.argument<0x100+std::size(kCohorts)) {return cleared(static_cast<Cohort>(s.argument-0x100));}
        if(s.argument==static_cast<std::uint32_t>(Event::ketchStarted)) {return frame_.ketchStarted;}
        if(s.argument==static_cast<std::uint32_t>(Event::flareStarted)) {return frame_.flareStarted;}
        if(s.argument==static_cast<std::uint32_t>(Event::assaultComplete)) {return frame_.assaultDefeated>=kAssaultTarget;}
        if(s.argument==static_cast<std::uint32_t>(Event::shipFound)) {return frame_.shipFound;}
        if(s.argument==static_cast<std::uint32_t>(Event::ghostLightsComplete)) {return frame_.ghost.ready();}
        if(s.argument==static_cast<std::uint32_t>(Event::ghostAtLights)) {return frame_.ghost.atLights;}
        if(s.argument==static_cast<std::uint32_t>(Event::lightsOn)) {return frame_.light;}
        if(s.argument==static_cast<std::uint32_t>(Event::firstVandalNear)) {return frame_.firstVandalNear;}
    }
    if(s.asset==kDialogueAsset) {return s.argument<std::size(kDialogue) && submitted_[s.argument] && now_>=voiceEnd_[s.argument];}
    const auto p=pickup_index(s.asset);
    if(p<std::size(kPickups)) {return s.argument==static_cast<std::uint32_t>(Event::used)?frame_.pickups[p].used:s.argument==static_cast<std::uint32_t>(Event::granted) && frame_.pickups[p].granted;}
    for(std::size_t n=0;n<std::size(kVolumes);++n) {if(kVolumes[n].asset==s.asset) {
        return s.asset==volume(kHangarRoute,9)?hasPosition_ && contains(kVolumes[n],previous_):seen_[n];
    }}return false;
}
coo::StallDetail Controller::missing(const coo::CommandSpec& s) const noexcept {
    if(s.wait==coo::Wait::requested || observed(s)) {return {};}
    if(s.asset==kModule && s.argument>=0x100 && s.argument<0x100+std::size(kCohorts)) {
        for(const auto m:kCohorts[s.argument-0x100].members) {
            const auto a=asset(m.registry,1,m.slot);const auto i=spawn_index(a);auto d=population_.missing(i,kSpawns[i].count,true);
            if(d.missing!=coo::Missing::none) {d.asset=a;return d;}
        }
    }
    return {s.asset==kDialogueAsset?coo::Missing::dialogue:coo::Missing::observation,s.asset,1,0,s.argument};
}
bool Controller::advance(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    if(!run_ || run!=run_ || !ready) {return false;}now_=now;
    frame_.gameplayClockTicks=clock_.sample(now);cinematics_.advance(now);frame_.cinematic=cinematics_.state();
    if(frame_.cinematic.phase==cinematics::Phase::failed) {frame_.fault=true;frame_.enabled=false;return true;}
    if(frame_.cinematic.phase==cinematics::Phase::complete) {frame_.towerRequested=true;return true;}
    if(frame_.cinematic.phase!=cinematics::Phase::gameplay) {return true;}
    if(!started_) {started_=executor_.start(graph().definition,run_);}if(!started_) {frame_.fault=true;return false;}
    reinforce();
    const auto skiffPhase=frame_.skiff.phase;
    frame_.skiff.advance(population_.admitted(spawn_index(transport::kTank),1),now);
    if(skiffPhase!=frame_.skiff.phase) {++frame_.revision;}
    if(frame_.section==3 && skiffPhase==transport::Phase::enter && frame_.skiff.phase==transport::Phase::deliver) {
        dialogue_.enqueue(kDialoguePolicy,29,now,0,frame_.section,frame_.revision);
    }
    executor_.update(*this);
    for(std::size_t i=0;i<graph().definition.steps.size();++i) {
        const auto state=executor_.step_state(i);if(state.phase!=coo::StepPhase::active) {continue;}
        const auto& step=graph().definition.steps[i];
        for(std::size_t j=0;j<step.commands.size();++j) if(state.commands[j].requested && coo::is_observation(step.commands[j].operation) && observed(step.commands[j])) {
            static_cast<void>(executor_.enqueue({executor_.token(i,j),coo::Milestone::observed}));
        }
    }
    executor_.update(*this);executor_.update(*this);
    dialogue_.advance(kDialoguePolicy,frame_.spawnGeneration-1U,now,false,frame_,frame_.revision);
    if(phaseFinished_ && executor_.diagnostics().phase!=coo::Phase::failed && frame_.section+1U<mission().phases.size()) {
        phaseFinished_=false;executor_.cancel(*this);++frame_.section;
        // Optional branches in an area do not hold later sections hostage.
        dialogue_.discard_before(frame_.section);started_=executor_.start(graph().definition,run_);++frame_.revision;
    }
    frame_.bubble=frame_.section==0?3:frame_.section<3?0:frame_.section==3?1:2;
    frame_.enabled=!frame_.fault && executor_.diagnostics().phase!=coo::Phase::failed;
    frame_.presentation=objectives_.state();return true;
}
}
