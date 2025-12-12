# Session 11: Display List Segment Fix

**Date:** December 2025
**Status:** MAJOR PROGRESS - Multiple DL fixes, game processes frames

---

## Summary

In this session we fixed multiple issues with display list processing:
1. Fixed segment 8 update to preserve original segment setup (was destroying all segments)
2. Added F3DEX2-style vertex opcode (0x01) support to F3DWAVE GBI
3. Added safety bounds checking to prevent crashes when DL walks into garbage data
4. Added max command limit to prevent infinite DL loops

---

## Key Discoveries

### 1. Segment Fix Was Destroying Original DL

**Problem:**
The previous fix completely overwrote the start of the display list with our segment 8 setup + G_DL + G_ENDDL. This destroyed the original segment setup commands (segments 0-7).

**Original DL structure:**
```
+0x00: gSPSegment(0, ...)
+0x08: gSPSegment(1, ...)
+0x10: gSPSegment(2, ...)  <- needed for vertex data!
+0x18: gSPSegment(3, ...)
+0x20: gSPSegment(7, ...)
+0x28: gSPSegment(8, ...)  <- wrong value, needs fixing
+0x30: gSPSegment(13, ...)
+0x38: gSPSegment(14, ...)
+0x40: G_DL(...)
```

**Solution:**
Instead of replacing the whole DL, we now scan for the segment 8 command and update its value in-place:

```cpp
for (int i = 0; i < 10; i++) {
    if (dl[i*2] == 0xBC002006) {  // G_MOVEWORD for segment 8
        dl[i*2 + 1] = 0x801CAF20;  // Correct segment 8 base
        break;
    }
}
```

### 2. F3DWAVE Uses F3DEX2-Style Vertex Opcode

**Problem:**
F3DWAVE inherits from F3D which uses opcode 0x04 for G_VTX. But Wave Race's display lists use opcode 0x01 (F3DEX2 style) for vertex loading.

**Solution:**
Added a new `vertexF3DEX2` function with F3DEX2-style parsing:

```cpp
void vertexF3DEX2(State *state, DisplayList **dl) {
    uint32_t addr = (*dl)->w1;
    uint8_t vtxCount = (*dl)->p0(12, 8);
    uint32_t dstIndex = (*dl)->p0(1, 7) - vtxCount;
    state->rsp->setVertex(addr, vtxCount, dstIndex);
}
```

And registered it for opcode 0x01:
```cpp
gbi->map[0x01] = vertexF3DEX2;  // F3DEX2-style VTX opcode
```

### 3. DL Walks Into Garbage Data

**Problem:**
The display list parser would walk past the end of valid DL data into garbage memory (floating point data, etc.), causing crashes when trying to draw triangles with invalid vertex indices.

**Solution:**
Added bounds checking to tri1 function:

```cpp
if (v0 >= 48 || v1 >= 48 || v2 >= 48) {
    fprintf(stderr, "[F3DWAVE] tri1: Invalid indices %d,%d,%d - terminating DL\n", v0, v1, v2);
    *dl = nullptr;  // Terminate DL
    return;
}
```

Also added max command limit in the interpreter:
```cpp
int max_commands = 50000;
while (dl != nullptr && cmd_count < max_commands) {
    // process commands...
}
```

---

## Current Status

### Working:
- Segment 8 value corrected in-place
- Original segments (0-7) preserved
- F3DEX2-style vertex loading (opcode 0x01) supported
- Game processes multiple frames (1, 2, 3+)
- Safety bounds checking prevents crashes from garbage data

### Issues Remaining:
- DL still walks into garbage data (root cause not fixed)
- Game crashes after a few frames
- Need to investigate why DL doesn't terminate properly

---

## Files Modified This Session

| File | Change |
|------|--------|
| lib/N64ModernRuntime/librecomp/src/sp.cpp | Changed fix_display_list to update segment 8 in-place |
| lib/rt64/src/gbi/rt64_gbi_f3dwave.cpp | Added vertexF3DEX2 function, tri1 bounds check |
| lib/rt64/src/hle/rt64_interpreter.cpp | Added max command limit, removed debug spam |
| chris docs/hypotheses/SESSION_11_DL_SEGMENT_FIX.md | This documentation |

---

## Technical Details

### Segment Values
```
Segment 2 = 0x0011EDE0 (physical address for vertex data)
Segment 8 = 0x801CAF20 (calculated: 0x802310A0 - 0x66180)
```

### F3DWAVE Vertex Formats
Two vertex formats are used:
1. F3D-style (opcode 0x04): `p0(9,7)` for count, `p0(16,8)/5` for dstIndex
2. F3DEX2-style (opcode 0x01): `p0(12,8)` for count, `p0(1,7)-count` for dstIndex

### Triangle Index Parsing
For F3DWAVE tri1 (opcode 0xBF):
- w1 contains packed vertex indices
- Each index is divided by 5 (Wave Race specific)
- Valid range is typically 0-31 (32 vertices loaded at a time)

---

## Next Steps

### Priority 1: Root Cause of DL Termination Issue
The DL parser walks past valid data because:
- G_ENDDL might not be processed correctly
- Return address stack might be corrupted
- DL branch might go to wrong address

### Priority 2: Visual Output
Need to verify if the game actually renders anything to screen. The display lists ARE being processed.

### Priority 3: Frame Progression
Game only processes ~3 frames before issues. Need to investigate state transitions.

---

## Key Learnings

1. **In-place segment updates** - Don't overwrite entire DL structures, update values in place
2. **Multiple microcode formats** - Wave Race mixes F3D and F3DEX2 command formats
3. **Safety bounds checking** - Essential when DL parsing might fail
4. **Max command limits** - Prevent infinite loops from corrupted DLs

---

*Session 11 - Display List Segment Fix*
