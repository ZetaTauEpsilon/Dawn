#pragma once
#include "controller.h"

namespace dawn::state::activity::vanilla::homecoming::music {
inline constexpr auto kSensor=kMusicAsset;
inline constexpr std::size_t kBits=128+129*55;
// Bank 80B5090F: twenty authored sections. The graphs select them at the authored music
// volumes and mission beats (music_section); the native bank owns transitions and playback.
constexpr int section(const Frame& f) noexcept {
    if(!f.enabled || f.finished || f.cinematic.phase!=cinematics::Phase::gameplay || f.musicSection==music_section::none) {return -1;}
    return f.musicSection;
}
template<class W> bool write(W& w,const Frame& f) noexcept {
    const auto selected=section(f);
    if(!w.write(selected<0?0U:1U<<selected,32) || !w.write(0,32) || !w.write(0,32) || !w.write(0,32)) {return false;}
    for(unsigned i=0;i<129;++i) {
        if(!w.write(0x811C9DC5U,32) || !w.write(0,7) || !w.write(0x7FFFU,16)) {return false;}
    }
    return true;
}
}
