# Session 13: Crash Fix - State Transition to Unimplemented Overlay

**Date:** December 2025
**Status:** FIXED - Game now stable 45+ seconds in state 6

---

## Summary

Fixed a crash that occurred ~30 seconds after startup. The crash was caused by the game attempting to transition from state 6 to state 2, which loads the unimplemented ovl_i0 overlay.

---

## Key Discoveries

### 1. AUTO-ADVANCE State Machine Issue

**Problem:**
The game crashed approximately 30 seconds after startup. Analysis revealed:
- Game was in state 6 (Nintendo logo display)
- AUTO-ADVANCE code was forcing transition to state 2
- State 2 loads ovl_i0 overlay which is NOT implemented

**Evidence from logs:**
```
[STUB] Setting game_state from 6 to 2
[OVL] Loading overlay ovl_i0 to 0x802C5800
... crash ...
```

**Solution:**
Disabled the AUTO-ADVANCE from state 6 to state 2 in `waverace_stubs.cpp`:

```cpp
// AUTO-ADVANCE state 6 -> 2 (to ovl_i0) after 3 frames in state 6
// DISABLED: ovl_i0 not implemented yet, causes crash
// TODO: Implement ovl_i0 overlay (ROM 0x1B3EC0 - 0x1B55A0)
/*
static int state6_frames = 0;
if (game_state == 6) {
    state6_frames++;
    if (state6_frames == 3) {
        // ... code to transition to state 2
    }
}
*/
```

### 2. Overlay System Analysis

Wave Race 64 uses 19 different overlays that all load to the same VRAM address (0x802C5800):

| Overlay | ROM Start | ROM End | Description |
|---------|-----------|---------|-------------|
| ovl_ings | 0x18C340 | 0x197F10 |Ings (intro graphics?) |
| ovl_result | 0x197F10 | 0x1A6560 | Results screen |
| ovl_race | 0x1A6560 | 0x1B3EC0 | Main race |
| ovl_i0 | 0x1B3EC0 | 0x1B55A0 | **NOT IMPLEMENTED** |
| ovl_i1 | 0x1B55A0 | 0x1B6F10 | Unknown |
| ... | ... | ... | ... |

The game state machine controls which overlay is loaded:
- State 5: Initial state
- State 6: Nintendo logo (uses ovl_ings)
- State 2: Menu/intro? (uses ovl_i0) - **CRASHES**

### 3. White Screen After Logo

**Observation:**
User reported seeing the Nintendo logo, then the screen goes white with raster lines appearing outside the screen bounds (top-right and bottom-left).

**Analysis:**
- G_SETSCISSOR commands are correct: `0,0 to 639,479`
- Fade counter increments each frame correctly
- ~500 DL commands generated per frame
- The white screen may be intentional fade-out before next state

**Hypothesis:**
The game is fading to white and waiting for state transition input (button press or timer) that isn't being triggered because we disabled the AUTO-ADVANCE.

---

## Current Status

### Working:
- Nintendo logo renders correctly
- Display lists process without errors
- Segment 8 addresses resolve correctly
- Game runs 45+ seconds stable in state 6
- No crashes from DL parsing

### Issues Remaining:
- White screen after logo (likely waiting for input/transition)
- Raster lines outside screen bounds (viewport issue?)
- ovl_i0 overlay not implemented (state 2)

---

## Files Modified This Session

| File | Change |
|------|--------|
| waverace-recomp/src/game/waverace_stubs.cpp | Disabled AUTO-ADVANCE state 6->2 |
| chris docs/hypotheses/SESSION_13_CRASH_FIX_STATE_TRANSITION.md | This documentation |

---

## Technical Details

### Game State Flow
```
State 5 (init) -> State 6 (logo/ovl_ings) -> State 2 (ovl_i0) [CRASHES]
                                          |
                                          +-> DISABLED TRANSITION
```

### Overlay Loading Mechanism
The game calls overlay loading functions when transitioning states:
1. `func_8016AED0_ovl_init()` - Initialize overlay system
2. `load_overlay_by_id(id)` - Load specific overlay to 0x802C5800
3. Overlay's init function is called
4. Overlay's main loop runs

### ovl_i0 Implementation Requirements
To implement ovl_i0:
1. ROM offset: 0x1B3EC0 - 0x1B55A0 (5856 bytes)
2. VRAM address: 0x802C5800
3. Need to extract and analyze the overlay code
4. Add to overlay system in N64Recomp

---

## Next Steps

### Priority 1: Implement ovl_i0 Overlay
- Extract overlay from ROM
- Analyze functions
- Add to recompilation

### Priority 2: Investigate State 2 Requirements
- What does ovl_i0 do?
- What input triggers state 6 -> 2 transition naturally?

### Priority 3: Fix Raster Lines Outside Screen
- Check viewport settings
- Verify framebuffer dimensions

---

## Key Learnings

1. **State machine analysis is critical** - Understanding which overlay loads for which state prevents crashes
2. **Disable transitions to unimplemented code** - Better to stay in working state than crash
3. **Visual progress is meaningful** - Logo appearing confirms graphics pipeline works
4. **White screen may be intentional** - Could be fade effect waiting for input

---

*Session 13 - Crash Fix State Transition*
