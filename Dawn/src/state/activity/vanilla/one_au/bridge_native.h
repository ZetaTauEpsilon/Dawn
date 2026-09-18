#pragma once
#include "bridge_scan.h"
#include "../../../../client/hooks/bootflow/gateway_native_read.h"
#include <cmath>

namespace dawn::state::activity::vanilla::one_au::bridge_native {
namespace gn=client::hooks::bootflow::gateway_native;
inline constexpr std::uintptr_t kAuthorityTable=0x26BE0E0U,kAuthoritySetter=0x403BD0U;
inline constexpr std::array<unsigned char,16> kSetterPrefix{
    0x81,0xE1,0xFF,0x1F,0x00,0x00,0x4C,0x8D,0x05,0x03,0xA5,0x2B,0x02,0x8B,0xC1,0x83};
struct Identity {
    gn::Weak controller{},entity{};
    std::uintptr_t address{},row{};
    friend bool operator==(const Identity&,const Identity&)=default;
};
template<class Read>
bool capture(Read& read,std::uintptr_t sensor,const BridgeScanRequest& wanted,Identity& out) noexcept {
    gn::Ref ref{},controllerRef{};Identity found{};std::uintptr_t definition{};
    std::uint32_t registry{},sourceGeneration{},selector{},self{},owner{},actual{},flags{},revision{};
    std::uint16_t type{},slot{};std::uint8_t enabled{},mode{},active{};float duration{},elapsed{};
    if(!wanted.enabled() || !read.value(sensor,ref)
        || ref.handle!=kBridgeLink.definition || ref.kind!=0x80804D32U || ref.offset!=0x258
        || !read.resolve(ref.handle,definition) || !read.value(definition+0x288,registry) || registry!=kBridgeLink.registry
        || !read.value(definition+0x28C,type) || type!=kBridgeLink.type
        || !read.value(definition+0x28E,slot) || slot!=kBridgeLink.slot
        || !read.value(sensor+0x1C0,sourceGeneration) || sourceGeneration!=wanted.generation
        || !read.value(sensor+0x1C4,enabled) || enabled!=1
        || !read.value(sensor+0x1C8,selector) || selector!=0x811C9DC5U
        || !read.value(sensor+0x1D8,found.controller) || !read.weak(found.controller)
        || !read.resolve(found.controller.handle,found.address) || !read.value(found.address,controllerRef)
        || controllerRef.handle!=0x80C3D38CU || controllerRef.kind!=0x80804D3AU || controllerRef.offset!=0x358
        || !read.value(found.address+0x24,self) || self!=found.controller.handle
        || !read.value(found.address+0x2C,owner) || !read.make_weak(owner,found.entity)
        || !read.entity_row(found.entity,found.row) || !read.value(found.row+0xC,actual) || actual!=owner
        || !read.value(found.row+4,flags) || (flags&5U)
        || !read.value(found.address+0x294,revision) || revision!=wanted.generation
        || !read.value(found.address+0x298,mode) || mode!=2
        || !read.value(found.address+0x299,active) || active>1
        || !read.value(found.address+0x290,duration) || duration!=3.F
        || !read.value(found.address+0x29C,elapsed) || !std::isfinite(elapsed) || elapsed<0.F || elapsed>=duration) { return false; }
    out=found;return true;
}
enum class Result { ignored,local,granted };
template<class Read,class Current,class Setter>
Result retain(Read& read,std::uintptr_t image,std::uintptr_t sensor,const BridgeScanRequest& wanted,
              Current&& current,Setter&& setter,Identity& out) noexcept {
    Identity native{},again{};std::uint32_t word{};std::array<unsigned char,16> prefix{};
    if(!capture(read,sensor,wanted,native)) { return Result::ignored; }
    const auto address=image+kAuthorityTable+4U*((native.entity.handle&0x1FFFU)>>5U);
    const auto bit=1U<<(native.entity.handle&31U);
    if(!read.value(address,word)) { return Result::ignored; }
    if(word&bit) { return Result::local; }
    if(!read.value(image+kAuthoritySetter,prefix) || prefix!=kSetterPrefix
        || !capture(read,sensor,wanted,again) || again!=native || current()!=wanted) { return Result::ignored; }
    // Native E50740 refuses to register the player's Ghost without authority
    // on this controller's owner. Use the verified engine setter for this exact
    // live owner. Normal player interaction, participants, timing and type-65
    // completion receipts remain entirely native.
    setter(native.entity.handle);
    if(!read.value(address,word) || !(word&bit)) { return Result::ignored; }
    out=native;return Result::granted;
}
}
