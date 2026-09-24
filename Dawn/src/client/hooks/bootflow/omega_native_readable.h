#pragma once
#include <Windows.h>
#include <Psapi.h>
#include <array>
#include <cstddef>
#include <cstdint>

namespace dawn::client::hooks::bootflow::omega_native_memory {
inline bool protection_readable(DWORD protection) noexcept {
    return !(protection & (PAGE_GUARD | PAGE_NOACCESS))
        && (protection & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY
            | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
}
inline bool region_readable(const void* pointer, std::size_t size) noexcept {
    auto first = reinterpret_cast<std::uintptr_t>(pointer);
    if (!first || !size || size - 1 > UINTPTR_MAX - first) return false;
    // A native table can span adjacent committed regions with different readable
    // protections. Validate the entire span without requiring a single region.
    while (size) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<const void*>(first), &info, sizeof info)
            || info.State != MEM_COMMIT || !protection_readable(info.Protect)) return false;
        const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        if (first < base || first - base >= info.RegionSize) return false;
        const auto available = info.RegionSize - (first - base);
        if (size <= available) return true;
        size -= available;
        first += available;
    }
    return true;
}
inline bool readable(const void* pointer, std::size_t size) noexcept {
    const auto first = reinterpret_cast<std::uintptr_t>(pointer);
    if (!first || !size || size - 1 > UINTPTR_MAX - first) return false;
    // Query only the requested pages. VirtualQuery's region walk costs ~0.7ms
    // per call in the live handle directory's ~490MB region. No protection or
    // pointer is cached: every read rechecks pages, then copy_native uses SEH.
    static const std::size_t pageSize = [] { SYSTEM_INFO info{};
        GetSystemInfo(&info); return static_cast<std::size_t>(info.dwPageSize); }();
    const auto page = first - first % pageSize;
    const auto count = (first + size - 1 - page) / pageSize + 1;
    std::array<PSAPI_WORKING_SET_EX_INFORMATION, 16> pages{};
    if (count > pages.size()) return region_readable(pointer, size);
    for (std::size_t i = 0; i < count; ++i)
        pages[i].VirtualAddress = reinterpret_cast<void*>(page + i * pageSize);
    if (!QueryWorkingSetEx(GetCurrentProcess(), pages.data(),
            static_cast<DWORD>(count * sizeof(pages[0]))))
        return region_readable(pointer, size);
    for (std::size_t i = 0; i < count; ++i) {
        const auto attributes = pages[i].VirtualAttributes;
        // Invalid working-set entries have no usable protection bits. A valid
        // committed page can be paged out; retain the original query fallback.
        if (!attributes.Valid) return region_readable(pointer, size);
        if (attributes.Bad || !protection_readable(static_cast<DWORD>(attributes.Win32Protection)))
            return false;
    }
    return true;
}
}
