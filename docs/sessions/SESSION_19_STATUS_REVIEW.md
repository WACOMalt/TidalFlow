# Session 19: Status Review & Debug Cleanup

> **VOOR AI: Lees eerst `chris docs/prompt.md` voor volledige project context, N64Recomp configuratie, en build instructies!**

**Date:** December 2025
**Status:** DOCUMENTATION UPDATE - Game runs stable in State 6

---

## Build Instructions

```bash
cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp

# Build:
cmake --build build -j4

# Test:
timeout 15 ./build/WaveRace64Recompiled 2>&1 | grep -E '(FRAME|DL-IMPL)'
```

---

## Summary

This session focused on:
1. Reviewing all session documentation (Sessions 12-18)
2. Testing debug print timing theory
3. Analyzing decomp for state machine logic
4. Updating all chris docs to current status

---

## Key Findings

### 1. Debug Prints Are NOT Causing Timing Issues

The user suspected that debug `fprintf()` calls were necessary for the Nintendo logo to render (race condition theory). Testing proved this FALSE:

```cpp
// Disabled these in rt64_gbi_f3d.cpp and rt64_interpreter.cpp:
// fprintf(stderr, "[RT64-SEG] Segment %d set to 0x%08X\n", ...);
// fprintf(stderr, "[F3D-SETTIMG #%d] addr=0x%08X ...\n", ...);
// fprintf(stderr, "[RT64-DL-DBG] cmd %d: opCode=0x%02X ...\n", ...);
```

**Result:** Game still runs correctly without debug prints. No race condition exists.

### 2. State Machine Analysis from Decomp

From `src/game/code_4C750.c` - `func_80092CF0()`:

```c
switch (D_800DAB24) {  // Game state variable
    case 0x5:
    case 0x6:  // <- CURRENT STATE (Nintendo logo)
        dList = func_802C5BA4(dList);  // ovl_ings overlay
        break;

    case 0x2:  // <- NEXT STATE (menu)
        dList = func_802C5800(dList);  // ovl_i0 overlay - NEEDS IMPLEMENTATION
        break;

    case 0x7:
    case 0x8:  // Title screen
        dList = func_802C913C(dList);  // ovl_i1 overlay
        break;
}
```

### 3. Overlay Requirements

From `src/ovl_table.c`:

| State | Overlay | ROM | VRAM | Status |
|-------|---------|-----|------|--------|
| 5, 6 | ovl_ings (segment_1B1FB0) | 0x1B1FB0 | 0x802C5800 | ✅ Working |
| 2 | ovl_i0 | 0x1B3EC0 | 0x802C5800 | ⚠️ Stubs only |
| 7, 8 | ovl_i1 | 0x1B55A0 | 0x802C5800 | ❌ Not implemented |

**Conclusion:** To progress beyond state 6, we need EITHER:
1. Real ovl_i0 implementation (recompiled overlay)
2. State bypass (force state 6 → 7 transition)
3. Controller input (if transition is button-triggered)

---

## Files Modified This Session

| File | Change |
|------|--------|
| `lib/rt64/src/gbi/rt64_gbi_f3d.cpp` | Disabled segment 8 and texture debug logs |
| `lib/rt64/src/hle/rt64_interpreter.cpp` | Disabled DL command debug logs |
| `lib/N64ModernRuntime/librecomp/src/sp.cpp` | Disabled DL fix debug logs |
| `chris docs/PROJECT_STATUS.md` | Updated to Session 19 |
| `chris docs/hypotheses/CURRENT_TASKS.md` | Updated with decomp analysis |
| `chris docs/hypotheses/SESSION_19_STATUS_REVIEW.md` | This document |

---

## Current Game Status

### Working:
- Nintendo logo renders correctly
- Display lists process ~6500 commands/frame
- Segment 8 addressing fixed (0x801CAF20)
- Textures load at correct addresses (0x80215380-0x80219380)
- No crashes, no hangs, no infinite loops
- Runs stable in state 6

### Not Working:
- State 6 → 2 transition (ovl_i0 not implemented)
- Controller input (not reading button presses)
- Game progression beyond Nintendo logo

---

## Next Steps

### Priority 1: Implement ovl_i0 or State Bypass

**Option A: Recompile ovl_i0**
```toml
# Add to waverace.syms.toml:
[[section]]
name = ".ovl_i0"
rom = 0x1B3EC0
vram = 0x802C5800
size = 0x16E0  # 0x1B55A0 - 0x1B3EC0

functions = [
    { name = "func_802C5800", vram = 0x802C5800, size = 0x??? },
    # ... more functions from decomp
]
```

**Option B: Force State Transition**
```cpp
// In waverace_stubs.cpp, add timer to force state 6 → 7:
static int state6_frames = 0;
if (game_state == 6 && ++state6_frames > 180) {
    game_state = 7;  // Skip to title screen (needs ovl_i1)
}
```

### Priority 2: Controller Input

Check how other recomp projects handle controller:
- `mk64recomp/` - Template project
- `zelda64recomp/` - May have working controller

---

## Technical Notes

### Why All Overlays Load to 0x802C5800

Wave Race 64 uses a single overlay slot for all menu/UI overlays. Only one overlay can be active at a time. The game:
1. Unloads current overlay
2. DMAs new overlay from ROM to 0x802C5800
3. Calls overlay init function
4. Overlay functions are now available

This is different from games like Zelda OoT which have multiple overlay slots.

### Segment 8 Purpose

Segment 8 is set by the game to `D_800D45F0` (hardcoded 0x80316800 in ROM data). This is WRONG because assets are loaded via DMA to 0x802310A0.

Our fix overrides segment 8 to 0x801CAF20:
- Asset base: 0x802310A0
- First texture offset: 0x4A460
- Correct segment 8: 0x802310A0 - 0x66180 = 0x801CAF20
- Result: 0x801CAF20 + 0x4A460 = 0x80215380 (in DMA range)

---

*Session 19 - Status Review & Debug Cleanup*
