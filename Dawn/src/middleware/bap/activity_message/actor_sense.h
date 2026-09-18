#pragma once
#include <cstdint>
namespace dawn::middleware::bap::activity_message::actor_sense {
// Native 80807DA2 / nested 80807F6E. Preserve presence across sparse deltas.
struct Delta {
    std::uint32_t generation{},programRevision{},programState{},deliveryRevision{};
    std::int8_t deliveryState{-1};
    bool hasGeneration{},hasProgramRevision{},hasProgramState{},hasDeliveryRevision{},dead{};
};
template<class Reader> bool read(Reader& r,Delta& out) noexcept {
    Delta d{};std::uint64_t p{},v{};
    const auto optional=[&](unsigned width,std::uint32_t* value=nullptr,bool* known=nullptr) {
        if(!r.read(1,p)) {return false;}
        if(known) {*known=p!=0;}
        if(p && !r.read(width,v)) {return false;}
        if(p && value) {*value=static_cast<std::uint32_t>(v);}return true;
    };
    if(!optional(31,&d.generation,&d.hasGeneration) || !optional(9) || !optional(31) || !r.read(1,p)) {return false;}
    if(p && (!optional(6,&d.programState,&d.hasProgramState)
        || !optional(31,&d.programRevision,&d.hasProgramRevision) || !optional(31) || !r.skip(1))) {return false;}
    if(!r.read(1,p)) {return false;}
    if(p) {
        if(!r.read(1,p)) {return false;}
        if(p) {for(unsigned i=0;i<8;++i) {if(!optional(31)) {return false;}}}
        if(!optional(32)) {return false;}
    }
    if(!optional(31,&d.deliveryRevision,&d.hasDeliveryRevision) || !r.read(2,v)) {return false;}
    d.deliveryState=static_cast<std::int8_t>(static_cast<int>(v)-1);
    if(!optional(31) || !optional(7) || !optional(7) || !r.read(1,v) || !r.skip(1)) {return false;}
    d.dead=v!=0;out=d;return true;
}
}
