# Wave Race 64 Recompilation - Display Issue Analysis

## Huidige Status
De game draait, RT64 (renderer) verwerkt display lists succesvol (~35 FPS), maar het scherm toont alleen een cyclende paars/blauwe kleur in plaats van echte game graphics.

## Project Structuur

```
wave-race-64-recomp-claude-code-opus45/
├── waverace-recomp/                    # Hoofdproject
│   ├── build/                          # Build output (WSL/Linux)
│   │   └── WaveRace64Recompiled        # Executable (ELF, run via WSL)
│   ├── waverace.toml                   # N64Recomp configuratie
│   ├── waverace.syms.toml              # Functie definities (alleen .text sectie)
│   ├── waverace.us.z64                 # ROM file (moet hier staan)
│   ├── patches/
│   │   └── overlays.c                  # Overlay segment definities
│   ├── src/game/
│   │   └── waverace_stubs.cpp          # Custom stub implementaties
│   └── lib/N64ModernRuntime/
│       └── librecomp/src/sp.cpp        # RSP task handling + DL fixing
│
├── recompiled_games_use_these_for_info_and_reference_dont_makeup_ideas_analyse_these/
│   ├── zelda64recomp-reference/        # Referentie project
│   ├── dino-recomp/                    # Referentie project (796 DLLs!)
│   └── mk64recomp/                     # Referentie project
│
├── splat_output/                       # Leeg - splat niet volledig uitgevoerd
├── waverace64.yaml                     # Splat configuratie
└── *.py                                # Analyse scripts
```

## Het Probleem: Overlay Code

### Wat zijn Overlays?
N64 games laden code dynamisch in RAM vanwege beperkt geheugen. Wave Race 64 heeft:
- **Main code**: ROM 0x1000-0x8CDB0 → VRAM 0x80046800-0x800D25B0 (recompiled!)
- **RACING overlay**: ROM 0xF7510 → VRAM 0x8028DF00, size 0x2C470
- **ENDING overlay**: ROM 0x123640 → VRAM 0x80280000, size 0xDF00

### Het Probleem
De main code roept functies aan op adressen `0x801Exxxx` (bijv. `0x801ECAF4`), maar:
1. Dit is **NIET** het RACING segment (dat zit op 0x8028xxxx)
2. Er is ergens een **derde overlay** die naar 0x801Exxxx laadt
3. Deze overlay is waarschijnlijk **gecomprimeerd** in de ROM

### Bewijs uit de Code
In `waverace.toml` staan ~50 functies als "stubs" omdat ze overlay code aanroepen:
```toml
# Functions that call overlay/RSP code (0x801xxxxx, 0x84xxxxxx)
"func_80092CF0",  # Calls overlay 0x801ECAF4 - dit bouwt display lists!
```

## Hoe Reference Projects Overlays Laden

### dino-recomp (796 DLLs!)

**Structuur:**
```
dino-recomp/
├── dino.toml                               # Hoofd config
├── lib/dino-recomp-decomp-bridge/
│   ├── dino.syms.toml                      # Alle functies + overlay secties
│   └── dino.dlls.txt                       # Lijst van overlay sectie namen
```

**1. `dino.toml`** - Verwijst naar overlay lijst:
```toml
[input]
entrypoint = 0x80000400
symbols_file_path = "lib/dino-recomp-decomp-bridge/dino.syms.toml"
relocatable_sections_path = "lib/dino-recomp-decomp-bridge/dino.dlls.txt"
rom_file_path = "baserom.patched.z64"
```

**2. `dino.dlls.txt`** - Eén sectie naam per regel:
```
.dll1
.dll2
.dll3
...
.dll796
```

**3. `dino.syms.toml`** - Bevat ALLES: main code + overlays:
```toml
# Main code sectie
[[section]]
name = ".segment"
rom = 0x00001000
vram = 0x80000400
size = 0xA3AA0

functions = [
    { name = "bootproc", vram = 0x80001040, size = 0x58 },
    ...
]

# Overlay/DLL secties (deze staan in dino.dlls.txt)
[[section]]
name = ".dll1"
rom = 0x383184C
vram = 0x81000080
size = 0x8330
got_address = 0x81007680      # Global Offset Table (voor relocatie)

functions = [
    { name = "__dll1_dll_1_ctor", vram = 0x81000080, size = 0x180 },
    ...
]

[[section]]
name = ".dll2"
rom = 0x3839B88
vram = 0x8100908C
size = 0x2840
got_address = 0x8100B67C

functions = [...]
```

**Belangrijk:** De `got_address` is nodig voor relocaties - dit is waar de Global Offset Table zit die pointers naar andere functies bevat.

### zelda64recomp (Majora's Mask)

**Structuur:**
```
zelda64recomp-reference/
├── us.rev1.toml                    # Hoofd config
├── overlays.us.rev1.txt            # Lijst overlay namen (574 overlays!)
├── Zelda64RecompSyms/              # Externe submodule
│   └── mm.us.rev1.syms.toml        # Functies + overlay secties
```

**1. `us.rev1.toml`**:
```toml
[input]
entrypoint = 0x80080000
symbols_file_path = "Zelda64RecompSyms/mm.us.rev1.syms.toml"
relocatable_sections_path = "overlays.us.rev1.txt"
rom_file_path = "mm.us.rev1.rom_uncompressed.z64"    # <-- UNCOMPRESSED!
```

**2. `overlays.us.rev1.txt`** - Overlay namen met `..` prefix:
```
..ovl_title
..ovl_select
..ovl_opening
..ovl_file_choose
..ovl_kaleido_scope
..ovl_player_actor
..ovl_En_Test
...
```

**Belangrijk:** Zelda gebruikt een **UNCOMPRESSED ROM**! De originele MM ROM is Yaz0 gecomprimeerd. Er is een apart decompressie tool nodig.

### mk64recomp (Mario Kart 64)

**Simpelste setup** - geen overlays nodig:
```toml
[input]
entrypoint = 0x80000400
symbols_file_path = "MarioKart64RecompSyms/mk64.us.syms.toml"
rom_file_path = "mk64.us.z64"
use_absolute_symbols = true
# Geen relocatable_sections_path - MK64 heeft geen dynamische overlays!
```

MK64 laadt alle code statisch, dus geen overlay handling nodig.

## Huidige Workaround

Omdat overlay code niet beschikbaar is, bouwt `func_80092CF0_impl` een minimale display list:

```cpp
// In waverace_stubs.cpp
extern "C" void func_80092CF0_impl(uint8_t* rdram, recomp_context* ctx) {
    // Bouw minimale DL: G_SETCIMG, G_SETFILLCOLOR, G_FILLRECT, G_ENDDL
    // Dit vult het scherm met een cyclende kleur
}
```

En `sp.cpp` fixt ongeldige G_DL commands:
```cpp
static void fix_display_list(uint8_t* rdram, uint32_t data_ptr) {
    // Vervang G_DL commands naar 0x00000000 met NOOPs
    // Voeg G_ENDDL toe als terminator
}
```

## Bouwen en Runnen

```bash
# In WSL (Linux)
cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp

# Bouwen
cd build && make -j$(nproc)

# Runnen (vanuit waverace-recomp directory!)
./build/WaveRace64Recompiled
```

## Oplossing: Overlay Extractie

### GEVONDEN: De 0x801E Overlay

Via hex analyse van de ROM is de overlay code gelokaliseerd:

```
============================================================
OVERLAY PARAMETERS (gevonden via ROM analyse)
============================================================
ROM start:  0x0A95E8
VRAM start: 0x801E0000
Size:       0x19168 (102,760 bytes)
ROM end:    0x0C2750
VRAM end:   0x801F9168
============================================================
```

**Verificatie** - JAL targets matchen met code op de ROM offsets:
```
0x801E1290 -> ROM 0x0AA878: 27A5004C (ADDIU A1, SP, 0x4C) ✓
0x801E1E8C -> ROM 0x0AB474: 3C028023 (LUI V0, 0x8023)     ✓
0x801E3250 -> ROM 0x0AC838: C60600B4 (LWC1 F6, 0xB4(S0)) ✓
0x801ECAF4 -> ROM 0x0B60DC: 3C0E8023 (LUI T6, 0x8023)     ✓ <-- Display list builder!
```

### Stap 1: Voeg overlay sectie toe aan syms.toml

Voeg toe aan `waverace.syms.toml`:
```toml
[[section]]
name = ".overlay_801E"
rom = 0x000A95E8
vram = 0x801E0000
size = 0x19168

functions = [
    # Functies moeten geëxtraheerd worden - zie extract_overlay_funcs.py
    { name = "ovl_func_801E0000", vram = 0x801E0000, size = 0x??? },
    { name = "ovl_func_801E1290", vram = 0x801E1290, size = 0x??? },
    { name = "ovl_func_801ECAF4", vram = 0x801ECAF4, size = 0x??? },  # Display list builder
    ...
]
```

### Stap 2: Maak waverace.overlays.txt

Maak bestand `waverace.overlays.txt`:
```
.overlay_801E
```

### Stap 3: Update waverace.toml

```toml
[input]
entrypoint = 0x80046800
symbols_file_path = "./waverace.syms.toml"
rom_file_path = "waverace.us.z64"
output_func_path = "RecompiledFuncs"
use_absolute_symbols = true
relocatable_sections_path = "waverace.overlays.txt"    # <-- TOEVOEGEN
```

### Stap 4: Extracteer functie boundaries

Run `extract_overlay_funcs.py` met de nieuwe parameters:
```bash
python extract_overlay_funcs.py
```

Dit script zoekt naar function prologues (`ADDIU SP, SP, -N`) en JAL targets om functie grenzen te bepalen.

### Stap 5: Herbouw het project

```bash
cd waverace-recomp/build
cmake ..
make -j$(nproc)
```

### Stap 6: Verwijder stubs

Na succesvolle recompilatie van de overlay, verwijder de overlay-aanroepende functies uit de `stubs` lijst in `waverace.toml`:
- `func_80092CF0` (deze roept nu de echte `ovl_func_801ECAF4` aan)
- En andere functies die naar 0x801Exxxx springen

## Relevante Bestanden voor Debugging

| Bestand | Doel |
|---------|------|
| `waverace.toml` | N64Recomp config, stubs lijst |
| `waverace.syms.toml` | Functie definities |
| `patches/overlays.c` | Overlay segment definities |
| `src/game/waverace_stubs.cpp` | Custom functie implementaties |
| `lib/.../sp.cpp` | RSP task + display list handling |
| `extract_overlay_funcs.py` | Script om overlay functies te vinden |
| `find_overlay_funcs.py` | Script om overlay calls te vinden |

## Debug Output Interpreteren

Belangrijke log messages:
```
[RT64] send_dl: DONE          # RT64 heeft DL verwerkt (goed!)
[GFX-TASK #N]                 # Graphics task submitted
osViSwapBuffer                # Framebuffer swap (game loopt)
```

Als RT64 hangt, check:
- Ongeldige G_DL commands (naar 0x00000000)
- Missende G_ENDDL terminator
- Adressen buiten RDRAM range (< 0x80000000 of >= 0x80800000)
