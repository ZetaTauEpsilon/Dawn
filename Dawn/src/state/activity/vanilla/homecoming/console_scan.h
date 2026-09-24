#pragma once
#include "controller.h"

namespace dawn::state::activity::vanilla::homecoming {
// A small snapshot for the per-component native tick; do not copy the complete
// mission frame for every Ghost link in the loaded world.
struct ConsoleScanRequest {
    coo::Generation owner{};
    std::uint32_t generation{};
    bool enabled() const noexcept { return owner.valid() && generation!=0; }
    friend bool operator==(const ConsoleScanRequest&,const ConsoleScanRequest&)=default;
};
inline ConsoleScanRequest console_scan_request(coo::Generation owner,const Frame& frame) noexcept {
    if(!owner.valid() || !frame.enabled || frame.fault || frame.finished
        || frame.section!=static_cast<std::uint8_t>(Section::ship)
        || !frame.consoleArmed || frame.consoleScanned
        || !frame.spawnGeneration || frame.spawnGeneration>=INT32_MAX) { return {}; }
    return {owner,frame.spawnGeneration+1U};
}
ConsoleScanRequest console_scan_request() noexcept;
}
