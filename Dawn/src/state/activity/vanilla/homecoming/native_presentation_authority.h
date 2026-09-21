#pragma once
#include <cstdint>
#include <span>
#include "../../coo/objective_service.h"
#include <array>
namespace dawn::state::activity::vanilla::homecoming::native_presentation {
using coo::MarkerTarget;
using coo::ObjectiveState;
// Native reflected schemas 80804F77 (dialogue) and 80804F67 (directive).
// The caller must validate the mission's exact descriptor before publication.
inline constexpr std::size_t kDialogueBits=19767, kDirectiveBits=4802;
template<class Writer> bool absent(Writer& w) noexcept {
    return w.write(0x811C9DC5U,32) && w.write(0,7) && w.write(32767U,16);
}
template<class Writer> bool dialogue(Writer& w,std::span<const std::uint32_t> generations,std::uint8_t active) noexcept {
    if(generations.size()>128 || (active!=0xFF && (active>=generations.size() || generations[active]==0))) { return false; }
    for(const auto generation:generations) { if(generation>0x7FFFFFFFU) { return false; } }
    const auto start=w.bit_count();if(!absent(w)) { return false; }
    for(std::size_t row=0;row<128;++row) {
        const auto generation=row<generations.size()?generations[row]:0U;
        const bool playing=row==active;
        if(!w.write(UINT64_MAX,64) || !w.write(playing?1U:0U,1) || (playing && !w.write(1,64))
            || !absent(w) || !w.write(std::uint64_t{generation}+0x80000000ULL,32)
            || !w.write(playing?3U:1U,2)) { return false; }
    }
    return w.bit_count()-start==kDialogueBits+(active==0xFF?0U:64U);
}
struct TimedState {std::array<std::uint64_t,5> words{};bool running{};std::uint32_t scale{};};
template<class Writer> bool directive_record(Writer& w,bool active,std::uint32_t event,MarkerTarget marker={},const TimedState* time=nullptr,bool nativeMarkers=false,std::uint32_t progress=0,std::uint32_t target=0,std::uint32_t variant=0) noexcept {
    if(!w.write(active?event:0x811C9DC5U,32) || !w.write(0x80000000ULL+variant,32)
        || !w.write(active?1U:0U,2) || !w.write(time && time->running?1U:0U,1)) { return false; }
    for(unsigned i=0;i<5;++i) { if(!w.write(time?time->words[i]:nativeMarkers && i<4?0:UINT64_MAX,64)) { return false; } }
    if(!w.write(time?time->scale:0U,32)) { return false; }
    for(unsigned i=0;i<4;++i) {
        const auto count=active && target && progress<=target && i<2?(i?target:progress):0U;
        if(!w.write(nativeMarkers?0x80000000ULL+count:0x7FFFFFFFULL,32)) { return false; }
    }
    if(!w.write(1,2) || !absent(w) || !w.write(nativeMarkers && active && marker.valid()?3U:1U,3)) { return false; }
    for(unsigned i=0;i<4;++i) {
        const bool selected=active && i==0 && marker.valid();
        if(selected) {
            // Directive markers store the native slot ordinal, unlike the
            // bias-one references in general sensor authority structures.
            if(!w.write(marker.asset.registry,32) || !w.write(marker.asset.type+(nativeMarkers?0U:1U),7) || !w.write(marker.asset.slot+32768U,16)) { return false; }
        } else if(!absent(w)) { return false; }
        if(!absent(w)) { return false; }
        for(unsigned j=0;j<4;++j) { if(!w.write(selected?marker.locator[j]:0U,32)) { return false; } }
        if(!w.write(0,1)) { return false; }
    }
    return true;
}
template<class Writer> bool objective(Writer& w,const ObjectiveState& state,const TimedState* time=nullptr,bool nativeMarkers=false,std::uint32_t progress=0,std::uint32_t target=0,std::uint32_t variant=0) noexcept {
    if(!state.published) { return false; }
    const auto begin=w.bit_count();
    return absent(w) && absent(w) && directive_record(w,state.active,state.event,state.marker,time,nativeMarkers,progress,target,variant)
        && directive_record(w,false,state.retiredEvent,{},nullptr,nativeMarkers) && directive_record(w,false,0,{},nullptr,nativeMarkers) && w.write(1,3)
        && w.bit_count()-begin==kDirectiveBits;
}
}
