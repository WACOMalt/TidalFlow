# Session 4: Overlay Progress - Connecting Real Overlay Functions

## Summary
We zijn een stap verder! De 0x801E overlay code is gerecompileerd en geladen. Nu proberen we de **echte** overlay functies aan te roepen in plaats van de fake display list.

## Wat we hadden (voor deze sessie):
- `func_80092CF0` was gestubbed
- `func_80092CF0_impl` bouwde een **FAKE** display list (cycling colors)
- Dit was een workaround omdat de overlay niet werkte

## Wat we ontdekt hebben:

### func_80092CF0 is een State Machine
`func_80092CF0` is een grote state machine die verschillende display list builders aanroept:

```
Game State (D_800DAB24):
├── State 0: func_801ECAF4 (0x801E overlay) ✅ HEBBEN WE
├── State 1: func_802C5BA4 (0x802C overlay) ❌ NIET
├── State 2-103: Diverse 0x802C functies ❌ NIET
```

### 0x802C Overlay Functies (NIET geimplementeerd)
```
func_802C583C, func_802C5800, func_802C5924, func_802C5968,
func_802C5A7C, func_802C5AE4, func_802C5B40, func_802C5B4C,
func_802C5B74, func_802C5B78, func_802C5BA4, func_802C5C1C,
func_802C5D24, func_802C5D3C, func_802C5F50, func_802C5F6C,
func_802C6944, func_802C7484, func_802C7D00, func_802C913C
```

## Huidige Aanpak

We hebben `func_80092CF0_impl` aangepast om:
1. **State 0**: De ECHTE `ovl_func_801ECAF4` aan te roepen!
2. **Andere states**: Fallback (return input pointer)

### Code in waverace_stubs.cpp:
```cpp
extern "C" void func_80092CF0_impl(uint8_t* rdram, recomp_context* ctx) {
    uint32_t game_state = read_game_state(rdram);  // D_800DAB24

    // STATE 0: Call the REAL 0x801E overlay function!
    if (game_state == 0) {
        ovl_func_801ECAF4(rdram, ctx);  // ECHTE overlay!
        return;
    }

    // OTHER STATES: Fallback
    ctx->r2 = ctx->r4;
}
```

## Waarom func_80092CF0 niet de echte code kan zijn

N64Recomp kan `func_80092CF0` niet recompileren omdat:
1. Het een **jump table** heeft (`jtbl_800EAFA8`)
2. Het **0x802C overlay** functies aanroept die niet bestaan

Daarom moet het gestubbed blijven en gebruiken we de `_impl` workaround.

## Status

- [x] 0x801E overlay gerecompileerd (245 functies)
- [x] Overlay geladen in RDRAM (0x801DAFA0)
- [x] `func_80092CF0_impl` aangepast om echte overlay aan te roepen
- [ ] Testen of de echte overlay graphics produceert
- [ ] Mogelijk 0x802C overlay later toevoegen

## Files Gewijzigd

- `waverace.toml`: func_80092CF0 terug als stub
- `waverace_stubs.cpp`: impl roept nu echte ovl_func_801ECAF4 aan
- `waverace.syms.toml`: declared_funcs voor 0x802C (nog niet gebruikt)

## Volgende Stappen

1. Test of game_state=0 voorkomt en overlay wordt aangeroepen
2. Als overlay werkt: echte graphics!
3. Als niet: debug waarom func_80092CF0 niet wordt aangeroepen

## Test Resultaten

### Wat werkt:
- ✅ Game start
- ✅ 0x801E overlay wordt geladen (`rdram=0x801DAFA0, size=0x4CAC0`)
- ✅ N64 logo wordt getoond (knipperend)
- ✅ Threads draaien (game=3, audio=4, render=5)

### Wat niet werkt:
- ❌ `func_80092CF0` wordt NIET aangeroepen
- Dit betekent de game is nog in de "boot/logo" fase
- `func_80092CF0` wordt pas later in de game loop aangeroepen

### Observatie
Je ziet het N64 logo knipperen! Dit is **beter** dan de cycling colors van voorheen. De game komt verder maar de display list builder wordt nog niet aangeroepen.

## Key Insight

Het originele probleem in `chris docs/PROJECT_STATUS.md`:
> "Game runs at ~35 FPS but shows only cycling purple/blue colors"

Dit kwam door de FAKE display list in `func_80092CF0_impl`. Nu de 0x801E overlay werkt, kunnen we de ECHTE overlay aanroepen voor state=0!

**Update**: Het logo verschijnt nu, wat betekent dat er ergens graphics worden getekend - mogelijk via een andere code path dan `func_80092CF0`.
