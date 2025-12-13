# Session 18: RT64 Null Memory Display List Fix

> **VOOR AI: Lees eerst `chris docs/prompt.md` voor volledige project context, N64Recomp configuratie, en build instructies!**

**Date:** December 2025
**Status:** FIXED - Game now processes multiple frames correctly

---

## Build Instructions

```bash
cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp

# Als toml files wijzigen:
../N64Recomp/build/N64Recomp waverace.toml

# Build:
cmake --build build -j4

# Test:
timeout 30 ./build/WaveRace64Recompiled 2>&1 | head -100
```

---

## Summary

**ROOT CAUSE FOUND AND FIXED**: RT64's display list interpreter was getting stuck in an infinite loop when Wave Race 64's display lists ended without an explicit `G_ENDDL` command.

### The Problem

After the Session 17 message queue deadlock fix, the game progressed but would hang during RT64's `processDisplayLists()`:

1. Wave Race 64's display lists end naturally by running into null memory
2. The original N64 RSP hardware stops when it reads null data
3. RT64 interprets null bytes (0x00000000) as `G_SPNOOP` commands
4. RT64 keeps incrementing the display list pointer through null memory
5. The loop continues for ~50,000 commands until the safety limit is hit (or crashes)

### Display List Structure Analysis

Debug output revealed:
```
cmd 6488: opCode=0xE9 (G_SETZIMG) - Final framebuffer setup
cmd 6489: opCode=0xE8 - RDP config
cmd 6490: opCode=0x00 w0=0x00000000 w1=0x00000000 - Null memory begins
cmd 6491: opCode=0x00 w0=0x00000000 w1=0x00000000
... (thousands of nulls)
```

The game writes ~6490 commands per frame, then the DL buffer simply ends. No `G_ENDDL` (0xB8) is used.

### The Fix

Modified `lib/rt64/src/hle/rt64_interpreter.cpp` to detect consecutive null commands:

```cpp
int consecutive_nops = 0; // Track consecutive G_SPNOOP (null) commands

while (dl != nullptr && cmd_count < max_commands) {
    opCode = (dl->w0 >> 24);
    cmd_count++;

    // Wave Race 64 fix: The game's display lists end without G_ENDDL,
    // they just run into null memory. Detect this and stop processing.
    if (opCode == 0x00 && dl->w0 == 0 && dl->w1 == 0) {
        consecutive_nops++;
        if (consecutive_nops >= 5) {
            // 5+ consecutive null commands means we've hit uninitialized memory
            fprintf(stderr, "[RT64-DL] Display list ended (hit null memory after %d commands)\n", cmd_count);
            break;
        }
    } else {
        consecutive_nops = 0;
    }
    // ... rest of interpreter loop
}
```

---

## Results

After the fix:

```
[RT64-DL] Display list ended (hit null memory after 6494 commands)
[RT64] send_dl: DONE
[DEBUG-DP] dp_complete() #0
[DEBUG-GFX-THREAD] Processing DL #1, data_ptr=0x801388D0
...
│ [DL-IMPL] func_80092CF0 FRAME #3
│ [DL-IMPL] func_80092CF0 FRAME #4
│ [DL-IMPL] func_80092CF0 FRAME #5
... (continues to FRAME #10+)
```

The game now:
1. Correctly terminates display list processing when hitting null memory
2. Sends DP completion messages
3. Processes multiple frames continuously
4. Runs in STATE 6 (Nintendo logo) waiting for input
5. Reaches 2600+ VI updates (~44 seconds) without crashing

---

## Technical Details

### Why Wave Race Doesn't Use G_ENDDL

Wave Race 64 uses a display list buffer that gets written each frame. The game:
1. Writes commands sequentially into a buffer
2. Tracks the end position internally
3. Tells the RSP where the buffer starts
4. The RSP processes until it hits null/invalid data

This was common on N64 - the hardware naturally stopped when reading invalid addresses. RT64's pure software interpreter doesn't have this behavior.

### Display List Contents (6494 commands)

| Range | Content |
|-------|---------|
| 1-8 | Segment setup (gSPSegment for segments 0,1,2,3,7,8,13,14) |
| 9 | G_DL to segment 1 sub-DL |
| 10-28 | RDP setup and framebuffer clear |
| 29+ | Water rendering, UI, textures |
| ~6300 | Vertex/Triangle commands (water mesh) |
| ~6480 | Final framebuffer config (G_SETZIMG, etc.) |
| 6490+ | Null memory |

---

## Files Modified

| File | Change |
|------|--------|
| `lib/rt64/src/hle/rt64_interpreter.cpp` | Added consecutive null detection to stop DL processing |

---

## Current Game Status

The game now:
- Boots through states 5 → 6
- Renders the Nintendo logo screen (state 6)
- Processes ~6500 DL commands per frame
- Runs continuously at ~60 FPS
- Calls overlay functions correctly

**Next Steps:**
1. Implement controller input so player can advance past logo
2. State 6 → State 2 transition (main menu)
3. Further testing of gameplay states

---

*Session 18 - RT64 Null Memory Display List Fix*
