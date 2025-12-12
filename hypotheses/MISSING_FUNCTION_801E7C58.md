# Missing Function Discovery: func_801E7C58

**Date:** December 2025
**Analyst:** Claude Code (Opus 4.5)
**Status:** ACTIONABLE - Ready to test

---

## Executive Summary

De display list chain is **gebroken** door een ontbrekende functie. Dit is een eenvoudige fix: voeg 1 functie toe aan `waverace.syms.toml`.

---

## Het Probleem

### Call Chain (gebroken)
```
func_80092CF0 (main display caller)     ✅ Werkt
    ↓
ovl_func_801ECAF4 (display list builder) ✅ Gerecompileerd
    ↓
func_801E7C58 (fill rectangle helper)   ❌ ONTBREEKT!
```

### Evidence

In `RecompiledFuncs/funcs_16.c` regel 5047:
```c
static_1_801E7C58(rdram, ctx);  // Deze functie bestaat niet!
```

In decomp `ovl_symbols.txt`:
```
func_801E7C58 = 0x801e7c58; //defined:true
```

In `waverace.syms.toml` - **ONTBREEKT**:
```toml
# Springt van:
{ name = "ovl_func_801E7C24", vram = 0x801E7C24, size = 0x38 }  # eindigt 0x801E7C5C
# direct naar:
{ name = "ovl_func_801E7C64", vram = 0x801E7C64, size = 0x634 }
# func_801E7C58 ontbreekt ertussen!
```

---

## Root Cause

- `waverace.syms.toml` heeft **153 overlay functies**
- Decomp `ovl_symbols.txt` heeft **215 overlay functies**
- Er ontbreken **62 functies**, waaronder `func_801E7C58`

De syms.toml was gegenereerd met een incomplete method of er zijn functies overgeslagen.

---

## De Ontbrekende Functie

### Decomp Info (functions.h regel 146)
```c
Gfx* func_801E7C58(Gfx* dList, u32 ulx, u32 uly, u32 lrx, u32 lry, u32 r, u32 g, u32 b, u32 a);
```

Dit is een **display list helper** die een filled rectangle tekent met RGBA kleuren.

### Locatie in ROM
- VRAM: 0x801E7C58
- Overlay base VRAM: 0x801E0000
- Offset binnen overlay: 0x7C58
- ROM base: 0xA95E8
- **ROM locatie: 0xA95E8 + 0x7C58 = 0xB1240**

### Size Berekening
- Volgende functie in decomp: func_801E81E8 op VRAM 0x801E81E8
- Size = 0x801E81E8 - 0x801E7C58 = **0x590 bytes**

---

## De Fix (Minimaal - 1 Stap Verder)

### Incremental Approach

We hoeven NIET alle 215 functies in één keer toe te voegen. We voegen alleen toe wat **nodig is voor de volgende stap**.

Voor display lists werkend te krijgen, voeg deze ene regel toe aan `waverace.syms.toml`:

```toml
{ name = "ovl_func_801E7C58", vram = 0x801E7C58, size = 0x590 },
```

Voeg toe **tussen** `ovl_func_801E7C24` en `ovl_func_801E7C64`.

---

## Waarom Incrementeel?

### Het Principe
Bij N64 recompilatie werk je **stap voor stap**:
1. Probeer te builden
2. Krijg een error (functie X ontbreekt of faalt)
3. Fix alleen functie X
4. Herhaal tot het werkt

### Voordelen
- Je leert gaandeweg welke functies echt nodig zijn
- Minder kans op fouten door te veel tegelijk te veranderen
- Makkelijker te debuggen als iets fout gaat

### Later (indien nodig)
Als meer functies nodig blijken, kunnen we altijd de complete lijst van 215 functies genereren met `generate_codeseg_symbols.py`.

---

## Test Plan

### Stap 1: Voeg functie toe
Edit `waverace.syms.toml`, voeg `ovl_func_801E7C58` toe.

### Stap 2: Regenereer code
```bash
cd waverace-recomp
../N64Recomp/N64Recomp.exe waverace.toml
```

### Stap 3: Rebuild
```bash
cmake --build build-windows --config Release
```

### Stap 4: Test
Run de game en check of de display list nu verder komt.

---

## Verwacht Resultaat

### Als het werkt
- `ovl_func_801ECAF4` kan nu `ovl_func_801E7C58` aanroepen
- Display list builder kan verder
- Mogelijk meer graphics zichtbaar (of nieuwe error voor volgende stap)

### Als het niet werkt
- Mogelijk roept `func_801E7C58` weer andere ontbrekende functies aan
- Dan voegen we die ook toe (incrementeel)

---

## Files te Wijzigen

| File | Wijziging |
|------|-----------|
| `waverace-recomp/waverace.syms.toml` | Voeg 1 functie entry toe |

---

## Update: Complexer dan Gedacht

### Bevindingen na het toevoegen van func_801E7C58

Na het toevoegen van `func_801E7C58` aan de syms.toml en N64Recomp runnen, kwamen nieuwe problemen naar voren:

1. **func_801E7C58 zelf heeft problemen:**
   - Branches buiten de functie grenzen (naar 0x801E7F20, 0x801E81D4, 0x801E7EC0)
   - Dit suggereert dat de decomp's functie-grenzen niet kloppen
   - Of dat er "inlined" code is die als aparte functies behandeld moet worden

2. **Andere functies roepen ontbrekende main code aan:**
   - `ovl_func_801E6ED4` → roept 0x80094A44 aan (bestaat niet)
   - `ovl_func_801E70A0` → roept 0x800949B8 aan (bestaat niet)
   - `ovl_func_801E72E4` → roept 0x80094338 aan (bestaat niet)
   - `ovl_func_801E854C` → roept 0x800949B8 aan (bestaat niet)

3. **Jump table issues:**
   - `ovl_func_801E7584` - jump table at 0x802262F8
   - `ovl_func_801E62D8` - jump table at 0x80226288

### Het Echte Probleem

De syms.toml is gegenereerd met een tool die:
- Niet alle functies heeft gevonden (153 vs 215)
- Functie grenzen verkeerd heeft bepaald
- Main code functies rond 0x80094xxx mist

### Nieuwe Aanpak Nodig

De "incrementele" aanpak werkt niet goed hier omdat:
1. Toevoegen van 1 functie breekt andere functies
2. Er zijn cascade-effecten door verkeerde functie grenzen
3. Main code heeft ook ontbrekende functies

### Opties

**Optie A: Stub alle problematische functies**
- Voordeel: Snel werkend krijgen
- Nadeel: Display list functie is dan ook gestubbed (dus nog steeds geen echte graphics)

**Optie B: Regenereer volledige overlay met decomp data**
- Gebruik decomp's ovl_symbols.txt (215 functies)
- Corrigeer functie sizes gebaseerd op volgende functie
- Dit fixte de functie-grens problemen

**Optie C: Vind en voeg ontbrekende main code functies toe**
- Check waarom 0x80094xxx functies ontbreken
- Mogelijk gegenereerd door andere tool met andere instellingen

### Huidige Status

`func_801E7C58` is gestubbed (samen met 12 andere problematische functies) zodat de build kan doorgaan. De display list functie zelf is nog steeds een stub.

---

## Conclusie

De incrementele aanpak liet zien dat het probleem **dieper zit** dan alleen één ontbrekende functie. De hele symbol table heeft problemen:
- Verkeerde functie grenzen
- Ontbrekende functies in zowel overlay als main code

Voor echte display list support is een volledige regeneratie van de symbol tables nodig.

---

*Incrementele development is nuttig voor het ontdekken van problemen, maar soms is een grotere refactor nodig.*

---

## Update 2: BSS Sectie Probleem Ontdekt

**Datum:** December 2025

### Wat er gebeurde

Na het reverten van de syms.toml wijzigingen om terug te gaan naar een "schone" staat, ontdekten we een dieper probleem:

1. **De originele syms.toml had een syntax error:**
   ```toml
   # FOUT - geen newline tussen ] en [[section]]
   { name = "func_800D2220", vram = 0x800D2220, size = 0x390 }
   ][[section]]
   ```

2. **Na de fix was er geen BSS sectie meer** - de originele file had kennelijk wel een BSS sectie die verloren is gegaan

3. **Overlay functies roepen BSS functies aan:**
   - `ovl_func_801E012C` → roept `0x801DAFB8` aan
   - `ovl_func_801E0224` → roept BSS functies aan
   - `ovl_func_801E02CC` → roept BSS functies aan
   - `ovl_func_801E0478` → roept BSS functies aan
   - ... en waarschijnlijk 50+ meer

### Het Echte Probleem

De **BSS/Data sectie** (0x801DAFB8 - 0x801DFFFF range) ontbreekt in de syms.toml. Deze sectie bevat:
- Runtime-gegenereerde data
- Function pointers
- Gedeelde state tussen overlay en main code

Zonder deze sectie kan N64Recomp de overlay functies niet compileren omdat ze "No function found for jal target" errors geven.

### Waarom Dit Gebeurde

De git history laat zien dat er ooit een BSS sectie was:
```toml
[[section]]
name = ".bss_801D"
rom = 0x000A45A0
vram = 0x801DAFB8
size = 0x5048

functions = [
    { name = "bss_func_801DAFB8", vram = 0x801DAFB8, size = 0x6C },
    # ... 33 functies totaal
]
```

Deze sectie is ergens verloren gegaan of was nooit correct gecommit.

### Opties om Verder te Gaan

**Optie A: Stub alle ~50 falende overlay functies**
- Voordeel: Snel werkende build
- Nadeel: Bijna alle overlay code wordt gestubbed, inclusief display lists
- Resultaat: Nog steeds geen echte graphics

**Optie B: Herstel de BSS sectie in syms.toml**
- Voordeel: Overlay functies kunnen BSS aanroepen
- Nadeel: BSS "functies" zijn eigenlijk data, niet code
- Resultaat: N64Recomp kan compileren, maar BSS calls crashen mogelijk at runtime

**Optie C: Volledige symbol regeneratie**
- Gebruik decomp data om alles opnieuw te genereren
- Correcte functie grenzen
- Correcte BSS/data handling
- Meeste werk, maar beste resultaat

### Huidige Staat

- `waverace.toml` heeft ~20 overlay functies gestubbed
- `waverace.syms.toml` mist de BSS sectie
- N64Recomp faalt op elke overlay functie die BSS aanroept
- Build is niet mogelijk zonder verdere actie

### Geleerde Lessen

1. **Incrementeel testen is waardevol** - Het liet ons de BSS dependency ontdekken
2. **Symbol tables zijn complex** - Functie grenzen, BSS, en overlays moeten allemaal kloppen
3. **Git commits zijn belangrijk** - De BSS sectie was ooit aanwezig maar verloren gegaan
4. **N64 recompilatie vereist complete data** - Je kunt niet "half" een overlay werkend krijgen

### Volgende Stappen voor Toekomstige Sessie

1. **Check git log** voor wanneer BSS sectie verdween
2. **Herstel BSS sectie** uit git history of regenereer uit decomp
3. **Overweeg volledige symbol regeneratie** met decomp's 215 functies
4. **Fix main code ontbrekende functies** (0x80094xxx range)

---

*De incrementele aanpak onthulde dat het probleem dieper zit dan één ontbrekende functie - de hele symbol infrastructure moet herzien worden.*
