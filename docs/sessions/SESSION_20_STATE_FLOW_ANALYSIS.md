# Session 20: Complete State Flow Analysis

> **VOOR AI: Lees eerst `chris docs/prompt.md` voor volledige project context, N64Recomp configuratie, en build instructies!**

**Date:** December 2025
**Status:** ANALYSIS COMPLETE - Implementing Option A (real N64Recomp)

---

## Build Instructions

```bash
cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp

# If syms.toml or waverace.toml changed, run N64Recomp first:
../N64Recomp/build/N64Recomp waverace.toml

# Build:
cmake --build build -j4

# Test:
timeout 15 ./build/WaveRace64Recompiled 2>&1 | grep -E '(FRAME|DL-IMPL|STATE)'
```

---

## Summary

Dit session bevat een complete analyse van de Wave Race 64 state machine, getraceerd vanuit de decomp naar onze recomp implementatie. **Belangrijkste bevinding:** Onze implementatie bypassed de echte game flow met een hack!

---

## Complete State Flow

### Boot Sequence

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    WAVE RACE 64 BOOT/STATE FLOW                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────┐    auto (1 frame)    ┌──────────┐                            │
│  │ STATE 5  │ ──────────────────►  │ STATE 6  │                            │
│  │ Boot 1   │                      │ Boot 2   │                            │
│  │ Clear FB │                      │ Nintendo │                            │
│  └──────────┘                      │ Logo     │                            │
│       │                            └────┬─────┘                            │
│       │                                 │                                   │
│       │                                 │ D_802C76F4 >= 14 (fade timer)    │
│       │                                 │ OR button press (0xB000)         │
│       │                                 ▼                                   │
│       │                            ┌──────────┐                            │
│       │                            │ STATE 2  │ ◄── NEEDS ovl_i0!          │
│       │                            │ Menu     │                            │
│       │                            │ (ovl_i0) │                            │
│       │                            └────┬─────┘                            │
│       │                                 │                                   │
│       │                                 │ button press OR timer            │
│       │                                 ▼                                   │
│       │                            ┌──────────┐                            │
│       │                            │ STATE 3  │                            │
│       │                            │ Menus    │                            │
│       │                            │(ovl_i0)  │                            │
│       │                            └────┬─────┘                            │
│       │                                 │                                   │
│       │                                 ▼                                   │
│       │                            ┌──────────┐                            │
│       │                            │ STATE 7  │                            │
│       │                            │ Title    │                            │
│       │                            │ (ovl_i1) │                            │
│       │                            └──────────┘                            │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## KRITIEK: Vergelijking Decomp vs Onze Recomp

### 1. State 6 → 2 Transition

| Aspect | Decomp (echte game) | Onze Recomp |
|--------|---------------------|-------------|
| **Trigger** | Fade counter `D_802C76F4 >= 14` | Timer `state6_frames == 180` |
| **Functie** | `func_801EB180()` wordt aangeroepen | We schrijven direct `D_800DAB24 = 2` |
| **Variabelen** | Zet 20+ variabelen (gPlayers, gRiders, etc.) | Zet alleen state + boot flag |
| **Locatie** | In boot overlay (`func_1B1FB0_802C5BA4`) | In `waverace_stubs.cpp:425` |

### 2. ovl_i0 Implementatie

| Aspect | Decomp | Onze Recomp |
|--------|--------|-------------|
| **Status** | Echte code in `ovl_1B3EC0.c` | **LEGE STUBS** in `waverace_stubs.cpp:628` |
| **In syms.toml** | N/A | Section aanwezig (`.ovl_802C_ovl_i0`) |
| **In waverace.toml** | N/A | **IN `ignored` LIJST!** |
| **func_i0_802C5800** | Bouwt DL, checkt buttons, calls `func_i0_802C6878` | `ctx->r2 = ctx->r4` (doet niets) |

### 3. func_801EB180 (State Transition)

| Aspect | Decomp | Onze Recomp |
|--------|--------|-------------|
| **In syms.toml** | N/A | `ovl_func_801EB180` at 0x801EB180, size 0x374 |
| **In ignored?** | N/A | Niet in ignored - zou moeten werken! |
| **Roept aan** | Wordt door boot overlay aangeroepen | Wordt **NIET** aangeroepen door onze hack |

---

## Het Probleem: Onze Hack Bypassed de Echte Game Flow

```
DECOMP FLOW (correct):
┌─────────────────────────────────────────────────────────────────────────────┐
│  boot overlay (func_1B1FB0_802C5BA4)                                        │
│       │                                                                     │
│       │ fade counter >= 14                                                  │
│       ▼                                                                     │
│  func_801EB180()                                                            │
│       │                                                                     │
│       │ Sets: D_800DAB24 = 2                                               │
│       │       D_801CE630 = 0                                               │
│       │       D_801CE638 = 0                                               │
│       │       D_801CE63C = 1 (boot flag)                                   │
│       │       D_800DAB1C = 0                                               │
│       │       D_800D461C = 3                                               │
│       │       gPlayers = 1                                                 │
│       │       gRiders = 2                                                  │
│       │       gGameModes = 0                                               │
│       │       D_800D49B0 = 0x14                                            │
│       │       D_800D8174 = 5                                               │
│       │       D_801CE728 = 3                                               │
│       │       ... 10+ more variables ...                                   │
│       ▼                                                                     │
│  State 2 with CORRECT initialization                                        │
└─────────────────────────────────────────────────────────────────────────────┘

ONZE HACK (incorrect):
┌─────────────────────────────────────────────────────────────────────────────┐
│  waverace_stubs.cpp:func_80092CF0()                                         │
│       │                                                                     │
│       │ static int state6_frames = 0;                                      │
│       │ if (game_state == 6) {                                             │
│       │     state6_frames++;                                               │
│       │     if (state6_frames == 180) {  // 3 seconds!                     │
│       │                                                                     │
│       │         write_u32(rdram, ADDR_GAME_STATE, 2);    // ONLY this      │
│       │         write_u32(rdram, ADDR_BOOT_FLAG, 1);     // and this       │
│       │                                                                     │
│       │         // MISSING: 18+ other variables!                           │
│       ▼                                                                     │
│  State 2 with BROKEN initialization                                         │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Waarom ovl_i0 Niet Werkt

1. **Functions in `ignored` list** - N64Recomp genereert GEEN code voor deze functies
2. **Lege stubs in waverace_stubs.cpp** - Onze stubs doen letterlijk niets
3. **Missende variabelen** - `func_801EB180` wordt niet echt aangeroepen, dus 18+ variabelen zijn niet geïnitialiseerd

**Bewijs uit waverace.toml:**
```toml
ignored = [
    ...
    # ovl_i0 overlay functions - custom stubs in waverace_stubs.cpp
    "func_i0_802C5800",   # ← DEZE WORDEN GENEGEERD!
    "func_i0_802C5A7C",
    "func_i0_802C6044",
    "func_i0_802C63AC",
    "func_i0_802C6878",
    "func_i0_802C6944",
    "func_i0_802C6A1C",
    "func_i0_802C6AE4",
]
```

**En de lege stub in waverace_stubs.cpp:**
```cpp
extern "C" void func_i0_802C5800(uint8_t* rdram, recomp_context* ctx) {
    static int call_count = 0;
    call_count++;
    if (call_count <= 5 || call_count % 60 == 0) {
        printf("[OVL_I0] func_i0_802C5800 STUB (main DL builder) call #%d\n", call_count);
    }
    // Return display list pointer unchanged  ← DOET NIETS!
    ctx->r2 = ctx->r4;
}
```

---

## Key Discoveries

### 1. State Transition Trigger (6 → 2)

From decomp analysis of `func_1B1FB0_802C5BA4` (boot overlay):

```assembly
# In func_1B1FB0_802C5BA4 (at 0x802C5DC4):
lw     $t2, D_1B1FB0_802C76F4  # Load fade counter
slti   $at, $t2, 0xE           # Check if counter < 14
bnel   $at, $zero, .skip       # If < 14, skip transition
...
jal    func_801EB180           # Call state transition function!
```

**Conclusion:** After 14 frames in state 6, `func_801EB180` is called which sets `D_800DAB24 = 2`.

### 2. func_801EB180 Sets State 2

From `asm/nonmatchings/codeseg/B97B0/func_801EB180.s`:

```assembly
glabel func_801EB180
    lui    $v0, %hi(D_800DAB24)
    addiu  $v0, $v0, %lo(D_800DAB24)
    ...
    addiu  $v1, $zero, 0x2     # v1 = 2
    ...
    sw     $v1, 0x0($v0)       # D_800DAB24 = 2   ← STATE 2!
```

### 3. Main Render Loop (sys_main.c)

```c
// From src/game/sys_main.c line 197:
for (D_80151960 = 0; ; D_80151960++) {
    osContStartReadData(&D_801540D0);      // Read controller
    osRecvMesg(&D_80154100, D_80151958, OS_MESG_BLOCK);
    func_80047B00();
    func_80046D2C();
    func_800922E4();
    func_80046850();
    func_800468E0();

    gDisplayListHead = func_80092CF0(gDisplayListHead);  // ← MAIN STATE MACHINE

    func_80046BF4();                        // End display list
    osRecvMesg(&D_80154118, D_8015195C, OS_MESG_BLOCK);

    // Handle state transitions, overlay loading, etc.
    if (D_801CE63C != 0) {
        unk_game_load();
    }
    if (D_801CE63C != 0) {
        Overlay_Load();
    }
    // ...
}
```

### 4. State Machine (func_80092CF0)

```c
// From src/game/code_4C750.c lines 154-300:
Gfx* func_80092CF0(Gfx* dList) {
    switch (D_800DAB24) {
        case 0x0:  // Gameplay/racing
            dList = func_80093F78(dList);
            func_801EB180();  // or func_801ECAF4()
            break;

        case 0x5:
        case 0x6:  // Boot states (Nintendo logo) ← WE ARE HERE
            dList = func_802C5BA4(dList);  // ovl_ings (segment_1B1FB0)
            break;

        case 0x2:  // Menu intro ← NEXT STATE (needs ovl_i0)
            dList = func_802C5800(dList);  // ovl_i0
            break;

        case 0x3:  // Menu screens (uses ovl_i0)
            dList = func_802C5A7C(dList);  // ovl_i0
            break;

        case 0x4:  // Transition (uses ovl_i0)
            dList = func_802C6944(dList);  // ovl_i0
            break;

        case 0x7:
        case 0x8:  // Title screen
            dList = func_802C913C(dList);  // ovl_i1
            break;

        // ... many more states (0x28, 0x29, 0x2D, 0x36, etc.)
    }
    return dList;
}
```

---

## What We Have vs What Decomp Shows

### Our Recomp Implementation

| Component | Status | Location |
|-----------|--------|----------|
| func_80092CF0 | Custom stub | `waverace_stubs.cpp:228` |
| ovl_func_802C5BA4 | REAL recompiled | Via N64Recomp |
| func_i0_802C5800 | Stub (empty) | `waverace_stubs.cpp` |
| func_801EB180 | REAL recompiled | Main code section |

---

## ovl_i0 Functions Needed

From decomp `src/overlays/ovl_i0/ovl_1B3EC0.c`:

| Function | VRAM | Description | Status |
|----------|------|-------------|--------|
| func_i0_802C5800 | 0x802C5800 | Main DL builder for state 2 | STUB |
| func_i0_802C5A7C | 0x802C5A7C | DL builder for state 3 | STUB |
| func_i0_802C6044 | 0x802C6044 | Unknown (asm only) | STUB |
| func_i0_802C63AC | 0x802C63AC | Unknown (asm only) | STUB |
| func_i0_802C6878 | 0x802C6878 | Transition to state 3 | STUB |
| func_i0_802C6944 | 0x802C6944 | DL builder for state 4 | STUB |
| func_i0_802C6A1C | 0x802C6A1C | Transition to state 4 | STUB |
| func_i0_802C6AE4 | 0x802C6AE4 | Unknown (asm only) | STUB |

### Key Logic in func_i0_802C5800

```c
Gfx* func_i0_802C5800(Gfx* arg0) {
    if (D_801CE63C != 0) {
        D_801CE63C = 0;
        if (D_80154344 == 0) {
            D_i0_802C6BEC = 1;
        }
        arg0 = func_80093C44(arg0);
        return arg0;
    }

    arg0 = func_8009328C(arg0);
    gSPDisplayList(arg0++, &D_805AF88);
    gSPDisplayList(arg0++, &D_106F168);

    // ... display memory card message ...

    // Button check: 0xB000 = Start+A+B
    if ((D_801CE65A->unk0 & 0xB000) || (D_800DAB0C != 0)) {
        D_800DAB0C = 0;
        func_i0_802C6878();  // Transition to state 3
    }

    return arg0;
}
```

---

## Overlay Memory Layout

All menu overlays share the SAME VRAM address 0x802C5800:

```
ROM Layout:
├── 0x1B1FB0 - 0x1B3EC0: ovl_ings (boot) - LOADED for state 5,6
├── 0x1B3EC0 - 0x1B55A0: ovl_i0 (menu) - NEEDED for state 2,3,4
├── 0x1B55A0 - 0x1B9440: ovl_i1 (title) - NEEDED for state 7,8
├── 0x1B9440 - 0x1BC890: ovl_i2
└── ... more overlays ...

All load to VRAM 0x802C5800!
```

---

## The Fix: Option A (Real N64Recomp)

### Step 1: Remove ovl_i0 functions from `ignored` list in waverace.toml

```toml
# REMOVE THESE from ignored list:
# "func_i0_802C5800",
# "func_i0_802C5A7C",
# "func_i0_802C6044",
# "func_i0_802C63AC",
# "func_i0_802C6878",
# "func_i0_802C6944",
# "func_i0_802C6A1C",
# "func_i0_802C6AE4",
```

### Step 2: Remove auto-advance hack from waverace_stubs.cpp

Remove lines ~419-437 that force state 6 → 2.

### Step 3: Remove empty stubs from waverace_stubs.cpp

Remove the empty `func_i0_*` functions (lines ~628-665).

### Step 4: Run N64Recomp

```bash
cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp
../N64Recomp/build/N64Recomp waverace.toml
```

### Step 5: Build and test

```bash
cmake --build build -j4
./build/WaveRace64Recompiled
```

---

## Files Read This Session

| File | Purpose |
|------|---------|
| `src/game/sys_main.c` | Main game loop |
| `src/game/code_4C750.c` | State machine (func_80092CF0) |
| `src/overlays/ovl_i0/ovl_1B3EC0.c` | ovl_i0 decomp |
| `src/unused_code_1B1FB0.c` | Boot overlay (ovl_ings) |
| `src/codeseg/B97B0.c` | State transition functions |
| `asm/.../func_1B1FB0_802C5BA4.s` | Boot overlay assembly |
| `asm/.../func_801EB180.s` | State 6->2 transition |
| `waverace_stubs.cpp` | Our custom implementations |

---

## Key Variables

| Variable | Address | Purpose |
|----------|---------|---------|
| D_800DAB24 | 0x800DAB24 | Game state (0=gameplay, 5/6=boot, 2=menu, etc.) |
| D_801CE63C | 0x801CE63C | Boot/init flag (1=first frame of state) |
| D_801CE65A | 0x801CE65A | Controller input struct |
| D_802C76F4 | 0x802C76F4 | Fade counter in boot overlay |

---

*Session 20 - Complete State Flow Analysis + Comparison*
