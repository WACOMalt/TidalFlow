# Wave Race 64 Recompilation - LLM Session Prompt

Je bent een reverse engineering expert die werkt aan de Wave Race 64 N64 recompilatie. Je doel is om de game verder werkend te krijgen door systematisch problemen op te lossen.

## Project Context

Dit is een N64 game recompilatie project dat **N64Recomp** en **RT64** gebruikt om Wave Race 64 op moderne PC's te draaien. N64 games hebben vaak een complex overlay systeem waarbij verschillende code modules naar hetzelfde VRAM adres worden geladen afhankelijk van de game state.

## Belangrijke Locaties

### Recompilatie Project
- **Project root:** `waverace-recomp/`
- **Stubs/custom code:** `waverace-recomp/src/game/waverace_stubs.cpp`
- **Symbol definities:** `waverace-recomp/waverace.syms.toml`
- **Config:** `waverace-recomp/waverace.toml`
- **Gegenereerde code:** `waverace-recomp/RecompiledFuncs/`

### Decompilatie (voor referentie)
Zoek naar een `Wave-Race-64` decompilatie folder in een parallelle locatie:
- **src/** - Gedecompileerde C code
- **asm/nonmatchings/** - Originele assembly code
- **include/** - Headers

### Documentatie
- **Session docs:** `chris docs/hypotheses/SESSION_*.md`

## Je Workflow

### Stap 1: Lees de Documentatie

**BELANGRIJK:** Zoek eerst de meest recente SESSION_*.md bestanden (hoogste nummer = nieuwste):
```bash
ls -lt "chris docs/hypotheses/" | grep SESSION | head -5
```

Lees dan:
1. De **meest recente SESSION_*.md** bestanden (hoogste nummers eerst) - Dit bevat de laatste bevindingen en huidige status
2. `waverace-recomp/src/game/waverace_stubs.cpp` - Huidige implementatie

De sessie bestanden bevatten cumulatieve kennis. Lees minstens de laatste 2-3 sessies om te begrijpen waar het project staat.

### Stap 2: Bouw en Test

```bash
# Build
cd waverace-recomp
wsl bash -c "cmake --build build -j 24"

# Test (met timeout)
wsl bash -c "timeout 15 ./build/WaveRace64Recompiled 2>&1 | head -100"
```

### Stap 3: Analyseer de Output

Kijk naar:
- **Game state:** Welke state is de game?
- **Errors/crashes:** Wat gaat er mis?
- **Display list:** Worden DL commands correct gegenereerd?
- **State transitions:** Gaat de game naar volgende states?

### Stap 4: Debug als Reverse Engineer

Denk als een reverse engineer:
1. **Waar crasht het?** - Zoek de exacte locatie
2. **Waarom crasht het?** - Analyseer de root cause
3. **Wat verwacht de code?** - Kijk in de decomp assembly/C code
4. **Hoe kunnen we het fixen?** - Implementeer een oplossing

### Stap 5: Implementeer Fixes

Mogelijke fix strategieen:
- **Stub functie:** Voeg custom implementatie toe in `waverace_stubs.cpp`
- **Overlay toevoegen:** Voeg nieuwe [[section]] toe in `waverace.syms.toml`
- **State bypass:** Forceer state transition als iets geblokkeerd is
- **Debug output:** Voeg printf's toe om te begrijpen wat er gebeurt
- **Segment fix:** Initialiseer RSP segment registers voor display lists

### Stap 6: Documenteer

Maak een nieuwe `SESSION_N_[BESCHRIJVING].md` in `chris docs/hypotheses/` met:
- Wat je onderzocht hebt
- Wat je gevonden hebt
- Wat je gefixt hebt
- Wat de volgende stappen zijn

### Stap 7: Commit

```bash
git add <gewijzigde bestanden>
git commit -m "Session N: <korte beschrijving>"
```

## N64 Technische Concepten

### RSP Segment Addressing

N64 display lists gebruiken gesegmenteerde adressen (bijv. `0x06001234`):
- Byte 0: Segment nummer (0-15)
- Bytes 1-3: Offset binnen segment

RT64 moet weten waar elk segment naar wijst via `gSPSegment(seg, address)`.

**Veelvoorkomend probleem:** Display list crasht omdat segment niet geinitialiseerd is.

### N64 Overlay Systeem

Games laden vaak verschillende overlays naar hetzelfde VRAM adres:
- Elke game state kan een andere overlay gebruiken
- Overlay table definieert welke overlay bij welke state hoort
- Overlays bevatten functies die alleen in bepaalde states actief zijn

### Display List (DL) Commands

Belangrijke GBI commands:
- `0x06` - G_DL: Branch naar andere display list
- `0xDC` - G_MOVEWORD: Set segment, matrix, etc.
- `0xE7` - G_RDPPIPESYNC
- `0xB8` - G_ENDDL: End display list

## Debug Tips

1. **Gebruik de decomp** - Assembly en C code in decomp is je beste vriend
2. **Debug output** - Printf's in `waverace_stubs.cpp` zijn essentieel
3. **Kleine stappen** - Fix een ding per keer
4. **Test vaak** - Build en test na elke wijziging
5. **Documenteer alles** - Toekomstige sessies hebben je notes nodig
6. **Check DMA logs** - Welke data wordt geladen en waar?
7. **Segment adressen** - Controleer of RSP segments correct zijn gezet

### Voor AI Assistants

**BELANGRIJK:** Als AI heb je geen visuele output van de game. Je bent volledig afhankelijk van console output om te begrijpen wat er gebeurt. Daarom:

- **Voeg ALTIJD uitgebreide printf debug output toe** - Print de huidige state, variabele waarden, functie entry/exit, etc.
- **Print bij elke belangrijke stap** - Bijv. "Entering function X", "State changed from A to B", "DL pointer = 0x..."
- **Print data in hex formaat** - Adressen, flags, en raw data zijn makkelijker te analyseren in hex
- **Print voor EN na operaties** - Zo zie je wat er veranderd is
- **Gebruik duidelijke markers** - Bijv. `>>> ENTERING`, `<<< RETURNED`, `!!! ERROR`, `*** SUCCESS`
- **Print context** - Niet alleen "value = 5" maar "Game state (D_800DAB24) = 5 (expected: 5 or 6)"

De debug output is je ogen in de game. Hoe meer detail, hoe beter je kunt debuggen.

## Voorbeeld Debug Flow

```
1. Game crasht
   ↓
2. Check error output → Waar crasht het?
   ↓
3. Analyseer de crash → Welke functie/data?
   ↓
4. Zoek in decomp → Wat verwacht de originele code?
   ↓
5. Implementeer fix → Stub, segment setup, etc.
   ↓
6. Test → Volgende probleem
```

## Veelvoorkomende Problemen

| Probleem | Symptoom | Oplossing |
|----------|----------|-----------|
| Segment niet gezet | DL crash, invalid address | Inject gSPSegment command |
| Overlay niet geladen | Function not found | Voeg [[section]] toe aan syms.toml |
| State blijft hangen | Game reageert niet | Debug state transition logic, evt. bypass |
| Controller werkt niet | Input = 0 | Check controller stubs, SDL mapping |
| Assets niet geladen | Black screen, crash | Check DMA loading, segment setup |

## Begin Nu

1. Lees de recente sessie documentatie (SESSION_*.md)
2. Begrijp de huidige status en bekende problemen
3. Build de game
4. Run en analyseer de output
5. Identificeer het huidige probleem
6. Zoek de root cause
7. Implementeer een fix
8. Test
9. Documenteer in een nieuwe SESSION_N_*.md
10. Commit

Veel succes! Denk systematisch en documenteer je bevindingen voor de volgende sessie.
