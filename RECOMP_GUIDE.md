# N64 Recomp Quick Start Guide

Hoe je een N64 game recompiled naar een native PC port.

## Vereisten

1. **N64Recomp tool** - https://github.com/N64Recomp/N64Recomp
2. **Game ROM** - bijv. `game.z64`
3. **Symbols file** - uit een decompilatie project of handmatig gemaakt
4. **Een bestaand recomp project als template** - bijv. MK64Recomp, Zelda64Recomp

## Stap 1: N64Recomp Tool Bouwen

```bash
git clone --recurse-submodules https://github.com/N64Recomp/N64Recomp
cd N64Recomp
mkdir build && cd build
cmake ..
cmake --build .
```

Output: `N64Recomp` en `RSPRecomp` executables.

## Stap 2: Symbols Verkrijgen

Je hebt een `.syms.toml` file nodig met functies. Opties:

**A) Met Splat (aanbevolen):**

Splat is een tool om N64 ROMs te splitsen en symbols te extraheren.
https://github.com/ethteck/splat

```bash
# Installeer splat
pip install splat64

# Maak een yaml config voor je ROM (zie decomp projects voor voorbeelden)
# Run splat op je ROM
splat split config.yaml
```

Splat genereert:
- `asm/*.s` files met functies en labels
- `symbol_addrs.txt` met adressen
- Segmenten met ROM/VRAM info

Gebruik deze info om je `.syms.toml` te maken.

**B) Van een decompilatie project:**
```bash
# Voorbeeld: Wave Race 64 decomp
git clone https://github.com/LLONSIT/Wave-Race-64
cd Wave-Race-64
make extract  # genereert asm/ met functies (gebruikt splat intern)
```

Parse de asm files om symbols te extraheren (zie generate_symbols.py).

**C) Van een bestaand recomp project:**
Veel recomps hebben hun symbols al beschikbaar (bijv. MarioKart64RecompSyms, Goemon64RecompSyms).

## Stap 3: TOML Config Maken

Maak een config file (bijv. `game.toml`):

```toml
[input]
entrypoint = 0x80046800  # Start adres van de game
symbols_file_path = "./game.syms.toml"
rom_file_path = "game.z64"
output_func_path = "RecompiledFuncs"
use_absolute_symbols = true

[patches]
stubs = []
ignored = []
```

## Stap 4: MIPS naar C Recompileren

```bash
./N64Recomp game.toml
```

Dit genereert:
- `RecompiledFuncs/funcs_0.c` (en meer)
- `RecompiledFuncs/funcs.h`

## Stap 5: Runtime Project Opzetten

Clone een bestaand recomp project als basis:

```bash
# MK64 Recomp is een goede simpele basis
git clone --recurse-submodules https://github.com/sonicdcer/MarioKart64Recomp
```

## Stap 6: Aanpassingen Maken

1. **CMakeLists.txt**: Rename project naam
2. **src/main/main.cpp**: Update `GameEntry`:
   - `rom_hash` - Hash van je ROM
   - `internal_name` - Game naam
   - `game_id` - ID string
   - `save_type` - Type save (Eep4k, Eep16k, Sram, FlashRam, None)

3. **RecompiledFuncs/**: Kopieer je gegenereerde C files
4. **TOML files**: Pas paden aan

## Stap 7: Bouwen

```bash
mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -G Ninja ..
cmake --build . -j4
```

## Belangrijke Info

### Wat werkt direct:
- MIPS -> C conversie
- Basis game logica

### Wat game-specifiek is:
- RSP microcode (audio/graphics tasks)
- Overlays (dynamisch geladen code)
- Specifieke patches voor bugs

### Referentie Projecten:
- **Zelda64Recomp** - Meest complete, goed gedocumenteerd
- **MarioKart64Recomp** - Simpele structuur, goed als template
- **Goemon64Recomp** - Nog een voorbeeld
- **Dino-recomp** - Meer geavanceerd met DLL systeem

## Bronnen

- N64Recomp: https://github.com/N64Recomp/N64Recomp
- N64ModernRuntime: https://github.com/N64Recomp/N64ModernRuntime
- RT64 Renderer: https://github.com/rt64/rt64
- Splat: https://github.com/ethteck/splat
- HackMD Modding Guide: https://hackmd.io/fMDiGEJ9TBSjomuZZOgzNg
- MarioKart64Recomp: https://github.com/sonicdcer/MarioKart64Recomp
- Goemon64Recomp: https://github.com/klorfmorf/Goemon64Recomp

## Tips

1. **Begin met een werkend project** - Clone MK64Recomp en pas aan
2. **Symbols zijn cruciaal** - Zonder symbols geen recomp
3. **Overlays later** - Focus eerst op main segment
4. **Test vaak** - Build na elke aanpassing
