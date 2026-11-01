// Bring-up tracing of the game's frame/RSP handshake (INDY64_TRACE_MQ=1 writes mq_trace.log).
#include <cstdio>
#include <cstdlib>
#include <mutex>

#include "indy64_game.h"
#include "ultramodern/ultramodern.hpp"

namespace {
    struct TracedQueue {
        uint32_t address;
        const char* name;
    };

    // See NOTES.md: gfx_task_dispatch, FUN_4000aab4 / FUN_4000a990, vi_scheduler_thread, FUN_40021980.
    constexpr TracedQueue traced_queues[] = {
        { 0x4004FFF8, "dispatch" },   // task requests (type 1 gfx, 2 audio) and SP done (0x81)
        { 0x40050E10, "gfx_done" },   // dispatch -> main thread after a gfx task
        { 0x40050DD0, "dp_done" },    // FUN_40021980 -> main thread after DP done
        { 0x4004FFD8, "dp_event" },   // OS_EVENT_DP
        { 0x40058DC0, "frame_sync" }, // vi_scheduler -> main thread
    };

    FILE* trace_file = nullptr;
    std::mutex trace_mutex;

    uint8_t read_byte(uint8_t* rdram, uint32_t vaddr) {
        return rdram[uint32_t((vaddr ^ 3) - 0x80000000u)];
    }

    void trace(uint8_t* rdram, PTR(OSMesgQueue) mq, OSMesg msg, char kind, int priority) {
        for (const TracedQueue& q : traced_queues) {
            if (uint32_t(mq) != q.address) {
                continue;
            }

            std::lock_guard lock{ trace_mutex };
            fprintf(trace_file, "%c %-10s msg %08X pri %3d | e8b %u e8e %u e8f %u e90 %u e91 %u e92 %u e93 %u dfd %u\n",
                kind, q.name, uint32_t(msg), priority,
                read_byte(rdram, 0x40050E8B), read_byte(rdram, 0x40050E8E), read_byte(rdram, 0x40050E8F),
                read_byte(rdram, 0x40050E90), read_byte(rdram, 0x40050E91), read_byte(rdram, 0x40050E92),
                read_byte(rdram, 0x40050E93), read_byte(rdram, 0x40058DFD));
            fflush(trace_file);
        }
    }
}

void indy64::install_debug_traces() {
    if (getenv("INDY64_TRACE_MQ") == nullptr) {
        return;
    }

    trace_file = fopen("mq_trace.log", "w");
    if (trace_file != nullptr) {
        ultramodern::set_mesg_queue_trace_callback(trace);
    }
}
