#pragma once
#include <bit>
#include <cmath>
#include <cstdint>
namespace dawn::middleware::bap::activity_message::ghost_sense {
// Reflected 80804D3E: active, elapsed/duration, accepted signed Auth generation.
struct Output {bool active{};float progress{};std::int32_t generation{};};
template<class Reader> bool read(Reader& reader,Output& output) noexcept {
    std::uint64_t active{},progress{},generation{};
    if(!reader.read(1,active) || !reader.read(32,progress) || !reader.read(32,generation)) {return false;}
    Output result{active!=0,std::bit_cast<float>(static_cast<std::uint32_t>(progress)),
        std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(generation)^0x80000000U)};
    if(!std::isfinite(result.progress) || result.progress<0.F) {return false;}
    output=result;return true;
}
}
