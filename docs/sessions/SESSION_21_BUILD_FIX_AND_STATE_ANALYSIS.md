# Session 21: Build Fix and State Transition Analysis

> **Lees eerst `chris docs/prompt.md` voor project context en build instructies!**

**Datum:** December 2025
**Focus:** Fix build process, analyze why state transitions don't work

---

## Samenvatting

Deze sessie heeft twee belangrijke dingen opgeleverd:
1. **Build proces gefixed** - `prompt.md` was incorrect, nu consistent met `MY_SETUP.md`
2. **State transition probleem geïdentificeerd** - Game blijft op state 5, fade counter blijft 0

---

## Wat We Gefixed Hebben

### 1. Build Instructies in prompt.md

**Probleem:** `prompt.md` had verkeerde build commando's (zonder `wsl bash -c "..."` wrapper)

**Fix:** Updated `prompt.md` met correcte format:
```bash
# CORRECT format (altijd gebruiken!)
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && cmake --build build -j 24"
```

**Belangrijk:** LLM draait in Windows context, alle Linux commands MOETEN via `wsl bash -c "..."`.

### 2. CURRENT_TASKS.md Update

Toegevoegd: Duidelijke waarschuwing bovenaan voor AI/LLM om eerst `prompt.md` te lezen.

---

## Build Resultaat

```
[ 74%] Built target WaveRace64Recompiled
```

Build succesvol! Geen linker errors.

---

## State Transition Analyse

### Huidige Status

```
Game State (D_800DAB24): 5 (blijft constant)
Fade counter (0x802C76F4): 0 (wordt niet geïncrementeerd)
```

### Verwachte Flow (uit decomp)

```
State 5 → State 6 (1 frame) → State 2 (na 14 frames)
```

### Waarom Het Niet Werkt

Bij analyse van `ovl_func_802C5BA4` (boot overlay functie):

```c
// 0x802C5BAC: lw $t6, 0x0($v0)   // Load D_801CE63C
// 0x802C5BBC: beq $t6, $zero, L_802C5BEC   // If 0, skip init code
```

**Root Cause:** `D_801CE63C` (init flag) = 0, dus de functie skipt naar de normale DL code en roept NIET `ovl_func_802C73B0` aan (die de state transition zou doen).

### Chicken-and-Egg Probleem

De boot overlay verwacht dat bepaalde variabelen al geïnitialiseerd zijn:
- `D_801CE63C` - Init flag (moet != 0 zijn)
- Mogelijk andere variabelen

Deze worden normaal gezet door eerdere code in de boot sequence die we nog niet correct hebben geïmplementeerd.

---

## Volgende Stappen

### Optie A: Analyseer Boot Sequence Volledig
1. Zoek in decomp waar `D_801CE63C` wordt gezet
2. Trace de volledige init sequence
3. Implementeer missende initialisatie

### Optie B: Tijdelijke Init Fix
1. Zet `D_801CE63C = 1` aan het begin van de game
2. Test of state transitions dan werken
3. Refine later met echte init code

### Optie C: Debug ovl_func_802C73B0
1. Voeg debug output toe om te zien wat deze functie doet
2. Check of de state transition code correct is gegenereerd

---

## Gewijzigde Bestanden

| Bestand | Wijziging |
|---------|-----------|
| `chris docs/prompt.md` | Build commando's gefixed naar `wsl bash -c "..."` format |
| `chris docs/hypotheses/CURRENT_TASKS.md` | Warning toegevoegd voor AI/LLM |

---

## Key Learnings

### Les 1: Documentatie Consistent Houden
`prompt.md` en `MY_SETUP.md` hadden inconsistente build instructies. Nu beide consistent.

### Les 2: Init Flags Zijn Kritiek
N64 games hebben vaak init flags die bepalen of code correct draait. Als deze niet gezet zijn, kunnen hele code paths worden overgeslagen.

### Les 3: State Machine Dependencies
State transitions hangen af van meerdere variabelen, niet alleen de state zelf. De boot overlay checkt `D_801CE63C` voordat het de state transition code uitvoert.

---

## Quick Reference

### Build Commands
```bash
# Build
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && cmake --build build -j 24"

# Test
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && timeout 15 ./build/WaveRace64Recompiled 2>&1 | head -200"

# Check state
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && timeout 10 ./build/WaveRace64Recompiled 2>&1 | grep -E '(Game State|Fade counter)'"
```

### Key Addresses
| Address | Variable | Purpose |
|---------|----------|---------|
| 0x800DAB24 | D_800DAB24 | Game state (5=boot1, 6=logo, 2=menu) |
| 0x801CE63C | D_801CE63C | Init flag (moet != 0 voor state transition) |
| 0x802C76F4 | D_802C76F4 | Fade counter (overlay-local, in boot overlay) |

---

*Volgende sessie: Analyseer waar D_801CE63C wordt geïnitialiseerd en fix de boot sequence.*
