#pragma once
#include "cinematics.h"
#include "../../lifecycle_generation.h"
#include "../../omega_ending_transit_rules.h"

namespace dawn::state::activity::vanilla::adieu::transit {
namespace native=omega_ending_transit;
struct Scope {
    coo::Generation owner{};ActivityInstanceKey activity{};std::uint64_t member{};std::uint8_t route{};
    bool operator==(const Scope&) const = default;
};
// Package-derived point sets. Do not interpolate world coordinates or reuse the
// city default at the two remote parts of the route.
inline constexpr std::uint32_t kArrivals[]{0x2EA8FB98U,0x777EB298U,0xEDE37EC8U};
constexpr native::Target destination(std::uint8_t route) noexcept {
    return route>=1 && route<=3?native::Target{cinematics::kMovies[route-1].region,0x811C9DC5U}
        :route>=4 && route<=6?native::Target{kRegion,kArrivals[route-4]}:native::Target{};
}
constexpr native::Target destination(Scope scope) noexcept {return destination(scope.route);}
class Transaction final {
public:
    bool next(Scope s,native::Observation o) noexcept {
        if(!bound_) {return begin(s,o);}
        if(s==scope_) {return true;}
        // Keep completed host0 while an old/failed membership after-image is
        // retried. Dropping it would echo local3 and replay the old cinematic.
        if(pending() || !o.hasTeleport || o.local.state!=0 || o.local.token!=host_.token
            || o.local.sliceSetIndex!=host_.sliceSetIndex || o.local.sliceSetHash!=host_.sliceSetHash
            || s.owner!=scope_.owner || s.activity!=scope_.activity || s.member!=scope_.member) {return false;}
        Transaction replacement;
        if(!replacement.begin(s,o)) {return false;}
        *this=replacement;return true;
    }
    bool begin(Scope s,native::Observation o) noexcept {
        if(bound_ || !s.owner.valid() || !s.activity || !s.member || s.member==UINT64_MAX || !destination(s).valid()
            || o.local.state!=0 || (!o.hasTeleport && o.local!=native::Teleport{})) {return false;}
        scope_=s;bound_=true;auto token=static_cast<std::uint8_t>(o.local.token+1);if(!token) {token=1;}
        const auto d=destination(s);host_={1,token,d.region,d.spawnSet};return true;
    }
    native::Authority project(Scope s,native::Observation o) noexcept {
        if(!bound_ || s!=scope_) {return {};}
        if(o.hasTeleport && o.local.token==host_.token && o.local.sliceSetIndex==host_.sliceSetIndex
            && o.local.sliceSetHash==host_.sliceSetHash) {
            if(host_.state==1 && o.local.state==3 && o.hasRegion && o.currentRegion==host_.sliceSetIndex) {host_.state=3;arrived_=true;}
            else if(host_.state==3 && o.local.state==0) {host_.state=0;}
        }
        return {host_,true,arrived_,host_.state==0};
    }
    bool bound() const noexcept {return bound_;}
    bool pending() const noexcept {return bound_ && host_.state!=0;}
    Scope scope() const noexcept {return scope_;}
private:
    Scope scope_{};native::Teleport host_{};bool bound_{},arrived_{};
};
}
