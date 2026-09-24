namespace continuation=state::activity::vanilla::homecoming::continuation;
using MissionOwner=state::activity::coo::Generation;
MissionOwner g_homecomingOwner{},g_homecomingClaimed{};
std::uint64_t g_homecomingSession{};
GatewayExit g_homecomingExit{};
std::atomic<std::uint64_t> g_homecomingLoadingUntil{};
bool g_homecomingLeftWorld{};

bool homecoming_source(const state::activity::destination::DestinationSelection& actual) noexcept {
    return actual.activityIndex==continuation::kSourceActivity
        && actual.packageNameLength==continuation::kSourcePackage.size()
        && std::memcmp(actual.packageName.data(),continuation::kSourcePackage.data(),actual.packageNameLength)==0;
}
void clear_homecoming_continuation() noexcept {
    g_homecomingOwner={};g_homecomingSession=0;g_homecomingExit={};
    g_homecomingLoadingUntil.store(0,std::memory_order_release);
    g_homecomingLeftWorld=false;
}
void poll_homecoming_loading(std::int32_t step) noexcept {
    const auto until=g_homecomingLoadingUntil.load(std::memory_order_acquire);
    if(!until) return;
    if(step!=38) g_homecomingLeftWorld=true;
    if(step<0 || now()>=until || (step==38 && g_homecomingLeftWorld))
        g_homecomingLoadingUntil.store(0,std::memory_order_release);
}
void queue_homecoming_continuation(const state::activity::destination::DestinationSelection& actual,
                                  std::uint64_t session) noexcept {
    if(!homecoming_source(actual)) return;
    const auto owner=continuation::request();
    if(!owner.valid() || owner.run!=state::activity::mission_run_generation() || owner==g_homecomingClaimed) return;
    std::size_t mission=openings::kMissions.size();
    for(std::size_t i=0;i<openings::kMissions.size();++i)
        if(openings::kMissions[i].activity==continuation::kTargetActivity) mission=i;
    const auto route=openings::resolve(mission,state::build_data::activities::entries());
    g_homecomingClaimed=owner;
    if(!route.valid() || destination_name(route.destination)!=continuation::kTargetPackage) {
        core::log::write(core::log::Channel::client,core::log::Level::warn,
            "ev=homecoming stage=continuation result=unavailable activity=288");return;
    }
    AcquireSRWLockExclusive(&g_lock);
    if(g_state.busy) {ReleaseSRWLockExclusive(&g_lock);return;}
    // This private path alone can queue from a completed mission. Public UI
    // requests retain their ordinary return-to-orbit requirement.
    g_state.status=Status::requested;g_state.index=route.transport;g_state.busy=true;
    g_state.manual=g_state.opening=true;g_state.destination=route.destination;
    g_state.nightfallOptions={};g_requestedAt=now();
    g_homecomingOwner=owner;g_homecomingSession=session;g_homecomingExit={};
    ReleaseSRWLockExclusive(&g_lock);
    std::array<char,160> line{};std::snprintf(line.data(),line.size(),
        "ev=homecoming stage=continuation result=requested run=%llu owner=%u activity=288",
        static_cast<unsigned long long>(owner.run),owner.value);
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
}
void homecoming_depart(std::uintptr_t base,std::int32_t step,std::uint64_t sessionId) noexcept {
    if(!g_homecomingExit.pending || step!=38 || sessionId!=g_homecomingSession
        || g_homecomingExit.run!=state::activity::mission_run_generation()
        || continuation::request()!=g_homecomingOwner) return;
    const auto world=resolve<World>(base,0xC03430,{0x40,0x56,0x48,0x83,0xEC,0x30,0x48,0x8B});
    const auto manager=world?world():0;std::int32_t primary{};
    std::uint64_t nonce{};std::int16_t index{};std::uint8_t kind{0xFF};
    const auto session=g_homecomingExit.session;
    if(!manager || !read(manager+0x10,primary) || primary<0 || primary>3
        || session!=manager+0x18+static_cast<std::uintptr_t>(primary)*0x1C8A0
        || !gateway_nonce(session,nonce) || nonce!=g_homecomingExit.nonce
        || !read(session+0x182C0+0x148+4,index) || index!=continuation::kTargetActivity
        || !read(session+0x182C0+0x378,kind) || kind==0xFF) return;
    const auto leave=resolve<Leave>(base,0xE2DEB0,{0x48,0x89,0x5C,0x24,0x18,0x55,0x56,0x57});
    if(!leave) return;
    g_homecomingExit.pending=false;
    // Suppress only loading/fly-in movies. Exodus owns its vision cinematic.
    g_homecomingLeftWorld=false;g_homecomingLoadingUntil.store(now()+120000,std::memory_order_release);
    core::log::write(core::log::Channel::client,core::log::Level::info,
        "ev=homecoming stage=continuation result=departure_confirmed activity=288");
    leave(28,309);
}
