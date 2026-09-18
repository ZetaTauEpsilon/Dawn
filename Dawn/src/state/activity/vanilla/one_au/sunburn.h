#pragma once
#include "shadow_catalog.h"
#include <cmath>
namespace dawn::state::activity::vanilla::one_au::sunburn {
// Native placement scale is a half extent: deck_global's Y=100 spans the
// outdoor route between its two airlocks. Retain the authored rotations.
inline bool contains(const Cover& c,Point p) noexcept {
    const double x=double(p.x)-c.position.x,y=double(p.y)-c.position.y,z=double(p.z)-c.position.z;
    const double qx=-c.rotation[0],qy=-c.rotation[1],qz=-c.rotation[2],qw=c.rotation[3];
    const double tx=2*(qy*z-qz*y),ty=2*(qz*x-qx*z),tz=2*(qx*y-qy*x);
    return std::abs(x+qw*tx+qy*tz-qz*ty)<=c.halfExtents.x
        && std::abs(y+qw*ty+qz*tx-qx*tz)<=c.halfExtents.y
        && std::abs(z+qw*tz+qx*ty-qy*tx)<=c.halfExtents.z;
}
inline bool sheltered(Point p) noexcept {
    if(!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {return false;}
    for(const auto& c:kCover) {if(contains(c,p)) {return true;}}return false;
}
}
