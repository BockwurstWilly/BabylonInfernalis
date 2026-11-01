#include "indy64_game.h"
#include "../../RecompiledFuncs/recomp_overlays.inl"

#include "librecomp/overlays.hpp"

void indy64::register_overlays() {
    recomp::overlays::overlay_section_table_data_t sections {
        .code_sections = section_table,
        .num_code_sections = ARRLEN(section_table),
        .total_num_sections = num_sections,
    };

    recomp::overlays::overlays_by_index_t overlays {
        .table = overlay_sections_by_index,
        .len = ARRLEN(overlay_sections_by_index),
    };

    recomp::overlays::register_overlays(sections, overlays);
}

void indy64::register_vm_functions() {
    for (const SectionTableEntry& section : section_table) {
        if (section.ram_addr != vm_window_vram) {
            continue;
        }

        for (size_t i = 0; i < section.num_funcs; i++) {
            recomp::overlays::add_loaded_function(int32_t(section.ram_addr + section.funcs[i].offset), section.funcs[i].func);
        }
    }
}
