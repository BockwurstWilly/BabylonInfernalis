// Writes crash.log on access violations: faulting address (also as N64 address when it lies in rdram) and the call
// stack with symbols from the PDB. The recompiled functions are named after the game's functions (FUN_4000xxxx).
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>

#include "indy64_game.h"
#include "ultramodern/ultramodern.hpp"

#pragma comment(lib, "dbghelp.lib")

namespace {
    std::atomic<uint8_t*> rdram_base{ nullptr };
    // Milliseconds since process start of the last audio task (0: none yet), for INDY64_STALL_TRACE.
    std::atomic<int64_t> last_audio_task_ms{ 0 };
    const auto process_start = std::chrono::steady_clock::now();
    std::atomic<bool> crashed{ false };

    LONG CALLBACK on_exception(EXCEPTION_POINTERS* info) {
        const DWORD code = info->ExceptionRecord->ExceptionCode;
        if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_INT_DIVIDE_BY_ZERO &&
            code != EXCEPTION_BREAKPOINT) {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        if (crashed.exchange(true)) {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        FILE* f = fopen("crash.log", "w");
        if (f == nullptr) {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        const uintptr_t pc = uintptr_t(info->ExceptionRecord->ExceptionAddress);
        fprintf(f, "exception 0x%08lX at 0x%016" PRIXPTR "\n", code, pc);
        if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2) {
            const uintptr_t target = uintptr_t(info->ExceptionRecord->ExceptionInformation[1]);
            const char* kind = info->ExceptionRecord->ExceptionInformation[0] == 1 ? "write" : "read";
            fprintf(f, "%s of 0x%016" PRIXPTR "\n", kind, target);
            uint8_t* base = rdram_base.load();
            if (base != nullptr && target >= uintptr_t(base) && target - uintptr_t(base) < 0x100000000ULL) {
                const uint32_t n64 = uint32_t(target - uintptr_t(base)) + 0x80000000u;
                fprintf(f, "  = N64 address 0x%08X (rdram offset 0x%08X)\n", n64, uint32_t(target - uintptr_t(base)));
            }
        }

        // Indy64: the game's task objects (list at 0x40056B94, next at +0x1C, OSThread at +0x48) and libultra's
        // __osRunningThread, which FUN_40005f30 compares to find the current task.
        if (uint8_t* base = rdram_base.load()) {
            auto word = [base](uint32_t vaddr) { return *reinterpret_cast<uint32_t*>(base + uint32_t(vaddr - 0x80000000u)); };
            const uint32_t running = word(0x40047A00);
            const uint32_t self = uint32_t(ultramodern::this_thread());
            fprintf(f, "__osRunningThread = %08X (id %d), this_thread = %08X (id %d)\n", running,
                running ? int(word(running + 0x14)) : -1, self, self ? int(word(self + 0x14)) : -1);
            uint32_t task = word(0x40056B94);
            for (int i = 0; i < 32 && task != 0; i++) {
                fprintf(f, "  task %08X id %d thread %08X entry %08X\n", task, int(word(task)), task + 0x48, word(task + 0x18));
                task = word(task + 0x1C);
            }
        }

        const CONTEXT* c = info->ContextRecord;
        fprintf(f, "rax=%016llX rbx=%016llX rcx=%016llX rdx=%016llX\nrsi=%016llX rdi=%016llX r8=%016llX r9=%016llX\n",
            c->Rax, c->Rbx, c->Rcx, c->Rdx, c->Rsi, c->Rdi, c->R8, c->R9);

        HANDLE process = GetCurrentProcess();
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
        SymInitialize(process, nullptr, TRUE);

        CONTEXT context = *c;
        STACKFRAME64 frame{};
        frame.AddrPC.Offset = context.Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrStack.Mode = AddrModeFlat;

        alignas(SYMBOL_INFO) char symbol_buffer[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer);
        fprintf(f, "stack:\n");
        for (int i = 0; i < 48; i++) {
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, &context, nullptr,
                    SymFunctionTableAccess64, SymGetModuleBase64, nullptr) || frame.AddrPC.Offset == 0) {
                break;
            }

            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = 255;
            DWORD64 displacement = 0;
            IMAGEHLP_LINE64 line{ sizeof(IMAGEHLP_LINE64) };
            DWORD line_displacement = 0;
            const bool has_symbol = SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol);
            const bool has_line = SymGetLineFromAddr64(process, frame.AddrPC.Offset, &line_displacement, &line);
            fprintf(f, "  %2d %016llX %s+0x%llX %s:%lu\n", i, frame.AddrPC.Offset, has_symbol ? symbol->Name : "?",
                displacement, has_line ? line.FileName : "", has_line ? line.LineNumber : 0);
        }

        fclose(f);
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

#include <TlHelp32.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {
    std::mutex dbghelp_mutex;

    // Writes the top frames of every other thread of this process (debugging aid for hangs).
    void sample_threads(FILE* f) {
        std::lock_guard lock{ dbghelp_mutex };
        HANDLE process = GetCurrentProcess();
        const DWORD self = GetCurrentThreadId();
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        THREADENTRY32 entry{ sizeof(THREADENTRY32) };
        for (BOOL ok = Thread32First(snapshot, &entry); ok; ok = Thread32Next(snapshot, &entry)) {
            if (entry.th32OwnerProcessID != GetCurrentProcessId() || entry.th32ThreadID == self) {
                continue;
            }

            HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
            if (thread == nullptr) {
                continue;
            }

            if (SuspendThread(thread) != DWORD(-1)) {
                CONTEXT context{};
                context.ContextFlags = CONTEXT_FULL;
                if (GetThreadContext(thread, &context)) {
                    PWSTR description = nullptr;
                    char name[128] = "";
                    if (SUCCEEDED(GetThreadDescription(thread, &description)) && description != nullptr) {
                        WideCharToMultiByte(CP_UTF8, 0, description, -1, name, sizeof(name), nullptr, nullptr);
                        LocalFree(description);
                    }

                    fprintf(f, "thread %lu %s:", entry.th32ThreadID, name);
                    STACKFRAME64 frame{};
                    frame.AddrPC.Offset = context.Rip;
                    frame.AddrPC.Mode = AddrModeFlat;
                    frame.AddrFrame.Offset = context.Rbp;
                    frame.AddrFrame.Mode = AddrModeFlat;
                    frame.AddrStack.Offset = context.Rsp;
                    frame.AddrStack.Mode = AddrModeFlat;
                    alignas(SYMBOL_INFO) char symbol_buffer[sizeof(SYMBOL_INFO) + 256];
                    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer);
                    for (int i = 0; i < 12; i++) {
                        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, nullptr,
                                SymFunctionTableAccess64, SymGetModuleBase64, nullptr) || frame.AddrPC.Offset == 0) {
                            break;
                        }
                        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                        symbol->MaxNameLen = 255;
                        DWORD64 displacement = 0;
                        fprintf(f, " %s", SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol) ? symbol->Name : "?");
                    }
                    fprintf(f, "\n");
                }
                ResumeThread(thread);
            }
            CloseHandle(thread);
        }
        CloseHandle(snapshot);
        if (uint8_t* rdram = rdram_base.load()) {
            fprintf(f, "message queue waits:\n");
            ultramodern::debug_dump_waits(rdram, f);
        }
        fprintf(f, "\n");
        fflush(f);
    }
}

void indy64::install_crash_log() {
    AddVectoredExceptionHandler(1, on_exception);

    // INDY64_STALL_TRACE=1: whenever no audio task has run for 60 ms while the game runs, log all thread stacks to
    // stall.log once per stall (finds game code that keeps the audio thread from running).
    if (getenv("INDY64_STALL_TRACE") != nullptr) {
        std::thread([]() {
            SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
            SymInitialize(GetCurrentProcess(), nullptr, TRUE);
            FILE* f = fopen("stall.log", "w");
            bool in_stall = false;
            for (int n = 0; f != nullptr && n < 60; ) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - process_start).count();
                const int64_t last = last_audio_task_ms.load();
                if (last == 0) {
                    continue;
                }
                const bool stalled = (now - last) > 60;
                if (stalled && !in_stall) {
                    fprintf(f, "=== stall at %lld ms (last audio task %lld ms)\n", (long long)now, (long long)last);
                    sample_threads(f);
                    fflush(f);
                    n++;
                }
                in_stall = stalled;
            }
        }).detach();
    }

    // INDY64_SAMPLE=<seconds>: log all thread stacks to sample.log periodically.
    if (const char* interval = getenv("INDY64_SAMPLE")) {
        const int seconds = std::max(1, atoi(interval));
        std::thread([seconds]() {
            SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
            SymInitialize(GetCurrentProcess(), nullptr, TRUE);
            FILE* f = fopen("sample.log", "w");
            for (int n = 0; f != nullptr; n++) {
                std::this_thread::sleep_for(std::chrono::seconds(seconds));
                fprintf(f, "=== sample %d (%d s)\n", n, (n + 1) * seconds);
                sample_threads(f);
            }
        }).detach();
    }
}

namespace {
    // INDY64_WATCH=<physical address>: logs to watch.log which code writes the 4th row (bytes 0x18-0x1F and
    // 0x38-0x3F of each 0x40 block) of the fixed-point matrices in that page. Uses guard pages on the kseg0 and the
    // 0x40000000 views and re-arms them after a single step. Debugging aid for the F5 matrix pool at 0x700000.
    uint32_t watch_page = 0;
    thread_local uint8_t* watch_rearm = nullptr;
    FILE* watch_file = nullptr;
    uintptr_t watch_seen[64] = {};
    int watch_seen_count = 0;

    LONG CALLBACK on_watch(EXCEPTION_POINTERS* info) {
        const DWORD code = info->ExceptionRecord->ExceptionCode;
        if (code == EXCEPTION_SINGLE_STEP && watch_rearm != nullptr) {
            DWORD old;
            VirtualProtect(watch_rearm, 0x1000, PAGE_READWRITE | PAGE_GUARD, &old);
            watch_rearm = nullptr;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (code != STATUS_GUARD_PAGE_VIOLATION || info->ExceptionRecord->NumberParameters < 2) {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        uint8_t* base = rdram_base.load();
        const uintptr_t target = uintptr_t(info->ExceptionRecord->ExceptionInformation[1]);
        const uint32_t offset = uint32_t(target - uintptr_t(base));
        const uint32_t physical = offset & 0x3FFFFFFF;
        if ((physical & ~0xFFFu) != watch_page) {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        watch_rearm = reinterpret_cast<uint8_t*>(target & ~uintptr_t(0xFFF));
        info->ContextRecord->EFlags |= 0x100;

        const bool write = info->ExceptionRecord->ExceptionInformation[0] == 1;
        const uint32_t in_block = physical & 0x3F;
        const bool all_rows = getenv("INDY64_WATCH_ALL") != nullptr;
        // INDY64_WATCH_RANGE=<start hex>,<end hex>: only writes inside this physical range count.
        static uint32_t range_start = 0;
        static uint32_t range_end = 0;
        static bool range_parsed = false;
        if (!range_parsed) {
            range_parsed = true;
            if (const char* range = getenv("INDY64_WATCH_RANGE")) {
                char* comma = nullptr;
                range_start = uint32_t(strtoul(range, &comma, 16));
                range_end = (comma && *comma == ',') ? uint32_t(strtoul(comma + 1, nullptr, 16)) : range_start + 4;
            }
        }
        const bool in_range = (range_end != 0) ? ((physical >= range_start) && (physical < range_end))
            : (all_rows ? (physical & 0xFFF) < 0x100 : ((in_block >= 0x18 && in_block < 0x20) || in_block >= 0x38));
        if (write && in_range) {
            const uintptr_t pc = uintptr_t(info->ExceptionRecord->ExceptionAddress);
            std::lock_guard lock{ dbghelp_mutex };
            bool known = false;
            static const bool log_every = getenv("INDY64_WATCH_EVERY") != nullptr;
            for (int i = 0; i < watch_seen_count && !log_every; i++) {
                known |= (watch_seen[i] == pc);
            }
            if (!known && watch_seen_count < 64 && watch_file != nullptr) {
                watch_seen[watch_seen_count++] = pc;
                alignas(SYMBOL_INFO) char symbol_buffer[sizeof(SYMBOL_INFO) + 256];
                SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer);
                symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                symbol->MaxNameLen = 255;
                DWORD64 displacement = 0;
                IMAGEHLP_LINE64 line{ sizeof(IMAGEHLP_LINE64) };
                DWORD line_displacement = 0;
                const bool has_symbol = SymFromAddr(GetCurrentProcess(), pc, &displacement, symbol);
                const bool has_line = SymGetLineFromAddr64(GetCurrentProcess(), pc, &line_displacement, &line);
                fprintf(watch_file, "write of %08X by %s+0x%llX %s:%lu (thread %lu, old %08X)\n", physical, has_symbol ? symbol->Name : "?",
                    displacement, has_line ? line.FileName : "", has_line ? line.LineNumber : 0, GetCurrentThreadId(),
                    _byteswap_ulong(*reinterpret_cast<const uint32_t*>(target & ~uintptr_t(3))) );
                fflush(watch_file);
            }
        }
        return EXCEPTION_CONTINUE_EXECUTION;
    }
}

namespace {
    // INDY64_HWWATCH=<physical hex>: hardware write breakpoints (debug registers DR0/DR1) on that word in the kseg0
    // and 0x40000000 views of every thread. Logs each write with the code location and the new value to hwwatch.log.
    uintptr_t hw_addr[2] = {};
    FILE* hw_file = nullptr;
    int hw_count = 0;

    LONG CALLBACK on_hw_watch(EXCEPTION_POINTERS* info) {
        if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        CONTEXT* c = info->ContextRecord;
        if ((c->Dr6 & 3) == 0) {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        const int which = (c->Dr6 & 1) ? 0 : 1;
        c->Dr6 = 0;
        if (hw_file != nullptr && hw_count < 400) {
            hw_count++;
            std::lock_guard lock{ dbghelp_mutex };
            const uintptr_t pc = uintptr_t(info->ExceptionRecord->ExceptionAddress);
            alignas(SYMBOL_INFO) char symbol_buffer[sizeof(SYMBOL_INFO) + 256];
            SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = 255;
            DWORD64 displacement = 0;
            IMAGEHLP_LINE64 line{ sizeof(IMAGEHLP_LINE64) };
            DWORD line_displacement = 0;
            const bool has_symbol = SymFromAddr(GetCurrentProcess(), pc, &displacement, symbol);
            const bool has_line = SymGetLineFromAddr64(GetCurrentProcess(), pc, &line_displacement, &line);
            const uint32_t value = *reinterpret_cast<const uint32_t*>(hw_addr[which]);
            float as_float;
            memcpy(&as_float, &value, 4);
            fprintf(hw_file, "#%d view %d by %s+0x%llX %s:%lu -> %08X (%g)\n", hw_count, which, has_symbol ? symbol->Name : "?",
                displacement, has_line ? line.FileName : "", has_line ? line.LineNumber : 0, value, as_float);
            fflush(hw_file);
        }
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    void hw_arm_all_threads() {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        THREADENTRY32 entry{ sizeof(THREADENTRY32) };
        const DWORD self = GetCurrentThreadId();
        for (BOOL ok = Thread32First(snapshot, &entry); ok; ok = Thread32Next(snapshot, &entry)) {
            if (entry.th32OwnerProcessID != GetCurrentProcessId() || entry.th32ThreadID == self) {
                continue;
            }
            HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, entry.th32ThreadID);
            if (thread == nullptr) {
                continue;
            }
            if (SuspendThread(thread) != DWORD(-1)) {
                CONTEXT context{};
                context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                if (GetThreadContext(thread, &context)) {
                    context.Dr0 = hw_addr[0];
                    context.Dr1 = hw_addr[1];
                    // L0/L1 enabled, RW = 01 (write), LEN = 11 (4 bytes) for both.
                    context.Dr7 = (1 << 0) | (1 << 2) | (0x1 << 16) | (0x3 << 18) | (0x1 << 20) | (0x3 << 22);
                    SetThreadContext(thread, &context);
                }
                ResumeThread(thread);
            }
            CloseHandle(thread);
        }
        CloseHandle(snapshot);
    }
}

void indy64::trace_threads_now(const char* reason) {
    static FILE* f = nullptr;
    static int count = 0;
    static auto last = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    if ((getenv("INDY64_STALL_TRACE") == nullptr) || (count >= 40) || (std::chrono::steady_clock::now() - last < std::chrono::milliseconds(500))) {
        return;
    }
    last = std::chrono::steady_clock::now();
    if (f == nullptr) {
        f = fopen("repeat.log", "w");
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        SymInitialize(GetCurrentProcess(), nullptr, TRUE);
    }
    if (f != nullptr) {
        fprintf(f, "=== %s\n", reason);
        sample_threads(f);
        fflush(f);
        count++;
    }
}

void indy64::note_audio_task() {
    last_audio_task_ms = std::max<int64_t>(1, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - process_start).count());
}

void indy64::set_crash_log_rdram(uint8_t* rdram) {
    rdram_base.store(rdram);
    indy64::set_report_rdram(rdram);

    if (const char* hw = getenv("INDY64_HWWATCH")) {
        const uint32_t physical = uint32_t(strtoul(hw, nullptr, 16)) & ~3u;
        hw_addr[0] = uintptr_t(rdram + physical);
        hw_addr[1] = uintptr_t(rdram + 0xC0000000u + physical);
        hw_file = fopen("hwwatch.log", "w");
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
        SymInitialize(GetCurrentProcess(), nullptr, TRUE);
        AddVectoredExceptionHandler(1, on_hw_watch);
        // Threads come and go (game threads are created later), so re-arm periodically.
        std::thread([]() {
            for (;;) {
                hw_arm_all_threads();
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }).detach();
    }

    // INDY64_GS="<code>,<code>,...": GameShark codes, applied every millisecond. Supported: 80 (8-bit write),
    // 81 (16-bit write), D0 (8-bit equal test), D1 (16-bit equal test; both guard the next code). Example, start in
    // level 5: "D0080C37 0000,80080C37 0005".
    if (const char* gs = getenv("INDY64_GS")) {
        struct Code { uint8_t type; uint32_t addr; uint16_t value; };
        std::vector<Code> codes;
        for (const char* p = gs; *p != '\0'; ) {
            char* end = nullptr;
            const uint32_t word = uint32_t(strtoul(p, &end, 16));
            const uint16_t value = uint16_t(strtoul(end, &end, 16));
            codes.push_back({ uint8_t(word >> 24), word & 0x7FFFFF, value });
            p = (*end == ',') ? end + 1 : end;
        }
        std::thread([rdram, codes]() {
            auto byte = [rdram](uint32_t a) -> volatile uint8_t& { return *reinterpret_cast<volatile uint8_t*>(rdram + (a ^ 3)); };
            auto half = [rdram](uint32_t a) -> volatile uint16_t& { return *reinterpret_cast<volatile uint16_t*>(rdram + (a ^ 2)); };
            for (;;) {
                bool skip = false;
                for (const Code& c : codes) {
                    if (skip) { skip = false; continue; }
                    switch (c.type) {
                    case 0x80: byte(c.addr) = uint8_t(c.value); break;
                    case 0x81: half(c.addr) = c.value; break;
                    case 0xD0: skip = (byte(c.addr) != uint8_t(c.value)); break;
                    case 0xD1: skip = (half(c.addr) != c.value); break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }).detach();
    }

    // INDY64_POLL=<physical hex>,<size hex>: polls that RDRAM range every 2 ms and logs changed words to poll.log,
    // in the same format as the BizHawk script indy_thing.lua (milliseconds instead of frames).
    if (const char* poll = getenv("INDY64_POLL")) {
        char* comma = nullptr;
        const uint32_t start = uint32_t(strtoul(poll, &comma, 16)) & ~3u;
        const uint32_t size = (comma && *comma == ',') ? uint32_t(strtoul(comma + 1, nullptr, 16)) : 0x100;
        std::thread([rdram, start, size]() {
            FILE* f = fopen("poll.log", "w");
            std::vector<uint32_t> prev(size / 4, 0xDEADBEEF);
            const auto t0 = std::chrono::steady_clock::now();
            for (;;) {
                std::string line;
                char buf[32];
                for (uint32_t o = 0; o < size; o += 4) {
                    const uint32_t v = *reinterpret_cast<volatile uint32_t*>(rdram + start + o);
                    if (prev[o / 4] != v) {
                        prev[o / 4] = v;
                        snprintf(buf, sizeof(buf), " %03X=%08X", o, v);
                        line += buf;
                    }
                }
                if (!line.empty()) {
                    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                    fprintf(f, "t %lld%s\n", (long long)ms, line.c_str());
                    fflush(f);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }).detach();
    }

    if (const char* watch = getenv("INDY64_WATCH")) {
        watch_page = uint32_t(strtoul(watch, nullptr, 16)) & ~0xFFFu;
        watch_file = fopen("watch.log", "w");
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
        SymInitialize(GetCurrentProcess(), nullptr, TRUE);
        AddVectoredExceptionHandler(1, on_watch);
        DWORD old;
        VirtualProtect(rdram + watch_page, 0x1000, PAGE_READWRITE | PAGE_GUARD, &old);
        VirtualProtect(rdram + 0xC0000000u + watch_page, 0x1000, PAGE_READWRITE | PAGE_GUARD, &old);
    }
}
#else
void indy64::install_crash_log() {}
void indy64::set_crash_log_rdram(uint8_t*) {}
#endif
