#include "client/hooks/bootflow/omega_native_readable.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace memory = dawn::client::hooks::bootflow::omega_native_memory;
unsigned checks{};
void check(bool value, const char* message) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL %s\n", message); std::exit(1); }
}
int main() {
    SYSTEM_INFO info{}; GetSystemInfo(&info); const auto page = info.dwPageSize;
    auto* data = static_cast<std::byte*>(VirtualAlloc(nullptr, page * 32,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    check(data != nullptr, "allocate");
    check(memory::readable(data, page), "untouched committed page remains readable through fallback");
    std::memset(data, 0xA5, page * 32);
    check(memory::readable(data + page - 1, 2), "checks both pages of unaligned read");
    check(memory::readable(data, page * 32), "large span fallback");
    check(!memory::readable(nullptr, 1), "null rejected");
    check(!memory::readable(data, 0), "empty span rejected");
    check(!memory::readable(reinterpret_cast<void*>(UINTPTR_MAX - 2), 8), "overflow rejected");
    DWORD previous{};
    check(VirtualProtect(data + page, page, PAGE_NOACCESS, &previous) != 0, "protect no access");
    check(!memory::readable(data + page, 1), "no access rejected");
    check(!memory::readable(data + page - 1, 2), "crossing into no access rejected");
    check(!memory::readable(data, page * 32), "large span rejects an inaccessible middle region");
    check(VirtualProtect(data + page, page, PAGE_READWRITE | PAGE_GUARD, &previous) != 0, "protect guard");
    check(!memory::readable(data + page, 1), "guard rejected without touching");
    check(!memory::readable(data + page - 1, 2), "crossing into guard rejected");
    check(!memory::readable(data, page * 32), "large span rejects a guarded middle region without touching it");
    MEMORY_BASIC_INFORMATION region{}; VirtualQuery(data + page, &region, sizeof region);
    check((region.Protect & PAGE_GUARD) != 0, "guard not consumed");
    check(VirtualProtect(data + page, page, PAGE_READONLY, &previous) != 0, "protect read only");
    check(memory::readable(data + page, 1), "read only accepted");
    check(memory::readable(data, page * 32), "large readable table may cross regions with different readable protection");
    check(VirtualFree(data + page, page, MEM_DECOMMIT) != 0, "decommit");
    check(!memory::readable(data + page, 1), "decommitted rejected, no cached permissions");
    check(!memory::readable(data + page - 1, 2), "crossing into decommitted page rejected");
    check(!memory::readable(data, page * 32), "large span rejects a decommitted middle region");
    check(VirtualFree(data, 0, MEM_RELEASE) != 0, "release");
    check(!memory::readable(data, 1), "released address rejected");
    // Same-process check against a large resident allocation, matching the
    // live region-walk bottleneck. Timings are evidence, not a flaky threshold.
    constexpr std::size_t bytes = 128 * 1024 * 1024;
    data = static_cast<std::byte*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    check(data != nullptr, "benchmark allocate"); std::memset(data, 1, bytes);
    LARGE_INTEGER frequency{}, start{}, end{}; QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    for (unsigned i = 0; i < 1000; ++i) check(memory::region_readable(data, 8), "baseline read");
    QueryPerformanceCounter(&end); const double baseline = double(end.QuadPart - start.QuadPart) * 1000 / frequency.QuadPart;
    QueryPerformanceCounter(&start);
    for (unsigned i = 0; i < 1000; ++i) check(memory::readable(data, 8), "bounded read");
    QueryPerformanceCounter(&end); const double bounded = double(end.QuadPart - start.QuadPart) * 1000 / frequency.QuadPart;
    VirtualFree(data, 0, MEM_RELEASE);
    std::printf("%u checks passed; 1000 reads in 128MiB: region_ms=%.3f bounded_ms=%.3f\n", checks, baseline, bounded);
}
