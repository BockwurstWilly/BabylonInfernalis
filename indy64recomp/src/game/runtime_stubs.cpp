// libultra functions the game calls that neither N64Recomp translates nor librecomp/ultramodern provide.
#include "recomp.h"

// The game only uses the field to pick a framebuffer; the runtime presents whole frames.
extern "C" void osViGetCurrentField_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0;
}

// Only used by main_thread's non-paging branch to map the 0x41000000 segment; librecomp already maps it.
extern "C" void osMapTLB_recomp(uint8_t* rdram, recomp_context* ctx) {
}

// gfx_task_start re-uploads the microcode data with a raw SP DMA after osSpTaskLoad; the runtime's task submission
// already provides the data, so the DMA is skipped and the device is never busy.
extern "C" void __osSpDeviceBusy_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0;
}

extern "C" void __osSpRawStartDma_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0;
}
