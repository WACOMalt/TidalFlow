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
// func_80092CF0: Display list building - calls REAL overlay when possible
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
extern "C" void func_i0_802C5800(uint8_t* rdram, recomp_context* ctx);   // 0x802C overlay (ovl_i0) - State 2!

// Debug counter for state transition function
static int ovl_801EB180_calls = 0;

// Forward declarations for subfunctions that ovl_func_801EB180 calls
extern "C" void func_80096960(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_8009684C(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_8004A208(uint8_t* rdram, recomp_context* ctx);
extern "C" void ovl_FadeTransition_SetProps(uint8_t* rdram, recomp_context* ctx);

// ============================================================================
// func_800C21F4: STUB - Audio/sound init, was blocking (Session 24)
// ============================================================================
extern "C" void func_800C21F4(uint8_t* rdram, recomp_context* ctx) {
    printf("[STUB] func_800C21F4(a0=%lld, a1=%lld) - audio init stubbed\n",
           (long long)ctx->r4, (long long)ctx->r5);
    fflush(stdout);
}

// ============================================================================
// ovl_func_801E6A4C: STUB - Was blocking the state 6→2 transition (Session 24)
// ============================================================================
// This function appears to do menu/UI setup. With a0=0, a1=0 it should
// just do some memory copies and return, but the N64Recomp version blocks.
// We stub it for now to allow state transitions to work.
// ============================================================================
extern "C" void ovl_func_801E6A4C(uint8_t* rdram, recomp_context* ctx) {
    // For now, just do nothing - the original function was blocking
    // TODO: Investigate why the original blocked and implement if needed
    printf("[STUB] ovl_func_801E6A4C(a0=%lld, a1=%lld) - stubbed, returning immediately\n",
           (long long)ctx->r4, (long long)ctx->r5);
    fflush(stdout);
}

// ============================================================================
// ovl_func_801EB180: State 6 → 2 Transition (Custom Debug Implementation)
// ============================================================================
// This function sets up the game for state 2 (menu/ovl_i0).
// The original N64Recomp version was blocking somewhere.
// This custom version calls each subfunctie with debug output to trace blocking.
//
// Based on assembly at 0x801EB180 (B97B0 in ROM)
// ============================================================================
extern "C" void ovl_func_801EB180(uint8_t* rdram, recomp_context* ctx) {
    ovl_801EB180_calls++;

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║ >>> STATE TRANSITION: ovl_func_801EB180 CALLED! #%d\n", ovl_801EB180_calls);
    printf("║     This function transitions from state 6 → state 2\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    fflush(stdout);

    // ========================================================================
    // Part 1: Set all the state variables (from assembly lines B97B0-B9878)
    // ========================================================================
    printf("[801EB180] Part 1: Setting state variables...\n"); fflush(stdout);

    // D_801CE634 = D_800DAB24 (save previous state)
    uint32_t prev_state = read_u32(rdram, 0x000DAB24);
    write_u32(rdram, 0x001CE634, prev_state);

    // D_801CE630 = 0
    write_u32(rdram, 0x001CE630, 0);

    // D_800DAB24 = 2 (STATE 6 → 2!)
    write_u32(rdram, 0x000DAB24, 2);
    printf("[801EB180] STATE CHANGED: %d → 2\n", prev_state); fflush(stdout);

    // D_801CE638 = 0
    write_u32(rdram, 0x001CE638, 0);

    // D_801CE63C = 1 (init flag)
    write_u32(rdram, 0x001CE63C, 1);

    // D_801CE640 = 0
    write_u32(rdram, 0x001CE640, 0);

    // D_801CE644 = 0
    write_u32(rdram, 0x001CE644, 0);

    // D_800DAB1C = 0
    write_u32(rdram, 0x000DAB1C, 0);

    // D_800D461C = 3
    write_u32(rdram, 0x000D461C, 3);

    // gGameModes (D_801CE620) = 0
    write_u32(rdram, 0x001CE620, 0);

    // gPlayers (D_800DAB28) = 1
    write_u32(rdram, 0x000DAB28, 1);

    // gRiders (D_801982F0) = 2
    write_u32(rdram, 0x001982F0, 2);

    // D_800D49B0 = 0x14 (20)
    write_u32(rdram, 0x000D49B0, 0x14);

    // D_800D8174 = 5
    write_u32(rdram, 0x000D8174, 5);

    // D_801CE728 = 3
    write_u32(rdram, 0x001CE728, 3);

    // D_800D8178 = 1
    write_u32(rdram, 0x000D8178, 1);

    // D_801CE600 = 0
    write_u32(rdram, 0x001CE600, 0);

    // D_801CE6F8 = 0
    write_u32(rdram, 0x001CE6F8, 0);

    // D_801CB334 = 0
    write_u32(rdram, 0x001CB334, 0);

    // gCourseID (D_800D8170) - check and possibly set to 0
    uint32_t course_id = read_u32(rdram, 0x000D8240);
    if (course_id == 0) {
        write_u32(rdram, 0x000D8170, 0);
    }

    // D_801CE6F0 = 0 (halfword)
    *(uint16_t*)(rdram + 0x001CE6F0) = 0;

    // D_800DAB68 = 0 (halfword)
    *(uint16_t*)(rdram + 0x000DAB68) = 0;

    // D_800DAB0C = 0 (halfword)
    *(uint16_t*)(rdram + 0x000DAB0C) = 0;

    // D_800DAB60 = 0 (halfword) - or 1 depending on D_801CB280
    *(uint16_t*)(rdram + 0x000DAB60) = 0;

    // D_800DAB64 = 0 (halfword)
    *(uint16_t*)(rdram + 0x000DAB64) = 0;

    // D_800DAA08 = 0
    write_u32(rdram, 0x000DAA08, 0);

    // gRiderGameModes (D_801CE648) = 1
    write_u32(rdram, 0x001CE648, 1);

    // D_801CE64C = 0
    write_u32(rdram, 0x001CE64C, 0);

    // D_801CE650 = 3
    write_u32(rdram, 0x001CE650, 3);

    printf("[801EB180] Part 1 complete: All state variables set\n"); fflush(stdout);

    // ========================================================================
    // Part 2: Call subfunctions (these might block!)
    // ========================================================================

    // Save original r4-r7 for later
    gpr orig_r4 = ctx->r4;
    gpr orig_r5 = ctx->r5;
    gpr orig_r6 = ctx->r6;
    gpr orig_r7 = ctx->r7;

    // func_80096960(1, something, 0, 0, 0)
    printf("[801EB180] Calling func_80096960...\n"); fflush(stdout);
    ctx->r4 = 1;  // a0 = 1
    ctx->r5 = 0;  // a1 = from stack (we use 0)
    ctx->r6 = 0;  // a2 - not set explicitly
    ctx->r7 = 0;  // a3 = 0
    // Note: also needs stack arg at 0x10($sp) = 0
    func_80096960(rdram, ctx);
    printf("[801EB180] func_80096960 returned\n"); fflush(stdout);

    // func_8009684C(a1, a2) - with values from table lookup
    printf("[801EB180] Calling func_8009684C...\n"); fflush(stdout);
    ctx->r4 = 0x806 << 16;  // Some ROM address
    ctx->r5 = 0;  // Default
    ctx->r6 = 0;
    func_8009684C(rdram, ctx);
    printf("[801EB180] func_8009684C returned\n"); fflush(stdout);

    // func_8004A208()
    printf("[801EB180] Calling func_8004A208...\n"); fflush(stdout);
    func_8004A208(rdram, ctx);
    printf("[801EB180] func_8004A208 returned\n"); fflush(stdout);

    // FadeTransition_SetProps(0, 0, 0)
    printf("[801EB180] Calling ovl_FadeTransition_SetProps...\n"); fflush(stdout);
    ctx->r4 = 0;
    ctx->r5 = 0;
    ctx->r6 = 0;
    ovl_FadeTransition_SetProps(rdram, ctx);
    printf("[801EB180] ovl_FadeTransition_SetProps returned\n"); fflush(stdout);

    // func_801E6A4C(0, 0) - STUBBED because it was blocking (Session 24)
    printf("[801EB180] Skipping ovl_func_801E6A4C (was blocking)\n"); fflush(stdout);
    // Skip the call - function was blocking the state transition
    // ovl_func_801E6A4C(rdram, ctx);
    printf("[801EB180] ovl_func_801E6A4C skipped\n"); fflush(stdout);

    // gCameraPerspective (D_80227C80) = 5
    write_u32(rdram, 0x00227C80, 5);

    // D_800DA9AC = 1 (halfword)
    *(uint16_t*)(rdram + 0x000DA9AC) = 1;

    // func_800C21F4(0, 0)
    printf("[801EB180] Calling func_800C21F4...\n"); fflush(stdout);
    ctx->r4 = 0;
    ctx->r5 = 0;
    func_800C21F4(rdram, ctx);
    printf("[801EB180] func_800C21F4 returned\n"); fflush(stdout);

    // Restore r4-r7
    ctx->r4 = orig_r4;
    ctx->r5 = orig_r5;
    ctx->r6 = orig_r6;
    ctx->r7 = orig_r7;

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║ <<< STATE TRANSITION COMPLETE: ovl_func_801EB180 DONE!\n");
    printf("║     Game state is now: %d\n", read_u32(rdram, 0x000DAB24));
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    fflush(stdout);
}

extern "C" void func_80092CF0(uint8_t* rdram, recomp_context* ctx) {
    static int call_count = 0;
    call_count++;
    global_frame_count = call_count;

    // Read game state
    uint32_t game_state = read_u32(rdram, ADDR_GAME_STATE);
    uint32_t boot_flag = read_u32(rdram, ADDR_BOOT_FLAG);
    uint32_t dl_ptr_in = (uint32_t)ctx->r4;

    // Check for state changes
    check_state_change(rdram, "func_80092CF0");

    // DEBUG output - ALWAYS print first 30 calls, then every DEBUG_INTERVAL
    if (call_count <= 30 || call_count % DEBUG_INTERVAL == 0) {
        printf("┌────────────────────────────────────────────────────────────────┐\n");
        printf("│ [DL-IMPL] func_80092CF0 FRAME #%d\n", call_count);
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
            overlay_name = "func_i0_802C5800 (0x802C ovl_i0)";
            overlay_status = "IMPLEMENTED";
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

        // BOOT INIT FIX (Session 22):
        // The boot overlay has complex state machine logic that depends on multiple
        // overlay BSS variables (D_802C76A4, D_802C76A8, etc.) being in specific states.
        // Without controller input, the internal state machine never progresses.
        //
        // DIRECT FIX: After overlay returns on frame 2, directly force state 5→6
        // by calling ovl_func_802C7510 which sets all the required variables.
        static int state5_frame_count = 0;
        if (game_state == 5) {
            state5_frame_count++;

            // Frame 1: Set init flag for one-time init path
            if (state5_frame_count == 1) {
                write_u32(rdram, ADDR_BOOT_FLAG, 1);  // D_801CE63C = 1
                printf("!!! BOOT INIT FIX: Set D_801CE63C = 1 (frame 1 init)\n");
                fflush(stdout);
            }
            // Frame 2: After init path returns, directly force state 5→6
            // by setting the variables that ovl_func_802C7510 would set
            else if (state5_frame_count == 2) {
                // These are the writes that ovl_func_802C7510 does:
                // D_801CE634 = D_800DAB24 (save current state)
                // D_801CE630 = 0
                // D_800DAB24 = 6 (new state!)
                // D_801CE638 = 0x13
                // D_801CE63C = 1
                // D_801CE640 = 0
                // D_801CE644 = 0
                // D_800DAB1C = 3
                // D_800D461C = 2
                write_u32(rdram, 0x001CE634, 5);   // D_801CE634 = previous state
                write_u32(rdram, 0x001CE630, 0);   // D_801CE630 = 0
                write_u32(rdram, ADDR_GAME_STATE, 6);  // D_800DAB24 = 6!
                write_u32(rdram, 0x001CE638, 0x13); // D_801CE638 = 0x13
                write_u32(rdram, ADDR_BOOT_FLAG, 1);  // D_801CE63C = 1
                write_u32(rdram, 0x001CE640, 0);   // D_801CE640 = 0
                write_u32(rdram, 0x001CE644, 0);   // D_801CE644 = 0
                write_u32(rdram, 0x000DAB1C, 3);   // D_800DAB1C = 3
                write_u32(rdram, 0x000D461C, 2);   // D_800D461C = 2
                printf("!!! BOOT INIT FIX: FORCED state 5→6 (like ovl_func_802C7510)\n");
                fflush(stdout);
            }
        }

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

        // Call the overlay function first (don't modify r4 beforehand!)
        ovl_func_802C5BA4(rdram, ctx);

        uint32_t dl_ptr_out = (uint32_t)ctx->r2;
        uint32_t dl_size = dl_ptr_out - dl_ptr_in;

        // DEBUG: Dump first 20 DL commands to see what the overlay generated
        if (call_count <= 5) {
            uint32_t dl_phys = dl_ptr_in & 0x1FFFFFFF;
            uint32_t* dl = (uint32_t*)(rdram + dl_phys);
            int num_cmds = (dl_size / 8);
            if (num_cmds > 20) num_cmds = 20;
            printf("\n=== DISPLAY LIST DUMP (first %d commands) ===\n", num_cmds);
            for (int i = 0; i < num_cmds; i++) {
                uint32_t w0 = dl[i*2];
                uint32_t w1 = dl[i*2 + 1];
                uint8_t cmd = (w0 >> 24) & 0xFF;
                printf("  [%3d] %08X %08X", i, w0, w1);
                // Identify common commands
                switch (cmd) {
                    case 0x00: printf("  (G_NOOP)"); break;
                    case 0x01: printf("  (G_MTX)"); break;
                    case 0x03: printf("  (G_MOVEMEM)"); break;
                    case 0x04: printf("  (G_VTX)"); break;
                    case 0x06: printf("  (G_DL) -> segmented addr 0x%08X", w1); break;
                    case 0xB6: printf("  (G_CLEARGEOMETRYMODE)"); break;
                    case 0xB7: printf("  (G_SETGEOMETRYMODE)"); break;
                    case 0xB8: printf("  (G_ENDDL)"); break;
                    case 0xBA: printf("  (G_SETOTHERMODE_L)"); break;
                    case 0xBB: printf("  (G_SETOTHERMODE_H)"); break;
                    case 0xBC: printf("  (G_MOVEWORD - F3D)");
                        if (((w0 >> 16) & 0xFF) == 0x06) {
                            printf(" SEGMENT %d = 0x%08X", (w0 >> 8) & 0xF, w1);
                        }
                        break;
                    case 0xDB: printf("  (G_MOVEWORD - F3DEX2)");
                        if (((w0 >> 16) & 0xFF) == 0x06) {
                            printf(" SEGMENT %d = 0x%08X", (w0 >> 2) & 0xF, w1);
                        }
                        break;
                    case 0xE4: printf("  (G_TEXRECT)"); break;
                    case 0xE6: printf("  (G_RDPLOADSYNC)"); break;
                    case 0xE7: printf("  (G_RDPPIPESYNC)"); break;
                    case 0xE8: printf("  (G_RDPTILESYNC)"); break;
                    case 0xE9: printf("  (G_RDPFULLSYNC)"); break;
                    case 0xED: printf("  (G_SETSCISSOR)"); break;
                    case 0xEF: printf("  (G_SETOTHERMODE)"); break;
                    case 0xF0: printf("  (G_LOADTLUT)"); break;
                    case 0xF2: printf("  (G_SETTILESIZE)"); break;
                    case 0xF3: printf("  (G_LOADBLOCK)"); break;
                    case 0xF4: printf("  (G_LOADTILE)"); break;
                    case 0xF5: printf("  (G_SETTILE)"); break;
                    case 0xF6: printf("  (G_FILLRECT)"); break;
                    case 0xF7: printf("  (G_SETFILLCOLOR)"); break;
                    case 0xF8: printf("  (G_SETFOGCOLOR)"); break;
                    case 0xF9: printf("  (G_SETBLENDCOLOR)"); break;
                    case 0xFA: printf("  (G_SETPRIMCOLOR)"); break;
                    case 0xFB: printf("  (G_SETENVCOLOR)"); break;
                    case 0xFC: printf("  (G_SETCOMBINE)"); break;
                    case 0xFD: printf("  (G_SETTIMG)"); break;
                    case 0xFE: printf("  (G_SETZIMG)"); break;
                    case 0xFF: printf("  (G_SETCIMG)"); break;
                    default: printf("  (unknown cmd 0x%02X)", cmd); break;
                }
                printf("\n");
                // Stop at G_ENDDL
                if (cmd == 0xB8) break;
            }
            printf("=== END DUMP ===\n\n");
            fflush(stdout);
        }

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

        // NOTE: Auto-advance hacks REMOVED in Session 20
        // The real game flow now handles state transitions:
        // - Boot overlay (func_1B1FB0_802C5BA4) increments fade counter
        // - After 14 frames, calls func_801EB180 which sets state = 2
        // - This properly initializes 20+ variables needed for ovl_i0

        return;
    }

    // STATE 2: Call ovl_i0 overlay (memory card check / intro)
    if (game_state == 2) {
        static int state2_frame = 0;
        state2_frame++;

        printf(">>> [STATE 2] FRAME %d - CALLING func_i0_802C5800 (ovl_i0)...\n", state2_frame);
        printf("    DL input ptr: 0x%08X\n", dl_ptr_in);

        // Check D_801CE63C (boot flag)
        uint32_t boot_flag_val = read_u32(rdram, ADDR_BOOT_FLAG);
        printf("    Boot flag (D_801CE63C) = %d\n", boot_flag_val);

        // Check D_802C6BEC (ovl_i0 internal flag at 0x6BEC offset)
        uint32_t ovl_flag = *(uint32_t*)(rdram + 0x002C6BEC);
        printf("    ovl_i0 flag (D_802C6BEC) = %d\n", ovl_flag);

        // Check D_802C6BC4 (another flag used in func_i0_802C5800)
        uint32_t ovl_flag2 = *(uint32_t*)(rdram + 0x002C6BC4);
        printf("    ovl_i0 flag2 (D_802C6BC4) = %d\n", ovl_flag2);
        fflush(stdout);

        // For debugging: limit to first 3 state 2 frames to avoid crash
        if (state2_frame > 3) {
            printf(">>> [STATE 2] SKIPPING func_i0_802C5800 (frame > 3, preventing crash)\n");
            printf("    Generating placeholder DL instead\n");
            fflush(stdout);

            // Generate a minimal display list
            uint32_t dl_phys = dl_ptr_in & 0x1FFFFFFF;
            uint32_t* dl = (uint32_t*)(rdram + dl_phys);
            int idx = 0;

            // gDPPipeSync
            dl[idx++] = 0xE7000000;
            dl[idx++] = 0x00000000;

            // gSPEndDisplayList
            dl[idx++] = 0xB8000000;
            dl[idx++] = 0x00000000;

            ctx->r2 = ctx->r4 + (idx * 4);
            return;
        }

        // Call the ovl_i0 main display list builder
        printf(">>> About to call func_i0_802C5800...\n"); fflush(stdout);
        func_i0_802C5800(rdram, ctx);
        printf("<<< func_i0_802C5800 returned!\n"); fflush(stdout);

        uint32_t dl_ptr_out = (uint32_t)ctx->r2;
        printf("<<< [STATE 2] RETURNED from func_i0_802C5800\n");
        printf("    DL output: 0x%08X\n", dl_ptr_out);
        fflush(stdout);
        return;
    }

    // OTHER STATES: Need different 0x802C overlays (ovl_i1, etc.)
    if (call_count <= 30 || call_count % DEBUG_INTERVAL == 0) {
        printf("!!! [STATE %d] OVERLAY NOT IMPLEMENTED - generating placeholder DL\n", game_state);
        printf("    Need to implement: ");
        switch (game_state) {
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

// ============================================================================
// func_80095050: unk_game_load - Debug wrapper
// This function is called after state transition to load game assets
// ============================================================================
extern "C" void func_80095050_real(uint8_t* rdram, recomp_context* ctx);

extern "C" void func_80095050(uint8_t* rdram, recomp_context* ctx) {
    static int call_count = 0;
    call_count++;

    // Read D_801CE63C (boot flag)
    uint32_t boot_flag = read_u32(rdram, 0x001CE63C);
    uint32_t game_state = read_u32(rdram, 0x000DAB24);

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║ [GAME-LOAD] func_80095050 (unk_game_load) CALLED #%d\n", call_count);
    printf("║   Game state: %d, Boot flag (D_801CE63C): %d\n", game_state, boot_flag);
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    fflush(stdout);

    // For now, SKIP the real function to see if this is where the crash is
    printf("[GAME-LOAD] SKIPPING func_80095050 (testing if this causes crash)\n");
    fflush(stdout);
    // Don't call the real function for now
    // func_80095050_real(rdram, ctx);
}

// ============================================================================
// ovl_i0 overlay - NOW USING REAL N64Recomp CODE (Session 20)
// ============================================================================
// The ovl_i0 functions (func_i0_802C5800, func_i0_802C5A7C, etc.) are now
// generated by N64Recomp from the ROM binary. They are defined in the
// generated RecompiledFuncs/funcs_*.c files.
//
// Previous stubs were REMOVED because they did nothing (just returned).
// The real code properly:
// - Builds display lists for state 2/3/4
// - Handles state transitions via func_i0_802C6878
// - Checks controller input
// - Manages memory card screens
//
// NOTE: Static variables (static_0_*, static_1_*) are auto-generated by N64Recomp
// Do NOT define them here - they're in the generated funcs_*.c files

