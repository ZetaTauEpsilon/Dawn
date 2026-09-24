#pragma once
#include "controller.h"

namespace dawn::state::activity::vanilla::one_au {
// A small snapshot for the per-component native tick; do not copy the complete
// mission frame for every Ghost link in the loaded world.
struct BridgeScanRequest {
    coo::Generation owner{};
    std::uint32_t generation{};
    bool enabled() const noexcept { return owner.valid() && generation!=0; }
    friend bool operator==(const BridgeScanRequest&,const BridgeScanRequest&)=default;
};
inline BridgeScanRequest bridge_scan_request(coo::Generation owner,const Frame& frame) noexcept {
    if(!owner.valid() || !frame.enabled || frame.fault || frame.finished || frame.recovery.holding()
        || frame.section!=static_cast<std::uint8_t>(Section::bridge)
        || !frame.interactions[0].armed || frame.interactions[0].completed
        || !frame.spawnGeneration || frame.spawnGeneration>=INT32_MAX) { return {}; }
    return {owner,frame.spawnGeneration+1U};
}
BridgeScanRequest bridge_scan_request() noexcept;
}
