#pragma once
#include "controller.h"

namespace dawn::state::activity::vanilla::homecoming::music {
inline constexpr auto kSensor=kMusicAsset;
inline constexpr std::size_t kBits=128+129*55;
// Bank 80B5090F: only section 1 (first_cabal) is authenticated for the plaza defence.
// The original host's complete score mapping has not been recovered, so the
// selector starts that section on plaza arrival and releases it on leaving.
constexpr int section(const Frame& f) noexcept {
    if(!f.enabled || f.finished || f.cinematic.phase!=cinematics::Phase::gameplay) {return -1;}
    return f.section==static_cast<std::uint8_t>(Section::plaza)?1:-1;
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
