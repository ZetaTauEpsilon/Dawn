#pragma once
#include "../../../state/activity/vanilla/homecoming/entrance_native.h"
#include "../../../state/activity/vanilla/homecoming/console_native.h"
#include "../../../core/logging/log.h"
#include "native_authority_bitmap.h"
#include "omega_native_readable.h"
#include <atomic>
#include <cstdio>

namespace dawn::client::hooks::bootflow::homecoming_entrance {
namespace hc=state::activity::vanilla::homecoming;
namespace entrance=hc::entrance_native;
namespace console=hc::console_native;
namespace gn=gateway_native;
inline std::atomic_flag busy=ATOMIC_FLAG_INIT;
// A validated table view avoids thousands of process-memory queries per device
// callback. SEH still guards each access if an allocation disappears mid-scan.
inline bool copy(const void* source,void* out,std::size_t bytes) noexcept {
    __try { std::memcpy(out,source,bytes);return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
struct World {
    std::uintptr_t base{},table{};
    bool stable() const noexcept {
        gn::Read read{base};std::uintptr_t now{};std::uint32_t stride{};
        return read.value(base+entrance::kEntities,now) && now==table
            && read.value(base+entrance::kEntityStride,stride) && stride==entrance::kStride;
    }
    bool row(std::size_t slot,entrance::Row& out) const noexcept {
        std::array<std::byte,0x98> bytes{};
        if(slot>=entrance::kRows || !copy(reinterpret_cast<const void*>(table+slot*entrance::kStride),bytes.data(),bytes.size())) return false;
        out={gn::at<std::uint32_t>(bytes.data()+4),gn::at<std::uint32_t>(bytes.data()+0xC),
            gn::at<std::uint32_t>(bytes.data()+0x4C),gn::at<std::uint32_t>(bytes.data()+0x88),
            gn::at<std::uint32_t>(bytes.data()+0x8C),gn::at<std::uint64_t>(bytes.data()+0x90)};
        return true;
    }
    bool owned(std::uint32_t entity,bool& out) const noexcept {
        std::uint32_t word{};
        if(!copy(reinterpret_cast<const void*>(base+console::kAuthorityTable+4*((entity&0x1FFFU)>>5U)),&word,sizeof word)) return false;
        out=(word&(1U<<(entity&31U)))!=0;return true;
    }
    void grant(std::uint32_t entity) const noexcept {
        reinterpret_cast<void(__fastcall*)(std::uint32_t,std::uint8_t) noexcept>(base+console::kAuthoritySetter)(entity,1U);
    }
};
// Serialized by busy; report transitions and a bounded heartbeat so a failed
// grant is distinguishable from a callback that never ran. No identities or
// native pointers are retained for later mutation.
inline void report(const hc::EntranceRequest& wanted,std::string_view status,
            std::uint32_t callbackEntity,const entrance::Result& result={}) noexcept {
    static hc::EntranceRequest previous{};
    static std::string_view previousStatus;
    static ULONGLONG next{};
    const auto now=GetTickCount64();
    if(wanted==previous && status==previousStatus && now<next && !result.doors) return;
    previous=wanted;previousStatus=status;next=now+10000;
    core::log::writef(core::log::Channel::client,core::log::Level::info,
        "ev=homecoming stage=entrance_probe status=%.*s run=%llu generation=%u section=%u callback=%08X rows_checked=%u matches=%u doors=%u",
        static_cast<int>(status.size()),status.data(),static_cast<unsigned long long>(wanted.owner.run),
        wanted.generation,static_cast<unsigned>(wanted.section),callbackEntity,result.rows,result.matches,result.doors);
}
__declspec(noinline) inline void update(std::uintptr_t image,void* component) noexcept {
    const auto wanted=hc::entrance_request();
    if(!component || busy.test_and_set(std::memory_order_acquire)) return;
    struct Unlock { ~Unlock(){busy.clear(std::memory_order_release);} } unlock;
    if(!wanted.enabled()) {report(wanted,"inactive",UINT32_MAX);return;}
    World world{image};gn::Read read{world.base};
    std::uint32_t stride{},callbackEntity{};std::array<unsigned char,16> setter{};
    if(!read.value(world.base+entrance::kEntities,world.table) || world.table<0x10000
        || !read.value(world.base+entrance::kEntityStride,stride) || stride!=entrance::kStride) {
        report(wanted,"table_unavailable",UINT32_MAX);return;
    }
    if(!read.value(reinterpret_cast<std::uintptr_t>(component)+0x2C,callbackEntity)) {
        report(wanted,"callback_unreadable",UINT32_MAX);return;
    }
    if(!read.value(world.base+console::kAuthoritySetter,setter) || setter!=console::kSetterPrefix) {
        report(wanted,"signature_mismatch",callbackEntity);return;
    }
    if(!omega_native_memory::readable(reinterpret_cast<const void*>(world.table),entrance::kRows*entrance::kStride)
        || !omega_native_memory::readable(reinterpret_cast<const void*>(image+console::kAuthorityTable),native_authority_bitmap::View::kBytes)) {
        report(wanted,"table_unreadable",callbackEntity);return;
    }
    const auto result=entrance::repair(world,wanted,[] {return hc::entrance_request();});
    report(wanted,"scanned",callbackEntity,result);
    if(result.doors) {
        std::array<char,224> line{};
        const int n=std::snprintf(line.data(),line.size(),"ev=homecoming stage=entrance_repair run=%llu generation=%u section=%u doors=%u",
            static_cast<unsigned long long>(wanted.owner.run),wanted.generation,static_cast<unsigned>(wanted.section),result.doors);
        if(n>0 && static_cast<std::size_t>(n)<line.size())
            core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});
    }
}
}
