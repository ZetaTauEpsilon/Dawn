#include "controller.h"
#include <cmath>
#include <bit>
#include <algorithm>
namespace dawn::state::activity::vanilla::homecoming {
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
    executor_.cancel(*this);lifecycle_.reset();clock_.reset();run_=now_=0;arrived_=started_=phaseFinished_=false;
    objectives_={};dialogue_={};objects_={};population_={};seen_.reset();inside_.reset();hasPosition_=false;previousPosition_={};
    submitted_.reset();voiceEnd_={};clearedSince_={};requestedAt_={};cinematics_={};frame_={};
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
bool Controller::mounted(const MountedPosition& sample) noexcept {
    if(sample.owner!=owner() || !frame_.enabled || frame_.finished
        || frame_.cinematic.phase!=cinematics::Phase::gameplay
        || sample.player==UINT32_MAX || sample.vehicle==UINT32_MAX
        || sample.player==sample.vehicle || !std::isfinite(sample.position.x)
        || !std::isfinite(sample.position.y) || !std::isfinite(sample.position.z)) {return false;}
    position(sample.owner.run,sample.position);return true;
}
void Controller::position(std::uint64_t run,Point p) noexcept {
    if(!run_ || run!=run_ || frame_.finished || !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {return;}
    inside_.reset();
    for(std::size_t i=0;i<std::size(kVolumes);++i) {if(contains(kVolumes[i],p)) {inside_.set(i);}}
    if(arrived_) {
        seen_|=inside_;
        const double dx=double(p.x)-previousPosition_.x,dy=double(p.y)-previousPosition_.y,dz=double(p.z)-previousPosition_.z;
        // Loading/cinematic discontinuities are not a traversed route.
        if(hasPosition_ && dx*dx+dy*dy+dz*dz<=10000.) {
            for(std::size_t i=0;i<std::size(kVolumes);++i) {if(!seen_[i] && crosses(kVolumes[i],previousPosition_,p)) {seen_.set(i);}}
        }
    }
    previousPosition_=p;hasPosition_=true;
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
            // The native object retires the created entity on a NEW generation.
            const auto next=objects_.state(o).generation+1;
            if(!lifecycle_.reserve_through(owner(),next) || !objects_.rearm(owner(),o,next)) {return false;}
            objects_.retire(o);s.observed=false;
        }
        const auto state=objects_.state(o);
        if(active && state.phase==coo::ObjectPhase::retired) {return false;}
        s.generation=state.generation;s.prepared=state.phase>=coo::ObjectPhase::create;s.active=active && state.create;
    } else if(a.type==2) {s.active=active;s.bound=active;s.retired=false;}
    else {s.active=active;}
    if(a.type==1 && active) {const auto n=spawn_index(a);if(n==kSpawns.size()) {return false;}population_.enable(n);}
    if(a.type==65) {frame_.consoleArmed=active;if(!active) {frame_.consoleStarted=false;}}
    ++frame_.revision;return true;
}
bool Controller::device(coo::Generation gen,coo::Asset a,const middleware::bap::activity_message::device_sense::Output& d) noexcept {
    const auto i=asset_index(a);
    if(gen!=owner() || !frame_.enabled || frame_.finished || a.type!=23 || i==std::size(kAssets)) {return false;}
    for(unsigned n=0;n<3;++n) {
        if((d.present&(1U<<(n*2))) && !std::isfinite(d.values[n])) {return false;}
        if((d.present&(2U<<(n*2))) && (d.revisions[n]<0 || d.revisions[n]>=32766)) {return false;}
    }
    auto& s=frame_.native[i];
    for(unsigned n=0;n<3;++n) {
        if(d.present&(2U<<(n*2))) {s.deviceVersions[n]=(std::max)(s.deviceVersions[n],d.revisions[n]);s.deviceSeen|=static_cast<std::uint8_t>(1U<<n);}
    }
    bool changed=false;
    // The measured pose is the interpolated native position, never the commanded target.
    if(d.present&2U) {if(d.revisions[0]>=s.observedRevision) {s.observedRevision=d.revisions[0];}}
    if(d.present&1U) {s.observedPosition=d.values[0];s.poseKnown=true;}
    // Generator performer 80C23AA6 couples turbine destruction to position 0.4 (v32).
    for(std::size_t g=0;g<3;++g) {
        if(a==asset(kShip,23,static_cast<std::uint16_t>(143+2*g)) && s.managed && s.poseKnown
            && s.observedPosition>=.3999F && s.observedPosition<=.4001F && !frame_.generatorDown[g]) {frame_.generatorDown[g]=true;changed=true;}
    }
    if(s.managed && s.deviceSynchronized && !s.acknowledged && s.poseKnown && s.observedRevision>=0
        && static_cast<std::uint32_t>(s.observedRevision)==s.generation
        && (std::abs(s.observedPosition-s.position)<.002F || (s.snap && !(d.present&1U)))) {s.acknowledged=true;changed=true;}
    if(changed) {++frame_.revision;}
    if(!s.managed || s.deviceSynchronized || s.deviceSeen!=7) {return changed;}
    // 106AD60 accepts only a strictly newer version than the entity controller.
    // Bootstrap against its counters once; feedback during animation must never
    // restart that animation or manufacture a completed movement.
    const auto next=(std::max)(s.generation,static_cast<std::uint32_t>(*std::max_element(s.deviceVersions.begin(),s.deviceVersions.end()))+1U);
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
    if(gen!=owner() || !frame_.enabled || frame_.fault || frame_.finished
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
    for(const auto m:kCohorts[i].members) {const auto n=spawn_index(asset(m.registry,1,m.slot));if(n==kSpawns.size() || !population_.cleared(n,expected(kSpawns[n]))) {return false;}}
    return true;
}
// A clear must hold for the settle window before it counts (audit constraint).
bool Controller::settled(Cohort id) const noexcept {
    const auto i=static_cast<std::size_t>(id);
    return i<clearedSince_.size() && clearedSince_[i] && now_>=clearedSince_[i]+kSettleMs;
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
    const bool changed=objects_.observe(o,r,true,0.F,1);
    if(objects_.owner(o)!=r) {return false;}
    if(changed || !frame_.native[i].observed) {frame_.native[i].acknowledged=true;frame_.native[i].observed=true;++frame_.revision;return true;}
    return changed;
}
bool Controller::use(coo::Generation gen,coo::Asset a,const middleware::bap::activity_message::object_sense::Output& d) noexcept {
    const auto n=asset_index(a);
    if(gen!=owner() || !frame_.enabled || frame_.finished
        || frame_.cinematic.phase!=cinematics::Phase::gameplay || n==std::size(kAssets)
        || !use_subscription(a) || !d.hasUse || !d.used || !d.alive || !d.present) {return false;}
    const auto& source=frame_.native[n];
    if(!source.managed || !source.prepared || !source.active || d.generation<=0
        || static_cast<std::uint32_t>(d.generation)!=source.generation) {return false;}
    if(!frame_.reviveArmed || frame_.reviveUsed) {return false;}
    // Native 80804FB2 has already checked the interaction and accepted use.
    frame_.reviveUsed=true;++frame_.revision;return true;
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
        const auto generation=source.generation+1;
        if(!lifecycle_.reserve_through(owner(),generation) || !population_.restart(i)) {frame_.fault=true;return false;}
        source.generation=generation;source.sourceOwner=r.owner;++frame_.revision;return false;
    }
    struct Expected {std::uint16_t source;std::uint32_t registry;std::uint8_t count;bool required;};
    // Admission is bounded by the expected actors per source, including request overrides.
    std::array<Expected,kSpawns.size()> catalog{};
    for(std::size_t k=0;k<kSpawns.size();++k) {catalog[k]={kSpawns[k].source,kSpawns[k].registry,expected(kSpawns[k]),kSpawns[k].required};}
    const auto result=population_.admit(catalog,r,run_,source.generation);
    if(result==coo::Admission::accepted) {source.sourceOwner=r.owner;}
    if(result==coo::Admission::overflow) {frame_.fault=true;}return result==coo::Admission::accepted;
}
bool Controller::died(const EnemyReceipt& r) noexcept {
    const auto a=asset(r.registry,1,r.source);const auto i=spawn_index(a),n=asset_index(a);
    if(!frame_.enabled || frame_.fault || frame_.finished || i==kSpawns.size() || n==std::size(kAssets)) {return false;}
    auto& source=frame_.native[n];
    if(r.owner!=source.sourceOwner || !population_.died(r,run_,source.generation)) {return false;}
    if(population_.cleared(i,expected(kSpawns[i]))) {source.sourceCleared=true;++frame_.revision;}
    return true;
}
bool Controller::ghost(coo::Generation gen,coo::Asset a,const middleware::bap::activity_message::ghost_sense::Output& d) noexcept {
    if(gen!=owner() || a!=kConsoleLink || !frame_.enabled || frame_.finished
        || !frame_.consoleArmed || frame_.consoleScanned
        || d.generation!=static_cast<std::int32_t>(frame_.spawnGeneration+1U)
        || !std::isfinite(d.progress) || d.progress<0.F) {return false;}
    // Positive progress, then inactive in the same generation, is the only scan receipt.
    const bool started=d.active || d.progress>0.F,completed=frame_.consoleStarted && !d.active && d.progress>=1.F;
    if((!started || frame_.consoleStarted) && !completed) {return false;}
    frame_.consoleStarted|=started;frame_.consoleScanned=completed;++frame_.revision;return true;
}
bool Controller::submitted(std::uint64_t run,std::uint32_t bank,std::uint8_t row,std::uint32_t generation,std::uint64_t now) noexcept {
    if(run!=run_ || !frame_.enabled || !dialogue_.submitted(kDialoguePolicy,bank,row,generation,now,frame_,frame_.revision)) {return false;}
    submitted_.set(row);voiceEnd_[row]=dialogue_.voice_until();return true;
}
bool Controller::door_handled() noexcept {
    if(!frame_.enabled || !frame_.doorReleased || frame_.doorHandled) {return false;}
    frame_.doorHandled=true;++frame_.revision;return true;
}
coo::MarkerTarget Controller::marker(std::uint32_t event) const noexcept {
    // Native point references follow their live placement; no world-coordinate markers.
    const auto navigation=[](std::string_view name) {
        for(const auto& p:kNavigation) {if(p.name==name) {return coo::MarkerTarget{p.asset,{0x811C9DC5U,0,0,0}};}}return coo::MarkerTarget{};
    };
    if(event==kObjectives[0]) {return navigation("ap_ikora");}
    if(event==kObjectives[3]) {return navigation("ap_weapon");}
    if(event==kObjectives[2]) {return navigation("ap_goto_military");}
    if(event==kObjectives[4]) {return navigation("ap_goto_plaza");}
    if(event==kObjectives[5]) {return navigation("ap_plaza");}
    if(event==kObjectives[9]) {return navigation("plaza_exit");}
    if(event==kObjectives[8]) {return navigation("speaker");}
    if(event==kObjectives[10]) {return navigation("board");}
    if(event==kObjectives[13]) {return navigation("ap_locate");}
    if(event==kObjectives[14]) {return navigation("ap_destroy_battleship");}
    if(event==kObjectives[11]) {return navigation("generator");}
    if(event==kObjectives[12]) {return navigation("escape");}
    return {};
}
bool Controller::scene_request(coo::Asset a,std::uint32_t input) noexcept {
    const auto i=scene_index(a);if(i==std::size(kScenes)) {return false;}
    const auto& scene=kScenes[i];auto& state=frame_.scenes[i];
    if(!input) {
        // The native Scene owns its actors: request every cast source and object, then the
        // scene itself on a fresh generation with an empty input list.
        for(const auto& cast:scene.cast) {
            if((cast.type==1 || cast.type==4) && !request(asset(cast.registry,cast.type,cast.slot),true)) {return false;}
        }
        state={};return request(a,true);
    }
    const auto& native=frame_.native[asset_index(a)];
    if(!native.managed || !native.active) {return false;}
    bool authored=false;for(const auto key:scene.inputs) {authored|=key==input;}
    if(!authored) {return false;}
    for(std::size_t n=0;n<state.count;++n) {if(state.events[n]==input) {return true;}}
    if(state.count>=state.events.size()) {return false;}
    state.events[state.count++]=input;++frame_.revision;return true;
}
bool Controller::publish(const coo::Command& c) noexcept {
    const auto& g=graph().definition;
    if(c.token.run!=run_ || c.token.step>=g.steps.size() || c.token.command>=g.steps[c.token.step].commands.size()
        || c.token!=executor_.token(c.token.step,c.token.command) || c.schema!=g.schema) {return false;}
    requestedAt_[c.token.step][c.token.command]=now_;
    const auto& s=c.spec;
    switch(s.operation) {
    case coo::Operation::objective:dialogue_.objective(kDialoguePolicy,s.argument,frame_,frame_.revision);objectives_.set(s.argument,marker(s.argument));return true;
    case coo::Operation::dialogue:dialogue_.enqueue(kDialoguePolicy,static_cast<std::uint8_t>(dialogue_row(s.argument)),now_,dialogue_delay(s.argument),frame_.section,frame_.revision);return true;
    case coo::Operation::population:return s.asset==kModule && cohort(static_cast<Cohort>(s.argument));
    case coo::Operation::scene:return scene_request(s.asset,s.argument);
    case coo::Operation::device:
        if(s.asset.type==23) {return request(s.asset,true,std::bit_cast<float>(s.argument));}
        if(s.asset.type==4) {return request(s.asset,s.argument!=0);}
        if(s.asset.type==65) {return request(s.asset,s.argument!=0);}
        return request(s.asset,true);
    case coo::Operation::mechanic:
        if(s.asset==kModule) {
            switch(static_cast<Mechanic>(s.argument)) {
            case Mechanic::finishSection:phaseFinished_=true;return true;
            case Mechanic::restrict:case Mechanic::allow:frame_.restricted=s.argument==static_cast<std::uint32_t>(Mechanic::restrict);++frame_.revision;return true;
            case Mechanic::pickup:
                if(frame_.section!=static_cast<std::uint8_t>(Section::boulevard) || !cinematics_.begin_pickup(owner(),now_)) {return false;}
                frame_.cinematic=cinematics_.state();objectives_.clear();++frame_.revision;return true;
            case Mechanic::releaseDoor:frame_.doorReleased=true;++frame_.revision;return true;
            default:return false;
            }
        }
        switch(static_cast<Mechanic>(s.argument)) {
        case Mechanic::arm:
            if(s.asset!=kReviveInteract) {return false;}
            frame_.reviveArmed=true;frame_.reviveUsed=false;++frame_.revision;return request(s.asset,true);
        case Mechanic::disarm:
            if(s.asset!=kReviveInteract) {return false;}
            frame_.reviveArmed=false;++frame_.revision;return request(s.asset,false);
        case Mechanic::stopScene:return s.asset.type==43 && scene_index(s.asset)<std::size(kScenes) && request(s.asset,false);
        case Mechanic::retireMember: {
            const auto i=asset_index(s.asset);if(s.asset.type!=2 || i==std::size(kAssets)) {return false;}
            auto& n=frame_.native[i];if(!n.managed) {return false;}
            if(n.generation>=32766 || !lifecycle_.reserve_through(owner(),n.generation+1)) {return false;}
            ++n.generation;n.bound=false;n.retired=true;n.active=false;n.desired=false;++frame_.revision;return true;
        }
        case Mechanic::powerOff:return s.asset.type==23 && request(s.asset,true,0.F,0.F);
        default:return false;
        }
    case coo::Operation::observation:case coo::Operation::eventAfter:return true;
    case coo::Operation::complete:
        if(frame_.section!=static_cast<std::uint8_t>(Section::escape) || !cinematics_.finish_gameplay(owner(),now_)) {return false;}
        frame_.cinematic=cinematics_.state();frame_.restricted=false;objectives_.clear();
        if(frame_.native[asset_index(kConsoleLink)].managed) {static_cast<void>(request(kConsoleLink,false));}
        ++frame_.revision;return true;
    default:return false;
    }
}
bool Controller::visited(coo::Asset a) const noexcept {
    for(std::size_t n=0;n<std::size(kVolumes);++n) {if(kVolumes[n].asset==a) {return seen_[n];}}return false;
}
bool Controller::observed(const coo::CommandSpec& s) const noexcept {
    if(s.operation==coo::Operation::eventAfter) {
        // Timers run from the step's activation; the executor stamps that time at publish.
        for(std::size_t i=0;i<graph().definition.steps.size();++i) {
            const auto& step=graph().definition.steps[i];
            for(std::size_t j=0;j<step.commands.size();++j) {
                if(&step.commands[j]==&s) {return requestedAt_[i][j] && now_>=requestedAt_[i][j]+s.argument;}
            }
        }
        return false;
    }
    if(s.asset==kModule) {
        if(s.argument>=0x100 && s.argument<0x100+std::size(kCohorts)) {return settled(static_cast<Cohort>(s.argument-0x100));}
        switch(static_cast<Milestone>(s.argument)) {
        case Milestone::caydeNear:return visited(trigger_area(kUnderwatch,"pt_shaxx_enters")) || visited(trigger_area(kUnderwatch,"pt_shaxx_enters_backup"));
        case Milestone::shaxxNear:return visited(trigger_area(kUnderwatch,"pt_start_shaxx_scene")) || visited(trigger_area(kUnderwatch,"pt_player_near_shaxx"));
        case Milestone::consoleScanned:return frame_.consoleScanned;
        case Milestone::generatorA:return frame_.generatorDown[0];
        case Milestone::generatorB:return frame_.generatorDown[1];
        case Milestone::generatorC:return frame_.generatorDown[2];
        default:return false;
        }
    }
    if(s.asset==kDialogueAsset) {return s.argument<std::size(kDialogue) && submitted_[s.argument] && now_>=voiceEnd_[s.argument];}
    if(s.asset.type==23) {
        const auto index=asset_index(s.asset);if(index==std::size(kAssets)) {return false;}
        const auto& state=frame_.native[index];
        return state.managed && state.acknowledged && state.position==std::bit_cast<float>(s.argument);
    }
    if(s.asset.type==4) {
        const auto index=asset_index(s.asset);if(index==std::size(kAssets)) {return false;}
        if(s.argument==static_cast<std::uint32_t>(ObjectEvent::used)) {return s.asset==kReviveInteract && frame_.reviveUsed;}
        return frame_.native[index].active && frame_.native[index].observed;
    }
    return visited(s.asset);
}
coo::StallDetail Controller::missing(const coo::CommandSpec& s) const noexcept {
    if(s.wait==coo::Wait::requested || observed(s)) {return {};}
    if(s.asset==kModule && s.argument>=0x100 && s.argument<0x100+std::size(kCohorts)) {
        for(const auto m:kCohorts[s.argument-0x100].members) {const auto a=asset(m.registry,1,m.slot);const auto i=spawn_index(a);auto d=population_.missing(i,expected(kSpawns[i]),true);if(d.missing!=coo::Missing::none) {d.asset=a;return d;}}
        return {coo::Missing::timer,s.asset,static_cast<std::uint32_t>(kSettleMs)};
    }
    if(s.operation==coo::Operation::eventAfter) {return {coo::Missing::timer,s.asset,s.argument};}
    if(s.asset==kDialogueAsset) {return {coo::Missing::dialogue,s.asset,s.argument};}
    if(s.asset.type==23) {return {coo::Missing::device,s.asset,s.argument};}
    if(s.asset.type==4) {return {coo::Missing::object,s.asset,s.argument};}
    if(s.asset==kModule && s.argument==static_cast<std::uint32_t>(Milestone::consoleScanned)) {return {coo::Missing::controller,kConsoleLink};}
    return {coo::Missing::observation,s.asset,1,0,s.argument};
}
bool Controller::advance(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    if(!run_ || run!=run_ || !ready) {return false;}
    frame_.bubble=bubble_of(static_cast<Section>(frame_.section));
    now_=now;frame_.enabled=!frame_.fault;frame_.gameplayClockTicks=clock_.sample(now);
    cinematics_.advance(now);frame_.cinematic=cinematics_.state();
    if(frame_.cinematic.phase==cinematics::Phase::complete && !frame_.finished) {
        frame_.finished=lifecycle_.complete(owner());frame_.completion=lifecycle_.publication();++frame_.revision;
    }
    if(frame_.cinematic.phase==cinematics::Phase::failed) {frame_.fault=true;}
    if(frame_.cinematic.phase!=cinematics::Phase::gameplay) {frame_.presentation=objectives_.state();return true;}
    arrived_=true;
    if(!started_) {started_=executor_.start(graph().definition,run_);}if(!started_) {frame_.fault=true;return false;}
    for(std::size_t i=0;i<clearedSince_.size();++i) {
        if(cleared(static_cast<Cohort>(i))) {if(!clearedSince_[i]) {clearedSince_[i]=now;}} else {clearedSince_[i]=0;}
    }
    executor_.update(*this);
    for(std::size_t i=0;i<graph().definition.steps.size();++i) {
        const auto state=executor_.step_state(i);if(state.phase!=coo::StepPhase::active) {continue;}
        const auto& step=graph().definition.steps[i];
        for(std::size_t j=0;j<step.commands.size();++j) {if(state.commands[j].requested && coo::is_observation(step.commands[j].operation) && observed(step.commands[j])) {static_cast<void>(executor_.enqueue({executor_.token(i,j),coo::Milestone::observed}));}}
    }
    executor_.update(*this);executor_.update(*this);
    if(objectives_.state().active) {objectives_.marker(marker(objectives_.state().event));}
    dialogue_.advance(kDialoguePolicy,frame_.spawnGeneration-1U,now_,false,frame_,frame_.revision);
    if(phaseFinished_ && executor_.diagnostics().phase!=coo::Phase::failed && std::size_t(frame_.section)+1<mission().phases.size()) {
        phaseFinished_=false;executor_.cancel(*this);++frame_.section;requestedAt_={};started_=executor_.start(graph().definition,run_);++frame_.revision;
    }
    frame_.enabled=!frame_.fault && executor_.diagnostics().phase!=coo::Phase::failed;
    frame_.presentation=objectives_.state();frame_.completion=lifecycle_.publication();return true;
}
}
