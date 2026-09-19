#pragma once
#include "client/hooks/bootflow/omega_vex_lattice_probe.h"
#include <cstdarg>

namespace entrance_callback_test {
namespace hook=dawn::client::hooks::bootflow::omega_vex_lattice_probe::detail;
namespace adapter=dawn::client::hooks::bootflow::one_au_entrance;
namespace native=m::entrance_native;
inline m::EntranceRequest wanted{};
inline unsigned requests{},nativeCalls{},expectedRequests{};
inline std::byte* expectedDevice{};
inline const void* expectedContext{};
inline std::array<char,1024> probe{};
inline std::uint8_t __fastcall original(std::byte* device,const void* context) {
    ++nativeCalls;
    check(device==expectedDevice && context==expectedContext,"actual device hook preserves both native arguments");
    check(requests==expectedRequests,"entrance observer runs before the native device callback");
    check(hook::callGate.active_calls()==1,"repair and native forwarding share the existing hook lifetime");
    return 0xA5;
}
template<class T> void put(std::byte* bytes,std::size_t offset,const T& value) {
    std::memcpy(bytes+offset,&value,sizeof value);
}
inline void run() {
    // Call the production DF7FF0 replacement and memory adapter with synthetic
    // data. The original engine callback is a stub; no game code executes.
    constexpr auto imageBytes=m::bridge_native::kAuthorityTable+4096;
    auto* image=static_cast<std::byte*>(VirtualAlloc(nullptr,imageBytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    auto* rows=static_cast<std::byte*>(VirtualAlloc(nullptr,native::kRows*native::kStride,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    check(image && rows,"allocate synthetic native image and entity table");
    for(std::size_t i=0;i<native::kRows;++i) put(rows,i*native::kStride+0xC,UINT32_MAX);
    put(image,native::kEntities,reinterpret_cast<std::uintptr_t>(rows));
    put(image,native::kEntityStride,static_cast<std::uint32_t>(native::kStride));
    put(image,m::bridge_native::kAuthoritySetter,m::bridge_native::kSetterPrefix);
    put(image,native::kRetire,native::kRetirePrefix);
    SYSTEM_INFO system{};GetSystemInfo(&system);DWORD previous{};
    check(VirtualProtect(rows+system.dwPageSize,system.dwPageSize,PAGE_READONLY,&previous)!=0,
        "split real table into adjacent readable regions");
    std::array<std::byte,64> device{};const std::uint32_t context=0x12345678;
    put(device.data(),0x2C,UINT32_MAX);
    expectedDevice=device.data();expectedContext=&context;
    hook::image=reinterpret_cast<std::uintptr_t>(image);
    hook::tickOriginal.store(&original);hook::callGate.accept();
    wanted={{7,1},1,0};requests=nativeCalls=0;expectedRequests=1;probe={};
    check(hook::tick(device.data(),&context)==0xA5,"actual hook preserves native return value");
    check(nativeCalls==1 && hook::callGate.idle(),"actual hook calls native exactly once and releases lifetime");
    check(std::strstr(probe.data(),"status=scanned") && std::strstr(probe.data(),"rows_checked=8192"),
        "non-lattice device with no owner reaches complete entrance scan before native tick");
    hook::callGate.quiesce();probe={};
    check(hook::tick(device.data(),&context)==0xA5 && requests==1 && nativeCalls==2 && !probe[0],
        "quiescing stops all entrance work while preserving native forwarding");
    hook::callGate.accept();wanted={};expectedRequests=2;
    check(hook::tick(device.data(),&context)==0xA5 && requests==2 && nativeCalls==3,
        "other missions retain native callback behavior");
    check(std::strstr(probe.data(),"status=inactive")!=nullptr,"inactive mission reports a diagnostic without scanning");
    hook::callGate.quiesce();hook::tickOriginal.store(nullptr);hook::image=0;
    check(VirtualFree(rows,0,MEM_RELEASE)!=0 && VirtualFree(image,0,MEM_RELEASE)!=0,"release synthetic allocations");
}
}
namespace dawn::state::activity::vanilla::one_au {
EntranceRequest entrance_request() noexcept {
    ++entrance_callback_test::requests;return entrance_callback_test::wanted;
}
}
namespace dawn::core::log {
void write(Channel,Level,std::string_view) noexcept {}
void writef(Channel,Level,const char* format,...) noexcept {
    va_list args;va_start(args,format);
    std::vsnprintf(entrance_callback_test::probe.data(),entrance_callback_test::probe.size(),format,args);
    va_end(args);
}
}
