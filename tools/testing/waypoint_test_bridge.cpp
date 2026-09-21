#include <cstddef>
#include "../../Dawn/src/client/hooks/bootflow/deep_storage_navigation_rules.h"
#include "../../Dawn/src/client/hooks/bootflow/beyond_infinity_navigation_rules.h"
#include "../../Dawn/src/client/hooks/bootflow/deadly_trial_presentation.h"
#include "../../Dawn/src/client/hooks/bootflow/forest_strike_waypoints.h"
#include "../../Dawn/src/client/hooks/bootflow/hijacked_presentation.h"
#include "../../Dawn/src/state/activity/coo/native_presentation_authority.h"
#include "../../Dawn/src/state/activity/Newlight/launchpad/navigation.h"
#include "../../Dawn/src/state/activity/gateway/bindings.h"
#include "../../Dawn/src/middleware/encoding/bit_writer.h"
namespace hook=dawn::client::hooks::bootflow;
namespace activity=dawn::state::activity;
extern "C" __declspec(dllexport) unsigned layout(unsigned id) {
    switch(id) {
    case 0:return sizeof(activity::beyond_infinity::Frame);
    case 1:return offsetof(activity::beyond_infinity::Frame,presentation);
    case 2:return sizeof(activity::deep_storage::Frame);
    case 3:return offsetof(activity::deep_storage::Frame,presentation);
    case 4:return offsetof(activity::deep_storage::Frame,plates);
    case 5:return offsetof(activity::deep_storage::Frame,lensExposed);
    case 6:return offsetof(activity::hijacked::Frame,presentation);
    case 7:return offsetof(activity::deadly_trial::Frame,presentation);
    }return 0;
}
extern "C" __declspec(dllexport) unsigned run_rule(unsigned kind,std::byte* bytes,unsigned size,unsigned context,void* state) {
    hook::mission_waypoints::Result r{};
    switch(kind) {
    case 0:r=hook::deep_storage_navigation::build({bytes,size},context,*static_cast<activity::coo::ObjectiveState*>(state));break;
    case 1:r=hook::deep_storage_navigation::build({bytes,size},context,*static_cast<activity::deep_storage::Frame*>(state));break;
    case 2:r=hook::beyond_infinity_navigation::build({bytes,size},context,*static_cast<activity::beyond_infinity::Frame*>(state));break;
    case 3:r=hook::deadly_trial_presentation::build({bytes,size},*static_cast<activity::deadly_trial::Frame*>(state));break;
    case 4:return hook::forest_strike_waypoints::duplicate({bytes,size})==1;
    case 5:return hook::forest_strike_waypoints::duplicate({bytes,size})==2;
    case 6:return hook::hijacked_presentation::visible_row({bytes,size},{static_cast<const std::byte*>(state),0x1488},*reinterpret_cast<activity::hijacked::Frame*>(bytes+0xB10));
    }return unsigned(r.handled)|(unsigned(r.publish)<<16);
}
extern "C" __declspec(dllexport) void canonical(unsigned kind,unsigned event,void* frame,void* marker) {
    activity::coo::MarkerTarget m{};
    if(kind==0)m=activity::deep_storage::marker(event);
    if(kind==1)m=activity::beyond_infinity::navigation::marker(event,*static_cast<activity::beyond_infinity::Frame*>(frame));
    if(kind==2)m=activity::deadly_trial::navigation::goal(event).target;
    if(kind==3)m=activity::hijacked::marker(event);
    if(kind==4)m=activity::newlight::launchpad::navigation::marker(event);
    if(kind==5 && event<std::size(activity::gateway::kMarkers))m=activity::gateway::kMarkers[event].target;
    std::memcpy(marker,&m,sizeof m);
}
extern "C" __declspec(dllexport) unsigned wire(void* output,unsigned size,void* state,void* audience,unsigned authored,int current,int target) {
    dawn::middleware::encoding::bits::Writer writer({static_cast<std::byte*>(output),size});
    const auto ok=activity::coo::native_presentation::waypoint_objective(writer,*static_cast<activity::coo::ObjectiveState*>(state),*static_cast<activity::coo::Asset*>(audience),authored!=0,current,target);
    return ok?static_cast<unsigned>(writer.bit_count()):0;
}
extern "C" __declspec(dllexport) unsigned merge_meshes(void* list) {
    return hook::mission_waypoint_native::merge_meshes(*static_cast<hook::mission_waypoint_native::MeshList*>(list),hook::mission_waypoint_native::trialMeshes);
}
extern "C" __declspec(dllexport) unsigned valid_mesh(unsigned index,void* descriptor,unsigned dn,void* resource,unsigned rn) {
    namespace n=hook::mission_waypoint_native;
    const auto& mesh=index<2?n::trialMeshes[index]:n::hijackedMeshes[0];
    return n::mesh_valid(mesh,{static_cast<std::byte*>(descriptor),dn},{static_cast<std::byte*>(resource),rn});
}
extern "C" __declspec(dllexport) unsigned lifecycle_checks() {
    namespace c=activity::coo;namespace d=c::waypoint_delivery;
    unsigned count{};
#define REQUIRE(x) do {if(!(x))return __LINE__<<16;++count;}while(false)
    d::Registry registry;const d::Ticket a{{10,2},0x80B2E706},b{{10,2},0x80F47420};
    c::ObjectiveState objective{};objective.event=0x12345678;objective.revision=1;objective.active=objective.published=true;
    REQUIRE(!registry.project(a,objective).published);
    REQUIRE(registry.lookup(10,a.definition)==a);
    REQUIRE(!registry.lookup(9,a.definition).valid());
    REQUIRE(!registry.observe(a,UINT32_MAX,0x10000,0x20000,true));
    REQUIRE(registry.observe(a,21,0x10000,0x20000,true));
    auto delivered=registry.project(a,objective);REQUIRE(delivered.published && delivered.revision==1);
    for(unsigned i=0;i<10;++i)REQUIRE(registry.project(a,objective).revision==1);
    REQUIRE(!registry.project(b,objective).published); // No cross-mission readiness.
    REQUIRE(!registry.project({{9,999},a.definition},objective).published);
    REQUIRE(!registry.project({{10,1},a.definition},objective).published);
    REQUIRE(registry.project(a,objective).published);
    REQUIRE(registry.observe(a,21,0x10000,0x20000,false));
    REQUIRE(!registry.project(a,objective).published);
    REQUIRE(registry.observe(a,21,0x10000,0x20000,true));
    REQUIRE(registry.project(a,objective).revision==2);
    ++objective.revision;REQUIRE(registry.project(a,objective).revision==3);
    REQUIRE(!registry.project({{10,3},a.definition},objective).published);
    REQUIRE(!registry.observe(a,21,0x10000,0x20000,true));
    REQUIRE(!registry.project({{11,1},a.definition},objective).published);
    REQUIRE(!registry.lookup(10,b.definition).valid());
    c::ObjectiveDelivery delivery;const c::Generation owner{22,1};
    delivery.select(owner);REQUIRE(delivery.observe(owner,1,0x10000,0x20000,true));
    delivered=delivery.project(owner,objective);REQUIRE(delivered.published && delivered.revision==1);
    REQUIRE(delivery.observe(owner,1,0x10000,0x30000,true));
    delivery.acknowledge(owner,objective.revision,0);
    REQUIRE(delivery.project(owner,objective).revision==1); // Visible banner suppresses replay.
    REQUIRE(delivery.observe(owner,1,0x10000,0x40000,true));
    delivery.acknowledge(owner,objective.revision,1);
    REQUIRE(delivery.project(owner,objective).revision==2); // Wrong row cannot acknowledge.
    ++objective.revision;delivery.acknowledge(owner,objective.revision,1);
    REQUIRE(delivery.project(owner,objective).revision==3); // New logical objective still publishes.
    REQUIRE(delivery.observe(owner,1,0x10000,0x50000,true));
    delivery.acknowledge({23,1},objective.revision,2);
    REQUIRE(delivery.project(owner,objective).revision==4); // Wrong owner cannot acknowledge.
#undef REQUIRE
    return count;
}
