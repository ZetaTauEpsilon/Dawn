#pragma once
#include "bindings.h"
#include "../coo/lifecycle_service.h"
#include <bitset>
#include "../../../server/runtime/activity/mission_capture_service.h"

namespace dawn::state::activity::beyond_infinity {
struct NativeState {
    std::uint32_t generation{};
    bool managed{},desired{},prepared{},active{};
};
struct SceneReceipt {
    coo::Generation owner{};
    coo::Asset asset{};
    std::uint32_t group{UINT32_MAX},sensor{UINT32_MAX},selector{UINT32_MAX},serial{UINT32_MAX};
    bool valid() const noexcept { return owner.valid() && group!=UINT32_MAX && sensor!=UINT32_MAX && selector!=UINT32_MAX && serial!=UINT32_MAX; }
    friend bool operator==(const SceneReceipt&,const SceneReceipt&)=default;
};
struct LensReceipt {
    coo::Generation owner{};
    std::uintptr_t source{};
    std::uint32_t entity{UINT32_MAX},serial{UINT32_MAX},health{UINT32_MAX};
    bool valid() const noexcept { return owner.valid() && source>=0x10000 && source!=UINTPTR_MAX && entity!=UINT32_MAX && serial!=UINT32_MAX && health!=UINT32_MAX; }
    friend bool operator==(const LensReceipt&,const LensReceipt&)=default;
};
struct PlateReceipt {
    coo::Generation owner{};
    std::uintptr_t source{};
    std::uint32_t entity{UINT32_MAX},serial{UINT32_MAX},device{UINT32_MAX},timer{UINT32_MAX};
    bool valid() const noexcept { return owner.valid() && source>=0x10000 && source!=UINTPTR_MAX && entity!=UINT32_MAX && serial!=UINT32_MAX && device!=UINT32_MAX && timer!=UINT32_MAX; }
    friend bool operator==(const PlateReceipt&,const PlateReceipt&)=default;
};
struct SceneRequest {
    std::array<std::uint32_t,32> events{};
    std::uint8_t count{};
    bool silent{};
    std::span<const std::uint32_t> inputs() const noexcept { return std::span(events).first(count<=events.size()?count:0); }
};
struct NavigationProgress {
    bool forestPastComplete{},pastEncounter{},pastReturn{},forestFutureComplete{},futureEntered{},futureReflection{},futureReturn{};
    friend bool operator==(const NavigationProgress&,const NavigationProgress&)=default;
};
struct Frame {
    NavigationProgress navigation{};
    bool enabled{},checked{},finished{},lensDestroyed{},plateOccupied{},lensExposed{};
    std::uint8_t section{},forestPass{},transitRoute{},transitContact{},activeRow{coo::kNoDialogue};
    std::uint32_t spawnGeneration{},revision{},objective{},plateRevision{1};
    std::uint32_t forestSeed{};
    bool forestReady{},wellEntered{};
    std::array<std::uint32_t,49> generations{};
    std::array<NativeState,std::size(kAssets)> native{};
    std::array<SceneRequest,std::size(kScenes)> sceneRequests{};
    coo::ObjectiveState presentation{};
    coo::CompletionPublication completion{};
    // Authority time starts on this run's authenticated arrival and never rewinds.
    std::uint64_t gameplayClockTicks{};
    server::runtime::activity::mission_capture::Publication plateCapture{};
};
inline constexpr std::size_t asset_index(coo::Asset asset) noexcept {
    for(std::size_t i=0;i<std::size(kAssets);++i) { if(kAssets[i].asset==asset) { return i; } }
    return std::size(kAssets);
}
inline constexpr std::size_t scene_index(coo::Asset asset) noexcept {
    for(std::size_t i=0;i<std::size(kScenes);++i) { if(kScenes[i].asset==asset) { return i; } }
    return std::size(kScenes);
}
inline constexpr bool dialogue_identity(std::uint32_t definition,std::int64_t offset,std::uint32_t bank,std::uint8_t row) noexcept {
    const auto* binding=find(kDialogueAsset.registry,53,kDialogueAsset.slot);
    return binding && definition==kDialogueAsset.definition && offset==binding->offset && bank==kBank && row<49;
}
// Gateway's native box controller: shielded=1, exposed=.75, removed=0.
inline constexpr float lens_position(const Frame& frame,bool requested=true) noexcept {
    return !requested || frame.lensDestroyed?0.F:frame.lensExposed?.75F:1.F;
}
inline constexpr bool well_channel(std::uint16_t slot,const Frame& frame) noexcept {
    return slot==40 || (slot==38?!frame.lensDestroyed:frame.lensExposed && !frame.lensDestroyed);
}
inline constexpr coo::Asset kPlate{0x233E7149U,0x80F462ADU,4,34};
inline constexpr coo::Asset kLens{0x233E7149U,0x80F462B3U,4,36};
}
