# Lessons Learned from N64 Recompilation Projects

Dit document bevat alles wat we geleerd hebben van Zelda64Recomp en MK64Recomp.

## BELANGRIJK: Gebruik Splat, NIET incomplete decomps!

### Waarom NIET de LLONSIT decomp (of andere incomplete decomps) gebruiken
- **Incomplete decomps** hebben verkeerde/ontbrekende functiegrenzen
- Je raakt steeds dezelfde problemen aan het fixen (verkeerde sizes, missing functies)
- Functies krijgen willekeurige sizes (bijv. 0x10) die niet kloppen
- Dit leidt tot eindeloze "branching outside of function" errors

### Waarom WEL Splat gebruiken
Splat analyseert de **werkelijke ROM binary** en:
- Detecteert functiegrenzen automatisch door JAL targets en MIPS patronen
- Berekent accurate sizes uit de daadwerkelijke code
- Genereert `func_XXXXXXXX` namen (N64Recomp geeft niet om namen!)
- Werkt "out of the box" zonder handmatig fixen

### Splat installatie
```bash
# Ubuntu/Debian
pipx install splat64

# Of met pip (in venv)
python3 -m venv venv
source venv/bin/activate
pip install splat64
```

### Splat workflow voor N64Recomp
```bash
# 1. Maak een basis yaml config
splat init game.z64

# 2. Pas de yaml aan (zie splat docs)

# 3. Split de ROM om functies te detecteren
splat split game.yaml

# 4. Converteer splat output naar N64Recomp symbols.toml
# (Script nodig - zie converter scripts)
```

### Splat yaml voorbeeld (Wave Race 64)
```yaml
name: Wave Race 64 (US)
sha1: XXXX
options:
  platform: n64
  basename: waverace
  target_path: waverace.us.z64
  base_path: .
  compiler: IDO
  find_file_boundaries: yes
  auto_all_sections: [.data, .rodata, .bss]
segments:
  - [0x0, header, header]
  - [0x40, bin, ipl3]
  - name: main
    type: code
    start: 0x1000
    vram: 0x80046800
    subsegments:
      - [auto, c]
```

## TOML Config: stubs vs ignored

### stubs
- Functies in de `stubs` array worden door N64Recomp **automatisch uitgestubbed**
- N64Recomp genereert lege functies voor deze symbols
- Gebruik voor functies die direct RCP registers manipuleren of kseg1 adressen gebruiken

Voorbeeld uit Zelda64Recomp:
```toml
[patches]
stubs = [
    "RcpUtils_PrintRegisterStatus",
    "RcpUtils_Reset",
    "CIC6105_Init"
]
```

### ignored
- Functies in de `ignored` array worden **volledig geskipped** door N64Recomp
- Geen code wordt gegenereerd voor deze functies
- De game/runtime moet ze zelf implementeren of ze worden nooit aangeroepen

Voorbeeld uit MK64Recomp:
```toml
ignored = [
    "__osBlockSum",
    "__osPfsReleasePages",
    "__osPfsDeclearPage",
]
```

## N64Recomp Functie Generatie

### static_0_XXXXXXXX functies
- N64Recomp detecteert automatisch JAL targets die geen symbol hebben
- Deze worden gegenereerd als `static_0_XXXXXXXX` functies
- **NIET handmatig stubben** - N64Recomp genereert ze al!
- Als ze undefined reference geven, betekent dit dat ze in de symbols file moeten staan

### Functies die WEL handmatig gedefinieerd moeten worden:
1. `get_entrypoint_address()` - moet returnen: `(gpr)(int32_t)0x80XXXXXX`
2. `recomp_entrypoint` - extern declaration in main.cpp
3. RSP microcode functies (aspMain, njpgdspMain)
4. Custom game functions (input, audio callbacks, etc.)

## main.cpp Structuur (van Zelda64/MK64)

```cpp
// Extern declarations voor gegenereerde code
extern "C" void recomp_entrypoint(uint8_t* rdram, recomp_context* ctx);
gpr get_entrypoint_address();

// RSP microcode
extern RspUcodeFunc njpgdspMain;
extern RspUcodeFunc aspMain;

// Game entry
std::vector<recomp::GameEntry> supported_games = {
    {
        .rom_hash = 0xXXXXXXXXXXXXXXXXULL,
        .internal_name = "GAME NAME",
        .game_id = u8"game.n64.region.version",
        .save_type = recomp::SaveType::Eeprom4k,  // of Flashram, Sram, etc.
        .entrypoint_address = get_entrypoint_address(),
        .entrypoint = recomp_entrypoint,
    },
};
```

## Symbols TOML Formaat

Functies moeten gedefinieerd worden in de symbols file:
```toml
[[section]]
name = ".text"
rom = 0x1000
vram = 0x80001000
size = 0x100000

[[func]]
name = "func_name"
vram = 0x80001234
size = 0x100  # Berekend als verschil naar volgende functie
```

## Instruction Patches

Zelda64Recomp gebruikt instruction patches voor kleine fixes:
```toml
[[patches.instruction]]
func = "FunctionName"
vram = 0x80XXXXXX
value = 0x00000000  # nop, of andere instructie
```

## Belangrijk: Sign Extension

Op 64-bit systemen moet het entrypoint address sign-extended worden:
```cpp
gpr get_entrypoint_address() {
    return (gpr)(int32_t)0x80046800u;  // NIET gewoon 0x80046800!
}
```

## Multiple Definition Errors

Als je "multiple definition" linker errors krijgt:
1. Check of N64Recomp de functie al genereert (grep in RecompiledFuncs/*.c)
2. Verwijder dubbele definities uit je stubs file
3. N64Recomp genereert: sqrtf_recomp, guOrtho, guTranslate, game_dma_copy, etc.

## Project Structuur (van MK64/Zelda64)

```
project/
├── src/
│   ├── game/        # Game-specifieke code
│   ├── main/        # main.cpp, support.cpp
│   └── ui/          # UI code
├── patches/         # Game patches
├── rsp/            # RSP microcode
├── RecompiledFuncs/ # N64Recomp output (gegenereerd)
├── us.toml         # N64Recomp config
└── CMakeLists.txt
```

## RSP Microcode

RSPRecomp genereert RSP microcode. Stub implementatie:
```cpp
RspExitReason aspMain(uint8_t* rdram, uint32_t ucode_addr) {
    return RspExitReason::Broke;  // Minimale stub
}
```

## Data Symbols vs Function Symbols

Sommige symbols zijn data, niet functies:
- osClockRate, osViClock, osViModeNtscLan1 - **data symbols**
- gColorRed, gColorGreen, etc. - **data symbols**

Deze moeten in de `ignored` list of als `extern uint32_t` gedefinieerd worden.

## NIEUW: Symbols File Format (BELANGRIJK!)

### Correct formaat - functies in `functions` array
N64Recomp leest ALLEEN functies die in de `functions = []` array van de [[section]] staan:
```toml
[[section]]
name = ".text"
rom = 0x00001000
vram = 0x80046800
size = 0xF0000

functions = [
    { name = "func_80046800", vram = 0x80046800, size = 0x38 },
    { name = "static_0_80047EE0", vram = 0x80047EE0, size = 0x100 },
]
```

### VERKEERD formaat - losse [[func]] blokken
Deze worden NIET geparsed door N64Recomp:
```toml
# WERKT NIET!
[[func]]
name = "static_0_80047EE0"
vram = 0x80047EE0
size = 0x100
```

### Waarom dit belangrijk is
- Als functies niet in de `functions` array staan, kan N64Recomp ze niet vinden
- "Function X is set as ignored but does not exist!" error = functie niet in array
- Alle static_0_* functies moeten in de functions array staan

## NIEUW: Functie Size Problemen

### Symptoom
```
[Warn] Function static_0_XXXXXXXX is branching outside of the function
error: label 'L_XXXXXXXX' used but not defined
```

### Oorzaak
De `size` parameter is te klein - de code springt naar adressen buiten de functie grenzen.

### Oplossingen
1. **Beste oplossing**: Vergroot de `size` in de symbols file
2. **Workaround**: Voeg functie toe aan `ignored` lijst

Voorbeeld:
```toml
# Size 0x10 is te klein, verhoog naar 0x100
{ name = "static_0_80047B00", vram = 0x80047B00, size = 0x100 },
```

## NIEUW: Overlay Functies

### Wat zijn overlay functies?
- Functies die dynamisch geladen code aanroepen (0x801xxxxx adressen)
- Kunnen NIET statisch gerecompileerd worden

### Symptoom
```
Error: No function found for jal target: 0x801ED154
```

### Oplossing
Voeg deze functies toe aan de `ignored` lijst:
```toml
ignored = [
    "static_0_80098FF8",  # Roept overlay 0x801ED154 aan
]
```

### Later implementeren
- Overlay functies kunnen later via "live recompilation" worden toegevoegd
- De game start, maar overlay-specifieke features werken niet

## NIEUW: funcs.h Header Generatie

### Wat N64Recomp genereert
N64Recomp genereert `funcs.h` met extern declarations voor alle functies:
```c
#include "recomp.h"

#ifdef __cplusplus
extern "C" {
#endif

void recomp_entrypoint(uint8_t* rdram, recomp_context* ctx);
void sqrtf_recomp(uint8_t* rdram, recomp_context* ctx);
// ... alle andere functies
```

### Waarom dit belangrijk is
- Zonder funcs.h krijg je "undefined reference" linker errors
- De header wordt gegenereerd in RecompiledFuncs/funcs.h
- Als funcs.h leeg is, is N64Recomp gefaald of verkeerd geconfigureerd

## NIEUW: Veelvoorkomende Linker Errors

### "undefined reference to `static_0_XXXXXXXX`"
**Oorzaak**: Functie ontbreekt in symbols file
**Oplossing**: Voeg functie toe aan `functions` array in symbols file

### "multiple definition of `function_name`"
**Oorzaak**: Functie gedefinieerd in zowel stubs.cpp als gegenereerd door N64Recomp
**Oplossing**: Verwijder uit stubs.cpp (N64Recomp genereert al: sqrtf_recomp, guOrtho, etc.)

### "undefined reference to `aspMain`"
**Oorzaak**: RSP microcode niet gedefinieerd
**Oplossing**: Voeg RSP stub toe:
```cpp
RspExitReason aspMain(uint8_t* rdram, uint32_t ucode_addr) {
    return RspExitReason::Broke;
}
```

### "undefined reference to `get_entrypoint_address`"
**Oorzaak**: Entrypoint niet gedefinieerd
**Oplossing**:
```cpp
gpr get_entrypoint_address() {
    return (gpr)(int32_t)0x80046800u;  // Sign-extend!
}
```

## NIEUW: Debug Workflow

1. **N64Recomp draaien**: Check warnings over buiten-grenzen branches
2. **Build starten**: Verzamel undefined reference errors
3. **Functies toevoegen**: Voeg ontbrekende static_0_* functies toe aan symbols
4. **Herhaal**: Tot alle errors opgelost zijn

### Handige commando's
```bash
# Vind alle ontbrekende static functies
cmake --build . 2>&1 | grep 'undefined reference' | grep -oP 'static_0_[0-9A-Fa-f]+' | sort -u

# Check of functie al in symbols staat
grep "static_0_80047EE0" waverace.syms.toml
```
