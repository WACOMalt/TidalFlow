# Session 24: Blocking Function Trace - ovl_func_801E6A4C

> **Lees eerst `chris docs/prompt.md` voor project context en build instructies!**
>
> **⚠️ VOOR AI/LLM: Maak ALTIJD een sessie log aan voordat je context vol raakt / auto-compact gebeurt!**

**Datum:** December 2025
**Focus:** Trace which function blocks the state 6→2 transition

---

## Samenvatting

Door `ovl_func_801EB180` te wrappen met debug output, hebben we geïdentificeerd dat **`ovl_func_801E6A4C`** de blokkerende functie is. De output stopt direct na het aanroepen van deze functie.

---

## Key Finding: Blocking Function Identified

### Test Output
```
[801EB180] Calling func_80096960...
[801EB180] func_80096960 returned
[801EB180] Calling func_8009684C...
[801EB180] func_8009684C returned
[801EB180] Calling func_8004A208...
[801EB180] func_8004A208 returned
[801EB180] Calling ovl_FadeTransition_SetProps...
[801EB180] ovl_FadeTransition_SetProps returned
[801EB180] Calling ovl_func_801E6A4C...
(GEEN OUTPUT MEER - FUNCTIE BLOKKEERT!)
```

### De Blokkerende Functie
**`ovl_func_801E6A4C`** (0x801E6A4C)
- Locatie: `RecompiledFuncs/funcs_17.c:7647`
- Deze functie roept `ovl_func_801E6F6C` meerdere keren aan (lijnen 7916, 8010, 8122, 8384)

---

## Code Analyse: ovl_func_801E6A4C

### Functie Signatuur
```c
// Called with: ovl_func_801E6A4C(0, 0)
// Arguments: a0 = 0, a1 = 0
void ovl_func_801E6A4C(uint8_t* rdram, recomp_context* ctx);
```

### Flow
1. Check `a0 < 5`, zo ja ga naar switch statement
2. Switch op `a0`:
   - case 0: `L_801E6A88` - Veel data copies, geen functies
   - case 1: `L_801E6BFC` - Roept `ovl_func_801E6F6C` aan
   - case 2: `L_801E6C98` - Roept `ovl_func_801E6F6C` aan
   - case 3: `L_801E6D48` - Roept `ovl_func_801E6F6C` aan
   - case 4: `L_801E6EC0` - Roept `ovl_func_801E6F6C` aan

### Wanneer a0 = 0
Met `a0 = 0` gaat de code naar `L_801E6A88`:
```c
L_801E6A88:
    if (a3 != 0) {  // a3 = a1 & 0xFFFF = 0
        goto L_801E6F5C;  // return
    }
    // ... veel memory copies ...
    goto L_801E6F5C;  // return
```

**DUS**: Met `a0=0, a1=0` zou de functie direct moeten returnen zonder subfuncties aan te roepen!

---

## Hypothese: Switch Table Bug

Het probleem kan zijn in de switch table lookup:
```c
switch (jr_addend_801E6A80 >> 2) {
    case 0: goto L_801E6A88; break;
    case 1: goto L_801E6BFC; break;
    ...
}
```

De `jr_addend_801E6A80` wordt berekend uit een table in memory (0x80226C8C). Als deze table niet correct geïnitialiseerd is, springt de code naar de verkeerde plek.

---

## Volgende Stappen

1. **Stub ovl_func_801E6A4C** - Simpelweg terugkeren zonder iets te doen
2. **Test** of de state transition dan werkt
3. Als dat werkt, analyseer wat de functie zou moeten doen en implementeer het correct

---

## Implementatie Plan

### Optie A: Stub de functie volledig
```cpp
extern "C" void ovl_func_801E6A4C(uint8_t* rdram, recomp_context* ctx) {
    printf("[STUB] ovl_func_801E6A4C called with a0=%d, a1=%d - returning immediately\n",
           ctx->r4, ctx->r5);
    fflush(stdout);
    // Do nothing, just return
}
```

### Optie B: Partial implementatie (alleen case 0)
```cpp
extern "C" void ovl_func_801E6A4C(uint8_t* rdram, recomp_context* ctx) {
    uint16_t a0 = ctx->r4 & 0xFFFF;
    uint16_t a1 = ctx->r5 & 0xFFFF;

    if (a0 >= 5) return;
    if (a0 == 0 && a1 == 0) {
        // Do the memory copies from L_801E6A88
        return;
    }
    // For other cases, return without doing anything
}
```

---

## Gewijzigde Bestanden (Session 24)

| Bestand | Wijziging |
|---------|-----------|
| `waverace.toml` | Added `ovl_func_801EB180` to ignored list |
| `waverace_stubs.cpp` | Added custom `ovl_func_801EB180` with debug tracing |

---

## Key Addresses

| Address | Variable | Purpose |
|---------|----------|---------|
| 0x801E6A4C | ovl_func_801E6A4C | **BLOCKING** - Menu/UI setup? |
| 0x801E6F6C | ovl_func_801E6F6C | Called by 801E6A4C |
| 0x80226C8C | Jump table | Switch table voor 801E6A4C |

---

## Build & Test Commands

```bash
# Build
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && cmake --build build -j 24"

# Test met debug output
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && LIBGL_ALWAYS_SOFTWARE=1 timeout 30 ./build/WaveRace64Recompiled 2>&1 | grep -v 'osRecvMesg' | grep -E '(FRAME|Fade|STATE|801EB180|Calling|returned|TRANSITION)'"
```

---

## Update: ovl_func_801E6A4C Stubbed - SUCCESS!

Na het stubben van `ovl_func_801E6A4C` werkt de state 6→2 transitie:

```
>>> [STATE 6] CALLING ovl_func_802C5BA4 (0x802C segment_1B1FB0)...
║ >>> STATE TRANSITION: ovl_func_801EB180 CALLED! #1
[801EB180] Part 1: Setting state variables...
[801EB180] STATE CHANGED: 6 → 2
[801EB180] Part 1 complete: All state variables set
[801EB180] Calling func_80096960...
[801EB180] func_80096960 returned
[801EB180] Calling func_8009684C...
[801EB180] func_8009684C returned
[801EB180] Calling func_8004A208...
[801EB180] func_8004A208 returned
[801EB180] Calling ovl_FadeTransition_SetProps...
[801EB180] ovl_FadeTransition_SetProps returned
[801EB180] Skipping ovl_func_801E6A4C (was blocking)
[801EB180] ovl_func_801E6A4C skipped
[801EB180] Calling func_800C21F4...
```

**MAAR**: Nu blokkeert `func_800C21F4`!

Dit is een audio/sound gerelateerde functie. Moet ook onderzocht worden.

---

## Volgende Blocker: func_800C21F4

De output stopt bij:
```
[801EB180] Calling func_800C21F4...
```

Geen return bericht, dus deze functie blokkeert ook.

---

## Update 2: func_800C21F4 Ook Gestubbed - STATE 6→2 VOLLEDIG WERKEND!

Na het stubben van beide blokkerende functies:

```
╔══════════════════════════════════════════════════════════════╗
║ <<< STATE TRANSITION COMPLETE: ovl_func_801EB180 DONE!
║     Game state is now: 2
╚══════════════════════════════════════════════════════════════╝
<<< [STATE 6] RETURNED from ovl_func_802C5BA4
    DL output: 0x801208F8 (wrote 4024 bytes = 503 commands)
    Fade counter changed: 13 -> 14
!!! BOOT FLAG CHANGED: 0 -> 1
[DEBUG-DMA] do_dma called: mq=0x801540B8, rdram=0xFFFFFFFF802310A0, phys=0x100FE320, size=0x678E0, dir=0
```

**SUCCES!** State 6→2 transitie werkt nu volledig!

---

## Nieuw Probleem: DMA Crash

Na de succesvolle state transition crasht de game:
```
[DEBUG-DMA] do_dma called: mq=0x801540B8, rdram=0xFFFFFFFF802310A0, phys=0x100FE320, size=0x678E0, dir=0
[DEBUG-DMA] rom_base=0x10000000, sram_base=0x08000000
[DEBUG-DMA] ROM read done, sending completion to mq=0x801540B8
...
timeout: the monitored command dumped core
```

Dit is waarschijnlijk het laden van de **ovl_i0 overlay** voor state 2.

### DMA Details:
- `rdram=0x802310A0` - Destination in RAM
- `phys=0x100FE320` - Physical ROM address (0xFE320 in ROM)
- `size=0x678E0` - 424,160 bytes
- `dir=0` - Read from ROM to RAM

De DMA operatie lijkt te slagen (`ROM read done`), maar daarna crasht de game.
Mogelijk probleem met de overlay loading of de func_i0_802C5800 die daarna wordt aangeroepen.

---

## Totale Progressie

| Stap | Status | Details |
|------|--------|---------|
| State 5 (Boot 1) | ✅ WERKT | Uit Session 22 |
| State 5 → 6 | ✅ WERKT | Uit Session 22 |
| State 6 (Logo) | ✅ WERKT | Nintendo logo rendert |
| State 6 → 2 | ✅ WERKT | **Session 24** - 2 functies gestubbed |
| State 2 (Menu) | ❌ CRASH | DMA/overlay loading crash |

---

## Gestubde Functies (Session 24)

| Functie | Reden | Impact |
|---------|-------|--------|
| `ovl_func_801E6A4C` | Was blocking | Menu/UI setup - kan later geïmplementeerd worden |
| `func_800C21F4` | Was blocking | Audio init - geen geluid maar game kan draaien |

---

## Volgende Stappen

1. **Analyseer de DMA crash** - Waarom crasht de game na ROM read?
2. **Check overlay loading** - Wordt ovl_i0 correct geladen op 0x802C5800?
3. **Bekijk func_i0_802C5800** - De entry point voor state 2

---

## Gewijzigde Bestanden (Final)

| Bestand | Wijziging |
|---------|-----------|
| `waverace.toml` | Added 3 functions to ignored: `ovl_func_801EB180`, `ovl_func_801E6A4C`, `func_800C21F4` |
| `waverace_stubs.cpp` | Custom implementations for all 3 functions |

---

*Session 24 - Blocking Function Trace - STATE 6→2 COMPLETE!*
