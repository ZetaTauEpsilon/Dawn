#pragma once
#include "client/hooks/bootflow/omega_vex_lattice_probe.h"
#include <cstdarg>

namespace entrance_callback_test {
namespace hook=dawn::client::hooks::bootflow::omega_vex_lattice_probe::detail;
namespace adapter=dawn::client::hooks::bootflow::one_au_entrance;
namespace native=m::entrance_native;
inline m::EntranceRequest wanted{};
inline unsigned requests{},nativeCalls{},expectedRequests{};
inline bool barrierPending{};
inline std::uint8_t originalResult=0xA5;
inline std::byte* expectedDevice{};
inline const void* expectedContext{};
inline std::array<char,1024> probe{};
inline unsigned lightingScenes{},lightingSwitches{},positionCalls{};
inline bool nativeReturned{};
inline float expectedPosition{};
inline char expectedSnap{};
inline std::uint8_t __fastcall original(std::byte* device,const void* context) {
    ++nativeCalls;
    check(device==expectedDevice && context==expectedContext,"actual device hook preserves both native arguments");
    check(requests==expectedRequests,"entrance observer runs before the native device callback");
    check(hook::callGate.active_calls()==1,"repair and native forwarding share the existing hook lifetime");
    nativeReturned=true;return originalResult;
}
inline std::uint64_t __fastcall original_position(std::byte* device,float requested,char snap) {
    ++positionCalls;
    check(device==expectedDevice && requested==expectedPosition && snap==expectedSnap,
        "shared position hook preserves all native arguments");
    check(hook::callGate.active_calls()==1,"native position runs inside the hook lifetime");
    nativeReturned=true;return 0x123456789ABCDEF0ULL;
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
    originalResult=0;barrierPending=true;expectedRequests=3;
    check(hook::tick(device.data(),&context)==1 && nativeCalls==4,
        "production shared hook keeps an idle barrier callback alive while release is pending");
    barrierPending=false;expectedRequests=4;
    check(hook::tick(device.data(),&context)==0 && nativeCalls==5,
        "production hook retires a completed barrier with the original idle result");
    hook::callGate.quiesce();barrierPending=true;
    check(hook::tick(device.data(),&context)==0 && nativeCalls==6 && requests==4,
        "quiescing cannot retain native callbacks for a pending barrier");
    barrierPending=false;originalResult=0xA5;
    hook::callGate.accept();expectedRequests=5;nativeReturned=false;
    put(device.data(),0,std::array<std::uint32_t,4>{0x80FA2F0AU,0x80803910U,0xA78U,0});
    check(hook::tick(device.data(),&context)==0xA5 && nativeCalls==7 && lightingScenes==1,
        "New Light scene discovery follows native initialization without changing its result");
    hook::callGate.quiesce();nativeReturned=false;
    check(hook::tick(device.data(),&context)==0xA5 && nativeCalls==8 && lightingScenes==1,
        "quiescing skips New Light discovery and still forwards the tick");
    hook::positionOriginal.store(&original_position);hook::callGate.accept();
    expectedPosition=1.F;expectedSnap=1;nativeReturned=false;
    check(hook::position(device.data(),1.F,1)==0x123456789ABCDEF0ULL && !lightingSwitches,
        "a scene device cannot masquerade as the logical light switch");
    put(device.data(),0,std::array<std::uint32_t,4>{0x80C7069BU,0x80803910U,0xA78U,0});
    nativeReturned=false;
    check(hook::position(device.data(),1.F,1)==0x123456789ABCDEF0ULL && lightingSwitches==1,
        "accepted logical switch dispatches lighting after native position and preserves its result");
    expectedPosition=0.F;nativeReturned=false;
    check(hook::position(device.data(),0.F,1)==0x123456789ABCDEF0ULL && lightingSwitches==1,
        "falling switch edge does not dispatch lighting");
    expectedPosition=1.F;expectedSnap=0;nativeReturned=false;
    check(hook::position(device.data(),1.F,0)==0x123456789ABCDEF0ULL && lightingSwitches==1,
        "unsnapped switch does not dispatch lighting");
    hook::callGate.quiesce();expectedSnap=1;nativeReturned=false;
    check(hook::position(device.data(),1.F,1)==0x123456789ABCDEF0ULL && lightingSwitches==1 && positionCalls==5,
        "quiescing skips lighting side effects and forwards every native position exactly once");
    hook::positionOriginal.store(nullptr);
    hook::callGate.quiesce();hook::tickOriginal.store(nullptr);hook::image=0;
    check(VirtualFree(rows,0,MEM_RELEASE)!=0 && VirtualFree(image,0,MEM_RELEASE)!=0,"release synthetic allocations");
}
}
namespace dawn::state::activity::vanilla::one_au {
EntranceRequest entrance_request() noexcept {
    ++entrance_callback_test::requests;return entrance_callback_test::wanted;
}
}
namespace dawn::state::activity::vanilla::homecoming {
// The shared device tick also runs the Homecoming repair; it stays inactive here.
EntranceRequest entrance_request() noexcept { return {}; }
bool update_ship_barrier(void*) noexcept {return entrance_callback_test::barrierPending;}
}
namespace dawn::state::activity::newlight::launchpad {
void observe_native_lighting_scene(void* device) noexcept {
    using namespace entrance_callback_test;
    check(device==expectedDevice && nativeReturned,"lighting discovery uses the initialized native device");
    check(hook::callGate.active_calls()==1,"lighting discovery shares the hook lifetime");
    ++lightingScenes;
}
void apply_native_lighting_switch(void* device,float requested,char snap) noexcept {
    using namespace entrance_callback_test;
    check(device==expectedDevice && requested==1.F && snap==1 && nativeReturned,
        "lighting switch receipt follows the native setter with its exact arguments");
    check(hook::callGate.active_calls()==1,"lighting switch application shares the hook lifetime");
    ++lightingSwitches;
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
