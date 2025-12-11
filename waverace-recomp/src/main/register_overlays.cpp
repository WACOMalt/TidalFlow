#include "ovl_patches.hpp"
#include "librecomp/overlays.hpp"
#include "librecomp/sections.h"

// Include the generated overlay data
#include "../RecompiledFuncs/recomp_overlays.inl"

void zelda64::register_overlays() {
    // Register the main code section - Wave Race 64 has no dynamic overlays
    recomp::overlays::overlay_section_table_data_t sections {
        .code_sections = section_table,
        .num_code_sections = num_sections,
        .total_num_sections = num_sections,
    };

    recomp::overlays::overlays_by_index_t overlays {
        .table = overlay_sections_by_index,
        .len = 0,  // No overlays
    };

    recomp::overlays::register_overlays(sections, overlays);

    // Load all functions from the main code section into func_map
    // Wave Race 64 has no overlays, so all code is in section 0
    const SectionTableEntry& main_section = section_table[0];
    int32_t base_addr = main_section.ram_addr;

    for (size_t i = 0; i < main_section.num_funcs; i++) {
        const FuncEntry& func = main_section.funcs[i];
        recomp::overlays::add_loaded_function(base_addr + func.offset, func.func);
    }
}
