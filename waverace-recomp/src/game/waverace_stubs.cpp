// Wave Race 64 stubs and helper functions
// N64Recomp generates stubs for functions in the "stubs" list
// This file only contains functions that N64Recomp does NOT generate

#include "recomp.h"
#include <cstdint>
#include <atomic>

// Sequence counter for events.cpp (used for debug timing)
static std::atomic<uint64_t> global_sequence_counter{0};
extern "C" uint64_t get_next_sequence() {
    return global_sequence_counter.fetch_add(1);
}

// Forward declaration for ultramodern's send_si_message
namespace ultramodern {
    void send_si_message(uint8_t* rdram);
}

// libultra data symbols - these are variables, not functions
extern "C" uint32_t osClockRate = 62500000;  // NTSC clock rate
extern "C" uint32_t osViClock = 48681812;    // VI clock
extern "C" uint32_t osViModeNtscLan1 = 0;    // VI mode placeholder

// Entrypoint - must be sign-extended for proper RDRAM address calculation
gpr get_entrypoint_address() {
    return (gpr)(int32_t)0x80046800u;  // Wave Race 64 entrypoint
}

// SI (Serial Interface) stubs - these are in ignored_funcs but need runtime implementations
// __osSiRawStartDma: Starts raw SI DMA transfer (controller/EEPROM communication)
// In the recompiled environment, controller input is handled differently
// We send SI completion message to unblock waiting threads
extern "C" void __osSiRawStartDma_recomp(uint8_t* rdram, recomp_context* ctx) {
    // Send SI completion message - this unblocks osContInit and other SI waiting code
    ultramodern::send_si_message(rdram);
    ctx->r2 = 0;
}

// __osSetCompare: Sets the CP0 Compare register (timer interrupt)
// In the recompiled environment, we don't need hardware timer interrupts
extern "C" void __osSetCompare_recomp(uint8_t* rdram, recomp_context* ctx) {
    // Parameter: compare value (r4)
    // No-op in recompiled environment
}

// Helper to swap bytes for big-endian N64
static inline uint32_t bswap32(uint32_t x) {
    return ((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) |
           ((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000);
}

// func_80092CF0_impl: Display list processing function (actual implementation)
// N64Recomp generates an empty stub for func_80092CF0 (in stubs list),
// which is patched by patch_stubs.py to call this implementation.
//
// This function is supposed to call overlay code at 0x801ECAF4 which builds
// display lists. Since the overlay code is compressed/not recompiled, we need
// to build a minimal display list that RT64 can process.
//
// GBI Command Reference (F3D microcode):
// - G_RDPSETOTHERMODE (0xEF): w0 = 0xEF | mode_h, w1 = mode_l
// - G_SETCIMG (0xFF): w0 = 0xFF | fmt<<21 | siz<<19 | (width-1), w1 = address
// - G_SETFILLCOLOR (0xF7): w0 = 0xF7000000, w1 = packed_color
// - G_FILLRECT (0xF6): w0 = 0xF6 | lrx<<14 | lry<<2, w1 = ulx<<14 | uly<<2
// - G_ENDDL (0xB8): w0 = 0xB8000000, w1 = 0
// - G_RDPPIPESYNC (0xE7): w0 = 0xE7000000, w1 = 0
// - G_RDPFULLSYNC (0xE9): w0 = 0xE9000000, w1 = 0
// - G_SETSCISSOR (0xED): w0 = 0xED | (ulx<<14) | (uly<<2), w1 = (lrx<<14) | (lry<<2)
//
// The game uses double-buffering with two framebuffers:
// - 0x803B5000 (physical 0x003B5000)
// - 0x8038F800 (physical 0x0038F800)
extern "C" void func_80092CF0_impl(uint8_t* rdram, recomp_context* ctx) {
    // Input: r4 = display list pointer (offset into buffer, typically +0x58 from task data_ptr)
    // Output: r2 = display list pointer (updated)
    uint32_t dl_ptr = (uint32_t)ctx->r4;

    static int call_count = 0;
    call_count++;

    if (dl_ptr >= 0x80000000 && dl_ptr < 0x80800000) {
        // Calculate buffer start (offset -0x58 from r4)
        uint32_t buffer_start_virt = dl_ptr - 0x58;
        uint32_t buffer_start_addr = buffer_start_virt & 0x7FFFFF;
        uint32_t* dl_start = (uint32_t*)(rdram + buffer_start_addr);

        // Get the current framebuffer - alternate based on call count
        // Framebuffer is 16-bit RGBA, 320x240
        uint32_t fb_addr = (call_count % 2 == 0) ? 0x003B5000 : 0x0038F800;

        // Build a minimal display list that fills the screen with a cycling color
        int cmd = 0;

        // G_RDPPIPESYNC - Synchronize RDP pipeline
        dl_start[cmd*2 + 0] = bswap32(0xE7000000);
        dl_start[cmd*2 + 1] = bswap32(0x00000000);
        cmd++;

        // G_SETCIMG - Set color image (framebuffer)
        uint32_t setcimg_w0 = 0xFF000000 | (0 << 21) | (2 << 19) | (320 - 1);
        dl_start[cmd*2 + 0] = bswap32(setcimg_w0);
        dl_start[cmd*2 + 1] = bswap32(0x80000000 | fb_addr);
        cmd++;

        // G_SETSCISSOR - Set scissor region to full screen
        dl_start[cmd*2 + 0] = bswap32(0xED000000 | (0 << 12) | (0 << 0));
        dl_start[cmd*2 + 1] = bswap32((320 << 14) | (240 << 2));
        cmd++;

        // G_RDPSETOTHERMODE - Set RDP to fill mode
        dl_start[cmd*2 + 0] = bswap32(0xEF000000 | (3 << 20));
        dl_start[cmd*2 + 1] = bswap32(0x00000000);
        cmd++;

        // G_SETFILLCOLOR - Set fill color (cycling purple/blue)
        uint16_t color16 = (call_count % 32) << 11;  // Red varies with frame
        color16 |= 0x001F;  // Blue
        color16 |= 0x0001;  // Alpha
        uint32_t fill_color = (color16 << 16) | color16;
        dl_start[cmd*2 + 0] = bswap32(0xF7000000);
        dl_start[cmd*2 + 1] = bswap32(fill_color);
        cmd++;

        // G_FILLRECT - Fill rectangle (full screen)
        dl_start[cmd*2 + 0] = bswap32(0xF6000000 | ((319 << 2) << 12) | (239 << 2));
        dl_start[cmd*2 + 1] = bswap32((0 << 14) | (0 << 2));
        cmd++;

        // G_RDPFULLSYNC - Full RDP sync before ending
        dl_start[cmd*2 + 0] = bswap32(0xE9000000);
        dl_start[cmd*2 + 1] = bswap32(0x00000000);
        cmd++;

        // G_ENDDL - End display list
        dl_start[cmd*2 + 0] = bswap32(0xB8000000);
        dl_start[cmd*2 + 1] = bswap32(0x00000000);
        cmd++;

        // Return pointer past our commands
        ctx->r2 = buffer_start_virt + (cmd * 8);
    } else {
        ctx->r2 = ctx->r4;
    }
}

// func_800C5DA0: __osSpGetStatus - reads RSP SP_STATUS register (0xA410000C)
// In recompiled environment, RSP is handled by RT64. Return 0 (RSP idle/ready).
extern "C" void func_800C5DA0_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0;  // RSP ready (no busy flags set)
}

// func_800C6300: __osSpSetStatus - writes to RSP SP_STATUS register
// No-op in recompiled environment
extern "C" void func_800C6300_recomp(uint8_t* rdram, recomp_context* ctx) {
    // No-op - RSP is handled by RT64
}
