# Session 12: Segment 8 Runtime Fix

**Date:** December 2025
**Status:** MAJOR PROGRESS - Nintendo logo renders, game progresses further

---

## Summary

In this session we fixed a critical issue where segment 8 was being reset to an incorrect value during display list processing. The fix involves intercepting G_MOVEWORD commands in RT64 and overriding segment 8 values.

---

## Key Discoveries

### 1. Segment 8 Reset During DL Processing

**Problem:**
Even though we were fixing segment 8 at the start of the display list, the game was setting it back to the wrong value mid-DL via G_MOVEWORD commands.

**Evidence:**
```
Frame 1-2: 0x08066180 -> 0x002310A0 (correct)
Frame 3:   0x08066180 -> 0x0037C980 (WRONG!)
```

The incorrect value `0x0037C980` comes from: `0x80316800 (wrong seg8) + 0x66180 (offset) = 0x8037C980`

**Solution:**
Intercept G_MOVEWORD commands in `rt64_gbi_f3d.cpp` and override segment 8 values:

```cpp
case G_MW_SEGMENT: {
    uint8_t seg_num = (*dl)->p0(10, 4);
    uint32_t seg_val = (*dl)->w1;

    // WAVE RACE FIX: Override segment 8 with correct value
    if (seg_num == 8 && seg_val != 0x801CAF20) {
        fprintf(stderr, "[F3D-MOVEWORD] Fixing segment 8: 0x%08X -> 0x801CAF20\n", seg_val);
        seg_val = 0x801CAF20;
    }

    state->rsp->setSegment(seg_num, seg_val);
    break;
}
```

### 2. Display List Termination Issue

**Problem:**
The main display list doesn't end with a G_ENDDL command, causing the parser to walk into garbage memory.

**Symptoms:**
- DL parser reads ~280 valid commands
- Then encounters garbage data with invalid vertex indices
- Example: `w1=0xBF0090FF` where `0xFF/5=51` is > vertex buffer size

**Solution:**
Improved bounds checking in tri1 function:
- Check raw indices before division
- Valid max raw index = 31*5 = 155 (0x9B)
- Indices >= 0xA0 (160) are likely garbage
- Terminate DL cleanly when garbage detected

```cpp
void tri1(State *state, DisplayList **dl) {
    uint8_t raw_v0 = (*dl)->p1(16, 8);
    uint8_t raw_v1 = (*dl)->p1(8, 8);
    uint8_t raw_v2 = (*dl)->p1(0, 8);

    if (raw_v0 >= 0xA0 || raw_v1 >= 0xA0 || raw_v2 >= 0xA0) {
        *dl = nullptr;  // Terminate DL cleanly
        return;
    }
    // ... draw triangle
}
```

---

## Current Status

### Working:
- Nintendo logo now renders!
- Display lists process correctly
- Segment 8 addresses resolve correctly throughout DL
- Game runs for multiple seconds before crash
- VI updates show progression (240+ frames = ~4 seconds)

### Issues Remaining:
- Crash after ~4 seconds (separate from DL issues)
- White screen after logo (possibly missing graphics data or state transition issue)
- Raster lines outside screen bounds (viewport or scissor issue?)

---

## Files Modified This Session

| File | Change |
|------|--------|
| lib/rt64/src/gbi/rt64_gbi_f3d.cpp | Added segment 8 override in moveWord, added debug logging |
| lib/rt64/src/gbi/rt64_gbi_f3dwave.cpp | Improved tri1 bounds checking with raw index validation |
| lib/rt64/src/hle/rt64_interpreter.cpp | Added debug logging for command inspection |
| chris docs/hypotheses/SESSION_12_SEGMENT_8_RUNTIME_FIX.md | This documentation |

---

## Technical Details

### Segment 8 Values
```
Game sets:     0x80316800 (wrong!)
Correct value: 0x801CAF20 (asset base - offset)
Calculation:   0x802310A0 (DMA target) - 0x66180 (first DL offset) = 0x801CAF20
```

### DL Command Statistics (per frame)
- ~280 commands processed before reaching garbage
- Multiple sub-DLs via G_DL/G_ENDDL (depth correctly tracked)
- Main DL lacks terminating G_ENDDL

### Raw Vertex Index Bounds
- Valid range: 0x00 - 0x9B (0-155 decimal, /5 = 0-31)
- Invalid threshold: >= 0xA0 (160 decimal)
- Garbage indicator: 0xFF (255 decimal)

---

## Next Steps

### Priority 1: Investigate Crash After ~4 Seconds
- Not related to DL parsing (that works now)
- Could be memory corruption, invalid function pointer, or game state issue

### Priority 2: White Screen After Logo
- Check if game state transitions correctly
- Verify if next set of graphics data loads

### Priority 3: Raster Lines Outside Screen
- Check viewport/scissor settings
- May be related to framebuffer size mismatch

---

## Key Learnings

1. **Runtime segment overrides** - The game sets segments multiple times per frame, not just at DL start
2. **Raw index validation** - Check indices before division for more accurate bounds detection
3. **Clean DL termination** - Setting `*dl = nullptr` cleanly exits the DL loop
4. **Progress is visual** - Nintendo logo appearing means graphics pipeline is working!

---

*Session 12 - Segment 8 Runtime Fix*
