#include <cstdio>
#include <fstream>
#include <ultramodern/ultramodern.hpp>
#include "recomp.h"

// Store the task pointer from osSpTaskLoad for use by osSpTaskStartGo
// The original libultra copies task to SP DMEM; game calls osSpTaskStartGo without argument
static PTR(OSTask) g_loaded_task = 0;

// Helper to swap bytes for big-endian N64
static inline uint32_t bswap32(uint32_t x) {
    return ((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) |
           ((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000);
}

// External functions from events.cpp - used to send SP/DP completion messages
// without going through RT64
void sp_complete();
void dp_complete();

extern "C" void osSpTaskLoad_recomp(uint8_t* rdram, recomp_context* ctx) {
    // Store the task pointer so osSpTaskStartGo can use it
    g_loaded_task = ctx->r4;
}

bool dump_frame = false;

// F3D GBI command opcodes
#define F3D_G_SPNOOP    0x00
#define F3D_G_DL        0x06
#define F3D_G_ENDDL     0xB8

// F3D GBI command for gSPSegment - G_MOVEWORD with G_MW_SEGMENT (0x06)
// Format: 0xBC0600SS AAAAAAAA where SS = segment << 2, A = base address
#define F3D_G_MOVEWORD 0xBC
#define G_MW_SEGMENT   0x06

// Fix invalid display list commands before sending to RT64
// Wave Race 64 builds DLs with G_DL commands pointing to invalid addresses (0x00000000, outside RAM, etc.)
// These cause RT64 to crash or hang. We walk the DL and replace bad G_DL commands with NOOPs.
//
// ALSO: Wave Race overlays use segment 8 for asset references but segment 8 is never set!
// We inject gSPSegment commands at the start of the DL to set up segment 8.
static void fix_display_list(uint8_t* rdram, uint32_t data_ptr) {
    // Convert N64 virtual address to physical
    if (data_ptr < 0x80000000 || data_ptr >= 0x80800000) {
        return;
    }

    uint32_t phys_addr = data_ptr & 0x7FFFFF;
    uint32_t* dl = (uint32_t*)(rdram + phys_addr);

    static int fix_count = 0;
    fix_count++;

    // ==============================================================
    // SEGMENT 8 FIX: Update segment 8 value IN-PLACE
    // ==============================================================
    // The original DL already has segment setup commands, including segment 8.
    // But the segment 8 value is wrong - it's set by the game to point to
    // a different location than where the assets are actually loaded.
    //
    // From DMA logs: assets loaded to 0x802310A0 (large asset block 0x678E0 bytes)
    // DL uses 0x08066180 -> offset 0x66180 into segment 8
    // Correct segment 8 base = 0x802310A0 - 0x66180 = 0x801CAF20
    //
    // The original DL sets segment 8 to 0x80316800 (wrong!)
    // We need to find and fix the segment 8 command without destroying other segments.
    //
    // DL structure (8 bytes per command):
    // +0x00: gSPSegment(0, ...)
    // +0x08: gSPSegment(1, ...)
    // +0x10: gSPSegment(2, ...)  <- needed for vertex data!
    // +0x18: gSPSegment(3, ...)
    // +0x20: gSPSegment(7, ...)
    // +0x28: gSPSegment(8, ...)  <- we need to fix this value
    // +0x30: gSPSegment(13, ...)
    // +0x38: gSPSegment(14, ...)
    // +0x40: G_DL or other commands

    // ==============================================================
    // SEGMENT 8 FIX: The game sets segment 8 to 0x80316800 but this is WRONG
    // ==============================================================
    // During boot (state 5/6), the DMA loads assets to 0x802310A0.
    // The DL uses segment 8 addresses like:
    //   - G_DL to 0x08066180 (offset 0x66180 into segment 8)
    //   - G_SETTIMG to 0x0804A460 (offset 0x4A460 into segment 8)
    //
    // If segment 8 = 0x80316800 (game's value), then:
    //   - 0x80316800 + 0x66180 = 0x8037C980 (OUTSIDE DMA range!)
    //
    // If segment 8 = 0x801CAF20 (our calculated value), then:
    //   - 0x801CAF20 + 0x66180 = 0x802310A0 (START of DMA data - correct!)
    //
    // Calculation: segment_8_base = DMA_dest - first_offset
    //              segment_8_base = 0x802310A0 - 0x66180 = 0x801CAF20
    uint32_t segment_8_base = 0x801CAF20;

    // Scan the first ~10 commands looking for the segment 8 setup
    for (int i = 0; i < 10; i++) {
        uint32_t w0 = dl[i*2];
        uint32_t w1 = dl[i*2 + 1];

        // Check if this is G_MOVEWORD for segment 8 (w0 = 0xBC002006)
        if (w0 == 0xBC002006) {
            // Only fix if different from our calculated value
            if (w1 != segment_8_base) {
                // DEBUG DISABLED: fprintf(stderr, "[DL-FIX] Frame %d: Fixing segment 8: 0x%08X -> 0x%08X\n", fix_count, w1, segment_8_base);
                dl[i*2 + 1] = segment_8_base;
            }
            break;
        }
    }

    // ==============================================================
    // FIX OTHER SEGMENTS: Some segment values are missing 0x80000000 prefix
    // ==============================================================
    // The DL has segment addresses like 0x00228E10 instead of 0x80228E10
    // These need to be fixed or RT64 will crash accessing invalid memory
    for (int i = 0; i < 10; i++) {
        uint32_t w0 = dl[i*2];
        uint32_t w1 = dl[i*2 + 1];

        // Check if this is a G_MOVEWORD for segment setup (opcode 0xBC, index 0x06)
        if ((w0 & 0xFF0000FF) == 0xBC000006) {
            // Extract segment number from offset field: offset = segment * 4
            uint32_t offset = (w0 >> 8) & 0xFF;
            uint32_t segment = offset / 4;

            // If the address looks like it's missing the 0x80000000 prefix
            // (non-zero, but less than 0x80000000), add the prefix
            if (w1 != 0 && w1 < 0x80000000) {
                uint32_t fixed_addr = w1 | 0x80000000;
                // DEBUG DISABLED: fprintf(stderr, "[DL-FIX] Frame %d: Fixing segment %d addr 0x%08X -> 0x%08X\n", fix_count, segment, w1, fixed_addr);
                dl[i*2 + 1] = fixed_addr;
            }
        }
    }

    // Debug: dump the full display list (first 50 commands) - DISABLED for timing test
    // if (fix_count <= 3) {
    //     fprintf(stderr, "[DL-FIX] DL at 0x%08X after fix (frame %d):\n", data_ptr, fix_count);
    //     // ... rest of debug code ...
    // }
}

// Exported function to be called from events.cpp right before send_dl
// This ensures the fix is applied AFTER the game has finished writing the DL
extern "C" void fix_display_list_for_rt64(uint8_t* rdram, uint32_t data_ptr) {
    fix_display_list(rdram, data_ptr);
}

extern "C" void osSpTaskStartGo_recomp(uint8_t* rdram, recomp_context* ctx) {
    // Use the task pointer saved by osSpTaskLoad, not ctx->r4
    // (Some games call osSpTaskStartGo without passing the task pointer)
    PTR(OSTask) task_ptr = g_loaded_task ? g_loaded_task : ctx->r4;

    OSTask* task = TO_PTR(OSTask, task_ptr);

    // Skip uninitialized tasks (type=0 with no ucode) - happens when osCreateViManager is stubbed
    if (task->t.type == 0 && task->t.ucode == 0 && task->t.ucode_data == 0) {
        ctx->r2 = task_ptr;
        return;
    }

    // NOTE: Display list fixes are now applied in gfx_thread (events.cpp) right before send_dl
    // This is because the game can overwrite the DL buffer between submit and actual processing

    ultramodern::submit_rsp_task(rdram, task_ptr);
    ctx->r2 = task_ptr; // Return the task pointer
}

extern "C" void osSpTaskYield_recomp(uint8_t* rdram, recomp_context* ctx) {
    // Ignore yield requests (acts as if the task completed before it received the yield request)
}

extern "C" void osSpTaskYielded_recomp(uint8_t* rdram, recomp_context* ctx) {
    // Task yield requests are ignored, so always return 0 as tasks will never be yielded
    ctx->r2 = 0;
}

extern "C" void __osSpSetPc_recomp(uint8_t* rdram, recomp_context* ctx) {
    assert(false);
}
