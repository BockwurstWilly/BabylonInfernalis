// Standalone check of librecomp's Indy64 rdram mapping: a byte written through one address view must be visible
// through the others. Mirrors allocate_rdram_mapped() in librecomp/src/recomp.cpp.
// Build: cl /std:c++20 /EHsc /O2 rdram_alias_test.cpp
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdint>
#include <cstdio>

using VirtualAlloc2_t = PVOID(WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, void*, ULONG);
using MapViewOfFile3_t = PVOID(WINAPI*)(HANDLE, HANDLE, PVOID, ULONG64, SIZE_T, ULONG, ULONG, void*, ULONG);

constexpr size_t allocation_size = 4096ULL * 1024 * 1024;
constexpr size_t mem_size = 512ULL * 1024 * 1024;

int main() {
    HMODULE kb = GetModuleHandleW(L"kernelbase.dll");
    auto va2 = reinterpret_cast<VirtualAlloc2_t>(GetProcAddress(kb, "VirtualAlloc2"));
    auto mv3 = reinterpret_cast<MapViewOfFile3_t>(GetProcAddress(kb, "MapViewOfFile3"));
    if (!va2 || !mv3) { puts("APIs missing"); return 1; }

    auto* base = static_cast<uint8_t*>(va2(nullptr, nullptr, allocation_size, MEM_RESERVE | 0x00040000, PAGE_NOACCESS, nullptr, 0));
    if (!base) { printf("reserve failed %lu\n", GetLastError()); return 1; }

    struct View { size_t off, sec, size; } views[] = {
        { 0, 0, mem_size },                        // 0x80000000 kseg0
        { 0xC0000000, 0, 0x01000000 },             // 0x40000000 -> phys 0
        { 0xC1000000, 0x00C00000, 0x00100000 },    // 0x41000000 -> phys 0xC00000
    };
    for (auto& v : views) {
        if (!VirtualFree(base + v.off, v.size, MEM_RELEASE | 0x00000002)) { printf("split %zx failed %lu\n", v.off, GetLastError()); return 1; }
    }

    HANDLE sec = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD)mem_size, nullptr);
    if (!sec) { printf("section failed %lu\n", GetLastError()); return 1; }
    for (auto& v : views) {
        if (!mv3(sec, GetCurrentProcess(), base + v.off, v.sec, v.size, 0x00004000, PAGE_READWRITE, nullptr, 0)) {
            printf("map %zx failed %lu\n", v.off, GetLastError()); return 1;
        }
    }

    auto at = [&](uint32_t addr) { return base + size_t(uint32_t(addr - 0x80000000u)); };
    *at(0x40000400) = 0x11;               // game writes through its TLB address
    *at(0x80C00010) = 0x22;               // runtime fills the VM window through kseg0
    printf("0x80000400 sees 0x%02X (expect 11)\n", *at(0x80000400));
    printf("0x41000010 sees 0x%02X (expect 22)\n", *at(0x41000010));
    printf("0x40C00010 sees 0x%02X (expect 22)\n", *at(0x40C00010));
    __try { volatile uint8_t x = *at(0x30000000); (void)x; puts("unmapped read did NOT fault"); }
    __except (EXCEPTION_EXECUTE_HANDLER) { puts("unmapped address faults as expected"); }
    const bool ok = *at(0x80000400) == 0x11 && *at(0x41000010) == 0x22 && *at(0x40C00010) == 0x22;
    puts(ok ? "OK" : "FAIL");
    return ok ? 0 : 1;
}
