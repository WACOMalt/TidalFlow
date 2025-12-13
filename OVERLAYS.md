# Wave Race 64: Overlay Structuur & Voortgang

## Huidige Status (2024-12)

```
GAME START
    │
    ▼
┌─────────────────────────────────────────────────────────────┐
│ MAIN CODE (.text)                                           │
│ ROM:  0x00001000 - 0x0008CDB0                               │
│ VRAM: 0x80046800 - 0x800D25B0                               │
│ ~600 functies                                               │
│ Status: Grotendeels OK                                      │
└─────────────────────────────────────────────────────────────┘
    │
    │ roept aan
    ▼
┌─────────────────────────────────────────────────────────────┐
│ OVERLAY 0x801E (.overlay_801E) - DISPLAY LIST CODE          │
│ ROM:  0x000A95E8 - 0x000C2850                               │
│ VRAM: 0x801E0000 - 0x801F9268                               │
│ ~153 functies gedefinieerd                                  │
│                                                             │
│ Voortgang compilatie:                                       │
│ ├── ovl_func_801E0000  OK                                   │
│ ├── ovl_func_801E006C  OK                                   │
│ ├── ovl_func_801E012C  OK                                   │
│ ├── ... (12 meer)      OK                                   │
│ ├── ovl_func_801E1D7C  WARNING (tail call - compileert OK)  │
│ ├── ovl_func_801E270C  ERROR: Jump table buiten overlay     │
│ ├── ... (~140 nog niet bereikt)                             │
│ └── ovl_func_801ECAF4  Belangrijkste: display list builder  │
│                                                             │
│ DATA SECTIE (nog niet gedefinieerd):                        │
│ VRAM: 0x80220000 - ???                                      │
│ Bevat: jump tables voor switch statements                   │
└─────────────────────────────────────────────────────────────┘
    │
    │ roept aan
    ▼
┌─────────────────────────────────────────────────────────────┐
│ BSS SECTIE (.bss_801D) - DATA/RUNTIME FUNCTIES              │
│ ROM:  0x000A45A0                                            │
│ VRAM: 0x801DAFB8 - 0x801DFFF8                               │
│ 33 functies (allemaal gestubbed - zijn data, geen code)     │
│ Status: Gestubbed                                           │
└─────────────────────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────────────────────┐
│ OVERLAY 0x802C (.overlay_802C) - EXTRA CODE                 │
│ ROM:  0x001AC7B0                                            │
│ VRAM: 0x802C0000 - 0x802C9950                               │
│ 20 functies                                                 │
│ Status: Nog niet bereikt                                    │
└─────────────────────────────────────────────────────────────┘
```

## Huidige Error

```
Failed to analyze ovl_func_801E270C
Error recompiling ovl_func_801E270C
Failed to determine size of jump table at 0x80225F68 for instruction at 0x801E2758
```

De functie `ovl_func_801E270C` heeft een switch statement met een jump table op `0x80225F68`.
Dit adres ligt in de DATA sectie van de overlay, niet in de CODE sectie.

## Error Types & Oplossingen

| Error | Betekenis | Oplossing |
|-------|-----------|-----------|
| `No function found for jal target` | Functie niet gedefinieerd | Voeg functie toe of splits bestaande functie |
| `branching outside of function` | Functie springt buiten eigen grenzen | Kan tail call zijn (OK) of size fout (fix) |
| `Failed to determine size of jump table` | Switch statement, data buiten overlay | Stub functie OF definieer data sectie |

## Tail Calls (Warnings die OK zijn)

```asm
; Tail call optimalisatie - NORMAAL
ovl_func_801E1D7C:
    ; doe werk
    j ovl_func_801E1E8C    ; spring direct naar andere functie
                           ; GEEN eigen return - andere functie doet dat
```

Dit geeft warning "branching outside of function" maar compileert correct.

## Jump Table Probleem: ovl_func_801E270C (Volledig Geanalyseerd)

### De Switch Code
```asm
0x801E2738: LUI r14, 0x800E       ; laad state variabele adres
0x801E273C: LW r14, -32400(r14)   ; r14 = *(0x800E8170) = switch index
0x801E2740: SLTIU r1, r14, 8      ; check index < 8
0x801E2744: BEQ r1, r0, exit      ; als >= 8, exit
0x801E2748: SLL r14, r14, 2       ; index *= 4
0x801E274C: LUI r1, 0x8022        ; r1 = 0x80220000
0x801E2750: ADDU r1, r1, r14      ; r1 += index
0x801E2754: LW r14, 0x5F68(r1)    ; r14 = *(0x80225F68 + index*4) = case target
0x801E2758: JR r14                ; spring naar case handler
```

### Verwachte Jump Table (0x80225F68)
```
[0x80225F68] = 0x801E2760  (case 0) -> roept 0x801EC274 aan
[0x80225F6C] = 0x801E276C  (case 1) -> roept 0x801EC404 aan
[0x80225F70] = 0x801E278C  (case 2) -> roept 0x801EC5A8 aan
[0x80225F74] = 0x801E2798  (case 3) -> roept 0x801EC60C aan
[0x80225F78] = 0x801E27A4  (case 4) -> roept 0x801ECD34 aan
[0x80225F7C] = 0x801E27B0  (case 5) -> roept 0x801ED2D0 aan
[0x80225F80] = 0x801E2870  (case 6) -> exit
[0x80225F84] = 0x801E2870  (case 7) -> exit
```

### Root Cause Analyse

**Verificatie uitgevoerd:**
1. Overlay ROM base 0x0A95E8 - BEVESTIGD met hex check
2. Functie start 0x801E270C op ROM 0x0ABCF4 - BEVESTIGD (ADDIU SP, SP, -24)
3. Switch instructies exact zoals gedocumenteerd - BEVESTIGD

**Waarom de jump table NIET in ROM staat:**
1. Jump table adressen (0x801E27xx) staan NIET als 32-bit waarden in ROM
2. ROM adres 0x0A95E8 staat nergens als pointer in ROM
3. Geen LUI 0x801E instructies in main code die overlay laden
4. 0x8022xxxx is een BSS/uninitialized data sectie

**Conclusie:** De overlay heeft een BSS sectie op 0x8022xxxx die RUNTIME wordt gevuld.
De N64 loader of een init functie initialiseert de jump table bij overlay load.

### N64Recomp Limitatie

N64Recomp berekent ROM adres van jump table als:
```
rom_addr = jtbl_vram - func_vram + func_rom
         = 0x80225F68 - 0x801E270C + 0x0ABCF4
         = 0x000EF550 (ongeldig - bevat graphics data)
```

Dit faalt omdat de jump table in een andere VRAM sectie (0x8022xxxx) zit
dan de code (0x801Exxxx), met een andere ROM mapping.

### Oplossingen

| Optie | Beschrijving | Impact |
|-------|--------------|--------|
| STUB | Voeg functie toe aan stubs | Switch werkt niet, state machine kapot |
| C++ PATCH | Herschrijf functie in C++ met handmatige switch | Volledig werkend |
| FAKE DATA | Creëer data sectie met jump table in ROM | Complex, ROM modificatie nodig |

## Aanpak

1. Overlay 0x801E gedefinieerd met 153 functies
2. BSS sectie 0x801D toegevoegd (33 data functies gestubbed)
3. Functies 1-voor-1 fixen:
   - Als "No function found" -> functie toevoegen/splitsen in syms.toml
   - Als "jump table" error -> functie stubben (voorlopig)
4. Later: DATA sectie definiëren voor volledige switch support
