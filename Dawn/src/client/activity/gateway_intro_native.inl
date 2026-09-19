namespace intro = state::activity::gateway_intro;
struct GatewayExit { std::uintptr_t session{};std::uint64_t nonce{},run{};bool pending{}; };
GatewayExit g_gatewayExit{};
using VideoManager = std::uintptr_t(__fastcall*)();
using VideoPlaying = bool(__fastcall*)(std::uintptr_t);
using Flag = bool(__fastcall*)(int);
using SetFlag = void(__fastcall*)(int,bool);
using ActiveIndex = void*(__fastcall*)(std::int16_t*);
using Leave = void(__fastcall*)(std::int32_t,std::int32_t);
bool gateway_video_poll(std::uintptr_t base,std::int32_t step) noexcept {
    const auto manager=resolve<VideoManager>(base,0x41B040,{0x48,0x8D,0x05,0xB9,0x15,0xB7,0x01,0xC3});
    const auto playing=resolve<VideoPlaying>(base,0x41B420,{0x48,0x83,0xEC,0x28,0x83,0x79,0x58,0xFF});
    const auto flag=resolve<Flag>(base,0x1764D20,{0x40,0x53,0x48,0x83,0xEC,0x20,0x8B,0xD9});
    const auto index=resolve<ActiveIndex>(base,0xC294B0,{0x48,0x89,0x5C,0x24,0x18,0x55,0x48,0x8D});
    if(!manager || !playing || !flag || !index) return false;
    std::int16_t selected{-1};
    if(step==39) index(&selected);
    intro::video(step,selected,step==39 && playing(manager()),step==39 && flag(8),now());
    return true;
}
bool gateway_video_reset(std::uintptr_t base) noexcept {
    const auto flag=resolve<SetFlag>(base,0x1764C60,{0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83});
    if(!flag) return false;
    flag(8,false);flag(9,false);return true;
}
bool gateway_nonce(std::uintptr_t session,std::uint64_t& nonce) noexcept {
    std::uint8_t flags{};
    return read(session+0x182C0+0x140,flags) && (flags&1U)
        && read(session+0x182C0+0x148+0x18,nonce) && nonce && nonce!=UINT64_MAX;
}
// Leaves a cinematic activity once the queued mission launch is committed natively.
void gateway_depart(std::uintptr_t base,std::int32_t step,std::int16_t mission) noexcept {
    if(!g_gatewayExit.pending) return;
    if(g_gatewayExit.run!=state::activity::mission_run_generation()) {g_gatewayExit={};return;}
    std::uint64_t nonce{};std::int16_t index{};std::uint8_t kind{0xFF};
    const auto session=g_gatewayExit.session;
    if(step!=38 || !gateway_nonce(session,nonce) || nonce!=g_gatewayExit.nonce
        || !read(session+0x182C0+0x148+4,index) || index!=mission
        || !read(session+0x182C0+0x378,kind) || kind==0xFF) return;
    const auto leave=resolve<Leave>(base,0xE2DEB0,{0x48,0x89,0x5C,0x24,0x18,0x55,0x56,0x57});
    if(!leave) return;
    g_gatewayExit.pending=false;
    // Retain retail's committed Gateway descriptor and normal Mercury fly-in.
    core::log::write(core::log::Channel::client,core::log::Level::info,mission==intro::kMission?"ev=gateway_intro stage=mission_departure":"ev=homecoming stage=prologue_departure");
    leave(28,309);
}
