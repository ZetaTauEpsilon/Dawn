#pragma once
#include "../../../state/activity/gateway/cannon_gate.h"
namespace dawn::client::hooks::bootflow::gateway_cannon {
template<class Read> bool owns(Read& read,std::uintptr_t address) noexcept {
    struct Prefix { std::uint32_t definition,kind;std::int64_t offset; } prefix{};
    if(!read.value(address,prefix)
        || !state::activity::gateway::cannon::matches(prefix.definition,prefix.kind,prefix.offset)) return false;
    std::uint32_t self{},owner{};std::uintptr_t resolved{};
    return read.value(address+0x24,self) && self!=UINT32_MAX && read.resolve(self,resolved) && resolved==address
        && read.value(address+0x2C,owner) && owner!=UINT32_MAX;
}
}
