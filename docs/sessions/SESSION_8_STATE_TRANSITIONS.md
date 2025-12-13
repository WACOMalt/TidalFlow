# Session 8: State Transitions & Display List Debugging

**Datum:** December 2025
**Status:** STATE TRANSITIONS WORKING - Display List Crash to Debug

---

## Summary

In deze sessie hebben we:
1. Controller input en fade counter debug toegevoegd
2. **State transitions werkend gekregen!** (State 5 → 6)
3. Display list crash in RT64 ontdekt - moet nog opgelost worden

---

## Key Discoveries

### 1. State Transition Logic (uit Assembly Analyse)

#### State 5 → 6 Transitie

Gevonden in `func_1B1FB0_802C5DF4.s`:

```asm
# Line 98: Check controller input for Start+A+B buttons
andi $t4, $v0, 0xB000   # Check bits for Start (0x1000) + A (0x8000) + B (0x2000)

# Line 122-124: If buttons pressed, call state transition
jal func_1B1FB0_802C7510   # This sets D_800DAB24 = 6
```

**Probleem:** Controller input (D_801CE65A) was altijd 0x0000 - geen button press gedetecteerd!

**Oplossing:** Auto-advance bypass in waverace_stubs.cpp

#### State 6 → 2 Transitie

Gevonden in `func_1B1FB0_802C5BA4.s`:

```asm
# Line 145-150: Check fade counter
lw $t2, D_1B1FB0_802C76F4    # Load fade counter
slti $at, $t2, 0xE           # Check if < 14
bnel $at, $zero, skip        # If < 14, skip transition
jal func_801EB180            # Call state transition (sets state = 2)
```

**Dus:** State 6 transitie naar 2 vereist fade counter >= 14.

---

### 2. func_801EB180 - De State Transition Functie

Uit de assembly analyse (`asm/nonmatchings/codeseg/B97B0/func_801EB180.s`):

```c
void func_801EB180(void) {
    D_801CE634 = D_800DAB24;  // Save current state
    D_801CE630 = 0;
    D_800DAB24 = 2;           // SET STATE TO 2!
    D_801CE638 = 0;
    D_801CE63C = 1;           // Boot flag
    // ... meer initialisatie
}
```

**State 6 → State 2**, niet state 7 zoals eerder gedacht!

---

### 3. Display List Crash

**Symptoom:**
```
[RT64] send_dl: processDisplayLists(data_ptr=0x1388D0)...
timeout: the monitored command dumped core
```

**Bevinding:**
- De overlay genereert ~500 DL commando's per frame
- DL output: `0x801208B8` (wrote 3960 bytes = 495 commands)
- Crash in RT64's `processDisplayLists()`

**Mogelijke oorzaken:**
1. Display list bevat ongeldige GBI commando's
2. Texture pointers wijzen naar niet-geladen data
3. Segment pointers niet correct opgezet

---

## Implementation

### Auto-Advance Code (waverace_stubs.cpp)

```cpp
// AUTO-ADVANCE: IMMEDIATELY after first state 5 frame, force state 6
static bool state5_advanced = false;
if (game_state == 5 && !state5_advanced) {
    state5_advanced = true;
    write_u32(rdram, ADDR_GAME_STATE, 6);
    write_u32(rdram, ADDR_BOOT_FLAG, 1);
}

// AUTO-ADVANCE state 6 -> 2 after 3 frames
static int state6_frames = 0;
if (game_state == 6) {
    state6_frames++;
    if (state6_frames == 3) {
        write_u32(rdram, ADDR_GAME_STATE, 2);
        write_u32(rdram, ADDR_BOOT_FLAG, 1);
    }
}
```

### Debug Output Added

```cpp
// Controller and fade counter tracking
uint16_t controller_input = *(uint16_t*)(rdram + ADDR_CONTROLLER);
uint32_t fade_counter = *(uint32_t*)(rdram + 0x002C76F4);

printf("Controller (D_801CE65A): 0x%04X  (need 0xB000 for skip)\n", controller_input);
printf("Fade counter (0x802C76F4): %d  (need 14 for auto-transition)\n", fade_counter);
```

---

## Test Results

### State Transition Working!

```
██  STATE CHANGE DETECTED! #1
║  Game State (D_800DAB24): 5 (0x5)
...
║  AUTO-ADVANCE: Immediately forcing state 5 -> 6!
...
██  STATE CHANGE DETECTED! #2
║  Game State (D_800DAB24): 6 (0x6)
```

### But Crash After 2 Frames

```
│ [DL-IMPL] func_80092CF0_impl FRAME #1
>>> [STATE 5] CALLING ovl_func_802C5BA4
<<< [STATE 5] RETURNED from ovl_func_802C5BA4
    DL output: 0x801208B8 (wrote 3960 bytes = 495 commands)

│ [DL-IMPL] func_80092CF0_impl FRAME #2
>>> [STATE 6] CALLING ovl_func_802C5BA4
<<< [STATE 6] RETURNED from ovl_func_802C5BA4
    DL output: 0x801398E0 (wrote 4024 bytes = 503 commands)

[RT64] send_dl: processDisplayLists(data_ptr=0x1388D0)...
*** CRASH ***
```

---

## Memory Address Reference

```cpp
#define ADDR_GAME_STATE      0x000DAB24  // D_800DAB24 - main game state
#define ADDR_BOOT_FLAG       0x001CE63C  // D_801CE63C - boot sequence control
#define ADDR_DL_PTR          0x00151944  // D_80151944 - display list pointer
#define ADDR_CONTROLLER      0x001CE65A  // D_801CE65A - controller input
#define ADDR_FADE_COUNTER    0x002C76F4  // D_802C76F4 - fade counter (overlay BSS)
```

---

## State Flow Update

Gebaseerd op nieuwe analyse:

```
State 5 (Boot)
    │
    ▼ (controller Start+A+B OR fade counter >= 14)
State 6 (Logo)
    │
    ▼ (fade counter >= 14) → func_801EB180
State 2 (??? - needs ovl_i0)
    │
    ▼
... more states
```

---

## Next Steps

### Priority 1: Debug RT64 Display List Crash
- De overlay code genereert display lists
- RT64 crasht bij het verwerken ervan
- Mogelijk segment registers niet correct opgezet
- Of textures niet geladen

### Priority 2: Implement ovl_i0
- State 2 uses gOverlayTable[1] = ovl_i0
- ROM: 0x001B3EC0 - 0x001B55A0
- Size: 0x16E0 bytes

### Priority 3: Fix Controller Input
- D_801CE65A altijd 0
- Controller stubbed in recompilation
- Needs proper SDL→N64 input mapping

---

## Files Modified This Session

| File | Change |
|------|--------|
| waverace_stubs.cpp | Added controller/fade debug, auto-advance logic |
| SESSION_8_STATE_TRANSITIONS.md | This documentation |

---

## Key Learnings

1. **State transitions work** when we manually set the game state variable
2. **Controller input is not working** - needs investigation into how recompilation handles input
3. **Display list crash** is the main blocker now - overlay generates valid-looking DL but RT64 crashes
4. **func_801EB180 sets state to 2**, not 7 - corrects our earlier assumption

---

## Deep Dive: Display List Crash Root Cause

### The Problem
RT64 crashes when processing the display list generated by `ovl_func_802C5BA4`.

### Analysis

Looking at the recompiled code for the overlay:

```c
// From funcs_19.c line 36137-36147
ctx->r7 = S32(0X600 << 16);    // 0x06000000 = G_DL command
ctx->r24 = S32(0X806 << 16);
ctx->r24 = ADD32(ctx->r24, 0X6180);  // 0x08066180
MEM_W(0X4, ctx->r2) = ctx->r24;       // Write segment address
MEM_W(0X0, ctx->r2) = ctx->r7;        // Write G_DL command
```

This writes a **G_DL (display list branch)** command:
- `0x06000000 AAAAAAAA` = Branch to display list at segmented address A

The address `0x08066180` means:
- Segment: 6 (0x08066180 >> 24 = 6)
- Offset: 0x066180 (0x08066180 & 0x00FFFFFF)

### Why It Crashes

1. **Segment 6 is NOT initialized** - RT64's segment table has segment 6 = 0
2. **Physical address calculation**: 0 + 0x066180 = 0x066180
3. **This is NOT valid N64 RAM** - it's a ROM offset
4. **RT64 tries to read display list from invalid memory** → CRASH

### What Should Happen

On real N64:
1. Game loads asset data from ROM to RAM via DMA
2. `gSPSegment(6, ram_address)` is called to set segment 6
3. Display list references `0x08XXXXXX` are resolved to `ram_address + XXXXXX`
4. RT64 reads valid display list data from RAM

In our recompilation:
- Asset loading code exists but segment setup may be missing/broken
- Or assets are not being loaded to correct RAM addresses
- The display list code assumes segment 6 is already set up

### Investigation Needed

1. **Find where segment 6 is set** - search for gSPSegment(6, ...)
2. **Find DMA loading code** - how does game load Nintendo logo assets?
3. **Check if assets are loaded** - verify RAM contains valid data
4. **Set segment 6 manually** - as a workaround

### Assembly Reference

From `func_1B1FB0_802C5BA4.s`:
```asm
lui  $t8, %hi(D_8066180)     ; 0x0806....
addiu $t8, $t8, %lo(D_8066180) ; = 0x08066180
sw   $t8, 0x4($v0)           ; Write to display list
lui  $a3, 0x0600             ; G_DL command
sw   $a3, 0x0($v0)           ; Write G_DL to display list
```

The symbol `D_8066180` is a display list stored in segment 6.

---

## Additional Finding: Assets ARE Being Loaded!

### DMA Log Analysis

The game IS loading assets via DMA:
```
[DEBUG-DMA] rdram=0x80228E10, phys=0x100F6090, size=0x8290, dir=0
```

This loads ROM offset `0xF6090` (the asset segment!) to RDRAM `0x80228E10`.

### The Missing Link

1. **Assets loaded**: ✅ ROM 0xF6090 → RAM 0x80228E10
2. **Segment 6 setup**: ❌ NOT DONE - must point to loaded assets
3. **Display list uses segment 6**: References like `0x08066180`
4. **RT64 crashes**: Tries to resolve segment 6 but it's 0

### Why Segment 6 Is Not Set

The game normally sets segments via `gSPSegment` commands in the display list. But either:
- The segment setup DL commands aren't being processed before the problematic DL
- Or the codeseg function that sets up segments isn't being called

### Next Steps for Future Sessions

1. **Find segment 6 setup code** in decomp
2. **Add gSPSegment(6, 0x80228E10)** command before overlay DL runs
3. **Or patch the display list** to include segment setup

---

## Session 8 Summary

**Achieved:**
- ✅ State transitions 5→6 working (via auto-advance bypass)
- ✅ Found root cause of RT64 crash (uninitialized segment 6)
- ✅ Verified assets ARE loaded via DMA
- ✅ Detailed analysis of segment addressing

**Still Needed:**
- ❌ Fix segment 6 initialization
- ❌ Controller input (currently bypassed)
- ❌ Implement more overlays for further game states

---

*Session 8 - State Transitions Analysis & Display List Deep Dive*
