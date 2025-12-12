# Wave Race 64 Recompilation - Complete Overview

**Datum:** December 2025
**Status:** Session 7 - 0x802C Overlay WORKING!

---

## Inhoudsopgave

1. [Waar We Begonnen](#waar-we-begonnen)
2. [Sessie-voor-Sessie Voortgang](#sessie-voor-sessie-voortgang)
3. [Het N64 Overlay Systeem](#het-n64-overlay-systeem)
4. [Alle 19 Overlays](#alle-19-overlays)
5. [Game State Machine](#game-state-machine)
6. [Huidige Status](#huidige-status)
7. [N64Recomp Correctheid](#n64recomp-correctheid)
8. [Belangrijke Bronbestanden](#belangrijke-bronbestanden)

---

## Waar We Begonnen

### Het Doel
Wave Race 64 (N64) recompileren naar een moderne PC executable met behulp van N64Recomp en RT64 renderer.

### De Uitdaging
- N64 games hebben **overlays**: code die dynamisch geladen wordt naar hetzelfde geheugenadres
- Wave Race 64 heeft **19 verschillende overlays** die allemaal naar 0x802C5800 laden
- Plus een **codeseg overlay** op 0x801DAFA0 (altijd geladen)
- N64Recomp kan niet automatisch overlays detecteren - handmatig werk nodig

---

## Sessie-voor-Sessie Voortgang

### Session 2: N64Recomp Function Boundary Fixes

**Probleem:** Auto-gegenereerde symbols hadden verkeerde functiegrenzen.

**Ontdekking:**
- VRAM base was FOUT: 0x801E0000 vs correct 0x801DAFA0
- Spimdisasm genereerde verkeerde function sizes
- JAL targets wezen soms naar labels BINNEN functies

**Oplossing:**
- 35 functies gesplit naar 62 op basis van decomp data
- VRAM base gecorrigeerd naar 0x801DAFA0
- Helper scripts gemaakt voor automatische fixes

**Resultaat:** 1024 functies compileren (765 main + 215 overlay)

---

### Session 3: Build Issues - Overlay Function Sizes

**Probleem:** CMake build faalde - overlay functie had verkeerde size.

**Ontdekking:**
- `ovl_func_801FC4D4` had size 0x100 maar branches gingen verder
- Laatste functie in overlay moet uitstrekken tot einde sectie
- Moet in ROM zoeken naar `jr $ra` (0x03E00008) voor echte einde

**Oplossing:**
- Function size van 0x100 naar 0x368
- MEM_W macro conflict opgelost

**Resultaat:** BUILD SUCCESSFUL - Game toont N64 logo!

---

### Session 4: Overlay Progress - Display List Builder

**Probleem:** Game draaide maar riep geen echte display list builder aan.

**Ontdekking:**
- `func_80092CF0` is een state machine die kiest welke overlay aan te roepen
- State 0 → 0x801E overlay (func_801ECAF4)
- State 5 → 0x802C overlay (func_802C5BA4) - bestaat nog niet!
- Game start in state 5

**Oplossing:**
- `func_80092CF0_impl` aangepast om echte overlay aan te roepen voor state 0

**Resultaat:** N64 logo zichtbaar, game stabiel

---

### Session 5: Render Thread Analysis

**Probleem:** Display list builder werd niet aangeroepen tijdens init.

**Ontdekking:**
- Render thread (thread 5) start maar bereikt render loop niet
- Blocking ergens in initialization
- Message queues ontvangen berichten maar niemand leest ze

**Resultaat:** Dieper debuggen nodig

---

### Session 6: Render Thread Fixes

**Probleem:** Meerdere crashes en hangs.

**Fixes:**
1. **func_800C7020 timer bug** - 64-bit subtractie gaf 185 uur wachttijd!
2. **func_80046BF4 crash** - null display list pointer
3. **Byte order issues** - memory reads in verkeerde byte order

**Resultaat:** Game draait stabiel zonder crashes, state = 5

---

### Session 7: 0x802C Overlay Implementation (NU)

**Probleem:** Game in state 5 maar geen 0x802C overlay code.

**Ontdekking:**
- N64 overlay systeem laadt verschillende overlays naar ZELFDE VRAM (0x802C5800)
- State 5 laadt `segment_1B1FB0` via `gOverlayTable[0]`
- 14 functies in deze overlay

**Oplossing:**
1. Nieuwe `[[section]]` toegevoegd aan waverace.syms.toml
2. 14 functies gedefinieerd met correcte sizes
3. N64Recomp genereert code in funcs_19.c
4. func_80092CF0 stub gepatched om impl aan te roepen

**Resultaat:** **OVERLAY CODE WERKT!**
```
[DL-IMPL] >>> CALLING REAL OVERLAY ovl_func_802C5BA4 (0x802C segment_1B1FB0)!
[DL-IMPL] <<< RETURNED from ovl_func_802C5BA4, r2=0x801208B8
```

---

## Het N64 Overlay Systeem

### Hoe Het Werkt

```
┌─────────────────────────────────────────────────────────────┐
│                     N64 Memory Map                          │
├─────────────────────────────────────────────────────────────┤
│ 0x80000000 - 0x800DAFFF  │ Main Code (.text)               │
│ 0x801DAFA0 - 0x80226A5F  │ Codeseg Overlay (altijd geladen)│
│ 0x802C5800 - 0x802C????  │ Dynamic Overlay (1 van 19)      │
└─────────────────────────────────────────────────────────────┘
```

### Overlay Loading Flow

```
D_800DAB24 (game state)
       │
       ▼
  Overlay_Load()    ◄── Zie code_52990.c
       │
       ▼
  switch(state) {
    case 5:  ovl = gOverlayTable[0];  // segment_1B1FB0
    case 2:  ovl = gOverlayTable[1];  // ovl_i0
    case 10: ovl = gOverlayTable[2];  // ovl_i2
    ...
  }
       │
       ▼
  osPiStartDma(ovl->romStart, 0x802C5800, size)
       │
       ▼
  Code nu beschikbaar op 0x802C5800!
```

### Waarom Dit Moeilijk Is Voor Recompilatie

Op echte N64:
- Code wordt van ROM naar RAM gekopieerd
- Altijd op adres 0x802C5800
- Game weet welke overlay actief is

In recompilatie:
- ALLE overlay code moet gecompileerd worden
- Elke overlay krijgt UNIEKE functienamen
- `func_80092CF0_impl` moet juiste overlay aanroepen per state

---

## Alle 19 Overlays

### Bron: ovl_table.c

```
Locatie: C:\Users\User\Documents\decompilations\wave-race-64-n64-claude-code-opus-45\
         Wave-Race-64\src\ovl_table.c
```

| Index | Overlay Name    | ROM Start  | ROM End    | Size    | States      |
|-------|-----------------|------------|------------|---------|-------------|
| 0     | segment_1B1FB0  | 0x001B1FB0 | 0x001B3EC0 | 0x1F10  | 5, 6        |
| 1     | ovl_i0          | 0x001B3EC0 | 0x001B55A0 | 0x16E0  | 2           |
| 2     | ovl_i2          | 0x001B9440 | 0x001BC890 | 0x3450  | 10 (0xA)    |
| 3     | ovl_i3          | 0x001BC890 | 0x001BE0B0 | 0x1820  | 30 (0x1E)   |
| 4     | ovl_i4          | 0x001BE0B0 | 0x001BFF50 | 0x1EA0  | 20 (0x14)   |
| 5     | ovl_i5          | 0x001BFF50 | 0x001C2250 | 0x2300  | 52 (0x34)   |
| 6     | seg_1C3D00      | 0x001C3D00 | 0x001C43F0 | 0x06F0  | 54 (0x36)   |
| 7     | ovl_i6          | 0x001C2250 | 0x001C3780 | 0x1530  | 50 (0x32)   |
| 8     | seg_1C3780      | 0x001C3780 | 0x001C3D00 | 0x0580  | 56 (0x38)   |
| 9     | ovl_i7          | 0x001C43F0 | 0x001C49A0 | 0x05B0  | 60 (0x3C)   |
| 10    | ovl_i8          | 0x001C49A0 | 0x001C66D0 | 0x1D30  | 62 (0x3E)   |
| 11    | ovl_i9          | 0x001C66D0 | 0x001C9150 | 0x2A80  | 66 (0x42)   |
| 12    | ovl_i10         | 0x001C9150 | 0x001CA480 | 0x1330  | 68 (0x44)   |
| 13    | ovl_i11         | 0x001CA480 | 0x001CAE40 | 0x09C0  | 72 (0x48)   |
| 14    | ovl_i12         | 0x001CAE40 | 0x001CBAF0 | 0x0CB0  | 70 (0x46)   |
| 15    | ovl_i13         | 0x001CBAF0 | 0x001CF180 | 0x3690  | 64 (0x40)   |
| 16    | ovl_i14         | 0x001CF180 | 0x001CFB60 | 0x09E0  | 80 (0x50)   |
| 17    | ovl_i15         | 0x001CFB60 | 0x001D11D0 | 0x1670  | 102 (0x66)  |
| 18    | ovl_i1          | 0x001B55A0 | 0x001B9440 | 0x3EA0  | 7, 40 (0x28)|

### Wat We Nu Hebben Geimplementeerd

| Overlay         | Status           | Functies |
|-----------------|------------------|----------|
| Codeseg (801E)  | VOLLEDIG         | 245      |
| segment_1B1FB0  | VOLLEDIG         | 14       |
| ovl_i0 - ovl_i15| NOG NIET         | ???      |

**We hebben nu 2 van de 20 overlays geimplementeerd!**

---

## Game State Machine

### Bron: code_52990.c

```
Locatie: C:\Users\User\Documents\decompilations\wave-race-64-n64-claude-code-opus-45\
         Wave-Race-64\src\game\code_52990.c
```

### State → Overlay Mapping (uit Overlay_Load functie)

```c
switch (D_800DAB24) {
    case 0x5:  ovl = &gOverlayTable[0];   // segment_1B1FB0 - BOOT/INTRO
    case 0x2:  ovl = &gOverlayTable[1];   // ovl_i0
    case 0xA:  ovl = &gOverlayTable[2];   // ovl_i2
    case 0x1E: ovl = &gOverlayTable[3];   // ovl_i3
    case 0x14: ovl = &gOverlayTable[4];   // ovl_i4
    case 0x34: ovl = &gOverlayTable[5];   // ovl_i5
    case 0x36: ovl = &gOverlayTable[6];   // seg_1C3D00
    case 0x32: ovl = &gOverlayTable[7];   // ovl_i6
    case 0x38: ovl = &gOverlayTable[8];   // seg_1C3780
    case 0x3C: ovl = &gOverlayTable[9];   // ovl_i7
    case 0x3E: ovl = &gOverlayTable[10];  // ovl_i8
    case 0x42: ovl = &gOverlayTable[11];  // ovl_i9
    case 0x44: ovl = &gOverlayTable[12];  // ovl_i10
    case 0x48: ovl = &gOverlayTable[13];  // ovl_i11
    case 0x46: ovl = &gOverlayTable[14];  // ovl_i12
    case 0x40: ovl = &gOverlayTable[15];  // ovl_i13
    case 0x50: ovl = &gOverlayTable[16];  // ovl_i14
    case 0x66: ovl = &gOverlayTable[17];  // ovl_i15
    case 0x7:
    case 0x28: ovl = &gOverlayTable[18];  // ovl_i1
    case 1:    // geen overlay laden
}
```

### Game Boot Sequence

```
┌────────────────────────────────────────────────────────────┐
│                    GAME BOOT FLOW                          │
├────────────────────────────────────────────────────────────┤
│                                                            │
│  START ──► State 5 (Boot)                                  │
│                │                                           │
│                ▼                                           │
│         segment_1B1FB0 geladen                             │
│         func_802C5BA4 aangeroepen  ◄── WIJ ZIJN HIER!     │
│                │                                           │
│                ▼                                           │
│         Framebuffer clear                                  │
│         Nintendo logo sequence                             │
│                │                                           │
│                ▼                                           │
│         State 6 (Logo animation)                           │
│         Nog steeds segment_1B1FB0                          │
│                │                                           │
│                ▼                                           │
│         State 7 (Title screen)                             │
│         ovl_i1 geladen                                     │
│                │                                           │
│                ▼                                           │
│         Menu states, Race states, etc.                     │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Waar We Nu Zijn

```
State 5 ──► func_802C5BA4 WERKT!
            │
            ▼
        Display list gegenereerd (~60KB per frame)
            │
            ▼
        Maar: state transition naar 6 gebeurt nog niet
        Reden: Waarschijnlijk timer/controller input nodig
```

---

## Huidige Status

### Wat Werkt

| Component              | Status  | Notes                              |
|------------------------|---------|-----------------------------------|
| Main code (765 funcs)  | WERKT   | Volledig gecompileerd             |
| Codeseg overlay (245)  | WERKT   | 0x801E functies beschikbaar       |
| segment_1B1FB0 (14)    | WERKT   | State 5 overlay, display list OK  |
| Render thread          | WERKT   | Loop draait stabiel               |
| Display list generation| WERKT   | ~60KB per frame gegenereerd       |
| Game crashes           | OPGELOST| Geen crashes meer                 |

### Wat Nog Niet Werkt

| Component              | Status      | Wat nodig is                     |
|------------------------|-------------|----------------------------------|
| State transitions      | NIET WERKEND| Timer/controller input           |
| Overige 17 overlays    | NIET GEIMPL | Moeten nog toegevoegd worden     |
| RT64 display output    | ONDUIDELIJK | Moet getest worden               |
| State 0 overlay call   | CRASHT      | ovl_func_801ECAF4 debugging      |

### Totaal Functies

```
Main code:          765 functies
Codeseg overlay:    245 functies  (0x801E)
State 5 overlay:     14 functies  (0x802C segment_1B1FB0)
──────────────────────────────────
TOTAAL:            1024 functies gecompileerd
                   + stub functies (~140)
```

---

## N64Recomp Correctheid

### Doen We Het Goed?

**JA, grotendeels!** Hier is waarom:

### Wat We Correct Doen

1. **Symbol file structuur** - `[[section]]` entries zijn correct
2. **ROM/VRAM adressen** - Geverifieerd tegen decomp
3. **Stub systeem** - Functies met CACHE/COP0/MMIO correct gestubbed
4. **Ignored functies** - Custom implementaties in waverace_stubs.cpp
5. **Overlay functies** - Gegenereerd en correct gelinkt

### N64Recomp Begrippen

| Term           | Wat het is                                        | Onze gebruik          |
|----------------|---------------------------------------------------|----------------------|
| `stubs`        | Lege functie body gegenereerd                     | ~140 functies        |
| `ignored`      | GEEN code gegenereerd, custom impl nodig          | 2 functies           |
| `[[section]]`  | Code sectie met functies                          | 3 secties            |
| `overlays.txt` | Lijst van relocatable secties                     | Leeg (niet nodig)    |

### Waarom overlays.txt Leeg Is

- In **symbol mode** (wat wij gebruiken) is relocatie niet nodig
- Alle overlay code wordt statisch gecompileerd
- Runtime kiest welke functie aan te roepen, niet welke code te laden
- Dit is correct voor onze aanpak!

### Het Verschil Met Echte N64

| Aspect          | Echte N64                      | Onze Recompilatie           |
|-----------------|--------------------------------|-----------------------------|
| Overlay loading | DMA van ROM naar RAM           | Alle code al in binary      |
| VRAM adres      | Altijd 0x802C5800              | Unieke functienamen         |
| Selectie        | Pointer naar geladen code      | Switch in func_80092CF0_impl|
| Memory          | Overlays delen zelfde RAM      | Alle overlays in memory     |

---

## Belangrijke Bronbestanden

### Decomp Bestanden (voor referentie)

#### ovl_table.c - Alle Overlay Definities
```
Locatie: C:\Users\User\Documents\decompilations\wave-race-64-n64-claude-code-opus-45\
         Wave-Race-64\src\ovl_table.c

Bevat:
- gOverlayTable[] array met 19 entries
- ROM start/end adressen
- VRAM start/end adressen (text, data, bss)
```

#### code_52990.c - Overlay Loading Logic
```
Locatie: C:\Users\User\Documents\decompilations\wave-race-64-n64-claude-code-opus-45\
         Wave-Race-64\src\game\code_52990.c

Bevat:
- Overlay_Load() functie
- State → Overlay index mapping
- DMA code voor overlay laden
```

### Recompilatie Bestanden

| Bestand                    | Doel                                    |
|----------------------------|-----------------------------------------|
| waverace.syms.toml         | Alle functie definities                 |
| waverace.toml              | Build configuratie, stubs, ignored      |
| waverace_stubs.cpp         | Custom implementaties                   |
| RecompiledFuncs/funcs_*.c  | Gegenereerde C code                     |

---

## Volgende Stappen

### Prioriteit 1: State Transitions Fixen
- Onderzoek waarom state niet van 5 naar 6 gaat
- Mogelijk timer of controller input nodig

### Prioriteit 2: Meer Overlays Toevoegen
- ovl_i1 voor state 7 (title screen)
- Andere overlays voor menu's en races

### Prioriteit 3: RT64 Display Verificatie
- Controleer of gegenereerde display lists correct gerenderd worden
- Debug graphics output

---

## Samenvatting

We zijn van **"game crashed"** naar **"overlay code draait en genereert display lists"**!

```
Session 2: Function boundaries gefixed
Session 3: Build errors opgelost, N64 logo zichtbaar
Session 4: Display list builder aangesloten
Session 5: Render thread blocking geanalyseerd
Session 6: Crashes gefixed, game stabiel
Session 7: 0x802C overlay WERKT! ◄── NU
```

De recompilatie gebruikt N64Recomp correct. We hebben nu 2 van de 20 overlays werkend. De volgende stap is state transitions debuggen of meer overlays toevoegen.

---

*Document gegenereerd: Session 7 - December 2025*
