# Session 10: Display List Fix Working

**Datum:** December 2025
**Status:** MAJOR PROGRESS - DL processing now working, game progresses further

---

## Summary

In deze sessie hebben we:
1. De root cause gevonden waarom de DL fix niet werkte (timing issue)
2. De fix verplaatst naar gfx_thread (events.cpp) zodat het net voor send_dl gebeurt
3. Het byte order probleem opgelost (native uint32_t ipv big-endian bytes)
4. **De game verwerkt nu display lists correct!**

---

## Key Discoveries

### 1. Timing Issue - Game Overwrites DL Between Submit and Process

**Probleem:**
De `fix_display_list` functie werd aangeroepen in `osSpTaskStartGo_recomp`, VOOR de task naar de action queue werd gestuurd. Maar:
1. De gfx_thread draait asynchroon
2. De game thread gaat gewoon door na submit
3. De game kan het DL buffer overschrijven voordat gfx_thread het verwerkt

**Oplossing:**
Verplaats de fix naar `gfx_thread_func` in events.cpp, direct VOOR `send_dl()`.

```cpp
// In events.cpp gfx_thread_func(), before send_dl:
extern "C" void fix_display_list_for_rt64(uint8_t* rdram, uint32_t data_ptr);
fix_display_list_for_rt64(rdram, task_action->task.t.data_ptr);
```

### 2. Byte Order Issue - Native vs Big-Endian

**Probleem:**
RT64 leest display lists als native `uint32_t` (little-endian op x86):
```cpp
opCode = (dl->w0 >> 24);  // Expects opcode in high byte of native word
```

Maar ik schreef bytes in big-endian volgorde (N64 format).

**Oplossing:**
Schrijf met native uint32_t pointers, niet bytes:
```cpp
// OLD (wrong - big-endian bytes):
bytes[0] = 0xBC; bytes[1] = 0x00; bytes[2] = 0x20; bytes[3] = 0x06;

// NEW (correct - native uint32_t):
dl[0] = 0xBC002006;  // w0: G_MOVEWORD for segment 8
dl[1] = segment_8_base;  // w1: segment base address
```

### 3. Display List Processing Nu Werkend

RT64 verwerkt nu correct:
```
cmd #0: w0=BC002006 w1=801CAF20 opCode=0xBC  <- gSPSegment(8, 0x801CAF20)
cmd #1: w0=06000000 w1=8011F940 opCode=0x06  <- G_DL -> overlay
cmd #2: w0=06000000 w1=08066180 opCode=0x06  <- G_DL -> segment 8 asset
...
[RT64] processDisplayLists: continuing to process assets...
```

De segment 8 adressen worden nu correct opgelost:
- `0x08066180` = segment 8 (0x801CAF20) + offset 0x66180 = 0x80230AA0

---

## Implementation

### File: lib/N64ModernRuntime/ultramodern/src/events.cpp

Added forward declaration and call:
```cpp
// Forward declaration for display list fix function (defined in sp.cpp)
extern "C" void fix_display_list_for_rt64(uint8_t* rdram, uint32_t data_ptr);

// In gfx_thread_func, before send_dl:
fix_display_list_for_rt64(rdram, task_action->task.t.data_ptr);
```

### File: lib/N64ModernRuntime/librecomp/src/sp.cpp

Changed fix_display_list to write native uint32_t:
```cpp
// Write gSPSegment(8, segment_8_base)
dl[0] = 0xBC002006;  // w0: G_MOVEWORD for segment 8
dl[1] = segment_8_base;  // w1: segment base address

if (overlay_has_content) {
    dl[2] = 0x06000000;  // w0: G_DL opcode
    dl[3] = overlay_dl_start;  // w1: address of overlay DL
    dl[4] = 0xB8000000;  // w0: G_ENDDL
    dl[5] = 0x00000000;  // w1
} else {
    dl[2] = 0xB8000000;  // w0: G_ENDDL
    dl[3] = 0x00000000;  // w1
}
```

Added export function:
```cpp
extern "C" void fix_display_list_for_rt64(uint8_t* rdram, uint32_t data_ptr) {
    fix_display_list(rdram, data_ptr);
}
```

Removed call from osSpTaskStartGo_recomp.

---

## Current Status

### Working:
- gSPSegment(8) injection works
- Overlay DL detection works
- G_DL branching works
- RT64 processes display list commands correctly
- Segment 8 addresses (0x08XXXXXX) resolve correctly

### Still Crashing:
- The game crashes later during DL processing (after many commands)
- This is now a different issue - the segment fix works!
- Crash might be related to other uninitialized segments or invalid data

---

## Test Output

```
[DL-FIX] Frame 3: D_80151944=0x80120908, overlay_start=0x8011F940, has_content=1
[DL-FIX] Adding G_DL call to overlay DL at 0x8011F940
[DL-FIX] DL at 0x8011F8E8 after fix:
  [0] BC002006 801CAF20 (opcode=0xBC)   <- gSPSegment(8, 0x801CAF20)
  [1] 06000000 8011F940 (opcode=0x06)   <- G_DL(overlay)
  [2] B8000000 00000000 (opcode=0xB8)   <- G_ENDDL

[RT64-DL] cmd #0: w0=BC002006 w1=801CAF20 opCode=0xBC  <- Processed!
[RT64-DL] cmd #1: w0=06000000 w1=8011F940 opCode=0x06  <- Branched!
[RT64-DL] cmd #2: w0=06000000 w1=08066180 opCode=0x06  <- Segment 8!
```

---

## Next Steps

### Priority 1: Debug Remaining Crash
- The crash happens later in DL processing
- Might be other uninitialized segments
- Might be invalid vertex/texture data

### Priority 2: Check Other Segments
- Segment 6, 7, 8, 13, 14 might all need initialization
- Check DMA logs for where different assets are loaded

### Priority 3: Test Visual Output
- The game now processes DLs - might actually render something
- Need to check if the logo/graphics appear on screen

---

## Files Modified This Session

| File | Change |
|------|--------|
| lib/N64ModernRuntime/ultramodern/src/events.cpp | Add fix_display_list_for_rt64 call in gfx_thread |
| lib/N64ModernRuntime/librecomp/src/sp.cpp | Fix byte order, add export function, remove from osSpTaskStartGo |
| lib/rt64/src/hle/rt64_interpreter.cpp | Add debug output for DL processing |
| chris docs/hypotheses/SESSION_10_DL_FIX_WORKING.md | This documentation |

---

## Key Learnings

1. **Async threading matters** - Game thread continues after submit, can overwrite DL
2. **Native byte order** - RT64 reads native uint32_t, not N64 big-endian
3. **Timing of fixes** - Apply fixes as late as possible, right before consumption
4. **Incremental progress** - Each fix reveals the next problem

---

*Session 10 - Display List Fix Now Working*
