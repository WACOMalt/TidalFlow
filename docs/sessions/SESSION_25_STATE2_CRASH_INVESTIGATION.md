# Session 25: State 2 Crash Investigation

> **Lees eerst `chris docs/prompt.md` voor project context en build instructies!**

**Datum:** December 2025
**Focus:** Debug crash after state 6→2 transition

---

## Samenvatting

Na de succesvolle state 6→2 transitie (Session 24) crashte de game. Deze sessie identificeerde twee problemen:
1. **DMA/game_load crash** - Opgelost door `func_80095050` te stubben
2. **ovl_i0 crash** - Crasht na 3-4 frames in `func_i0_802C5800`

---

## Probleem 1: DMA Crash (OPGELOST)

### Symptoom
Na state transition naar 2, game crasht met:
```
[DEBUG-DMA] ROM read done, sending completion to mq=0x801540B8
timeout: the monitored command dumped core
```

### Analyse
De main loop in `sys_main.c` (recompiled als `funcs_0.c`) roept na elke frame:
1. `func_80092CF0` - Display list builder
2. `func_80046BF4` - DL finalizer
3. `osRecvMesg` - Wait for message
4. `osDpGetStatus` loop - Wait for RDP
5. **`game_dma_copy`** - Als D_801CE634==6 en D_801CE63C!=0, laadt grote data (424KB)
6. **`func_80095050`** (unk_game_load) - Als D_801CE63C!=0, verwerkt geladen data

### Fix
`func_80095050` toegevoegd aan `ignored` list en gestubbed in `waverace_stubs.cpp`:
```cpp
extern "C" void func_80095050(uint8_t* rdram, recomp_context* ctx) {
    // SKIP - testing if this causes crash
    printf("[GAME-LOAD] SKIPPING func_80095050\n");
}
```

### Resultaat
DMA crash opgelost! Game komt nu in state 2 en roept `func_i0_802C5800` succesvol aan.

---

## Probleem 2: ovl_i0 Crash (IN PROGRESS)

### Symptoom
Game crasht na 3-4 frames in state 2:
```
>>> [STATE 2] FRAME 3 - CALLING func_i0_802C5800...
<<< func_i0_802C5800 returned!
>>> [STATE 2] FRAME 4 - CALLING func_i0_802C5800...
>>> About to call func_i0_802C5800...
Segmentation fault
```

### Observaties
- Frame 1-3: `func_i0_802C5800` werkt correct
- Frame 4: Crasht BINNEN de functie
- Boot flag (D_801CE63C) verandert: 1 → 0 → garbage → 0
- De overlay code roept veel 0x801E functies aan (codeseg overlay)

### Hypothese
Mogelijke oorzaken:
1. Memory corruption door missing game_load functionaliteit
2. BSS variabelen niet correct geinitialiseerd
3. Segment registers niet correct gezet voor overlay

### Workaround (WERKT!)
Tijdelijke fix: Na 3 frames, skip `func_i0_802C5800` en genereer placeholder DL:
```cpp
if (state2_frame > 3) {
    // Generate minimal DL
    dl[idx++] = 0xE7000000; // gDPPipeSync
    dl[idx++] = 0x00000000;
    dl[idx++] = 0xB8000000; // gSPEndDisplayList
    dl[idx++] = 0x00000000;
    return;
}
```

**Resultaat:** Game draait nu 600+ frames (35 seconden) zonder crash!

---

## Gewijzigde Bestanden

| Bestand | Wijziging |
|---------|-----------|
| `waverace.toml` | Added `func_80095050` to ignored list |
| `waverace_stubs.cpp` | Added stub for `func_80095050`, debug output for state 2 |

---

## State Flow Update

```
STATE 5  ──►  STATE 6  ──►  STATE 2 (ovl_i0)
(Boot 1)      (Logo)        (Menu intro)
✅ WORKING    ✅ WORKING     ⚠️ PARTIALLY WORKING (3 frames real, then placeholder)

Met workaround: Game draait 600+ frames stabiel!
```

---

## Volgende Stappen

1. **Analyseer crash in frame 4** - Welke instructie/memory access crasht?
2. **Check BSS initialization** - Is overlay BSS correct geïnitialiseerd?
3. **Implementeer func_80095050 deels** - Misschien is game_load nodig voor correcte data

---

## Build & Test Commands

```bash
# Build
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && cmake --build build -j 24"

# Test
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && LIBGL_ALWAYS_SOFTWARE=1 timeout 35 ./build/WaveRace64Recompiled 2>&1 | grep -E '(STATE 2|FRAME|func_i0)'"
```

---

*Session 25 - State 2 Crash Investigation - DMA crash fixed, ovl_i0 crash in progress*
