# Current Tasks - Wave Race 64 Recomp

> **⚠️ VOOR AI/LLM: LEES EERST `chris docs/prompt.md` VOOR PROJECT CONTEXT EN BUILD INSTRUCTIES!**
>
> Die prompt bevat essentiële informatie over:
> - Wat N64Recomp is en hoe het werkt
> - Belangrijke bestandslocaties (decomp, recomp, stubs)
> - Build commando's (WSL)
> - Debug tips en veelvoorkomende problemen
> - Workflow voor het oplossen van issues

**Last Updated:** December 2025 (Session 25)

---

## Priority Tasks (Blokkeren Voortgang)

### 1. [x] State 5→6 Transition - **DONE!**
**Status:** ✅ WORKING (Session 22)

### 2. [x] State 6→2 Transition - **DONE!**
**Status:** ✅ WORKING (Session 24)
**Details:**
- `ovl_func_801EB180` aangepast met custom implementatie
- Twee blokkerende functies gestubbed:
  - `ovl_func_801E6A4C` - Menu/UI setup (was blocking)
  - `func_800C21F4` - Audio init (was blocking)

### 3. [x] DMA Crash After State 2 Transition - **FIXED (Session 25)**
**Status:** ✅ FIXED
**Details:**
- `func_80095050` (unk_game_load) veroorzaakte de crash
- Opgelost door de functie te stubben
- Game komt nu in state 2 en roept `func_i0_802C5800` aan

### 4. [ ] ovl_i0 Crash After 3 Frames - **ACTIVE**
**Status:** ❌ CRASH - Needs investigation
**Details:**
- `func_i0_802C5800` werkt voor 3 frames, dan crash
- Crash gebeurt BINNEN de overlay code
- Mogelijk memory corruption of missing initialization

### 5. [ ] Controller Input (Real Implementation)
**Status:** Low Priority
**Details:**
- Currently simulated with 0xB000 in waverace_stubs.cpp
- Real SDL→N64 controller mapping needed later

---

## Completed (Session 24)

- [x] **STATE 6→2 TRANSITION WORKING!**
- [x] Traced blocking function: `ovl_func_801E6A4C` in `func_801EB180`
- [x] Stubbed `ovl_func_801E6A4C` - Menu/UI setup function
- [x] Stubbed `func_800C21F4` - Audio initialization function
- [x] Implemented custom `ovl_func_801EB180` with debug tracing
- [x] Documented blocking function trace in SESSION_24
- [x] Added auto-compact session doc instructions to prompt.md

## Completed (Session 23)

- [x] Documented fade counter logic and state 6→2 trigger
- [x] Found that frame 15 hang was due to `ovl_func_801EB180` blocking

## Completed (Session 22)

- [x] **ROOT CAUSE IDENTIFIED** - State 5 stuck due to init flag AND controller being 0
- [x] **FIX IMPLEMENTED** - Set D_801CE63C = 1 and controller = 0xB000 on first frame
- [x] Traced ovl_func_802C5BA4 code path in detail
- [x] Found ovl_func_802C5DF4 controller check that blocked state transition
- [x] Found ovl_func_802C7510 (the actual state 5→6 setter)

## Completed (Session 12-21)

- [x] Segment 8 runtime fix (Session 12)
- [x] State transition crash fix (Session 13)
- [x] G_TRI1 dual format fix (Session 14)
- [x] ovl_i0 stubs toegevoegd (Session 15)
- [x] Scheduler blocking analysis (Session 16)
- [x] External message queue deadlock fix (Session 17)
- [x] RT64 null memory DL fix (Session 18)
- [x] Nintendo logo rendert correct
- [x] Display lists verwerken ~6500 commands/frame
- [x] Build instructions fixed (Session 21)

---

## Complete State Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                    WAVE RACE 64 STATE FLOW                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  STATE 5  ──(1 frame)──►  STATE 6  ──(14 frames)──►  STATE 2       │
│  (Boot 1)                 (Logo)                     (Menu/ovl_i0)  │
│  ✅ WORKING               ✅ WORKING                  ❌ CRASH       │
│                                                                     │
│  Transition via: D_801CE63C flag   via: func_801EB180 (custom)     │
│                                                                     │
│  STATE 2  ──(button/timer)──►  STATE 3  ──►  STATE 7              │
│  ovl_i0                        ovl_i0         ovl_i1               │
│  ❌ NOT REACHED                ❌              ❌                    │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## State Machine (func_80092CF0)

```c
switch (D_800DAB24) {
    case 0x5:
    case 0x6:  // Boot/Logo ───────────► ovl_ings (segment_1B1FB0) ✅ WORKING
    case 0x2:  // Menu intro ──────────► ovl_i0 ❌ CRASH AFTER TRANSITION
    case 0x3:  // Menu screens ────────► ovl_i0 ❌ NOT REACHED
    case 0x4:  // Transitions ─────────► ovl_i0 ❌ NOT REACHED
    case 0x7:
    case 0x8:  // Title ───────────────► ovl_i1 ❌ NOT IMPLEMENTED
    // ... many more states
}
```

---

## Gestubde/Custom Functies (Session 24)

| Functie | Type | Reden |
|---------|------|-------|
| `ovl_func_801EB180` | Custom impl | State 6→2 transition met debug |
| `ovl_func_801E6A4C` | Stub | Was blocking - Menu/UI setup |
| `func_800C21F4` | Stub | Was blocking - Audio init |
| `func_800C7020` | Stub | Controller poll (185-hour bug) |
| `func_80046BF4` | Custom impl | DL finalizer null check |
| `func_80092CF0` | Custom impl | Main state machine |

---

## Key Files

| File | Purpose |
|------|---------|
| `waverace.toml` | N64Recomp config, stubs/ignored lists |
| `waverace.syms.toml` | Function definitions |
| `src/game/waverace_stubs.cpp` | Custom stubs and implementations |
| `lib/rt64/src/gbi/rt64_gbi_f3d.cpp` | Segment 8 fix |
| `lib/N64ModernRuntime/librecomp/src/sp.cpp` | DL preprocessing |

---

## Quick Test Commands

```bash
# WSL build & run
cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp
cmake --build build -j24
LIBGL_ALWAYS_SOFTWARE=1 timeout 35 ./build/WaveRace64Recompiled 2>&1 | grep -E '(STATE|TRANSITION|CRASH)'
```

---

*Update dit bestand aan het begin en einde van elke sessie.*
