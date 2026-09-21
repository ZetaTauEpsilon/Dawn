#pragma once
#include "../coo/objective_service.h"
namespace dawn::state::activity::deep_storage::navigation {
// Supplied 0.1.5.2 DLL (88C75AC8...), RVAs 726CA0-726EE4.
struct Target {std::uint32_t event;coo::MarkerTarget marker;std::array<float,4> position;std::uint8_t display;};
inline constexpr Target entrance[]{
    {0xB035525A,{{0x4324A238,0x80B5616B,47,4},{0x2D7B770F,0x22723FAD,0x4324A238,0x22BCA6B6}},{1284.5F,527.F,-28.733692169189453F,1.F},2},
    {0xF8F223A7,{{0x4324A238,0x80B5616B,47,12},{0x2D7B770F,0x22723FAD,0x4324A238,0x2AF83C0D}},{1309.5999755859375F,544.1000366210938F,-30.5F,1.F},2},
    {0x149B4756,{{0x4324A238,0x80B5616B,47,13},{0x2D7B770F,0x22723FAD,0x4324A238,0x5FE3E6D8}},{1461.800048828125F,548.2999877929688F,-118.5999984741211F,1.F},0},
};
inline constexpr Target finalTargets[]{
    {0x025CC54F,{{0xEA42F517,0x80B56A4F,47,1},{}},{267.3000183105469F,565.7000122070312F,-1036.3001708984375F,1.F},2},
    {0x025CC54F,{{0xEA42F517,0x80B56A4F,47,2},{}},{265.9000244140625F,516.7999877929688F,-1035.900146484375F,1.F},2},
    {0x025CC54F,{{0xEA42F517,0x80B56A4F,47,3},{}},{300.8999938964844F,539.7999877929688F,-1023.5487060546875F,1.F},2},
};
inline constexpr Target interior[]{
    {0x772F4471,{{0xABF05147,0x80B56A89,47,7},{}},{1017.216064453125F,568.3653564453125F,-285.9073791503906F,1.F},0},
    {0x7811B582,{{0xE86A5BFD,0x80B56A6E,47,11},{}},{1017.216064453125F,568.3653564453125F,-285.9073791503906F,1.F},0},
    {0x1A188E6E,{{0x54DA5E1B,0x80B56A7C,47,5},{}},{618.1632080078125F,540.2364501953125F,-618.422607421875F,1.F},0},
    {0x2A751789,{{0x559E8DE2,0x80B56A5D,47,9},{}},{259.3310852050781F,541.736572265625F,-1035.7044677734375F,1.F},0},
    {0xCB573BE1,{{0xEA42F517,0x80B56A4F,47,8},{}},{303.37713623046875F,539.7999877929688F,-1031.7734375F,1.F},2},
};
inline constexpr const Target* canonical(std::uint32_t event) noexcept {
    for(const auto& t:entrance)if(t.event==event)return &t;
    for(const auto& t:interior)if(t.event==event)return &t;
    return nullptr;
}
inline constexpr coo::MarkerTarget marker(std::uint32_t event) noexcept {
    const auto* t=canonical(event);return t?t->marker:coo::MarkerTarget{};
}
inline constexpr const Target* route(std::uint32_t event,std::uint32_t context) noexcept {
    if(context==4) {
        if(event==0x772F4471)return &entrance[2];
        for(const auto& t:entrance)if(t.event==event)return &t;
    } else if(context==19)for(const auto& t:interior)if(t.event==event)return &t;
    return nullptr;
}
}
