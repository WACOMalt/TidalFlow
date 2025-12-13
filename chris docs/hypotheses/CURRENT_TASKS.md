# Current Tasks - Wave Race 64 Recomp

> **VOOR AI/LLM: LEES EERST `chris docs/prompt.md` VOOR PROJECT CONTEXT EN BUILD INSTRUCTIES!**
>
> Die prompt bevat essentiële informatie over:
> - Wat N64Recomp is en hoe het werkt
> - Belangrijke bestandslocaties (decomp, recomp, stubs)
> - Build commando's (WSL)
> - Debug tips en veelvoorkomende problemen
> - Workflow voor het oplossen van issues

**Last Updated:** December 2025 (Session 26)

---

## Priority Tasks (Blokkeren Voortgang)

### 1. [x] State 5→6 Transition - **DONE!**
**Status:** WORKING (Session 22)

### 2. [x] State 6→2 Transition - **DONE!**
**Status:** WORKING (Session 24)

### 3. [x] DMA Crash After State 2 Transition - **FIXED (Session 25)**
**Status:** FIXED

### 4. [x] ovl_i0 Crash in State 2 - **FIXED (Session 26)**
**Status:** WORKING
**Details:**
- Root cause: Gfx* return values not sign-extended
- Fix: `ctx->r2 = (gpr)(int32_t)gfx_in;`
- Game now runs 549+ frames stable in state 2

### 5. [ ] Controller Input (Real Implementation)
**Status:** Low Priority
**Details:**
- Currently simulated with 0xB000 in waverace_stubs.cpp
- Real SDL→N64 controller mapping needed later

### 6. [ ] State 2→3 Transition
**Status:** Next Task
**Details:**
- State 2 runs stable, need to investigate transition to state 3
- Requires button press or timer

---

## Completed (Session 26)

- [x] **OVL_I0 CRASH FIXED!**
- [x] Root cause: Sign extension for N64 pointers (0x80xxxxxx)
- [x] Added `func_8008FB74` to ignored list with proper stub
- [x] Added `func_8009328C` to ignored list with proper stub
- [x] Removed workaround (skip after frame 4)
- [x] Game runs 549+ frames stable in state 2

**Key Lesson:** All stubs returning N64 pointers MUST use:
```cpp
ctx->r2 = (gpr)(int32_t)pointer;  // Sign extend!
```

## Completed (Session 25)

- [x] DMA crash fixed by stubbing `func_80095050`
- [x] Session doc created for state 2 crash investigation

## Completed (Session 24)

- [x] State 6→2 transition working
- [x] Stubbed blocking functions: `ovl_func_801E6A4C`, `func_800C21F4`

---

## Complete State Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                    WAVE RACE 64 STATE FLOW                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  STATE 5  ──(1 frame)──►  STATE 6  ──(14 frames)──►  STATE 2       │
│  (Boot 1)                 (Logo)                     (Menu/ovl_i0)  │
│  WORKING                  WORKING                    WORKING!       │
│                                                                     │
│  STATE 2  ──(button/timer)──►  STATE 3  ──►  STATE 7              │
│  ovl_i0                        ovl_i0         ovl_i1               │
│  WORKING                       NOT REACHED    NOT REACHED           │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Gestubde/Custom Functies (Session 26)

| Functie | Type | Reden |
|---------|------|-------|
| `ovl_func_801EB180` | Custom impl | State 6→2 transition met debug |
| `ovl_func_801E6A4C` | Stub | Was blocking - Menu/UI setup |
| `func_800C21F4` | Stub | Was blocking - Audio init |
| `func_800C7020` | Stub | Controller poll (185-hour bug) |
| `func_80046BF4` | Custom impl | DL finalizer null check |
| `func_80092CF0` | Custom impl | Main state machine |
| `func_80095050` | Stub | unk_game_load - crash prevention |
| `func_8008FB74` | Stub (sign-ext) | Main render DL builder (Session 26) |
| `func_8009328C` | Stub (sign-ext) | Main gameplay DL chain (Session 26) |

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
LIBGL_ALWAYS_SOFTWARE=1 timeout 35 ./build/WaveRace64Recompiled 2>&1 | grep -E '(STATE|FRAME|CRASH)'
```

---

*Update dit bestand aan het begin en einde van elke sessie.*
