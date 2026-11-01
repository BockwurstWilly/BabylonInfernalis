/* Indy64: minimal host for mupen64plus-rsp-hle's MusyX v2 task (musyx.c, GPL-2.0+, see LICENSES).
   The recomp RDRAM and librecomp's DMEM use the same byte order as mupen64plus (address ^ 3), so
   both buffers are handed to the HLE code as they are. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hle_external.h"
#include "hle_internal.h"

void musyx_v2_task(struct hle_t* hle);

static void print_message(const char* kind, const char* message, va_list args)
{
    static int count = 0;
    if (count++ < 50) {
        fprintf(stderr, "[audio-hle] %s: ", kind);
        vfprintf(stderr, message, args);
        fputc('\n', stderr);
    }
}

void HleVerboseMessage(void* user_defined, const char* message, ...) { (void)user_defined; (void)message; }

void HleInfoMessage(void* user_defined, const char* message, ...)
{
    va_list args;
    (void)user_defined;
    va_start(args, message);
    print_message("info", message, args);
    va_end(args);
}

void HleErrorMessage(void* user_defined, const char* message, ...)
{
    va_list args;
    (void)user_defined;
    va_start(args, message);
    print_message("error", message, args);
    va_end(args);
}

void HleWarnMessage(void* user_defined, const char* message, ...)
{
    va_list args;
    (void)user_defined;
    va_start(args, message);
    print_message("warn", message, args);
    va_end(args);
}

void rsp_break(struct hle_t* hle, unsigned int setbits) { (void)hle; (void)setbits; }

/* Runs one MusyX v2 audio task; the OSTask must already be at DMEM 0xFC0. */
void indy64_musyx_hle(unsigned char* rdram, unsigned char* dmem)
{
    static struct hle_t hle;
    hle.dram = rdram;
    hle.dmem = dmem;
    musyx_v2_task(&hle);
}
