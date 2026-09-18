#include "controller.h"
#include "navigation.h"
#include "sunburn.h"
#include "reactor_devices.h"
#include "escape_ship.h"
#include <cmath>
#include <bit>
#include <algorithm>
namespace dawn::state::activity::vanilla::one_au {
bool contains(const Volume& v,Point p) noexcept {
    if(!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || v.vertices.size()<3
        || p.x<v.min.x || p.x>v.max.x || p.y<v.min.y || p.y>v.max.y || p.z<v.min.z || p.z>v.max.z) {return false;}
    bool inside{};
    for(std::size_t i=0,j=v.vertices.size()-1;i<v.vertices.size();j=i++) {
        const auto a=v.vertices[j],b=v.vertices[i];const double dx=double(b.x)-a.x,dy=double(b.y)-a.y,px=double(p.x)-a.x,py=double(p.y)-a.y;
        if(dx==0 && dy==0) {continue;}
        if(std::abs(dx*py-dy*px)<0.00001 && px*dx+py*dy>=0 && px*dx+py*dy<=dx*dx+dy*dy) {return true;}
        if((a.y>p.y)!=(b.y>p.y) && p.x<dx*py/dy+a.x) {inside=!inside;}
    }
    return inside;
}
bool crosses(const Volume& v,Point from,Point to) noexcept {
    if(contains(v,from) || contains(v,to)) {return true;}
    if(v.vertices.size()<3) {return false;}
    double low=0,high=1;
    for(auto axis:{std::array<double,4>{from.x,to.x,v.min.x,v.max.x},
        std::array<double,4>{from.y,to.y,v.min.y,v.max.y},std::array<double,4>{from.z,to.z,v.min.z,v.max.z}}) {
        const auto delta=axis[1]-axis[0];
        if(std::abs(delta)<1e-8) {if(axis[0]<axis[2] || axis[0]>axis[3]) {return false;}continue;}
        auto a=(axis[2]-axis[0])/delta,b=(axis[3]-axis[0])/delta;if(a>b) {std::swap(a,b);}
        low=(std::max)(low,a);high=(std::min)(high,b);if(low>high) {return false;}
    }
    const auto at=[&](double t) {return Point{float(from.x+(to.x-from.x)*t),float(from.y+(to.y-from.y)*t),float(from.z+(to.z-from.z)*t)};};
    const auto a=at(low),b=at(high);if(contains(v,a) || contains(v,b)) {return true;}
    const double dx=double(b.x)-a.x,dy=double(b.y)-a.y;
    for(std::size_t i=0,j=v.vertices.size()-1;i<v.vertices.size();j=i++) {
        const auto p=v.vertices[j],q=v.vertices[i];const double ex=double(q.x)-p.x,ey=double(q.y)-p.y;
        const auto determinant=dx*ey-dy*ex;if(std::abs(determinant)<1e-8) {continue;}
        const double px=double(p.x)-a.x,py=double(p.y)-a.y;
        const auto t=(px*ey-py*ex)/determinant,u=(px*dy-py*dx)/determinant;
        if(t>=0 && t<=1 && u>=0 && u<=1) {return true;}
    }
    return false;
}
void Controller::reset() noexcept {
    executor_.cancel(*this);lifecycle_.reset();clock_.reset();run_=now_=0;livingPlayer_=UINT32_MAX;arrived_=started_=phaseFinished_=sunExposed_=false;
    objectives_={};dialogue_={};objects_={};population_={};seen_.reset();inside_.reset();hasPosition_=false;previousPosition_={};submitted_.reset();voiceEnd_={};missingSince_={};vents_={};ventPhase_={};rings_={};cinematics_={};refineryPoses_={};frame_={};
}
bool Controller::select(std::uint64_t run,std::uint64_t now) noexcept {
    if(run && run_==run) {return true;}reset();
    if(!run || !mission().valid() || !lifecycle_.begin(run,256) || !objects_.begin(owner(),kObjects)) {return false;}
    run_=run;frame_.spawnGeneration=owner().value;frame_.enabled=true;
    cinematics_.begin(owner(),now);frame_.cinematic=cinematics_.state();
    for(std::size_t i=0;i<kSpawns.size();++i) {const auto t=tactical(kSpawns[i]);population_.policy(i,{true,coo::EnemyIntent::combat,t.registry,t.slot,t.row});}
    return true;
}
bool Controller::fly_in_complete(coo::Generation gen) noexcept {
    if(!frame_.enabled || !cinematics_.fly_in_complete(gen)) {return false;}
    frame_.cinematic=cinematics_.state();++frame_.revision;return true;
}
bool Controller::arrival(coo::Generation gen,std::uint8_t route,std::uint64_t now) noexcept {
    const bool accepted=cinematics_.arrival(gen,route,now);
    if(accepted) {frame_.cinematic=cinematics_.state();++frame_.revision;}return accepted;
}
bool Controller::cinematic(coo::Generation gen,const cinematics::Incident& e,std::uint64_t now) noexcept {
    const bool accepted=cinematics_.incident(gen,e.target,e.registry,e.type,e.slot,e.runtime,now);
    if(accepted) {frame_.cinematic=cinematics_.state();++frame_.revision;}return accepted;
}
void Controller::life(coo::Generation gen,std::uint32_t entity,bool alive,std::uint64_t now) noexcept {
    if(gen!=owner() || entity==UINT32_MAX || frame_.cinematic.phase!=cinematics::Phase::gameplay || frame_.finished) {return;}
    if(alive) {
        livingPlayer_=entity;
        if(frame_.recovery.phase==recovery::Phase::countdown) {frame_.recovery={};++frame_.revision;}
    } else if(entity==livingPlayer_ && frame_.restricted && !frame_.recovery.active()) {
        frame_.recovery={};frame_.recovery.phase=recovery::Phase::countdown;frame_.recovery.point=recovery::checkpoint(frame_.section);
        frame_.recovery.deadline=now+recovery::kCountdownMs;++frame_.revision;
    }
}
void Controller::restore_checkpoint(std::uint64_t now) noexcept {
    const auto old=frame_;const auto run=run_;const auto clock=clock_;reset();
    if(!select(run,now)) {frame_.fault=true;return;}
    clock_=clock;cinematics_.resume_gameplay(owner());frame_.cinematic=cinematics_.state();arrived_=true;
    frame_.leverUsed=old.leverUsed;
    frame_.recovery=old.recovery;frame_.recovery.reset=true;
    frame_.section=static_cast<std::uint8_t>(old.recovery.point.section);frame_.bubble=old.recovery.point.bubble;
    frame_.restricted=true;frame_.bridgeRestore=old.recovery.point.section==Section::bridge;frame_.processingRestore=old.recovery.point.section==Section::processing;
    for(std::size_t i=0;i<frame_.native.size();++i) {
        if(!old.native[i].managed) {continue;}auto& n=frame_.native[i];
        n.managed=true;n.generation=frame_.spawnGeneration;n.position=old.native[i].position;n.power=0;n.snap=true;n.prepared=kAssets[i].asset.type!=4;
    }
    if(frame_.bridgeRestore) {frame_.interactions[0].completed=true;}
    if(old.transport.started) {
        frame_.transport.authorityGeneration=old.transport.authorityGeneration;
        for(auto& ship:frame_.transport.ships) {ship.phase=transport::Phase::retired;}
    }
    frame_.hazards.deckRevision=frame_.hazards.coreRevision=frame_.hazards.grinderRevision=frame_.hazards.burnRevision=frame_.spawnGeneration;
    // Replacement beam devices require the same initialization handshake as
    // first creation, independently of their retired instances' previous modes.
    for(const auto slot:std::initializer_list<std::uint16_t>{48,49}) {frame_.native[asset_index(asset(kCore,23,slot))]={};}
    if(old.recovery.point.section==Section::escape) {
        frame_.interactions[4].inserted=true;
        for(std::size_t i=5;i<8;++i) {frame_.interactions[i].destroyed=true;}
        // A newly created escape ship needs a fresh initial pose before its
        // approach. The old device's managed flag belongs to the retired ship.
        frame_.native[asset_index(asset(kCore,23,10))]={};
    }
    ++frame_.revision;
}
recovery::Spawn Controller::spawn(recovery::Spawn client,std::uint64_t now) noexcept {
    if(recovery::project(frame_.recovery,client)) {restore_checkpoint(now);static_cast<void>(recovery::project(frame_.recovery,client));}
    return frame_.recovery.holding()?frame_.recovery.host:client;
}
bool Controller::mounted(const MountedPosition& sample) noexcept {
    if(sample.owner!=owner() || !frame_.enabled || frame_.finished || frame_.recovery.active()
        || frame_.cinematic.phase!=cinematics::Phase::gameplay
        || sample.player==UINT32_MAX || sample.vehicle==UINT32_MAX
        || sample.player==sample.vehicle || !std::isfinite(sample.position.x)
        || !std::isfinite(sample.position.y) || !std::isfinite(sample.position.z)) {return false;}
    position(sample.owner.run,sample.position);return true;
}
void Controller::position(std::uint64_t run,Point p) noexcept {
    if(!run_ || run!=run_ || frame_.finished || frame_.recovery.holding() || !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {return;}
    inside_.reset();
    for(std::size_t i=0;i<std::size(kVolumes);++i) {if(contains(kVolumes[i],p)) {inside_.set(i);}}
    sunExposed_=false;
    for(std::size_t i=0;i<std::size(kVolumes);++i) {
        if(kVolumes[i].asset==hazards::kDeckVolume[0] && inside_[i]) {sunExposed_=!sunburn::sheltered(p);break;}
    }
    if(!arrived_) {for(std::size_t i=0;i<std::size(kVolumes);++i) {if(kVolumes[i].asset==volume(kLanding,86) && inside_[i]) {arrived_=true;}}}
    if(arrived_) {
        seen_|=inside_;
        const double dx=double(p.x)-previousPosition_.x,dy=double(p.y)-previousPosition_.y,dz=double(p.z)-previousPosition_.z;
        // Loading/checkpoint discontinuities are not a traversed route.
        if(hasPosition_ && dx*dx+dy*dy+dz*dz<=10000.) {
            for(std::size_t i=0;i<std::size(kVolumes);++i) {if(!seen_[i] && crosses(kVolumes[i],previousPosition_,p)) {seen_.set(i);}}
        }
    }
    previousPosition_=p;hasPosition_=true;
    if(frame_.section==static_cast<std::uint8_t>(Section::escape)) {
        for(std::size_t i=0;i<std::size(kVolumes);++i) {
            if(inside_[i] && kVolumes[i].asset==volume(0xAD062E98,17)) {frame_.escapeClock=false;frame_.escapeStopped=true;}
        }
    }
    if(frame_.enabled && frame_.section==static_cast<std::uint8_t>(Section::escape)) {
        // Reconstruction policy pairs the four authored explosion sets and
        // native input events in serialized order. Keep the native cast order
        // and send each input once; the retail trigger/event join is unverified.
        for(std::size_t set=0;set<4;++set) {
            const auto trigger=volume(kCore,static_cast<std::uint16_t>(242+set));bool crossed{};
            for(std::size_t n=0;n<std::size(kVolumes);++n) {if(kVolumes[n].asset==trigger && inside_[n]) {crossed=true;}}
            if(!crossed) {continue;}const auto event=kScenes[0].inputs[set];bool duplicate{};
            for(std::size_t n=0;n<frame_.explosionCount;++n) {duplicate|=frame_.explosionInputs[n]==event;}
            if(!duplicate && frame_.explosionCount<4) {frame_.explosionInputs[frame_.explosionCount++]=event;++frame_.revision;}
        }
    }
}
bool Controller::request(coo::Asset a,bool active,float position,float power,float lock,bool snap) noexcept {
    const auto i=asset_index(a);if(i==std::size(kAssets) || !std::isfinite(position)) {return false;}
    auto& s=frame_.native[i];
    if(a.type!=5 && s.managed && s.desired==active && s.position==position && s.power==power && s.lock==lock) {return true;}
    if(a.type==5) {if(s.sequenceRevision==254) {return false;}++s.sequenceRevision;}
    s.snap=!s.managed || snap;
    if(!s.managed) {s.managed=true;s.generation=frame_.spawnGeneration;s.prepared=a.type!=4;}
    else if(a.type!=1 && a.type!=4 && a.type!=5) {if(s.generation>=32766 || !lifecycle_.reserve_through(owner(),s.generation+1)) {return false;}++s.generation;}
    s.desired=active;s.position=position;s.power=power;s.lock=lock;s.acknowledged=false;
    if(a.type==23 && s.deviceSeen==7) {
        const auto next=static_cast<std::uint32_t>(*std::max_element(s.deviceVersions.begin(),s.deviceVersions.end()))+1U;
        s.generation=(std::max)(s.generation,next);s.deviceSynchronized=true;
        if(s.generation>=32767 || !lifecycle_.reserve_through(owner(),s.generation)) {return false;}
    }
    if(a.type==4) {
        const auto o=object_index(a);if(o==kObjects.size()) {return false;}
        if(!active) {
            // 9F19F0 retires the created entity on a NEW generation, including
            // a carry entity attached to the player. active=false alone is not enough.
            const auto next=objects_.state(o).generation+1;
            if(!lifecycle_.reserve_through(owner(),next) || !objects_.rearm(owner(),o,next)) {return false;}
            objects_.retire(o);
        }
        const auto state=objects_.state(o);
        if(active && state.phase==coo::ObjectPhase::retired) {return false;}
        s.generation=state.generation;s.prepared=state.phase>=coo::ObjectPhase::create;s.active=active && state.create;
    } else {s.active=active;}
    if(a.type==1 && active) {const auto n=spawn_index(a);if(n==kSpawns.size()) {return false;}population_.enable(n);}
    ++frame_.revision;return true;
}
bool Controller::device(coo::Generation gen,coo::Asset a,const middleware::bap::activity_message::device_sense::Output& d) noexcept {
    const auto i=asset_index(a);
    if(gen!=owner() || !frame_.enabled || frame_.finished || frame_.recovery.holding() || a.type!=23 || i==std::size(kAssets)) {return false;}
    for(unsigned n=0;n<3;++n) {
        if((d.present&(1U<<(n*2))) && !std::isfinite(d.values[n])) {return false;}
        if((d.present&(2U<<(n*2))) && (d.revisions[n]<0 || d.revisions[n]>=32766)) {return false;}
    }
    auto& s=frame_.native[i];
    for(unsigned n=0;n<3;++n) {
        if(d.present&(2U<<(n*2))) {s.deviceVersions[n]=(std::max)(s.deviceVersions[n],d.revisions[n]);s.deviceSeen|=static_cast<std::uint8_t>(1U<<n);}
    }
    if(a.registry==kProcessing && a.slot<refineryPoses_.size()) {
        const bool applied=refineryPoses_[a.slot].observe(d,s.generation,s.position);
        if(applied && s.managed && s.deviceSynchronized && !s.acknowledged) {
            s.acknowledged=true;++frame_.revision;return true;
        }
    }
    // Native initial modes can acknowledge their applied pose. A create
    // receipt or the mere publication of a device command is not that receipt.
    // Native sense may omit unchanged floats. A revision acknowledging a snap
    // command proves application; requiring those optional floats deadlocks
    // initial zero/one defaults. Animated targets cannot use this shortcut.
    if(s.managed && s.deviceSynchronized && !s.acknowledged && s.snap && s.position==0.F && s.deviceSeen==7
        && (a==asset(kCore,23,48) || a==asset(kCore,23,10))) {
        bool applied=true;
        for(unsigned n=0;n<3;++n) {applied&=s.deviceVersions[n]==static_cast<std::int32_t>(s.generation);}
        if(applied) {s.acknowledged=true;++frame_.revision;return true;}
    }
    if(!s.managed || s.deviceSynchronized || s.deviceSeen!=7) {return false;}
    // 106AD60 accepts only a strictly newer version than the entity controller.
    // Bootstrap against its counters once; position feedback during animation
    // must never restart that animation or manufacture a completed movement.
    const auto next=(std::max)(s.generation,static_cast<std::uint32_t>(*std::max_element(s.deviceVersions.begin(),s.deviceVersions.end()))+1U);
    // A retry lease can already exceed a newly streamed device's counters.
    // Reserve our command generation, never a value below the current owner.
    if(!lifecycle_.reserve_through(owner(),next)) {frame_.fault=true;return false;}
    s.generation=(std::max)(s.generation,next);s.deviceSynchronized=true;++frame_.revision;return true;
}
bool Controller::cohort(Cohort id) noexcept {
    const auto i=static_cast<std::size_t>(id);if(i>=std::size(kCohorts)) {return false;}
    for(const auto m:kCohorts[i].members) {
        const auto a=asset(m.registry,1,m.slot);const auto n=spawn_index(a);
        if(n==kSpawns.size() || kSpawns[n].sceneOwned || !request(a,true)) {return false;}
    }
    return true;
}
bool Controller::source(coo::Generation gen,coo::Asset a,const middleware::bap::activity_message::source_sense::Output& d) noexcept {
    const auto i=spawn_index(a),n=asset_index(a);
    if(gen!=owner() || !frame_.enabled || frame_.fault || frame_.finished || frame_.recovery.holding()
        || i==kSpawns.size() || n==std::size(kAssets) || kSpawns[i].sceneOwned
        || !frame_.native[n].active || frame_.native[n].sourceCleared) {return false;}
    auto& state=frame_.tactics[i];
    // Field 1 echoes the revision of the requested native cost pass.
    if(d.present&2U) {
        if(d.counters[1]!=frame_.native[n].generation) {return false;}
        if(state.revision!=d.counters[1]) {state={};state.revision=d.counters[1];}
    }
    if(state.revision!=frame_.native[n].generation) {return false;}
    auto task=tactical(kSpawns[i]);
    // The cost mask names the groups the task actually has. A separate group
    // count would only duplicate it, and disagree when it was wrong.
    constexpr unsigned kTaskGroups=24;
    for(unsigned k=0;k<kTaskGroups;++k) {if(d.costMask&(1U<<k)) {state.costs[k]=d.costs[k];state.known|=1U<<k;}}
    std::int8_t best=-1;unsigned cost=127;
    for(unsigned k=0;k<kTaskGroups;++k) {if((state.known&(1U<<k)) && state.costs[k]<cost) {best=static_cast<std::int8_t>(k);cost=state.costs[k];}}
    if(best>=0 && state.group>=0 && (state.known&(1U<<state.group)) && state.costs[state.group]==cost) {best=state.group;}
    if(best==state.group) {return false;}
    state.group=best;task.row=best;
    population_.policy(i,{true,coo::EnemyIntent::combat,task.registry,task.slot,task.row});
    ++frame_.revision;return true;
}
bool Controller::cleared(Cohort id) const noexcept {
    const auto i=static_cast<std::size_t>(id);if(i>=std::size(kCohorts)) {return false;}
    for(const auto m:kCohorts[i].members) {const auto n=spawn_index(asset(m.registry,1,m.slot));if(n==kSpawns.size() || !population_.cleared(n,kSpawns[n].count)) {return false;}}
    return true;
}
bool Controller::prepared(coo::Generation gen,coo::Asset a) noexcept {
    const auto i=asset_index(a),o=object_index(a);
    if(!frame_.enabled || gen!=owner() || i==std::size(kAssets) || o==kObjects.size()) {return false;}
    auto& s=frame_.native[i];if(!s.managed || !s.desired || s.prepared || !objects_.prepared(gen,o)) {return false;}
    const auto state=objects_.state(o);s.generation=state.generation;s.prepared=true;s.active=state.create;++frame_.revision;return true;
}
bool Controller::object(const coo::ObjectReceipt& r) noexcept {
    const auto i=asset_index(r.source),o=object_index(r.source);
    if(!frame_.enabled || frame_.fault || !r.valid() || i==std::size(kAssets) || o==kObjects.size()
        || !frame_.native[i].active || r.owner.run!=run_ || r.owner.value!=frame_.native[i].generation) {return false;}
    const auto n=interaction_index(r.source);const auto prior=objects_.owner(o);
    if(n<std::size(kInteractionAssets) && prior.valid() && (prior.entity!=r.entity || prior.serial!=r.serial)) {
        // The existing source observer has verified a different native entity
        // at this exact authored source. Recreate with a fresh command before
        // binding it; receipts from the unloaded incarnation stay invalid.
        if(frame_.interactions[n].destroyed) {return request(r.source,false);}
        if(!rearm_interaction(n)) {frame_.fault=true;}
        return false;
    }
    const bool changed=objects_.observe(o,r,true,0.F,1);
    if(objects_.owner(o)!=r) {return false;}
    if(n<missingSince_.size()) {missingSince_[n]=0;}
    if(changed) {frame_.native[i].acknowledged=true;++frame_.revision;}return changed;
}
bool Controller::rearm_interaction(std::size_t i) noexcept {
    if(i==0 || i>=std::size(kInteractionAssets)) {return false;}
    const auto rearm=[&](std::size_t n) {
        const auto a=kInteractionAssets[n];auto& native=frame_.native[asset_index(a)];
        if(!native.managed || !native.desired) {return true;}
        const auto generation=native.generation+1;
        if(!lifecycle_.reserve_through(owner(),generation+1) || !objects_.rearm(owner(),object_index(a),generation)) {return false;}
        native.generation=generation;native.active=native.prepared=native.acknowledged=false;
        auto& state=frame_.interactions[n];state.binding={};state.held=false;state.holder=UINT32_MAX;
        // Terminal mission facts survive streaming. Incomplete interactions
        // require fresh native pickup/use/health observations.
        if(!state.inserted && !state.completed && !state.destroyed) {state.started=false;state.acceptedCellGeneration=0;}
        missingSince_[n]=0;return true;
    };
    if(!rearm(i)) {return false;}
    // A sink's pending accepted-use receipt belongs to the previous cell.
    if((i==1 || i==3) && !frame_.interactions[i].inserted && !rearm(i+1)) {return false;}
    ++frame_.revision;return true;
}
bool Controller::lost(const ComponentReceipt& r,std::uint64_t now) noexcept {
    const auto i=interaction_index(r.asset);
    if(i!=1 && i!=3) {return false;}
    auto& cell=frame_.interactions[i];
    const bool currentCell=i==1?(frame_.section==static_cast<std::uint8_t>(Section::processing))
        :(frame_.section==static_cast<std::uint8_t>(Section::core));
    if(!frame_.enabled || !currentCell || !cell.armed || cell.inserted || cell.binding!=r || r.owner.run!=run_) {return false;}
    auto& since=missingSince_[i];if(!since) {since=now;return false;}if(now<since || now-since<1000) {return false;}
    if(!rearm_interaction(i)) {frame_.fault=true;return false;}return true;
}
bool Controller::bind(const ComponentReceipt& r) noexcept {
    const auto i=interaction_index(r.asset),o=object_index(r.asset);
    if(!r.valid() || !frame_.enabled || r.owner.run!=run_ || i==std::size(kInteractionAssets) || o==kObjects.size()) {return false;}
    const auto obj=objects_.owner(o);auto& state=frame_.interactions[i];
    if(obj.owner!=r.owner || obj.entity!=r.entity || obj.serial!=r.serial || state.binding.valid()) {return false;}
    state.binding=r;++frame_.revision;return true;
}
InteractionState* Controller::interaction(const ComponentReceipt& r) noexcept {
    const auto i=interaction_index(r.asset);
    if(!r.valid() || !frame_.enabled || frame_.finished || r.owner.run!=run_ || i==std::size(kInteractionAssets)) {return nullptr;}
    auto& s=frame_.interactions[i];return s.armed && s.binding==r?&s:nullptr;
}
bool Controller::carry(const ComponentReceipt& r,std::uint32_t holder,bool held) noexcept {
    auto* s=interaction(r);const auto i=interaction_index(r.asset);
    if(!s || (i!=1 && i!=3) || s->inserted || (held && holder==UINT32_MAX)) {return false;}
    if(s->held==held && s->holder==holder) {return false;}
    s->held=held;s->holder=held?holder:UINT32_MAX;if(held) {s->started=true;}++frame_.revision;static_cast<void>(complete_insert(i+1));return true;
}
bool Controller::complete_insert(std::size_t i) noexcept {
    auto& sink=frame_.interactions[i];auto& cell=frame_.interactions[i-1];
    const auto a=kInteractionAssets[i-1];const auto& native=frame_.native[asset_index(a)];
    if(!sink.started || sink.inserted || !cell.started || !cell.binding.valid()
        || cell.binding.owner.run!=run_ || sink.acceptedCellGeneration!=native.generation) {return false;}
    if(!request(a,false)) {frame_.fault=true;return false;}
    sink.inserted=cell.inserted=true;cell.held=false;cell.holder=UINT32_MAX;++frame_.revision;return true;
}
bool Controller::use(coo::Generation gen,coo::Asset a,const middleware::bap::activity_message::object_sense::Output& d) noexcept {
    const auto n=asset_index(a);
    if(gen!=owner() || !frame_.enabled || frame_.finished || frame_.recovery.holding()
        || frame_.cinematic.phase!=cinematics::Phase::gameplay || n==std::size(kAssets)
        || !use_subscription(a) || !d.hasUse || !d.used || !d.alive || !d.present) {return false;}
    const auto& source=frame_.native[n];
    if(!source.managed || !source.prepared || !source.active || d.generation<=0
        || static_cast<std::uint32_t>(d.generation)!=source.generation) {return false;}
    if(a==kLever) {
        if(frame_.leverUsed || frame_.section>static_cast<std::uint8_t>(Section::bridge)) {return false;}
        frame_.leverUsed=true;
        if(!request(asset(kBridge,23,7),true,1.F) || !request(asset(kBridge,23,8),true,1.F)
            || !request(asset(kBridge,23,9),true,1.F) || !cohort(Cohort::mercuryBonus)) {frame_.fault=true;return false;}
        ++frame_.revision;return true;
    }
    const auto i=interaction_index(a);if(i!=2 && i!=4) {return false;}
    auto& sink=frame_.interactions[i];
    if(!sink.armed || sink.started || sink.inserted
        || frame_.section!=static_cast<std::uint8_t>(i==2?Section::processing:Section::core)) {return false;}
    // Retain the authenticated reply if carry observation is still in flight.
    // Native 80804FB2 has already checked the held item's criteria and accepted use.
    sink.started=true;sink.acceptedCellGeneration=frame_.native[asset_index(kInteractionAssets[i-1])].generation;
    ++frame_.revision;static_cast<void>(complete_insert(i));return true;
}
bool Controller::landing_props() noexcept {
    if(frame_.section>static_cast<std::uint8_t>(Section::bridge)) {return true;}
    const auto position=frame_.leverUsed?1.F:0.F;
    return request(kLever,true) && request(asset(kBridge,23,7),true,position)
        && request(asset(kBridge,23,8),true,position) && request(asset(kBridge,23,9),true,position);
}
bool Controller::destruction(const ComponentReceipt& r,bool dead) noexcept {
    auto* s=interaction(r);const auto i=interaction_index(r.asset);
    if(!s || i<5 || s->destroyed) {return false;}
    // Binding alone, a missing health component, or an initially dead/reused box
    // cannot complete a target. Observe its live native health before its death.
    if(!dead) {s->started=true;return true;}
    if(!s->started) {return false;}s->destroyed=true;++frame_.revision;
    if(i==5 || i==6) {return request(asset(kCore,43,i==5?50:51),true);}
    return true;
}
bool Controller::admitted(const EnemyReceipt& r) noexcept {
    const auto a=asset(r.registry,1,r.source);const auto i=spawn_index(a),n=asset_index(a);
    if(!frame_.enabled || frame_.fault || frame_.finished || !r.valid() || r.run!=run_
        || i==kSpawns.size() || n==std::size(kAssets)) {return false;}
    auto& source=frame_.native[n];
    if(!source.active || source.sourceCleared || r.generation!=source.generation) {return false;}
    if(source.sourceOwner!=UINT32_MAX && source.sourceOwner!=r.owner) {
        // Backtracking re-streams this authored source under a different salted
        // native owner. Old live actors cannot satisfy the new fight. Retire
        // their generation and restart ONLY this unfinished source's requests.
        // Native categories spawn whole requests, so a partially killed source
        // restarts its authored group; cleared sources never reach this branch.
        const auto generation=source.generation+1;
        if(!lifecycle_.reserve_through(owner(),generation) || !population_.restart(i)) {frame_.fault=true;return false;}
        source.generation=generation;source.sourceOwner=r.owner;++frame_.revision;return false;
    }
    const auto result=population_.admit(kSpawns,r,run_,source.generation);
    if(result==coo::Admission::accepted) {source.sourceOwner=r.owner;}
    if(result==coo::Admission::overflow) {frame_.fault=true;}return result==coo::Admission::accepted;
}
bool Controller::died(const EnemyReceipt& r) noexcept {
    const auto a=asset(r.registry,1,r.source);const auto i=spawn_index(a),n=asset_index(a);
    if(!frame_.enabled || frame_.fault || frame_.finished || i==kSpawns.size() || n==std::size(kAssets)) {return false;}
    auto& source=frame_.native[n];
    if(r.owner!=source.sourceOwner || !population_.died(r,run_,source.generation)) {return false;}
    if(population_.cleared(i,kSpawns[i].count)) {source.sourceCleared=true;++frame_.revision;}
    return true;
}
bool Controller::actor(coo::Generation gen,std::uint32_t key,std::uint8_t type,std::uint16_t slot,const transport::Delta& d,std::uint64_t now) noexcept {
    const auto i=transport::member(key,type,slot);
    if(gen!=owner() || i<0 || !frame_.transport.started || frame_.recovery.holding()) {return false;}
    if(!transport::observe(frame_.transport.ships[static_cast<std::size_t>(i)],d,frame_.spawnGeneration,now)) {return false;}
    ++frame_.revision;return true;
}
bool Controller::ghost(coo::Generation gen,coo::Asset a,const middleware::bap::activity_message::ghost_sense::Output& d) noexcept {
    auto& s=frame_.interactions[0];
    if(gen!=owner() || a!=kBridgeLink || !frame_.enabled || frame_.finished || frame_.recovery.holding()
        || frame_.section!=static_cast<std::uint8_t>(Section::bridge) || !s.armed || s.completed
        || d.generation!=static_cast<std::int32_t>(frame_.spawnGeneration+1U)
        || !std::isfinite(d.progress) || d.progress<0.F) {return false;}
    const bool started=d.active || d.progress>0.F,completed=!d.active && d.progress>=1.F;
    if((!started || s.started) && !completed) {return false;}
    s.started|=started;s.completed=completed;++frame_.revision;return true;
}
bool Controller::transport() noexcept {
    auto& t=frame_.transport;
    if(frame_.section!=static_cast<std::uint8_t>(Section::bridge) && !t.started) {return true;}
    if(!t.prepared) {
        // Ships and their cargo are both created up front: a carried squad has to
        // exist for the harvester to hold it, and withholding it leaves the ship
        // stuck in hover waiting on an admission that never comes. Cargo is not
        // placed in the world, which is why it does not stand on the unbuilt deck.
        for(auto slot:transport::kParents) {if(!request(asset(kBridge,1,slot),true)) {return false;}}
        for(auto slot:transport::kCargo) {if(!request(asset(kBridge,1,slot),true)) {return false;}}
        for(auto slot:transport::kCarried) {if(!request(asset(kBridge,1,slot),true)) {return false;}}
        t.prepared=true;++frame_.revision;
    }
    if(frame_.interactions[0].completed && !t.started) {
        t.authorityGeneration=frame_.spawnGeneration;t.started=true;for(auto& ship:t.ships) {ship={};ship.phase=transport::Phase::entry;}++frame_.revision;
    }
    if(!t.started) {return true;}
    for(std::size_t i=0;i<t.ships.size();++i) {
        bool admitted=true;
        if(i<2) {for(auto slot:{transport::kCargo[i],transport::kCargo[i+2],transport::kCarried[i]}) {
            const auto index=spawn_index(asset(kBridge,1,slot));admitted&=population_.admitted(index,kSpawns[index].count);
        }}
        if(transport::advance(t,i,admitted,now_)) {++frame_.revision;}
    }
    return true;
}
bool Controller::vents() noexcept {
    if(frame_.section<static_cast<std::uint8_t>(Section::access)) {return true;}
    const bool overloaded=frame_.interactions[4].inserted;
    const bool bridges=frame_.interactions[5].destroyed && frame_.interactions[6].destroyed;
    for(const auto slot:std::initializer_list<std::uint16_t>{6,7}) {if(!request(asset(kCore,23,slot),true,bridges?0.F:1.F)) {return false;}}
    for(std::size_t i=0;i<2;++i) {
        if(!request(asset(kCore,23,static_cast<std::uint16_t>(4+i)),true,frame_.interactions[5+i].destroyed?1.F:0.F)) {return false;}
    }
    // Both doors lie on the route to the cell; the cell-room volume is behind them.
    for(const auto slot:std::initializer_list<std::uint16_t>{1,8}) {
        if(!request(asset(kCore,23,slot),true,frame_.interactions[7].destroyed?1.F:0.F)) {return false;}
    }
    if(!request(asset(kCore,4,46),true) || !request(asset(kCore,4,47),true)
        || !request(asset(kCore,4,45),!overloaded)) {return false;}
    // Cinder exposes the long weapon beam through a separate object/device.
    // The Apex laser effects do not create this regional beam for it.
    if(!overloaded && (!request(asset(0x8162BF78U,4,6),true)
        || !request(asset(0x8162BF78U,23,0),true,1.F))) {return false;}
    const auto& emitter=frame_.native[asset_index(asset(kCore,4,45))];
    const auto& laser=frame_.native[asset_index(asset(kCore,23,48))];
    // Preserve the laser's existing initialization handshake. Ring creation
    // and device echoes are not guaranteed; they must never gate this emitter
    // or the encounter. The ring receives its authored mode directly below.
    if(!overloaded && !frame_.laserPrimed && emitter.acknowledged
        && laser.managed && laser.acknowledged) {
        frame_.laserPrimed=true;++frame_.revision;
    }
    auto phase=VentCycle::Phase::idle;
    // Gameplay owns the weapon clock. Missing presentation feedback must not
    // stop alarms, damage windows, or the ring's cycle commands.
    if(frame_.interactions[5].armed && !overloaded) {phase=vents_.sample(now_);}
    // The alarm and laser fire graph share the same rising edge. Its three-second
    // lead-in overlaps the ring's final wait before the visible burst and throw.
    if(phase==VentCycle::Phase::windup && ventPhase_!=phase) {
        constexpr std::uint16_t alarm[]{28,36,37};
        for(std::size_t i=0;i<3;++i) {
            const auto& t=frame_.interactions[5+i];
            if(t.armed && !t.destroyed && !request(asset(kCore,5,alarm[i]),true)) {return false;}
        }
    }
    ventPhase_=phase;
    // These positions select graph modes, not physical transforms. Snap the
    // selector so device interpolation cannot miss a narrow band or delay one
    // graph relative to the other. The selected graph animates the actual VFX.
    const auto mode=[&](std::uint16_t slot,float position,float power=reactor_devices::kPowerOn) {
        return request(asset(kCore,23,slot),true,position,power,0.F,true);
    };
    if(overloaded) {
        if(!mode(48,reactor_devices::kLaserInitial,reactor_devices::kPowerOff)
            || !mode(49,reactor_devices::kRingInitial,reactor_devices::kPowerOff)) {return false;}
        // Other regions expose the same weapon through their own beam objects.
        for(const auto a:{asset(0x8162BF78U,4,6),asset(kProcessing,4,30)}) {
            if(frame_.native[asset_index(a)].managed && !request(a,false)) {return false;}
        }
        for(const auto a:{asset(0x8162BF78U,23,0),asset(kProcessing,23,7),asset(0x2894594AU,23,0)}) {
            if(frame_.native[asset_index(a)].managed && !request(a,true,0.F,0.F)) {return false;}
        }
    } else {
        if(emitter.acknowledged && !mode(48,frame_.laserPrimed?reactor_devices::laser(phase):reactor_devices::kLaserInitial)) {return false;}
        // Either side exchanger disables the outer ring first; both disable the
        // middle ring, and the final reactor disables the innermost ring.
        const auto destroyed=static_cast<std::uint8_t>(frame_.interactions[5].destroyed
            +frame_.interactions[6].destroyed+frame_.interactions[7].destroyed);
        if(!mode(49,rings_.sample(now_,phase,vents_.elapsed(),destroyed))) {return false;}
    }
    constexpr std::uint16_t doors[][2]{{21,22},{29,30},{38,39}};
    constexpr std::uint16_t lights[][2]{{23,24},{31,32},{40,41}};
    for(std::size_t i=0;i<3;++i) {
        const auto& target=frame_.interactions[5+i];
        const bool open=target.destroyed || (target.armed && phase==VentCycle::Phase::cooldown);
        for(const auto slot:doors[i]) {if(!request(asset(kCore,23,slot),true,open?1.F:0.F)) {return false;}}
        const auto lit=open && !target.destroyed?1.F:0.F;
        for(const auto slot:lights[i]) {if(!request(asset(kCore,23,slot),true,lit,lit)) {return false;}}
        if(i<2 && !request(asset(kCore,23,i==0?25:33),true,lit,lit)) {return false;}
        // 80B7125D's deactivate node selects 0.9..1.0. Keep that pose through
        // overload; returning it to zero reactivates the shield lattice.
        // Destroying the clamshell opens its doors but must not drop the
        // lattice: only the deposited cell overloads the core.
        const bool lattice=overloaded || (target.armed && !target.destroyed && phase==VentCycle::Phase::cooldown);
        if(i==2 && !request(asset(kCore,23,9),true,lattice?1.F:0.F)) {return false;}
    }
    if(overloaded) {
        for(const auto slot:std::initializer_list<std::uint16_t>{0,2,3}) {if(!request(asset(kCore,23,slot),true,1.F)) {return false;}}
        const auto& ship=frame_.native[asset_index(asset(kCore,4,16))];
        const auto& device=frame_.native[asset_index(asset(kCore,23,10))];
        // Creation and the initial snap must reach the client before movement.
        // Keep the animated command stable after it starts, including delayed
        // device feedback. Reverting to zero would undo the native approach.
        if(ship.acknowledged && !request(asset(kCore,23,10),true,
            device.managed && (device.position!=0.F || device.acknowledged)?escape_ship::kApproach:0.F)) {return false;}
    }
    return true;
}
void Controller::hazards() noexcept {
    const bool gameplay=frame_.cinematic.phase==cinematics::Phase::gameplay && !frame_.recovery.holding();
    const bool deck=gameplay && frame_.section>=static_cast<std::uint8_t>(Section::tunnel);
    auto mode=hazards::Mode::off;
    if(gameplay && frame_.interactions[7].destroyed) {mode=frame_.interactions[4].inserted?hazards::Mode::escape:hazards::Mode::climb;}
    for(std::size_t i=0;i<std::size(kVolumes);++i) {
        if(kVolumes[i].asset==volume(0xAD062E98,17) && seen_[i]) {mode=hazards::Mode::off;}
    }
    auto& state=frame_.hazards;
    const bool burn=deck && sunExposed_;
    if(state.burn!=burn) {state.burn=burn;state.burnRevision=state.burnRevision?state.burnRevision+1:frame_.spawnGeneration;++frame_.revision;}
    const auto& tunnelDoor=frame_.native[asset_index(asset(kProcessing,23,4))];
    const bool grinder=gameplay && frame_.section==static_cast<std::uint8_t>(Section::grinder)
        && frame_.restricted && tunnelDoor.position==0.F && tunnelDoor.acknowledged && !cleared(Cohort::grinderBoss);
    if(state.grinder!=grinder) {state.grinder=grinder;state.grinderRevision=state.grinderRevision?state.grinderRevision+1:frame_.spawnGeneration;++frame_.revision;}

    if(state.deck!=deck) {state.deck=deck;state.deckRevision=state.deckRevision?state.deckRevision+1:frame_.spawnGeneration;++frame_.revision;}
    if(state.core!=mode) {state.core=mode;state.coreRevision=state.coreRevision?state.coreRevision+1:frame_.spawnGeneration;++frame_.revision;}
    for(auto revision:{state.burnRevision,state.deckRevision,state.coreRevision,state.grinderRevision}) {
        if(revision && !lifecycle_.reserve_through(owner(),revision+1)) {frame_.fault=true;}
    }
    if(frame_.section>=static_cast<std::uint8_t>(Section::access)) {static_cast<void>(request(asset(kCore,4,17),false));}
}
bool Controller::submitted(std::uint64_t run,std::uint32_t bank,std::uint8_t row,std::uint32_t generation,std::uint64_t now) noexcept {
    if(run!=run_ || !frame_.enabled || !dialogue_.submitted(kDialoguePolicy,bank,row,generation,now,frame_,frame_.revision)) {return false;}
    submitted_.set(row);voiceEnd_[row]=dialogue_.voice_until();return true;
}
coo::MarkerTarget Controller::marker(std::uint32_t event) const noexcept {
    // Native object references follow their live placement; no world-coordinate
    // marker overrides or guessed positions are serialized.
    if(event==kObjectives[1]) {return navigation("powerhouse_directive_landing_mercury_clear_nav_point");}
    if(event==kObjectives[2]) {return navigation("powerhouse_directive_landing_mercury_interact_nav_point");}
    if(event==kObjectives[5]) {return navigation("link_directive_tumbler_obstruction_nav_point");}
    if(event==kObjectives[7]) {return navigation(frame_.interactions[1].held || frame_.interactions[1].inserted
        ?"link_directive_control_interact_nav_point":"link_carry_object_nav_point");}
    if(event==kObjectives[20] || event==kObjectives[21]) {
        return navigation(!frame_.interactions[5].destroyed?"reactor_clamshell_target_e_nav_point":"reactor_clamshell_target_w_nav_point");
    }
    if(event==kObjectives[22]) {return navigation("apex_directive_reactor_coffin_target_nav_point");}
    if(event==kObjectives[24]) {return navigation(frame_.interactions[3].held || frame_.interactions[3].inserted
        ?"ember_directive_reactor_mother_brain_delivery_nav_point":"ember_directive_reactor_mother_brain_carry_object_look_trigger");}
    if(event==kObjectives[25]) {return navigation("apex_directive_reactor_rails_escape_nav_point");}
    if(event==kObjectives[6]) {return navigation("link_carry_object_nav_point");}
    if(event==kObjectives[18]) {return navigation("apex_directive_security_door_nav_point");}
    if(event==kObjectives[0]) {return navigation("powerhouse_directive_landing_mercury_clear_nav_point");}
    if(event==kObjectives[3]) {return navigation("powerhouse_directive_byway_goto_nav_point");}
    if(event==kObjectives[4]) {return navigation("link_directive_link_goto_bubble_nav_point");}
    if(event==kObjectives[8]) {return navigation("link_directive_processing_defend_nav_point");}
    if(event==kObjectives[9]) {return navigation("cinder_directive_cinder_goto_bubble_nav_point");}
    if(event==kObjectives[10]) {return navigation("cinder_directive_sunburn_goto_nav_point");}
    if(event==kObjectives[11]) {return navigation("cinder_directive_sunburn_goto_nav_point");}
    if(event==kObjectives[12]) {return navigation("cinder_directive_ready_room_02_goto_nav_point");}
    if(event==kObjectives[13]) {return navigation("cinder_directive_ready_room_02_goto_nav_point");}
    if(event==kObjectives[14]) {return navigation("cinder_directive_meat_grinder_goto_nav_point");}
    if(event==kObjectives[15]) {return navigation("foundry_hatch_door_center_nav_point");}
    if(event==kObjectives[16]) {return navigation("cinder_directive_chute_goto_nav_point");}
    if(event==kObjectives[17]) {return navigation("apex_directive_security_goto_nav_point");}
    if(event==kObjectives[19]) {return navigation("apex_directive_reactor_goto_nav_point");}
    if(event==kObjectives[23]) {return navigation("ember_directive_reactor_mother_brain_carry_object_look_trigger");}
    return {};
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
    case coo::Operation::scene:
        for(const auto& scene:kScenes) {if(scene.asset==s.asset) {
            for(const auto& cast:scene.cast) {if(cast.type==1 && !request(asset(cast.registry,1,cast.slot),true)) {return false;}}
            return request(s.asset,true);
        }}return false;
    case coo::Operation::device:return request(s.asset,s.asset.type==23 || s.argument!=0,s.asset.type==23?std::bit_cast<float>(s.argument):0.F);
    case coo::Operation::mechanic:
        if(s.asset==kModule) {
            if(s.argument==static_cast<std::uint32_t>(Mechanic::finishSection)) {phaseFinished_=true;return true;}
            if(s.argument==static_cast<std::uint32_t>(Mechanic::restrict) || s.argument==static_cast<std::uint32_t>(Mechanic::allow)) {frame_.restricted=s.argument==1;++frame_.revision;return true;}
        }
        if(s.argument==static_cast<std::uint32_t>(Mechanic::arm)) {
            const auto i=interaction_index(s.asset);if(i==std::size(kInteractionAssets)) {return false;}frame_.interactions[i].armed=true;++frame_.revision;return request(s.asset,true);
        }
        return false;
    case coo::Operation::observation:return true;
    case coo::Operation::complete:
        if(frame_.section!=static_cast<std::uint8_t>(Section::escape) || !frame_.interactions[4].inserted
            || !cinematics_.finish_gameplay(owner(),now_)) {return false;}
        frame_.cinematic=cinematics_.state();frame_.restricted=false;objectives_.clear();hazards();++frame_.revision;return true;
    default:return false;
    }
}
bool Controller::observed(const coo::CommandSpec& s) const noexcept {
    const auto visited=[&](coo::Asset a) {for(std::size_t n=0;n<std::size(kVolumes);++n) {if(kVolumes[n].asset==a) {return seen_[n];}}return false;};
    if(s.asset==kModule) {
        if(s.argument>=0x100 && s.argument<0x100+std::size(kCohorts)) {return cleared(static_cast<Cohort>(s.argument-0x100));}
        if(s.argument>=0x201 && s.argument<=0x203) {std::uint32_t n{};for(std::size_t i=5;i<=7;++i) {n+=frame_.interactions[i].destroyed?1U:0U;}return n>=s.argument-0x200;}
        switch(static_cast<Milestone>(s.argument)) {
        case Milestone::readyOneReinforce:return cleared(Cohort::readyOne) || visited(volume(kTunnel,59)) || visited(volume(kTunnel,60));
        case Milestone::readyTwoReinforce:return cleared(Cohort::readyTwo) || visited(volume(kReady,54)) || visited(volume(kReady,55));
        case Milestone::foundryMid:return cleared(Cohort::foundryEntry) || visited(volume(kFoundry,118)) || visited(volume(kFoundry,119));
        case Milestone::foundryFinal:return cleared(Cohort::foundryMid) || visited(volume(kFoundry,119));
        case Milestone::meatReinforce:return cleared(Cohort::meatGrinder) || visited(volume(kAscent,92)) || visited(volume(kAscent,93));
        default:break;
        }
        if(s.argument==static_cast<std::uint32_t>(Milestone::lightsEndArrival)
            || s.argument==static_cast<std::uint32_t>(Milestone::processingDiscovery)) {
            const bool processing=s.argument==static_cast<std::uint32_t>(Milestone::processingDiscovery);
            if(processing && cleared(Cohort::processingEntry)) {return true;}
            const auto areas=processing?std::span<const coo::Asset>(kProcessingDiscoveryAreas):std::span<const coo::Asset>(kLightsEndArrivalAreas);
            for(std::size_t n=0;n<std::size(kVolumes);++n) {
                if(!seen_[n]) {continue;}
                for(const auto area:areas) {if(kVolumes[n].asset==area) {return true;}}
            }
            return false;
        }
    }
    if(s.asset.registry==kProcessing && s.asset.type==23 && s.asset.slot<refineryPoses_.size()) {
        const auto index=asset_index(s.asset);if(index==std::size(kAssets)) {return false;}
        const auto& state=frame_.native[index];
        return state.managed && state.acknowledged && state.position==std::bit_cast<float>(s.argument);
    }
    if(s.asset==kDialogueAsset) {return s.argument<std::size(kDialogue) && submitted_[s.argument] && now_>=voiceEnd_[s.argument];}
    const auto i=interaction_index(s.asset);
    if(i<std::size(kInteractionAssets)) {
        const auto& v=frame_.interactions[i];switch(static_cast<Event>(s.argument)) {
        case Event::started:return v.started;case Event::completed:return v.completed;case Event::pickedUp:return v.started;
        case Event::inserted:return v.inserted;case Event::destroyed:return v.destroyed;}
    }
    for(std::size_t n=0;n<std::size(kVolumes);++n) {if(kVolumes[n].asset==s.asset) {return seen_[n];}}return false;
}
coo::StallDetail Controller::missing(const coo::CommandSpec& s) const noexcept {
    if(s.wait==coo::Wait::requested || observed(s)) {return {};}
    if(s.asset==kModule && s.argument>=0x100 && s.argument<0x100+std::size(kCohorts)) {
        for(const auto m:kCohorts[s.argument-0x100].members) {const auto a=asset(m.registry,1,m.slot);const auto i=spawn_index(a);auto d=population_.missing(i,kSpawns[i].count,true);if(d.missing!=coo::Missing::none) {d.asset=a;return d;}}
    }
    if(s.asset==kDialogueAsset) {return {coo::Missing::dialogue,s.asset,s.argument};}
    const auto i=interaction_index(s.asset);
    if(i>0 && i<std::size(kInteractionAssets) && !frame_.interactions[i].binding.valid()) {return {coo::Missing::controller,s.asset};}
    return {coo::Missing::observation,s.asset,1,0,s.argument};
}
bool Controller::advance(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    if(!run_ || run!=run_ || !ready) {return false;}
    frame_.bubble=frame_.section>=static_cast<std::uint8_t>(Section::access)?0:frame_.section>=static_cast<std::uint8_t>(Section::tunnel)?5:frame_.section>=static_cast<std::uint8_t>(Section::processing)?7:8;
    now_=now;frame_.enabled=!frame_.fault;frame_.gameplayClockTicks=clock_.sample(now);
    frame_.wipeRemaining=frame_.recovery.phase==recovery::Phase::countdown && now<frame_.recovery.deadline?frame_.recovery.deadline-now:0;
    if(frame_.recovery.phase==recovery::Phase::countdown && now>=frame_.recovery.deadline) {frame_.recovery.phase=recovery::Phase::requested;++frame_.revision;}
    if(frame_.section==static_cast<std::uint8_t>(Section::escape) && frame_.cinematic.phase==cinematics::Phase::gameplay && !frame_.recovery.active()) {
        if(!frame_.escapeClock && !frame_.escapeStopped) {frame_.escapeClock=true;frame_.escapeStart=now;}
        if(frame_.escapeClock) {
            frame_.escapeElapsed=now>=frame_.escapeStart?now-frame_.escapeStart:0;
            if(frame_.escapeElapsed>=60000) {
                frame_.recovery={};frame_.recovery.phase=recovery::Phase::requested;frame_.recovery.point=recovery::checkpoint(frame_.section);frame_.restricted=true;++frame_.revision;
            }
        }
    }
    if(frame_.recovery.holding()) {objectives_.clear();hazards();frame_.presentation=objectives_.state();return true;}

    cinematics_.advance(now);frame_.cinematic=cinematics_.state();
    if(frame_.cinematic.phase==cinematics::Phase::complete && !frame_.finished) {
        frame_.finished=lifecycle_.complete(owner());frame_.completion=lifecycle_.publication();++frame_.revision;
    }
    if(frame_.cinematic.phase==cinematics::Phase::failed) {frame_.fault=true;}
    if(frame_.cinematic.phase!=cinematics::Phase::gameplay || !arrived_) {hazards();frame_.presentation=objectives_.state();return true;}
    if(!started_) {started_=executor_.start(graph().definition,run_);}if(!started_) {frame_.fault=true;return false;}
    executor_.update(*this);
    for(std::size_t i=0;i<graph().definition.steps.size();++i) {
        const auto state=executor_.step_state(i);if(state.phase!=coo::StepPhase::active) {continue;}
        const auto& step=graph().definition.steps[i];
        for(std::size_t j=0;j<step.commands.size();++j) {if(state.commands[j].requested && coo::is_observation(step.commands[j].operation) && observed(step.commands[j])) {static_cast<void>(executor_.enqueue({executor_.token(i,j),coo::Milestone::observed}));}}
    }
    executor_.update(*this);executor_.update(*this);
    if(!vents() || !transport() || !landing_props()) {frame_.fault=true;}hazards();
    if(objectives_.state().active) {objectives_.marker(marker(objectives_.state().event));}
    dialogue_.advance(kDialoguePolicy,frame_.spawnGeneration-1U,now_,false,frame_,frame_.revision);
    if(phaseFinished_ && executor_.diagnostics().phase!=coo::Phase::failed && std::size_t(frame_.section)+1<mission().phases.size()) {
        phaseFinished_=false;executor_.cancel(*this);++frame_.section;started_=executor_.start(graph().definition,run_);++frame_.revision;
    }
    frame_.enabled=!frame_.fault && executor_.diagnostics().phase!=coo::Phase::failed;
    frame_.presentation=objectives_.state();frame_.completion=lifecycle_.publication();return true;
}
}
