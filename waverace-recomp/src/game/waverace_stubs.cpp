// Wave Race 64 stubs and helper functions
// N64Recomp generates stubs for functions in the "stubs" list
// This file only contains functions that N64Recomp does NOT generate

#include "recomp.h"
#include <cstdint>
#include <atomic>
#include <cstdio>

// ============================================================================
// DEBUG: Comprehensive execution tracking
// ============================================================================

// Frame counter for periodic status
static int global_frame_count = 0;

// Overlay function call counters
static int ovl_801ECAF4_calls = 0;
static int ovl_801E7C58_calls = 0;
static int ovl_801E270C_calls = 0;
static int ovl_802C5BA4_calls = 0;

// Main function call counters
static int func_80092CF0_calls = 0;

// Thread tracking
static int render_thread_started = 0;
static int func_80046DA0_calls = 0;
static int game_thread_entry_calls = 0;

// State tracking
static uint32_t last_game_state = 0xFFFFFFFF;
static int state_change_count = 0;

// Debug helper to print every N calls
#define DEBUG_INTERVAL 60  // Print status every 60 frames (~1 second)

// ============================================================================
// MEMORY ADDRESSES - Important game variables
// ============================================================================
// D_800DAB24 = Game state (0=menu, 5=boot, 6=logo, 7=title, etc.)
// D_801CE63C = Boot sequence flag (controls framebuffer clear)
// D_80151944 = Current display list pointer
// D_80154100 = Message queue for render thread

#define ADDR_GAME_STATE      0x000DAB24  // D_800DAB24 - main game state
#define ADDR_BOOT_FLAG       0x001CE63C  // D_801CE63C - boot sequence control
#define ADDR_DL_PTR          0x00151944  // D_80151944 - display list pointer
#define ADDR_FRAME_COUNT     0x000D4620  // Frame counter (approximate)
#define ADDR_CONTROLLER      0x001CE65A  // D_801CE65A - controller input (buttons)
#define ADDR_FADE_COUNTER    0x002C76F4  // D_802C76F4 - fade counter in overlay BSS

// ============================================================================
// Helper functions
// ============================================================================

static inline uint32_t read_u32(uint8_t* rdram, uint32_t addr) {
    return *(uint32_t*)(rdram + addr);
}

static inline void write_u32(uint8_t* rdram, uint32_t addr, uint32_t val) {
    *(uint32_t*)(rdram + addr) = val;
}

// Print current game status - call this periodically
static void print_game_status(uint8_t* rdram, const char* caller) {
    uint32_t game_state = read_u32(rdram, ADDR_GAME_STATE);
    uint32_t boot_flag = read_u32(rdram, ADDR_BOOT_FLAG);
    uint32_t dl_ptr = read_u32(rdram, ADDR_DL_PTR);

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  GAME STATUS (called from: %s)\n", caller);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  Game State (D_800DAB24): %d (0x%X)\n", game_state, game_state);
    printf("║  Boot Flag  (D_801CE63C): %d\n", boot_flag);
    printf("║  DL Pointer (D_80151944): 0x%08X\n", dl_ptr);
    printf("║  State changes so far: %d\n", state_change_count);
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    fflush(stdout);
}

// Check for state change and report
static void check_state_change(uint8_t* rdram, const char* caller) {
    uint32_t game_state = read_u32(rdram, ADDR_GAME_STATE);

    if (game_state != last_game_state) {
        state_change_count++;
        printf("\n");
        printf("████████████████████████████████████████████████████████████████\n");
        printf("██  STATE CHANGE DETECTED! #%d\n", state_change_count);
        printf("██  From: %d (0x%X) -> To: %d (0x%X)\n",
               last_game_state, last_game_state, game_state, game_state);
        printf("██  Caller: %s\n", caller);
        printf("████████████████████████████████████████████████████████████████\n");
        printf("\n");
        fflush(stdout);

        last_game_state = game_state;

        // Print full status on state change
        print_game_status(rdram, caller);
    }
}

// ============================================================================
// Thread entry debug hooks
// ============================================================================

// Debug: Track when render thread (func_80046DA0) is called
extern "C" void debug_func_80046DA0_entry(uint8_t* rdram, recomp_context* ctx) {
    func_80046DA0_calls++;
    if (func_80046DA0_calls <= 10 || func_80046DA0_calls % DEBUG_INTERVAL == 0) {
        printf("[RENDER-THREAD] >>> func_80046DA0 CALLED (render loop) call #%d\n",
               func_80046DA0_calls);
        fflush(stdout);
    }
}

// Debug: Track when game thread entry is called
extern "C" void debug_game_thread_entry(uint8_t* rdram, recomp_context* ctx) {
    game_thread_entry_calls++;
    printf("[GAME-THREAD] >>> game_thread_entry CALLED call #%d\n", game_thread_entry_calls);

    // Read the flag at 0x800D4624 that controls if render thread is started
    uint32_t flag_addr = 0x000D4624;  // Physical address
    uint8_t flag_val = *(uint8_t*)(rdram + flag_addr);
    printf("[GAME-THREAD]     Render thread start flag (0x800D4624) = %d\n", flag_val);
    fflush(stdout);
}

// These wrappers are called from patches to track overlay function execution
extern "C" void debug_ovl_801ECAF4_entry(uint8_t* rdram, recomp_context* ctx) {
    ovl_801ECAF4_calls++;
    printf("[OVERLAY-DL] >>> ovl_func_801ECAF4 CALLED (display list builder) call #%d\n",
           ovl_801ECAF4_calls);
    printf("[OVERLAY-DL]     r4=0x%08X (dl_ptr) r5=0x%08X r6=0x%08X\n",
           (uint32_t)ctx->r4, (uint32_t)ctx->r5, (uint32_t)ctx->r6);
    fflush(stdout);
}

extern "C" void debug_ovl_801E7C58_entry(uint8_t* rdram, recomp_context* ctx) {
    ovl_801E7C58_calls++;
    printf("[OVERLAY] >>> ovl_func_801E7C58 CALLED call #%d\n", ovl_801E7C58_calls);
    printf("[OVERLAY]     r4=0x%08X r5=0x%08X r6=0x%08X\n",
           (uint32_t)ctx->r4, (uint32_t)ctx->r5, (uint32_t)ctx->r6);
    fflush(stdout);
}

// Debug: Track when func_80092CF0 is called (the function that should call overlay)
extern "C" void debug_func_80092CF0_entry(uint8_t* rdram, recomp_context* ctx) {
    func_80092CF0_calls++;
    if (func_80092CF0_calls <= 5 || func_80092CF0_calls % DEBUG_INTERVAL == 0) {
        printf("[MAIN-DL] >>> func_80092CF0 CALLED (should call overlay 801ECAF4) call #%d\n",
               func_80092CF0_calls);
        printf("[MAIN-DL]     r4=0x%08X (dl_ptr)\n", (uint32_t)ctx->r4);
        fflush(stdout);
    }
}

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

// ============================================================================
// func_80092CF0_impl: Display list building - calls REAL overlay when possible
// ============================================================================
//
// This is a stub for func_80092CF0 which is a big state machine that calls
// different display list builders based on D_800DAB24 (game state variable).
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  STATE → OVERLAY MAPPING                                                │
// ├─────────────────────────────────────────────────────────────────────────┤
// │  State 0:    0x801E overlay (ovl_func_801ECAF4) - Menu/gameplay         │
// │  State 5,6:  0x802C overlay (segment_1B1FB0) - Boot/intro    ◄── NOW!  │
// │  State 7:    0x802C overlay (ovl_i1) - Title screen          NOT IMPL   │
// │  Other:      Various 0x802C overlays                         NOT IMPL   │
// └─────────────────────────────────────────────────────────────────────────┘
// ============================================================================

// Forward declarations for REAL overlay functions
extern "C" void ovl_func_801ECAF4(uint8_t* rdram, recomp_context* ctx);  // 0x801E overlay (codeseg)
extern "C" void ovl_func_802C5BA4(uint8_t* rdram, recomp_context* ctx);  // 0x802C overlay (segment_1B1FB0) - State 5,6!

extern "C" void func_80092CF0_impl(uint8_t* rdram, recomp_context* ctx) {
    static int call_count = 0;
    call_count++;
    global_frame_count = call_count;

    // Read game state
    uint32_t game_state = read_u32(rdram, ADDR_GAME_STATE);
    uint32_t boot_flag = read_u32(rdram, ADDR_BOOT_FLAG);
    uint32_t dl_ptr_in = (uint32_t)ctx->r4;

    // Check for state changes
    check_state_change(rdram, "func_80092CF0_impl");

    // DEBUG output - ALWAYS print first 30 calls, then every DEBUG_INTERVAL
    if (call_count <= 30 || call_count % DEBUG_INTERVAL == 0) {
        printf("┌────────────────────────────────────────────────────────────────┐\n");
        printf("│ [DL-IMPL] func_80092CF0_impl FRAME #%d\n", call_count);
        printf("├────────────────────────────────────────────────────────────────┤\n");
        printf("│  Game State: %d (0x%X)\n", game_state, game_state);
        printf("│  Boot Flag:  %d\n", boot_flag);
        printf("│  DL Input:   0x%08X\n", dl_ptr_in);

        // Show which overlay will be called
        const char* overlay_name = "UNKNOWN";
        const char* overlay_status = "NOT IMPLEMENTED";
        if (game_state == 0) {
            overlay_name = "ovl_func_801ECAF4 (0x801E codeseg)";
            overlay_status = "IMPLEMENTED";
        } else if (game_state == 5 || game_state == 6) {
            overlay_name = "ovl_func_802C5BA4 (0x802C segment_1B1FB0)";
            overlay_status = "IMPLEMENTED";
        } else if (game_state == 7 || game_state == 0x28) {
            overlay_name = "ovl_i1 (0x802C)";
        } else if (game_state == 2) {
            overlay_name = "ovl_i0 (0x802C)";
        }
        printf("│  Overlay:    %s\n", overlay_name);
        printf("│  Status:     %s\n", overlay_status);
        printf("└────────────────────────────────────────────────────────────────┘\n");
        fflush(stdout);
    }

    // STATE 0: Call the REAL 0x801E overlay function!
    if (game_state == 0) {
        if (call_count <= 30) {
            printf(">>> [STATE 0] CALLING ovl_func_801ECAF4 (0x801E overlay)...\n");
            fflush(stdout);
        }
        ovl_func_801ECAF4(rdram, ctx);
        if (call_count <= 30) {
            printf("<<< [STATE 0] RETURNED from ovl_func_801ECAF4, r2=0x%08X\n", (uint32_t)ctx->r2);
            fflush(stdout);
        }
        return;
    }

    // STATE 5,6: Call the REAL 0x802C overlay function (segment_1B1FB0)!
    // This is the boot/intro state - should clear framebuffer and show Nintendo logo sequence
    if (game_state == 5 || game_state == 6) {
        ovl_802C5BA4_calls++;

        // Read controller input and fade counter for debug
        uint16_t controller_input = *(uint16_t*)(rdram + ADDR_CONTROLLER);
        uint32_t fade_counter = 0;
        // Fade counter is in overlay BSS at 0x802C76F4 - only valid when overlay is loaded
        // Physical address = 0x002C76F4
        if (game_state == 5 || game_state == 6) {
            fade_counter = *(uint32_t*)(rdram + 0x002C76F4);
        }

        if (call_count <= 30 || call_count % DEBUG_INTERVAL == 0) {
            printf(">>> [STATE %d] CALLING ovl_func_802C5BA4 (0x802C segment_1B1FB0)...\n", game_state);
            printf("    This overlay handles: boot sequence, framebuffer clear, logo\n");
            printf("    Controller (D_801CE65A): 0x%04X  (need 0xB000 for skip)\n", controller_input);
            printf("    Fade counter (0x802C76F4): %d  (need 14 for auto-transition)\n", fade_counter);
            fflush(stdout);
        }

        ovl_func_802C5BA4(rdram, ctx);

        uint32_t dl_ptr_out = (uint32_t)ctx->r2;
        uint32_t dl_size = dl_ptr_out - dl_ptr_in;

        // Read fade counter AFTER overlay runs to see if it changed
        uint32_t fade_counter_after = *(uint32_t*)(rdram + 0x002C76F4);

        if (call_count <= 30 || call_count % DEBUG_INTERVAL == 0) {
            printf("<<< [STATE %d] RETURNED from ovl_func_802C5BA4\n", game_state);
            printf("    DL output: 0x%08X (wrote %d bytes = %d commands)\n",
                   dl_ptr_out, dl_size, dl_size / 8);
            if (fade_counter_after != fade_counter) {
                printf("    Fade counter changed: %d -> %d\n", fade_counter, fade_counter_after);
            }
            fflush(stdout);
        }

        // Check if boot flag changed (indicates progress in boot sequence)
        uint32_t new_boot_flag = read_u32(rdram, ADDR_BOOT_FLAG);
        if (new_boot_flag != boot_flag) {
            printf("!!! BOOT FLAG CHANGED: %d -> %d\n", boot_flag, new_boot_flag);
            fflush(stdout);
        }

        // AUTO-ADVANCE: IMMEDIATELY after first state 5 frame, force state 6
        // This bypasses the controller input check since controller is not working
        // NOTE: The game crashes during display list processing, so we need to be quick
        static bool state5_advanced = false;
        if (game_state == 5 && !state5_advanced) {
            state5_advanced = true;
            printf("\n");
            printf("╔══════════════════════════════════════════════════════════════╗\n");
            printf("║  AUTO-ADVANCE: Immediately forcing state 5 -> 6!             ║\n");
            printf("║  (Controller input bypass - normally needs Start+A+B)        ║\n");
            printf("╚══════════════════════════════════════════════════════════════╝\n");
            printf("\n");
            fflush(stdout);

            // Set D_800DAB24 = 6 (next state)
            write_u32(rdram, ADDR_GAME_STATE, 6);
            // Also set D_801CE63C = 1 (boot flag) to trigger framebuffer clear
            write_u32(rdram, ADDR_BOOT_FLAG, 1);
        }

        // AUTO-ADVANCE state 6 -> 2 (to ovl_i0) after 3 frames in state 6
        static int state6_frames = 0;
        if (game_state == 6) {
            state6_frames++;
            if (state6_frames == 3) {
                printf("\n");
                printf("╔══════════════════════════════════════════════════════════════╗\n");
                printf("║  AUTO-ADVANCE: Forcing state 6 -> 2 (ovl_i0)!                ║\n");
                printf("║  (Bypass fade counter check)                                 ║\n");
                printf("╚══════════════════════════════════════════════════════════════╝\n");
                printf("\n");
                fflush(stdout);

                // Set D_800DAB24 = 2 (this is what func_801EB180 does)
                write_u32(rdram, ADDR_GAME_STATE, 2);
                write_u32(rdram, ADDR_BOOT_FLAG, 1);
            }
        }

        return;
    }

    // OTHER STATES: Need different 0x802C overlays (ovl_i0, ovl_i1, etc.)
    if (call_count <= 30 || call_count % DEBUG_INTERVAL == 0) {
        printf("!!! [STATE %d] OVERLAY NOT IMPLEMENTED - generating placeholder DL\n", game_state);
        printf("    Need to implement: ");
        switch (game_state) {
            case 2:  printf("ovl_i0\n"); break;
            case 7:
            case 0x28: printf("ovl_i1 (title screen)\n"); break;
            case 0xA: printf("ovl_i2\n"); break;
            default: printf("overlay for state %d\n", game_state); break;
        }
        fflush(stdout);
    }

    // Generate a FAKE display list with cycling colors (proof that render works)
    uint32_t dl_virt = (uint32_t)ctx->r4;
    uint32_t dl_phys = dl_virt & 0x1FFFFFFF;
    uint32_t* dl = (uint32_t*)(rdram + dl_phys);

    // Cycling color based on frame count
    static uint32_t frame = 0;
    frame++;
    uint8_t r = (frame * 3) & 0xFF;
    uint8_t g = (frame * 5) & 0xFF;
    uint8_t b = (frame * 7) & 0xFF;

    // Pack color as RGBA5551 (N64 format): RRRRR GGGGG BBBBB A
    uint16_t color5551 = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | 1;
    uint32_t fill_color = (color5551 << 16) | color5551;

    int idx = 0;

    // gDPSetCycleType(G_CYC_FILL) = 0xEF000000 | (G_CYC_FILL << 52 - 32 bits)
    dl[idx++] = 0xEF000000; dl[idx++] = 0x00300000;  // Set cycle type to FILL

    // gDPSetColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 320, framebuffer)
    // Use current framebuffer from osViGetCurrentFramebuffer
    dl[idx++] = 0xFF100000 | (320 - 1);  // Set color image (320 wide, 16-bit RGBA)
    dl[idx++] = 0x003B5000;              // Framebuffer address (physical) - use 0x003B5000

    // gDPSetFillColor(fill_color)
    dl[idx++] = 0xF7000000;
    dl[idx++] = fill_color;

    // gDPFillRectangle(0, 0, 319, 239) - fill entire screen
    // Format: 0xF6 XXXX XXXX YYYY YYYY
    dl[idx++] = 0xF6000000 | ((319 << 2) << 12) | (239 << 2);
    dl[idx++] = 0x00000000;  // XL=0, YL=0

    // gDPPipeSync
    dl[idx++] = 0xE7000000;
    dl[idx++] = 0x00000000;

    // gSPEndDisplayList
    dl[idx++] = 0xB8000000;
    dl[idx++] = 0x00000000;

    // Return pointer past the commands we wrote
    ctx->r2 = ctx->r4 + (idx * 4);
}

// ============================================================================
// ovl_func_801E270C: State machine / menu handler with switch statement
// This function has a jump table at 0x80225F68 which is in BSS (not in ROM).
// N64Recomp cannot analyze it.
//
// STUBBED: The original implementation tried to call overlay functions by name,
// but those names don't match the actual recompiled function names due to VRAM
// base address differences. For now, this is a simple stub that just returns.
//
// TODO: Map the correct overlay function names when display list support is needed.
// ============================================================================
extern "C" void ovl_func_801E270C_recomp(uint8_t* rdram, recomp_context* ctx) {
    ovl_801E270C_calls++;

    // DEBUG: Print when this is called
    if (ovl_801E270C_calls <= 10 || ovl_801E270C_calls % DEBUG_INTERVAL == 0) {
        printf("[OVERLAY-MENU] >>> ovl_func_801E270C CALLED (menu state machine) call #%d\n",
               ovl_801E270C_calls);
        printf("[OVERLAY-MENU]     r4=0x%08X (state?) r5=0x%08X r6=0x%08X\n",
               (uint32_t)ctx->r4, (uint32_t)ctx->r5, (uint32_t)ctx->r6);
        printf("[OVERLAY-MENU]     NOTE: This is STUBBED - menu transitions won't work!\n");
        fflush(stdout);
    }

    // Stubbed - just return without doing anything
    (void)rdram;
}

// func_800C7020: Controller polling with timer wait
// This function has a bug in the recompiled code where 64-bit subtraction
// produces an enormous countdown value (185 hours!) causing the thread to block forever.
// We stub this because controller input is handled by the runtime's SDL layer anyway.
extern "C" void func_800C7020(uint8_t* rdram, recomp_context* ctx) {
    static int call_count = 0;
    call_count++;
    if (call_count <= 5) {
        printf("[STUB] func_800C7020 (controller poll) STUBBED - call #%d\n", call_count);
        fflush(stdout);
    }
    // Return success - the runtime handles controller input via SDL
    ctx->r2 = 0;
}

// func_80046BF4: Display list finalizer - adds RDP sync commands
// This function crashes because display list pointer at 0x80151944 is not initialized yet
// when called early in the render thread init sequence.
// We stub it with a safety check - if the DL pointer is null, we skip the write.
extern "C" void func_80046BF4(uint8_t* rdram, recomp_context* ctx) {
    static int call_count = 0;
    call_count++;

    // Get the display list pointer from D_80151944
    // NOTE: Recompiled code uses native byte order, no swapping needed
    uint32_t dl_ptr_addr = 0x00151944;  // Physical address in RDRAM
    uint32_t dl_ptr_val = *(uint32_t*)(rdram + dl_ptr_addr);

    if (call_count <= 5) {
        printf("[STUB] func_80046BF4 (DL finalizer): dl_ptr=0x%08X call #%d\n", dl_ptr_val, call_count);
        fflush(stdout);
    }

    // If display list pointer is not initialized (0 or invalid), skip
    if (dl_ptr_val == 0 || dl_ptr_val < 0x80000000 || dl_ptr_val > 0x80800000) {
        if (call_count <= 5) {
            printf("[STUB] func_80046BF4: SKIPPING - display list not initialized\n");
            fflush(stdout);
        }
        return;
    }

    // Original function writes gDPPipeSync and gDPFullSync to display list
    // Write in native byte order (same as how MEM_W macro works)
    uint32_t phys_dl = dl_ptr_val & 0x1FFFFFFF;  // N64 physical address

    // gDPPipeSync (0xE9000000 00000000)
    *(uint32_t*)(rdram + phys_dl + 0) = 0xE9000000;
    *(uint32_t*)(rdram + phys_dl + 4) = 0x00000000;

    // gDPFullSync (0xE8000000 00000000)  - note: not B8, that's gSPEndDisplayList
    *(uint32_t*)(rdram + phys_dl + 8) = 0xE8000000;
    *(uint32_t*)(rdram + phys_dl + 12) = 0x00000000;

    // Update the display list pointer (D_80151944)
    uint32_t new_ptr = dl_ptr_val + 16;
    *(uint32_t*)(rdram + dl_ptr_addr) = new_ptr;
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
