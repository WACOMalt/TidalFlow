# Session 23: State 6 Fade Counter and Transition Analysis

> **Lees eerst `chris docs/prompt.md` voor project context en build instructies!**
>
> **⚠️ VOOR AI/LLM: Maak ALTIJD een sessie log aan voordat je context vol raakt / auto-compact gebeurt!**

**Datum:** December 2025
**Focus:** Analyze why state 6→2 transition doesn't happen after fade counter reaches 14

---

## Samenvatting

Deze sessie analyseerde waarom de game thread stopt bij frame 15 met fade counter 13. De root cause is nog niet gevonden, maar we hebben de volledige code flow gedocumenteerd.

---

## Huidige Status

- **State 5→6**: WERKT (via Session 22 fix)
- **Fade counter**: Incrementeert correct (0 → 13)
- **State 6→2**: BLOKKEERT (overlay functie keert niet terug na frame 15)

---

## Test Output

```
│ [DL-IMPL] func_80092CF0 FRAME #15
│  Game State: 6 (0x6)
>>> [STATE 6] CALLING ovl_func_802C5BA4 (0x802C segment_1B1FB0)...
    Controller (D_801CE65A): 0x0000  (need 0xB000 for skip)
    Fade counter (0x802C76F4): 13  (need 14 for auto-transition)
[DEBUG-RT] osRecvMesg_recomp(mq=0x801D7E78, msg=0x801530B4, flags=0)...
...
(geen "RETURNED from ovl_func_802C5BA4" - functie keert niet terug!)
```

Na frame 15 blijven de scheduler threads draaien (osRecvMesg), maar func_80092CF0 wordt niet meer aangeroepen.

---

## Code Flow Analyse (uit decomp)

### Boot Overlay: func_1B1FB0_802C5BA4

Locatie: `asm/nonmatchings/unused_code_1B1FB0/func_1B1FB0_802C5BA4.s`

```asm
; Check state == 5
.L1B1FB0_802C5D50:
    lui        $v1, %hi(D_800DAB24)
    lw         $v1, %lo(D_800DAB24)($v1)
    addiu      $at, $zero, 0x5
    bne        $v1, $at, .L1B1FB0_802C5D70   ; Als state != 5, ga naar fade logic

; Als state == 5: roep func_1B1FB0_802C5DF4 aan
    jal        func_1B1FB0_802C5DF4
    b          .L1B1FB0_802C5DE0              ; Return

; Fade counter logic (state 6)
.L1B1FB0_802C5D70:
    lui        $a1, %hi(D_1B1FB0_802C76F4)   ; fade_counter adres
    addiu      $a1, $a1, %lo(D_1B1FB0_802C76F4)
    lw         $v1, 0x0($a1)                  ; load fade_counter

    ; Berekent fade_value = (fade_counter * 255) / 10
    sll        $t8, $v1, 8                    ; fade_counter * 256
    subu       $t8, $t8, $v1                  ; * 255
    div        $zero, $t8, $at                ; / 10
    mflo       $v0                            ; result in v0

    ; Als fade_value >= 256, clamp naar 255
    sltiu      $at, $v0, 0x100
    bnez       $at, .L1B1FB0_802C5DA4
    sw         $v0, 0x0($a0)
    addiu      $v0, $zero, 0xFF               ; v0 = 255
    sw         $v0, 0x0($a0)

.L1B1FB0_802C5DA4:
    addiu      $t1, $v1, 0x1                  ; fade_counter + 1
    addiu      $at, $zero, 0xFF
    bne        $v0, $at, .L1B1FB0_802C5DC4    ; Als v0 != 255, skip osViBlack
    sw         $t1, 0x0($a1)                  ; Store fade_counter + 1

    ; Als v0 == 255 (fade_counter >= 11), roep osViBlack aan
    jal        osViBlack
    addiu      $a0, $zero, 0x1

    lui        $a1, %hi(D_1B1FB0_802C76F4)
    addiu      $a1, $a1, %lo(D_1B1FB0_802C76F4)

.L1B1FB0_802C5DC4:
    lw         $t2, 0x0($a1)                  ; Laad fade_counter
    slti       $at, $t2, 0xE                  ; fade_counter < 14?
    bnel       $at, $zero, .L1B1FB0_802C5DE0  ; Als < 14, return
    or         $v0, $s0, $zero

    ; Als fade_counter >= 14: ROEP STATE TRANSITION AAN!
    jal        func_801EB180                  ; <-- STATE 6 → 2!
    nop

    or         $v0, $s0, $zero

.L1B1FB0_802C5DE0:
    ; Return
```

### Fade Counter Berekening

| fade_counter | fade_value = (counter * 255) / 10 | >= 256? | Actie |
|--------------|-----------------------------------|---------|-------|
| 0 | 0 | Nee | Increment counter |
| 5 | 127 | Nee | Increment counter |
| 10 | 255 | Nee | Increment counter |
| 11 | 280 | Ja, clamp to 255 | osViBlack(1), increment |
| 12 | 306 | Ja, clamp to 255 | osViBlack(1), increment |
| 13 | 331 | Ja, clamp to 255 | osViBlack(1), increment |
| 14 | 357 | Ja, clamp to 255 | osViBlack(1), **func_801EB180()** |

---

## func_801EB180: State 6→2 Transition

Locatie: `asm/nonmatchings/codeseg/B97B0/func_801EB180.s`

Deze functie:
1. Zet D_800DAB24 = 2 (state naar 2)
2. Zet vele andere variabelen (D_801CE634, D_801CE630, etc.)
3. Roept andere functies aan:
   - `func_80096960` (line 191)
   - `func_8009684C` (line 209)
   - `func_8004A208` (line 211)
   - `FadeTransition_SetProps` (line 215)
   - `func_801E6A4C` (line 218)
   - `func_800C21F4` (line 227)

Een van deze functies kan blokkeren.

---

## Key Memory Addresses

| Address | Variable | Purpose |
|---------|----------|---------|
| 0x800DAB24 | D_800DAB24 | Game state (5=boot1, 6=logo, 2=menu) |
| 0x802C76F4 | D_1B1FB0_802C76F4 | Fade counter (overlay BSS) |
| 0x802C76F0 | D_1B1FB0_802C76F0 | Fade value (calculated) |

---

## Decomp Locaties

| File | Purpose |
|------|---------|
| `src/unused_code_1B1FB0.c` | Boot overlay (ovl_ings) |
| `asm/nonmatchings/unused_code_1B1FB0/func_1B1FB0_802C5BA4.s` | Boot overlay main entry |
| `asm/nonmatchings/codeseg/B97B0/func_801EB180.s` | State 6→2 transition |
| `src/ovl_table.c` | Overlay table met ROM/VRAM adressen |

---

## Hypothese

De output stopt bij frame 15 (fade counter 13). De volgende frame zou fade counter 14 bereiken en `func_801EB180` aanroepen.

Mogelijke oorzaken:
1. **func_801EB180 blokkeert** - Een van de functies die het aanroept wacht op iets
2. **Thread scheduling issue** - De game thread wordt nooit meer gescheduled
3. **Overlay laad probleem** - De overlay voor state 2 (ovl_i0) moet geladen worden

---

## Volgende Stappen

1. **Debug func_801EB180** - Voeg printf toe om te zien welke subfunctie blokkeert
2. **Check ovl_i0 loading** - Verifieer dat de state 2 overlay correct wordt geladen
3. **Check functies die 801EB180 aanroept**:
   - func_80096960
   - func_8009684C
   - func_8004A208
   - FadeTransition_SetProps
   - func_801E6A4C
   - func_800C21F4

---

## Build & Test Commands

```bash
# Build
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && cmake --build build -j 24"

# Test met fade counter output
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && LIBGL_ALWAYS_SOFTWARE=1 timeout 15 ./build/WaveRace64Recompiled 2>&1 | grep -E '(FRAME|Fade counter|STATE)'"
```

---

*Session 23 - State 6 Fade Counter and Transition Analysis*
