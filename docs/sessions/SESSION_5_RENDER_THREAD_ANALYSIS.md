# Session 5: Render Thread Analysis - Finding the Blocking Point

## Samenvatting

We hebben ontdekt waarom `func_80092CF0` (de display list builder) NIET wordt aangeroepen. De render thread start wel, maar blokkeert ergens VOOR de main render loop.

## Key Finding

**De render thread (thread 5) start correct maar bereikt de render loop NIET.**

Debug output toont:
```
[RENDER-THREAD] >>> func_80046DA0 ENTERED (render thread start)
```

Maar GEEN output van:
```
[RENDER-LOOP] >>> L_800471AC (main render loop)
```

Dit betekent dat de thread ergens tussen entry en loop blokkeert.

## Technische Details

### Thread Architectuur

```
Thread 1 (main_thread_entry): pri=100 → pri=0 (geeft prioriteit)
Thread 3 (game_thread_entry): pri=100
Thread 4 (audio_thread):      pri=20
Thread 5 (func_80046DA0):     pri=10  ← RENDER THREAD
```

### func_80046DA0 Flow

```
func_80046DA0 START
    │
    ├── osVirtualToPhysical calls (memory setup)
    ├── func_80097EC8 calls (memory allocation)
    ├── func_80047C38 (math init - sin/cos tables)
    ├── func_80048854 (viewport setup)
    ├── osGetTime / osSetTime
    ├── osViSwapBuffer
    ├── meer init functies...
    │
    └── L_800471AC: RENDER LOOP START  ← BEREIKT NIET!
            │
            ├── osContStartReadData
            ├── osRecvMesg(mq=0x80154100)
            ├── func_800922E4
            ├── func_80046850
            ├── func_800468E0
            ├── func_80092CF0  ← DISPLAY LIST BUILDER
            ├── func_80046BF4
            ├── osRecvMesg(mq=0x80154118)
            ├── ...
            └── goto L_800471AC
```

### Mogelijke Blocker

Ergens in de init code blokkeert de thread. Verdachte calls:
- `func_80097EC8` - Memory allocation, kan wachten
- `func_80047C38` - Sin/cos tables, zou niet moeten blokkeren
- `func_80048854` - Viewport setup, onbekend
- Andere osRecvMesg calls in init

## Debug Toegevoegd

We hebben debug printf's toegevoegd in `funcs_0.c`:

```cpp
// Bij entry
printf("[RENDER-THREAD] >>> func_80046DA0 ENTERED\n");

// Na func_80047C38 (after_20)
printf("[RENDER-THREAD] after func_80047C38 (after_20)\n");

// Bij loop start
printf("[RENDER-LOOP] >>> L_800471AC iteration #%d\n", render_loop_count);
```

## Message Queues Analyse

Uit de debug output:
- `mq 0x80154100`: Krijgt VEEL berichten (`osSendMesg` 0x29), maar geen `osRecvMesg`
- Dit is de render sync queue - als die niet gelezen wordt, draait de loop niet

## Volgende Stappen

1. **Meer debug toevoegen** aan kritieke punten in init code
2. **Binary search** - debug halverwege toevoegen om blocker te vinden
3. **Check gestubde functies** - sommige stubs retourneren niet correct

## Files Gewijzigd

- `RecompiledFuncs/funcs_0.c`: Debug printf's toegevoegd
  - Entry point tracking
  - Loop iteration tracking
  - Checkpoint na func_80047C38

## Hypothese

De blocker is waarschijnlijk een van:
1. Een gestubde functie die niet returnt
2. Een osRecvMesg in de init code die wacht op een message die nooit komt
3. Een infinite loop in een van de init functies

## Status

- [x] Render thread start bevestigd
- [x] Render loop bereikt NIET bevestigd
- [ ] Exacte blocker locatie vinden
- [ ] Blocker fixen
- [ ] Overlay graphics testen
