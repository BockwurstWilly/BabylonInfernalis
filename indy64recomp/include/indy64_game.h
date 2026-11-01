#ifndef __INDY64_GAME_H__
#define __INDY64_GAME_H__

#include <cstdint>
#include "recomp.h"

namespace recompui {
    class LauncherMenu;
}

namespace indy64 {
    // ROM location and size of the second code/data segment, mapped at virtual 0x41000000 (see NOTES.md).
    inline constexpr uint32_t vm_window_rom = 0x0004DC30;
    inline constexpr uint32_t vm_window_size = 0x000BE620;
    inline constexpr uint32_t vm_window_vram = 0x41000000;
    // libultra's __osRunningThread in the boot segment (found via the n64sym relocations).
    inline constexpr int32_t running_thread_vram = 0x40047A00;

    void register_overlays();
    // Registers the functions of the 0x41000000 segment for indirect calls (librecomp only loads the first 1 MiB).
    void register_vm_functions();
    void game_on_init(uint8_t* rdram, recomp_context* ctx);

    // Launcher background and title (launcher.cpp).
    void setup_launcher(recompui::LauncherMenu* menu);

    // crash_log.cpp: writes crash.log (fault address as N64 address, symbolized stack) on access violations.
    void install_crash_log();
    // debug_trace.cpp: INDY64_TRACE_MQ=1 logs the frame/RSP message handshake to mq_trace.log.
    void install_debug_traces();
    void set_crash_log_rdram(uint8_t* rdram);
    // Called for every audio task (INDY64_STALL_TRACE watchdog in crash_log.cpp).
    void note_audio_task();
    // INDY64_STALL_TRACE: logs all thread stacks to repeat.log (at most every 500 ms, 40 times).
    void trace_threads_now(const char* reason);

    // bug_report.cpp: session log (logs\), F9 bug reports (bugreports\) and watchers for frame rate, audio and level.
    void start_session_log();
    void session_log(const char* fmt, ...);
    void set_report_rdram(uint8_t* rdram);
    void set_report_window(void* hwnd);
    // Same as pressing F9 (used by INDY64_AUTOINPUT's REPORT).
    void request_bug_report();
    // Writes RDRAM (8 MiB, big-endian like the N64) to a file.
    void dump_ram(const char* path);
    // Counted by main.cpp's audio output: times the queue ran dry while the game was running.
    void note_audio_underrun();
};

#endif
