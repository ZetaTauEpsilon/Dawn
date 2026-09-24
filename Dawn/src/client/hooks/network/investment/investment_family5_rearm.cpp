/**
 * Arms one derived-state rebuild after the family-five object commit. The account's unlock
 * overrides reach that object only when the commit finishes, so a rebuild armed any earlier
 * reads an override list that is not there yet.
 */

#include <Windows.h>
#include <array>
#include <cstring>
#include <cstdio>
#include "../../../activity/campaign_dialogue.h"
#include "../../../../state/activity/coo/campaign_dialogue.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../../../../core/logging/log.h"
#include "../../../hooking/detour.h"
#include "internal.h"

namespace {
dawn::state::activity::coo::campaign_dialogue::SelectionLease g_dialogueLease{};
}
namespace dawn::client::hooks::network::investment {
namespace {

/**
 * The family-five object commit. Its prologue alone matches a dozen sites, some of which differ
 * only in their call displacements, so the exact trailing run is what makes this unique. Every
 * relative displacement is a wildcard.
 */
constexpr std::string_view kCommitSignatureText =
    "48 89 5C 24 ? 57 48 83 EC ? 48 8B DA 48 8B F9 E8 ? ? ? ? 48 8B C8 48 8B D3 E8 ? ? ? ? 48 8D "
    "44 24 ? C6 47 50 02 A8 03 75 ?";
/** Compiled pattern bytes of the signature text above. */
constexpr auto kCommitSignature =
    signature<signature_length(kCommitSignatureText)>(kCommitSignatureText);

/** Result returned when the trampoline is gone, so no commit ran. */
constexpr std::int64_t kNoCommit = 0;

using CommitFamily5 = std::int64_t(__fastcall*)(void*, std::uint64_t*);

hooking::detour::Handle g_handle{};
std::atomic<CommitFamily5> g_original{nullptr};
std::atomic_bool g_reportedArm{false};

/**
 * Runs the family-five commit, then arms one derived-state rebuild. The two callers pass different
 * second arguments, so it is passed on unread. Arming twice is harmless, and the next freshness
 * verdict uses it up, so repeat commits need no latch.
 * @param primaryRecordBlock Borrowed record block the commit writes into.
 * @param nested4 Borrowed caller-owned argument, passed on unread.
 * @return The commit's own result, or the no-commit result when the trampoline is gone.
 */
__declspec(noinline) std::int64_t __fastcall commit(void* primaryRecordBlock,
                                                    std::uint64_t* nested4) noexcept {
    const CommitFamily5 original = g_original.load(std::memory_order_acquire);
    if (original == nullptr) {
        return kNoCommit;
    }
    // Arm on the way out: the overrides are in the object only once the commit has run.
    const std::int64_t result = original(primaryRecordBlock, nested4);
    arm_derived_rebuild();
    if (!g_reportedArm.exchange(true, std::memory_order_relaxed)) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         "ev=investment stage=family5_commit result=armed");
    }
    return result;
}

} // namespace

/**
 * Attaches the family-five commit rearm.
 * @return True when the target is found and the detour attaches.
 */
bool install_family5_rearm() noexcept {
    if (g_handle.attached) {
        return true;
    }
    std::byte* const target = scan_main_image_unique(kCommitSignature, "queuez_family5_commit");
    if (target == nullptr) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=investment stage=family5_commit result=fail reason=target");
        return false;
    }
    const hooking::detour::Spec spec{target, reinterpret_cast<void*>(&commit)};
    if (!hooking::detour::install(spec, g_handle)) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=investment stage=family5_commit result=fail reason=attach");
        return false;
    }
    g_original.store(reinterpret_cast<CommitFamily5>(g_handle.original), std::memory_order_release);
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     "ev=investment stage=family5_commit result=ok");
    return true;
}

/** @return True when the family-five commit rearm is absent. */
bool uninstall_family5_rearm() noexcept {
    if (g_handle.attached && !hooking::detour::uninstall(g_handle)) {
        return false;
    }
    g_original.store(nullptr, std::memory_order_release);
    g_dialogueLease={};
    g_reportedArm.store(false, std::memory_order_release);
    return true;
}

/** @return True while the family-five commit rearm is attached. */
bool family5_rearm_is_installed() noexcept {
    return g_handle.attached;
}

} // namespace dawn::client::hooks::network::investment

namespace dawn::client::activity::campaign_dialogue {
namespace {
template<class T> bool read(std::uintptr_t address,T& out) noexcept {
    SIZE_T size{};
    return address && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),
        &out,sizeof(out),&size) && size==sizeof(out);
}
template<class F> F entry(std::uintptr_t base,std::uintptr_t rva,std::array<std::uint8_t,8> expected) noexcept {
    std::array<std::uint8_t,8> actual{};
    return read(base+rva,actual) && actual==expected ? reinterpret_cast<F>(base+rva) : nullptr;
}
}
bool select(std::int16_t activity) noexcept {
    namespace policy=state::activity::coo::campaign_dialogue;
    if (!policy::requested(activity) && !g_dialogueLease.active) { return true; }
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    using Get=std::uintptr_t(__fastcall*)();
    using Record=std::uintptr_t(__fastcall*)(std::uintptr_t);
    using Commit=void(__fastcall*)(std::uintptr_t,const void*);
    using Copy=void*(__fastcall*)(std::uintptr_t,const void*);
    using Provider=std::uintptr_t(__fastcall*)(std::uint32_t);
    using FlagValue=bool(__fastcall*)(std::uintptr_t,std::int16_t);
    // Native opcode-205 completion resolves its current family-five manager from
    // this same record key (B5E030), so no borrowed manager pointer survives frames.
    const auto record=entry<Record>(base,0xFCDF10,{0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x6C});
    const auto store=entry<Get>(base,0xE05C10,{0x48,0x8D,0x0D,0x59,0x6E,0x1B,0x01,0xE9});
    const auto object=entry<Get>(base,0xBE54A0,{0x40,0x53,0x48,0x83,0xEC,0x20,0xE8,0x65});
    const auto commit=entry<Commit>(base,0xE02590,{0x48,0x89,0x5C,0x24,0x18,0x56,0x57,0x41});
    const auto copy=entry<Copy>(base,0xBDB930,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x7C});
    const auto provider=entry<Provider>(base,0xFA7600,{0x40,0x57,0x41,0x57,0x48,0x83,0xEC,0x28});
    const auto flagValue=entry<FlagValue>(base,0x555760,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74});
    if (!record || !store || !object || !commit || !copy || !provider || !flagValue) { return false; }
    const auto manager=record(base+0x1FB75F0);
    std::uint8_t status{};
    if (!manager || !read(manager+0x50,status) || (status!=1 && status!=2)) { return false; }
    const auto owner=store(); const auto current=object();
    std::int32_t objectSize{};
    alignas(16) std::array<std::byte,0x6B0> body{};
    std::uint64_t soid{};
    if (!owner || !current || !read(owner+0xDEBF8,objectSize) || objectSize<0x6AC
        || objectSize>static_cast<std::int32_t>(body.size()) || !read(current,body)) { return false; }
    std::memcpy(&soid,body.data(),sizeof(soid));
    if (soid!=0x7FFFFFFFFFFFFFFFULL) { return false; }
    alignas(16) std::array<std::byte,0x6B0> cached{};
    std::uint64_t cachedSoid{};
    if (!read(manager+0x58,cached)) { return false; }
    std::memcpy(&cachedSoid,cached.data(),sizeof(cachedSoid));
    if (cachedSoid!=soid) { return false; }
    policy::Flags flags{}; std::memcpy(&flags,body.data()+0x7C,sizeof(flags));
    auto lease=g_dialogueLease; bool changed{};
    if (!lease.apply(flags,activity,changed)) { return false; }
    policy::Flags cachedFlags{};
    std::memcpy(&cachedFlags,cached.data()+0x7C,sizeof(cachedFlags));
    bool cacheChanged=cachedFlags.count!=flags.count;
    for (std::size_t i=0;!cacheChanged && i<flags.count;++i) {
        cacheChanged=cachedFlags.rows[i].slot!=flags.rows[i].slot || cachedFlags.rows[i].value!=flags.rows[i].value;
    }
    if (changed || cacheChanged) {
        if (changed) {
            std::memcpy(body.data()+0x7C,&flags,sizeof(flags));
            commit(owner,body.data());
        }
        std::memcpy(cached.data()+0x7C,&flags,sizeof(flags));
        // BDEA60 exposes this manager-owned copy to investment evaluators. Keep it
        // consistent with the object store, just as the native subscription tick does.
        // Preserve the cache's advancing native clock and every other field.
        copy(manager+0x58,cached.data());
        hooks::network::investment::arm_derived_rebuild();
        std::array<char,160> line{};
        const int size=std::snprintf(line.data(),line.size(),
            "ev=campaign_dialogue activity=%d veteran=%u result=native_commit",
            static_cast<int>(activity),activity==266?1U:0U);
        if (size>0 && static_cast<std::size_t>(size)<line.size()) {
            core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
        }
    }
    g_dialogueLease=lease;
    // Confirm the same evaluated flag the original dialogue condition reads.
    // An unresolved native investment accessor keeps the launch pending.
    for(const auto flag:policy::kFlags) if (const auto expected=policy::value(activity,flag)) {
        const auto accessor=provider(4);
        if (!accessor || flagValue(accessor,static_cast<std::int16_t>(flag))!=(*expected==2)) {
            hooks::network::investment::arm_derived_rebuild();
            return false;
        }
    }
    return true;
}
}
