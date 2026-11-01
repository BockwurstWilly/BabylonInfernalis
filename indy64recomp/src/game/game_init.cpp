#include "indy64_game.h"

#include "librecomp/addresses.hpp"
#include "librecomp/game.hpp"
#include "ultramodern/tlb_mappings.hpp"
#include "ultramodern/ultramodern.hpp"

// Runs after librecomp copied the first MiB of the ROM to the entrypoint, like IPL3 does.
void indy64::game_on_init(uint8_t* rdram, recomp_context* ctx) {
    (void)ctx;
    set_crash_log_rdram(rdram);
    install_debug_traces();

    // The game finds its current task object by comparing against libultra's __osRunningThread (FUN_40005f30).
    ultramodern::set_running_thread_address(running_thread_vram);

    // On the N64 the game pages the 0x41000000 segment in from ROM through the TLB (or DMAs it in one go).
    // Here it is copied once to its physical location, which librecomp also maps at 0x41000000.
    uint32_t physical = 0;
    for (const ultramodern::TlbMapping& mapping : ultramodern::tlb_mappings) {
        if (mapping.virtual_address == vm_window_vram) {
            physical = mapping.physical_address;
        }
    }

    const gpr kseg0_address = (gpr)(int32_t)(0x80000000u + physical);
    recomp::do_rom_read(rdram, kseg0_address, recomp::rom_base + vm_window_rom, vm_window_size);

    register_vm_functions();
}
