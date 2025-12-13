# Session 6: Render Thread Fixes & Display List Investigation

**Datum:** December 2025

---

## Samenvatting

Grote voortgang in deze sessie! We hebben:
1. De render thread blocking bug gefixed (func_800C7020 timer bug)
2. func_80046BF4 crash gefixed (null display list pointer)
3. Byte order issues gefixed in alle memory reads
4. Game draait nu stabiel zonder crashes
5. func_80092CF0 wordt aangeroepen en we begrijpen nu de state machine

---

## Key Fixes

### 1. Render Thread Timer Bug (func_800C7020)
**Probleem:** De render thread blokkeerde na "after_25" debug message. `osSetTimer` werd aangeroepen met een gigantische countdown waarde (~185 uur!) door een 64-bit subtractie bug.

**Oplossing:** Functie gestubbed in waverace_stubs.cpp. Controller input wordt toch al door SDL runtime afgehandeld.

```cpp
extern "C" void func_800C7020(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0;  // Return success
}
```

### 2. Display List Finalizer Crash (func_80046BF4)
**Probleem:** Crash bij schrijven naar display list pointer (D_80151944) die nog niet geïnitialiseerd was.

**Oplossing:** Stub met safety check - als pointer null/invalid is, skip de write.

### 3. Byte Order Fixes
**Probleem:** `read_game_state()` en andere functies deden onnodig byte swapping. De recompiled code gebruikt native byte order.

**Oplossing:** Byte swapping verwijderd uit alle memory read helpers.

### 4. func_80092CF0 Connection
**Probleem:** func_80092CF0 was gestubbed, maar onze `func_80092CF0_impl` werd niet aangeroepen.

**Oplossing:** In funcs_5.c de stub aangepast om onze impl aan te roepen:
```c
extern void func_80092CF0_impl(uint8_t* rdram, recomp_context* ctx);
RECOMP_FUNC void func_80092CF0(uint8_t* rdram, recomp_context* ctx) {
    func_80092CF0_impl(rdram, ctx);
}
```

---

## Huidige Status

### Wat Werkt:
- ✅ Game start en alle 4 threads draaien
- ✅ N64 logo zichtbaar (via andere code path)
- ✅ 0x801E overlay geladen (DMA naar 0x801DAFA0)
- ✅ func_80092CF0 wordt aangeroepen
- ✅ VI framebuffer swaps werken
- ✅ Game draait stabiel (geen crashes in normale flow)

### Wat Niet Werkt:
- ❌ Scherm is zwart (behalve N64 logo)
- ❌ Game state = 5 (intro state, heeft 0x802C overlay nodig)
- ❌ ovl_func_801ECAF4 crasht als we state 0 forceren

---

## Game State Machine (func_80092CF0)

Uit de decomp blijkt dat func_80092CF0 een grote switch-case heeft:

| State | Overlay | Functie | Status |
|-------|---------|---------|--------|
| 0 | 0x801E | ovl_func_801ECAF4 | ✅ Aanwezig, CRASHT |
| 5, 6 | 0x802C | func_802C5BA4 | ❌ Niet geïmplementeerd |
| 2 | 0x802C | func_802C5800 | ❌ Niet geïmplementeerd |
| 3 | 0x802C | func_802C5A7C | ❌ Niet geïmplementeerd |
| etc. | 0x802C | Diverse | ❌ Niet geïmplementeerd |

**De game start in state 5**, wat de 0x802C overlay nodig heeft. We genereren nu een minimale fake display list voor state 5.

---

## Code Paths

### N64 Logo (WERKT)
Ergens anders in de code wordt het N64 logo getekend - niet via func_80092CF0. Dit is waarschijnlijk een aparte boot sequence.

### func_80092CF0 (Onze focus)
```
Game loop → osRecvMesg → func_80092CF0 → [state switch] → display list
```

Voor state 5: we genereren een fake DL met gSPEndDisplayList
Voor state 0: zou ovl_func_801ECAF4 aanroepen (crasht)

---

## Fake Display List

We hebben een fake display list toegevoegd voor non-state-0 gevallen:
- Cycling colors (als test)
- gDPSetCycleType, gDPSetColorImage, gDPSetFillColor, gDPFillRectangle
- Maar het scherm blijft zwart - de commands worden waarschijnlijk niet correct verwerkt

---

## Volgende Stappen

### Optie A: Fix ovl_func_801ECAF4 crash
1. Onderzoek waarom de overlay functie crasht
2. Mogelijke oorzaken:
   - Afhankelijkheden niet geïnitialiseerd
   - Roept andere functies aan die niet bestaan
   - Memory access issues

### Optie B: Implement 0x802C overlay
1. Voeg 0x802C overlay section toe aan syms.toml
2. Run N64Recomp
3. Fix eventuele stub functies

### Optie C: Force game naar andere state
1. Zoek uit hoe D_800DAB24 wordt gewijzigd
2. Forceer een state transitie (bijv. naar state 0 na init)

---

## Files Gewijzigd

### waverace_stubs.cpp
- func_800C7020: Timer stub (blocking fix)
- func_80046BF4: DL finalizer stub met safety check
- func_80092CF0_impl: State machine met fake DL fallback
- read_game_state(): Byte order fix

### funcs_5.c
- func_80092CF0: Aangepast om impl aan te roepen

### funcs_0.c, funcs_12.c
- N64Recomp stubs gecommentarieerd (linker conflict fix)

---

## Test Commands

```bash
# Build
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && cmake --build build -j 24"

# Run met debug
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && timeout 10 ./build/WaveRace64Recompiled 2>&1" | head -100

# Filter DL-IMPL output
wsl bash -c "... ./build/WaveRace64Recompiled 2>&1" | grep "DL-IMPL"
```

---

## Key Insight

Het pad klopt! De flow van game start naar func_80092CF0:

```
Boot → Threads starten → Thread 5 (render) → func_80046DA0 →
→ [render loop] → func_80092CF0 → [state machine] → display list
```

Het probleem is niet het pad, maar dat:
1. Game in state 5 blijft (0x802C overlay nodig)
2. State 0 overlay crasht als we die forceren

---

*Session 6 - Significant progress on render thread*
