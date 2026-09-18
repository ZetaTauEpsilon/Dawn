#include "client/player/player_position.h"
#include "client/hooks/teleport/local_player_identity.h"
#include "state/activity/gateway/controller.h"
#include "state/activity/omega_presentation.h"
#include "state/activity/omega_first_lair_runtime.h"
#include "core/logging/log.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
namespace t=dawn::client::hooks::teleport;
namespace p=dawn::client::player::position;
namespace g=dawn::state::activity::gateway;
namespace c=dawn::state::activity::coo;
unsigned checks{};
#define CHECK(x) do { ++checks;if(!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1); } } while(false)
struct Body { std::uint32_t owner; t::Vector position;bool readable{true},replaceDuringRead{}; };
std::uint32_t controlled=0x08FAA000U;
void* fallback{};
unsigned identitySamples{},identityInvalidations{};bool identityPresent{};
unsigned publications{},trialPublications{},deepPublications{},hijackedPublications{},gardenPublications{};t::Vector trialPosition{},deepPosition{},hijackedPosition{},gardenPosition{};
unsigned physicsPublications{};void* physicsComponent{};std::uint32_t physicsPlayer{UINT32_MAX};
void physics_observer(void* component,std::uint32_t player,std::uint64_t at) noexcept {
    CHECK(component);CHECK(player==controlled);CHECK(at>0);
    ++physicsPublications;physicsComponent=component;physicsPlayer=player;
}
g::Controller* mission{};
namespace dawn::client::hooks::teleport {
bool read_local_player_entity(void* component,std::uint32_t& entity) noexcept {
    entity=UINT32_MAX;if(!component) { return false; }
    const auto& b=*static_cast<Body*>(component);
    if(!identity::current(controlled,b.owner,controlled)) { return false; }
    entity=controlled;return true;
}
bool owns_local_player(void* component) noexcept { std::uint32_t entity{};return read_local_player_entity(component,entity); }
void* local_player_component() noexcept { return fallback; }
bool read_position(void* component,Vector& position) noexcept {
    auto& b=*static_cast<Body*>(component);if(!b.readable) { return false; }
    position=b.position;if(b.replaceDuringRead) { controlled+=0x2000U;b.owner=controlled; }return true;
}
}
namespace dawn::client::hooks::bootflow::public_event_participant_observer {
void poll_local_identity() noexcept { ++identitySamples;identityPresent=true; }
}
namespace dawn::server::runtime::activity::public_event::participant_bridge {
void invalidate_local_identity() noexcept { ++identityInvalidations;identityPresent=false; }
}
namespace dawn::state::activity {
std::uint64_t mission_run_generation() noexcept { return 1; }
namespace newlight::launchpad { void observe_position(float,float,float) noexcept {} }
namespace deep_storage { void observe_position(float x,float y,float z) noexcept {++deepPublications;deepPosition={x,y,z};} }
namespace hijacked { void observe_position(float x,float y,float z) noexcept {++hijackedPublications;hijackedPosition={x,y,z};} }
namespace strike_bond { void observe_position(float x,float y,float z) noexcept {++gardenPublications;gardenPosition={x,y,z};} }
namespace beyond_infinity { void observe_position(float,float,float) noexcept {} }
namespace vanilla::one_au { void observe_position(float,float,float) noexcept {} void observe_native_player(std::uint32_t) noexcept {} }
namespace deadly_trial { void observe_position(float x,float y,float z) noexcept { ++trialPublications;trialPosition={x,y,z}; } }
namespace gateway { void observe_position(float x,float y,float z) noexcept { ++publications;if(mission) { mission->position(1,{x,y,z}); } } }
namespace omega_presentation { void observe_position(Point) noexcept {} Navigation navigation() noexcept { return {}; } }
namespace omega_first_lair { Status status(std::uint64_t) noexcept { return {}; } bool observe_gate_arrival(const CrownToken&,GateMilestone,std::uint32_t) noexcept { return false; } }
}
namespace dawn::core::log { void write(Channel,Level,std::string_view) noexcept {} }
int main() {
    p::set_physics_observer(&physics_observer);
    // The captured stale component had owner0, while retail controlled getter returned08FAA000.
    CHECK((controlled&0x1FFFU)==0);CHECK(!t::identity::current(controlled,0,controlled));
    CHECK(!t::identity::current(controlled,controlled-0x2000U,controlled));
    CHECK(!t::identity::current(UINT32_MAX,UINT32_MAX,UINT32_MAX));
    CHECK(!t::identity::current(controlled,controlled,controlled+0x2000U));
    CHECK(t::identity::current(controlled,controlled,controlled));
    Body a{controlled,{-772.25F,-130.75F,-7.5F}},b{controlled,{-681.F,50.F,5.F}},foreign{0,{500,500,500}};
    std::string error;auto document=c::script::MissionDocument::read("Dawn/scripts/gateway.lua",g::kProfile,error);CHECK(document);
    g::Controller controller;mission=&controller;CHECK(controller.select(document->views(),1));
    p::reset();CHECK(!identityPresent);p::observe(&a);CHECK(identityPresent);CHECK(p::snapshot().present);CHECK(p::snapshot().position==a.position);
    auto frame=controller.update(1,100,true);CHECK(frame.activeRow==1);
    CHECK(controller.submitted(1,g::kBank,1,frame.generations[1],101));frame=controller.update(1,102,true);CHECK(frame.marchers);CHECK(frame.cohorts==1);
    p::observe(&foreign);CHECK(p::snapshot().position==a.position);
    // Retired storage still has index0: it must not keep the cache or reject the new player.
    a.owner=0;a.readable=false;fallback=&a;p::poll();CHECK(!p::snapshot().present);CHECK(!identityPresent);
    p::observe(&foreign);CHECK(!p::snapshot().present);
    p::observe(&b);CHECK(p::snapshot().present);CHECK(p::snapshot().position==b.position);
    for(unsigned i=0;i<4;++i) { frame=controller.update(1,103+i,true); }
    CHECK(frame.marchers);CHECK(frame.cohorts==3);
    // Reacquire on the next physics sample without needing a camera poll first.
    b.owner=0;a.owner=controlled;a.readable=true;p::observe(&a);CHECK(p::snapshot().position==a.position);
    // A body can retire before its owner field is cleared. Failed reads evict it too.
    a.readable=false;p::poll();CHECK(!p::snapshot().present);b.owner=controlled;p::observe(&b);CHECK(p::snapshot().present);
    b.position[0]=std::numeric_limits<float>::quiet_NaN();const auto before=publications;p::poll();CHECK(!p::snapshot().present);CHECK(publications==before);
    b.position[0]=-681.F;p::observe(&b);CHECK(p::snapshot().present);
    // A full handle change during the body read cannot publish across player generations.
    b.replaceDuringRead=true;const auto prior=publications;p::poll();CHECK(!p::snapshot().present);CHECK(publications==prior);
    b.replaceDuringRead=false;p::observe(&b);CHECK(p::snapshot().present);
    p::reset();fallback=&foreign;p::poll();CHECK(!p::snapshot().present);
    fallback=&b;const auto beforeFallback=publications;p::poll();CHECK(p::snapshot().present);
    CHECK(publications==beforeFallback+1);
    b.position[0]+=1.F;const auto beforeCached=publications;p::poll();CHECK(p::snapshot().position==b.position);
    CHECK(publications==beforeCached+1);
    CHECK(deepPublications==publications);CHECK(deepPosition==b.position);
    CHECK(hijackedPublications==publications);CHECK(hijackedPosition==b.position);
    CHECK(gardenPublications==publications);CHECK(gardenPosition==b.position);
    CHECK(trialPublications==publications);CHECK(trialPosition==b.position);
    CHECK(identitySamples==publications);
    CHECK(physicsPublications==publications);CHECK(physicsComponent==&b);CHECK(physicsPlayer==controlled);
    // An observed body loss retires the participant identity immediately.
    b.readable=false;p::observe(&b);CHECK(!p::snapshot().present);CHECK(!identityPresent);
    b.readable=true;p::observe(&b);CHECK(identityPresent);
    const auto priorPhysics=physicsPublications;
    p::set_physics_observer(nullptr);p::poll();CHECK(physicsPublications==priorPhysics);
    p::reset();CHECK(!p::snapshot().present);CHECK(!identityPresent);CHECK(identityInvalidations>0);
    std::printf("Player position: %u checks; live stale-slot fixture, cache reacquisition, body loss, salt changes and Gateway traversal entry passed\n",checks);
}
