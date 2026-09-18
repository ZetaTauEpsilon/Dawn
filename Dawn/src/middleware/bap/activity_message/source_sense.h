#pragma once
#include <array>
#include <cstdint>
namespace dawn::middleware::bap::activity_message::source_sense {
// 80807ECC: native source counters and 80807ECD's 24 quantized task costs.
// Population/death accounting continues to use authenticated actor receipts.
struct Output {
    std::array<std::uint32_t,6> counters{};
    std::array<std::uint8_t,24> costs{};
    std::uint32_t costMask{};
    std::uint8_t present{};
};
template<class R> bool read(R& r,Output& out) noexcept {
    Output value{};std::uint64_t present{},raw{},count{};
    constexpr unsigned widths[]{31,31,31,6,7,31};
    for(unsigned i=0;i<6;++i) {
        if(!r.read(1,present)) {return false;}
        if(present) {
            if(!r.read(widths[i],raw)) {return false;}
            value.present|=static_cast<std::uint8_t>(1U<<i);
            value.counters[i]=static_cast<std::uint32_t>(raw);
        }
    }
    if(!r.skip(8) || !r.read(1,present)) {return false;}
    if(present && (!r.read(4,count) || count>8 || !r.skip(count*32))) {return false;}
    if(!r.read(1,present)) {return false;}
    if(present) for(unsigned i=0;i<24;++i) {
        if(!r.read(1,present)) {return false;}
        if(present) {
            if(!r.read(7,raw)) {return false;}
            value.costs[i]=static_cast<std::uint8_t>(raw);value.costMask|=1U<<i;
        }
    }
    out=value;return true;
}
}
