#pragma once
#include "mission.h"
#include "../../coo/lifecycle_service.h"
namespace dawn::state::activity::vanilla::one_au::recovery {
struct Checkpoint {Section section{Section::bridge};std::uint8_t bubble{8};std::uint32_t spawn{0x9C58857A};};
constexpr Checkpoint checkpoint(std::uint8_t section) noexcept {
    if(section>=static_cast<std::uint8_t>(Section::escape)) {return {Section::escape,0,0x45920385};}
    // The archive's DF59C25C belongs to arcade_ember. Red War loads cabal_ship;
    // retry at its native Light's End entrance and restore the Interceptor route.
    if(section>=static_cast<std::uint8_t>(Section::access)) {return {Section::access,0,0x2EA8FB98};}
    if(section>=static_cast<std::uint8_t>(Section::foundry)) {return {Section::foundry,5,0x782CAF4C};}
    if(section>=static_cast<std::uint8_t>(Section::ready)) {return {Section::ready,5,0x82328D63};}
    if(section>=static_cast<std::uint8_t>(Section::processing)) {return {Section::processing,7,0x4B27745D};}
    return {};
}
// How long the wipe countdown runs before the checkpoint reload. This is not the
// client's revive delay: a darkness zone holds that off for far longer so that
// the wipe lands first.
inline constexpr std::uint64_t kCountdownMs=3000;
enum class Phase : std::uint8_t { idle,countdown,requested,waiting,releasing };
struct Spawn {std::int8_t state{};std::uint8_t token{};std::uint64_t value{};};
struct State {
    Phase phase{};Checkpoint point{};Spawn host{};std::uint64_t deadline{};bool reset{};
    bool active() const noexcept {return phase!=Phase::idle;}
    bool holding() const noexcept {return phase>=Phase::requested;}
};
// Native membership spawn handshake: host 1 -> client 2/4 -> host 4 -> client 0.
// A distinct token is required even when the preceding wipe is complete.
inline bool project(State& state,Spawn client) noexcept {
    if(state.phase==Phase::requested && client.state==0) {
        state.host={1,static_cast<std::uint8_t>(client.token+1),0};state.phase=Phase::waiting;
    }
    if((state.phase!=Phase::waiting && state.phase!=Phase::releasing) || client.token!=state.host.token) {return false;}
    if(state.phase==Phase::waiting && client.state>=2 && !state.reset) {return true;}
    if(state.phase==Phase::waiting && state.reset && client.state==4) {state.host.state=4;state.phase=Phase::releasing;}
    else if(state.phase==Phase::releasing && client.state==0) {state.phase=Phase::idle;state.host.state=0;state.deadline=0;}
    return false;
}
}
