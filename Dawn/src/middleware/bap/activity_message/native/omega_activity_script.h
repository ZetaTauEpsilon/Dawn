#pragma once
#include <cstdint>

namespace dawn::middleware::bap::activity_message::native::omega_activity_script {
// Constructed 808099C4 time/progress state from the 0.1.3 Omega hotfix.
// Keep the script authority body and state; this only removes its generic HUD ring.
template<class Writer> bool time_state(Writer& writer) noexcept {
    return writer.write(0,1) && writer.write(0,64)
        && writer.write(0x134F00C00000ULL,64) && writer.write(0,64)
        && writer.write(0,64) && writer.write(UINT64_MAX,64)
        && writer.write(0x3F800000U,32);
}
}
