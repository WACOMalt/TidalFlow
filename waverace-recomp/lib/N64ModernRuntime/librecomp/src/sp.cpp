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

    // ==============================================================
    // SEGMENT FIX: Inject gSPSegment(8, base) at the start of the DL
    // ==============================================================
    // Wave Race's DL uses segment 8 addresses like 0x08066180 for assets.
    // Assets are loaded via DMA to 0x80200000 area.
    // The overlay expects segment 8 to be set, but it's not!
    //
    // From DMA logs: assets loaded to 0x802310A0 (large asset block 0x678E0 bytes)
    // Offset 0x66180 would be at 0x802310A0 - 0x66180 = ???
    // Let's try: segment 8 base = 0x801CA920 (so 0x801CA920 + 0x66180 = 0x802310A0)
    //
    // Actually, looking at typical N64 games, segment 8 often points to 0x80200000 area.
    // The offset 0x66180 suggests base at 0x802310A0 - 0x66180 + adjustment...
    // Let's try pointing segment 8 directly at 0x801CA920 which is 0x802310A0 - 0x66180
    //
    // UPDATE: After analysis, segment 8 should point to where the large asset block starts
    // minus the offsets used. The game DMA'd 0x678E0 bytes to 0x802310A0.
    // The DL uses offsets like 0x66180, 0x4A460, 0x4B460.
    // So segment 8 base should be: 0x802310A0 - 0x66180 = 0x801CAF20
    // But that doesn't work either... Let's try 0x801CA920 (base of asset area)
    //
    // ==============================================================
    // SEGMENT FIX: Set up segment 8 and potentially call overlay DL
    // ==============================================================
    // Wave Race's DL uses segment 8 addresses like 0x08066180 for assets.
    // The overlay generates a SUB display list that the master DL should call.
    //
    // TIMING ISSUE: The first task is submitted BEFORE the overlay runs!
    // So on frame 1, the overlay DL is empty. We need to:
    // - Frame 1: Just set segment 8 + ENDDL (no G_DL call)
    // - Frame 2+: Set segment 8 + G_DL call to overlay + ENDDL

    static int fix_count = 0;
    fix_count++;

    // Calculate segment 8 base address
    // From DMA: loaded 0x678E0 bytes from ROM 0x0FE320 to RDRAM 0x802310A0
    // DL uses 0x08066180 -> offset 0x66180 into segment 8
    // segment 8 base = 0x802310A0 - 0x66180 = 0x801CAF20
    uint32_t segment_8_base = 0x801CAF20;

    // Read where the overlay wrote its DL
    // D_80151944 is the current DL write pointer
    // After overlay runs, it points to END of overlay DL
    // Before overlay runs (frame 1), it points to start of buffer
    // NOTE: rdram is in native byte order (little-endian on x86), NOT N64 big-endian
    uint32_t dl_ptr_addr = 0x00151944;
    uint32_t current_dl_ptr = *(uint32_t*)(rdram + dl_ptr_addr);  // Already in native order

    // The overlay DL starts at 0x8011F940 (known from debug)
    // If current_dl_ptr > 0x8011F940, the overlay has written something
    uint32_t overlay_dl_start = 0x8011F940;
    bool overlay_has_content = (current_dl_ptr > overlay_dl_start) &&
                               (current_dl_ptr < overlay_dl_start + 0x10000);  // Sanity check

    fprintf(stderr, "[DL-FIX] Frame %d: D_80151944=0x%08X, overlay_start=0x%08X, has_content=%d\n",
            fix_count, current_dl_ptr, overlay_dl_start, overlay_has_content);

    // Always write segment 8 setup
    // F3D G_MOVEWORD format: BC tt oo oo | AAAAAAAA
    // - BC = opcode
    // - tt = type (06 = G_MW_SEGMENT)
    // - oooo = offset for segment = segment_number * 4 = 8 * 4 = 0x20
    //
    // But wait - RT64 extracts segment number as p0(10, 4) = (w0 >> 10) & 0xF
    // So segment 8 needs to be at bits 10-13: 8 << 10 = 0x2000
    //
    // Actually, looking at the N64 SDK gbi.h:
    // #define gsSPSegment(seg, base) gsMoveWd(G_MW_SEGMENT, (seg)*4, base)
    // So segment offset = seg * 4, and it goes in the "offset" field (bits 8-15 for F3D)
    //
    // For F3D: gMoveWd is BC tt oo oo where offset is 16-bit (high 8 bits in byte 2, low 8 bits in byte 3)
    // So for segment 8: offset = 8 * 4 = 32 = 0x0020
    // w0 = 0xBC 06 00 20 = 0xBC060020
    //
    // But RT64's extraction is (w0 >> 10) & 0xF for segment number
    // 0xBC060020 >> 10 = 0x002F0180
    // 0x002F0180 & 0xF = 0
    // That's segment 0, not 8!
    //
    // The issue is RT64's F3D handler uses p0(10, 4) but the offset is at bits 0-15
    // Let me check what the correct format is...
    //
    // Actually looking at F3D more carefully:
    // G_MOVEWORD: BC ii ss oo
    // - ii = index (type)
    // - ss = segment number (when type = G_MW_SEGMENT)
    // - oo = offset within segment table
    //
    // No wait, that's still wrong. Let me look at the actual N64 SDK format.
    // In gbi.h: #define gMoveWd(pkt, index, offset, data)
    // Word 0: (G_MOVEWORD << 24) | ((offset) << 8) | (index)
    //
    // So for gSPSegment(8, base):
    // - index = G_MW_SEGMENT = 0x06
    // - offset = seg * 4 = 32 = 0x20
    // w0 = (0xBC << 24) | (0x20 << 8) | 0x06 = 0xBC002006
    //
    // Let me recalculate RT64's extraction:
    // segment = p0(10, 4) = (w0 >> 10) & 0xF
    // 0xBC002006 >> 10 = 0x002F0008
    // 0x002F0008 & 0xF = 8  <- Correct!
    // RT64 reads display lists as native uint32_t (little-endian on x86).
    // We need to write the commands in native byte order!
    //
    // For gSPSegment(8, base):
    // w0 = 0xBC002006 (opcode=0xBC, offset=0x20, index=0x06)
    // w1 = segment_8_base
    //
    // For G_ENDDL:
    // w0 = 0xB8000000

    // Write gSPSegment(8, segment_8_base)
    dl[0] = 0xBC002006;  // w0: G_MOVEWORD for segment 8
    dl[1] = segment_8_base;  // w1: segment base address

    if (overlay_has_content) {
        // Overlay has content - add G_DL call to it
        fprintf(stderr, "[DL-FIX] Adding G_DL call to overlay DL at 0x%08X\n", overlay_dl_start);

        // G_DL command (branch to overlay display list)
        dl[2] = 0x06000000;  // w0: G_DL opcode
        dl[3] = overlay_dl_start;  // w1: address of overlay DL

        // G_ENDDL after G_DL
        dl[4] = 0xB8000000;  // w0: G_ENDDL
        dl[5] = 0x00000000;  // w1
    } else {
        // No overlay content yet - just segment setup + ENDDL
        fprintf(stderr, "[DL-FIX] No overlay content yet - just segment setup\n");

        dl[2] = 0xB8000000;  // w0: G_ENDDL
        dl[3] = 0x00000000;  // w1
    }

    // Debug: dump the first few commands we wrote
    fprintf(stderr, "[DL-FIX] DL at 0x%08X after fix:\n", data_ptr);
    fflush(stderr);
    for (int i = 0; i < 4; i++) {
        fprintf(stderr, "  [%d] %08X %08X (opcode=0x%02X)\n", i, dl[i*2], dl[i*2+1], (dl[i*2] >> 24) & 0xFF);
        fflush(stderr);
    }

    // Return early - we've set up the DL
    return;
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
