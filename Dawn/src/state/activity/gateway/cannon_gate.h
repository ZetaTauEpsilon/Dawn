#pragma once
#include <cstdint>

namespace dawn::state::activity::gateway::cannon {
struct Request final { std::uint64_t run{}; bool released{}; };
constexpr Request request(std::uint64_t selected,std::uint64_t current,bool released) noexcept {
    return selected && selected==current?Request{selected,released}:Request{};
}
// Read-only live capture: the final launcher is a placed force-volume component,
// separate from type23/2's VFX device and the two type4 cannon sources.
inline constexpr std::uint32_t kDefinition=0x80F46DB6U;
inline constexpr std::uint32_t kKind=0x80803DBCU;
inline constexpr std::int64_t kOffset=0x13E8;
constexpr bool matches(std::uint32_t definition,std::uint32_t kind,std::int64_t offset) noexcept {
    return definition==kDefinition && kind==kKind && offset==kOffset;
}
constexpr bool blocked(Request request) noexcept { return request.run!=0 && !request.released; }
}
