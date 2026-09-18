#pragma once
#include <array>
#include <cstdint>
namespace dawn::state::activity::vanilla::one_au::escape_ship {
// Initialize after the live source receipt, then animate a real position change.
// Holding the initial zero command never starts movement in the client. The
// native graph owns path output F6555E56 and its .638 pickup stop; device input
// 6D408B83 must stay below the .9999 departure/fade gate. Keep ample distance
// from that gate and retain this command after arrival, without recreating the ship.
inline constexpr float kApproach=0.5F;
inline constexpr std::uint64_t kApproachMs=7500;
static_assert(kApproach>0.F && kApproach<0.9998999834F);

// 80C77DA7 action 4 ends at this authored path sample. Its output getter
// (58BDE0) reports inactive once 58FFD0 retires the curve's last writer. Retain
// the animation inputs through the existing native entity-variable setter.
inline constexpr float kPickup=0.638F;
struct Variable {std::uint32_t name;float value;};
inline constexpr Variable kPickupVariables[]{{0xB9777E43U,1.F},{0x358254E1U,1.F},{0xF6555E56U,kPickup}};

// Observe the curve itself, not elapsed host time or device position 0.5.
// A graph waiting to start has zero duration and cannot pass this gate.
template<class Read> bool at_pickup(Read& read,std::uintptr_t graph) noexcept {
    if(graph<0x10000 || graph>UINTPTR_MAX-0x300000) {return false;}
    const auto header=[&](std::uintptr_t address,std::uint32_t kind,std::uint64_t offset) {
        std::array<std::uint32_t,4> words{};
        return read.value(address,words) && words[0]==0x80C77DA7U && words[1]==kind
            && words[2]==offset && words[3]==0;
    };
    std::uint64_t count{};std::int64_t actions{},outputs{},node{};
    if(!header(graph,0x808084E9U,0xB38)
        || !read.value(graph+0xB0,count) || count!=7 || !read.value(graph+0xB8,actions)
        || actions<=0 || actions>0x100000
        || !read.value(graph+0xC0,count) || count!=4 || !read.value(graph+0xC8,outputs)
        || outputs<=0 || outputs>0x100000) {return false;}
    const auto pointer=graph+0xB8+static_cast<std::uintptr_t>(actions)+5*0x30;
    if(!read.value(pointer,node) || node<=0 || node>0x100000) {return false;}
    const auto action=pointer+static_cast<std::uintptr_t>(node);
    const auto output=graph+0xC8+static_cast<std::uintptr_t>(outputs)+0x10+2*0x40;
    float duration{};std::uint64_t remaining{};std::uint8_t phase{};std::int32_t writers{};
    std::array<float,4> value{};
    if(!header(action,0x808093C4U,0x1788) || !header(output,0x808084DFU,0x1AD0)
        || !read.value(action+0x24,duration) || duration!=7.5F
        || !read.value(action+0x28,remaining) || remaining!=0
        || !read.value(action+0x30,phase) || phase!=0
        || !read.value(output+0x20,value) || !read.value(output+0x30,writers) || writers!=0) {return false;}
    for(float lane:value) {if(lane!=kPickup) {return false;}}
    return true;
}
}
