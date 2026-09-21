#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>

namespace dawn::client::hooks::bootflow {
// Diagnostics only. Never use this cache to suppress a native authority apply.
// Different components must not overwrite a single shared "last gate state".
// Keep all channels and the bound-device identity, not a lossy XOR signature.
struct GateTraceSample {
    std::uintptr_t component{};
    std::uint32_t definition{},self{};
    std::array<std::uint64_t,4> channelsAndDevice{};
    bool operator==(const GateTraceSample&) const = default;
};
template<std::size_t Capacity=128> class GateTraceCache final {
    static_assert(Capacity>0);
    struct Entry {GateTraceSample sample{};std::uint64_t reported{},seen{};};
    std::array<Entry,Capacity> entries_{};
    std::atomic_flag busy_=ATOMIC_FLAG_INIT;
public:
    // Contention drops diagnostics rather than delaying a game-thread callback.
    bool report(const GateTraceSample& sample,std::uint64_t now) noexcept {
        if(!sample.component || busy_.test_and_set(std::memory_order_acquire)) return false;
        struct Unlock {std::atomic_flag& busy;~Unlock(){busy.clear(std::memory_order_release);}} unlock{busy_};
        auto* selected=&entries_[0];
        for(auto& entry:entries_) {
            if(entry.sample.component==sample.component) {selected=&entry;break;}
            if(!entry.sample.component || entry.seen<selected->seen) selected=&entry;
        }
        const bool changed=selected->sample!=sample;
        const bool due=changed || now<selected->reported || now-selected->reported>=10000;
        selected->sample=sample;selected->seen=now;
        if(due) selected->reported=now;
        return due;
    }
    void clear() noexcept {
        if(busy_.test_and_set(std::memory_order_acquire)) return;
        entries_={};busy_.clear(std::memory_order_release);
    }
};
}
