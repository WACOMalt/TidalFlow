# Wave Race 64 - Game Flow Diagram

Dit document toont visueel waar we zijn in de game execution flow.

---

## BLOK 1: Systeem Startup (WERKT)

```
╔══════════════════════════════════════════════════════════════════════════════╗
║  SYSTEEM STARTUP                                                    [WERKT]  ║
╠══════════════════════════════════════════════════════════════════════════════╣
║                                                                              ║
║   ┌─────────────┐                                                            ║
║   │  ROM Load   │  → ROM geladen in geheugen                                 ║
║   └──────┬──────┘                                                            ║
║          │                                                                   ║
║          ▼                                                                   ║
║   ┌─────────────┐                                                            ║
║   │ Entrypoint  │  → 0x80046800 (get_entrypoint_address)                     ║
║   │ func_80046800│                                                           ║
║   └──────┬──────┘                                                            ║
║          │                                                                   ║
║          ▼                                                                   ║
║   ┌─────────────┐                                                            ║
║   │osInitialize │  → Libultra init, VI setup                                 ║
║   └──────┬──────┘                                                            ║
║          │                                                                   ║
║          ▼                                                                   ║
║   ┌─────────────┐      ┌─────────────┐      ┌─────────────┐                 ║
║   │  Thread 1   │      │  Thread 3   │      │  Thread 4   │                 ║
║   │   (Main)    │ ───► │  (Sched)    │ ───► │   (Game)    │                 ║
║   │   pri=0     │      │   pri=100   │      │   pri=20    │                 ║
║   └─────────────┘      └─────────────┘      └──────┬──────┘                 ║
║                                                    │                         ║
║                                                    ▼                         ║
║                                             ┌─────────────┐                  ║
║                                             │  Thread 5   │                  ║
║                                             │  (Render)   │  ◄── WIJ HIER   ║
║                                             │   pri=10    │                  ║
║                                             └─────────────┘                  ║
║                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════╝
```

---

## BLOK 2: Render Thread Initialization (WERKT)

```
╔══════════════════════════════════════════════════════════════════════════════╗
║  RENDER THREAD (Thread 5)                                           [WERKT]  ║
╠══════════════════════════════════════════════════════════════════════════════╣
║                                                                              ║
║   ┌─────────────┐                                                            ║
║   │func_80046DA0│  → Render thread entry point                               ║
║   │  (entry)    │                                                            ║
║   └──────┬──────┘                                                            ║
║          │                                                                   ║
║          ▼                                                                   ║
║   ┌─────────────┐                                                            ║
║   │ Init Memory │  → Allocate buffers, setup queues                          ║
║   │ Init Tables │  → Sin/Cos tables, viewport                                ║
║   └──────┬──────┘                                                            ║
║          │                                                                   ║
║          ▼                                                                   ║
║   ╔═════════════════════════════════════════════════════════════╗            ║
║   ║           RENDER LOOP (infinite)                            ║            ║
║   ╠═════════════════════════════════════════════════════════════╣            ║
║   ║                                                             ║            ║
║   ║   ┌───────────────┐                                         ║            ║
║   ║   │ Wait for VI   │  ← osRecvMesg (vertical interrupt)      ║            ║
║   ║   │   message     │                                         ║            ║
║   ║   └───────┬───────┘                                         ║            ║
║   ║           │                                                 ║            ║
║   ║           ▼                                                 ║            ║
║   ║   ┌───────────────┐                                         ║            ║
║   ║   │func_80092CF0  │  ← DISPLAY LIST BUILDER (zie Blok 3)    ║            ║
║   ║   │  (DL Builder) │    ◄── WIJ ZIJN HIER!                   ║            ║
║   ║   └───────┬───────┘                                         ║            ║
║   ║           │                                                 ║            ║
║   ║           ▼                                                 ║            ║
║   ║   ┌───────────────┐                                         ║            ║
║   ║   │func_80046BF4  │  ← DL Finalizer (adds sync commands)    ║            ║
║   ║   │ (DL Finalize) │    [STUBBED - null check]               ║            ║
║   ║   └───────┬───────┘                                         ║            ║
║   ║           │                                                 ║            ║
║   ║           ▼                                                 ║            ║
║   ║   ┌───────────────┐                                         ║            ║
║   ║   │ Submit to RDP │  ← osSendMesg to scheduler              ║            ║
║   ║   │ osViSwapBuf   │    RT64 receives display list           ║            ║
║   ║   └───────┬───────┘                                         ║            ║
║   ║           │                                                 ║            ║
║   ║           └──────────────────────────────────┐              ║            ║
║   ║                                              │              ║            ║
║   ║   ◄──────────────────────────────────────────┘              ║            ║
║   ║      (loop terug naar Wait for VI)                          ║            ║
║   ╚═════════════════════════════════════════════════════════════╝            ║
║                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════╝
```

---

## BLOK 3: Display List Builder - State Machine (WIJ ZIJN HIER!)

```
╔══════════════════════════════════════════════════════════════════════════════╗
║  func_80092CF0 - DISPLAY LIST STATE MACHINE                                  ║
╠══════════════════════════════════════════════════════════════════════════════╣
║                                                                              ║
║   Input: ctx->r4 = Display List pointer                                      ║
║   Output: ctx->r2 = Updated DL pointer (after commands written)              ║
║                                                                              ║
║   ┌───────────────────────────────────────────────────────────────────────┐  ║
║   │  Read D_800DAB24 (Game State Variable)                                │  ║
║   └───────────────────────────────┬───────────────────────────────────────┘  ║
║                                   │                                          ║
║                                   ▼                                          ║
║   ┌───────────────────────────────────────────────────────────────────────┐  ║
║   │                    SWITCH ON GAME STATE                               │  ║
║   └───────────────────────────────────────────────────────────────────────┘  ║
║                                   │                                          ║
║       ┌───────────────┬───────────┼───────────┬───────────────┐              ║
║       │               │           │           │               │              ║
║       ▼               ▼           ▼           ▼               ▼              ║
║   ┌───────┐      ┌───────┐   ┌───────┐   ┌───────┐      ┌───────┐           ║
║   │State 0│      │State 2│   │State 5│   │State 7│      │ Other │           ║
║   │ Menu  │      │       │   │ BOOT  │   │ Title │      │States │           ║
║   └───┬───┘      └───┬───┘   └───┬───┘   └───┬───┘      └───┬───┘           ║
║       │              │           │           │               │               ║
║       ▼              ▼           ▼           ▼               ▼               ║
║   ┌────────┐    ┌────────┐  ┌────────┐  ┌────────┐     ┌────────┐           ║
║   │ 0x801E │    │ 0x802C │  │ 0x802C │  │ 0x802C │     │ 0x802C │           ║
║   │codeseg │    │ ovl_i0 │  │segment │  │ ovl_i1 │     │  ...   │           ║
║   │        │    │        │  │_1B1FB0 │  │        │     │        │           ║
║   ├────────┤    ├────────┤  ├────────┤  ├────────┤     ├────────┤           ║
║   │IMPL    │    │NOT IMPL│  │IMPL    │  │NOT IMPL│     │NOT IMPL│           ║
║   │[CRASH] │    │        │  │[WORKS!]│  │        │     │        │           ║
║   └────────┘    └────────┘  └────────┘  └────────┘     └────────┘           ║
║                                  │                                           ║
║                                  │                                           ║
║                       ╔══════════▼══════════╗                                ║
║                       ║  ◄── WIJ ZIJN HIER! ║                                ║
║                       ║  State 5 = Boot     ║                                ║
║                       ║  Overlay WERKT!     ║                                ║
║                       ╚═════════════════════╝                                ║
║                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════╝
```

---

## BLOK 4: Game State Flow (Expected Progression)

```
╔══════════════════════════════════════════════════════════════════════════════╗
║  GAME STATE PROGRESSION                                                      ║
╠══════════════════════════════════════════════════════════════════════════════╣
║                                                                              ║
║                           ┌─────────────────────┐                            ║
║                           │                     │                            ║
║   ┌────────────┐          │  ┌────────────┐     │                            ║
║   │  State 5   │──────────┼─►│  State 6   │─────┘                            ║
║   │   BOOT     │          │  │   LOGO     │                                  ║
║   │            │          │  │            │                                  ║
║   │ segment_   │          │  │ segment_   │                                  ║
║   │ 1B1FB0     │          │  │ 1B1FB0     │                                  ║
║   ├────────────┤          │  ├────────────┤                                  ║
║   │ ◄── NU!   │          │  │ Niet       │                                  ║
║   │ [WERKT]    │          │  │ bereikt    │                                  ║
║   └────────────┘          │  └────────────┘                                  ║
║         │                 │        │                                         ║
║         │ Timer/          │        │ Timer/                                  ║
║         │ Controller?     │        │ Controller?                             ║
║         │                 │        │                                         ║
║         ▼ (BLOCKED!)      │        ▼                                         ║
║   ┌────────────┐          │  ┌────────────┐                                  ║
║   │  State 7   │          │  │  State 7   │                                  ║
║   │  TITLE     │◄─────────┘  │  TITLE     │                                  ║
║   │            │             │            │                                  ║
║   │  ovl_i1    │             │  ovl_i1    │                                  ║
║   ├────────────┤             ├────────────┤                                  ║
║   │ NOT IMPL   │             │ NOT IMPL   │                                  ║
║   └────────────┘             └────────────┘                                  ║
║         │                                                                    ║
║         │ Press Start                                                        ║
║         ▼                                                                    ║
║   ┌────────────┐                                                             ║
║   │  State 0   │                                                             ║
║   │   MENU     │                                                             ║
║   │            │                                                             ║
║   │  0x801E    │                                                             ║
║   │  codeseg   │                                                             ║
║   ├────────────┤                                                             ║
║   │ [CRASHES]  │                                                             ║
║   └────────────┘                                                             ║
║         │                                                                    ║
║         │ Select Race                                                        ║
║         ▼                                                                    ║
║   ┌────────────┐                                                             ║
║   │ State 0x14 │                                                             ║
║   │   RACE     │                                                             ║
║   │            │                                                             ║
║   │  ovl_i4    │                                                             ║
║   ├────────────┤                                                             ║
║   │ NOT IMPL   │                                                             ║
║   └────────────┘                                                             ║
║                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════╝
```

---

## BLOK 5: Overlay Implementatie Status

```
╔══════════════════════════════════════════════════════════════════════════════╗
║  OVERLAY IMPLEMENTATIE STATUS                                                ║
╠══════════════════════════════════════════════════════════════════════════════╣
║                                                                              ║
║   ┌─────────────────────────────────────────────────────────────────────┐    ║
║   │                    0x801E CODESEG (245 functies)                    │    ║
║   │                                                                     │    ║
║   │   ROM:  0x000A95D0 - 0x00106E90                                     │    ║
║   │   VRAM: 0x801DAFA0 - 0x80226A5F                                     │    ║
║   │                                                                     │    ║
║   │   Status: [████████████████████████████████████████████] VOLLEDIG   │    ║
║   │   Maar:   ovl_func_801ECAF4 CRASHT wanneer aangeroepen              │    ║
║   └─────────────────────────────────────────────────────────────────────┘    ║
║                                                                              ║
║   ┌─────────────────────────────────────────────────────────────────────┐    ║
║   │                 0x802C OVERLAYS (19 verschillende)                   │    ║
║   └─────────────────────────────────────────────────────────────────────┘    ║
║                                                                              ║
║   ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐       ║
║   │segment_1B1FB0│ │   ovl_i0     │ │   ovl_i1     │ │   ovl_i2     │       ║
║   │ State 5,6    │ │  State 2     │ │ State 7,0x28 │ │  State 0xA   │       ║
║   │              │ │              │ │              │ │              │       ║
║   │ [██████████] │ │ [          ] │ │ [          ] │ │ [          ] │       ║
║   │  WERKT!      │ │  NOT IMPL    │ │  NOT IMPL    │ │  NOT IMPL    │       ║
║   └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘       ║
║                                                                              ║
║   ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐       ║
║   │   ovl_i3     │ │   ovl_i4     │ │   ovl_i5     │ │   ovl_i6     │       ║
║   │ State 0x1E   │ │ State 0x14   │ │ State 0x34   │ │ State 0x32   │       ║
║   │              │ │   (RACE!)    │ │              │ │              │       ║
║   │ [          ] │ │ [          ] │ │ [          ] │ │ [          ] │       ║
║   │  NOT IMPL    │ │  NOT IMPL    │ │  NOT IMPL    │ │  NOT IMPL    │       ║
║   └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘       ║
║                                                                              ║
║   ... en nog 11 andere overlays (ovl_i7 t/m ovl_i15, seg_1C3D00, seg_1C3780)║
║                                                                              ║
║   ═══════════════════════════════════════════════════════════════════════   ║
║   TOTAAL: 2 van 20 overlays geïmplementeerd (codeseg + segment_1B1FB0)       ║
║   ═══════════════════════════════════════════════════════════════════════   ║
║                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════╝
```

---

## BLOK 6: Wat Elk Frame Doet (Current State)

```
╔══════════════════════════════════════════════════════════════════════════════╗
║  ELK FRAME (60x per seconde)                                                 ║
╠══════════════════════════════════════════════════════════════════════════════╣
║                                                                              ║
║   Frame N:                                                                   ║
║   ┌──────────────────────────────────────────────────────────────────────┐   ║
║   │                                                                      │   ║
║   │  1. VI Interrupt → Thread 5 wakes up                                 │   ║
║   │     ↓                                                                │   ║
║   │  2. Read D_800DAB24 → game_state = 5                                 │   ║
║   │     ↓                                                                │   ║
║   │  3. Call ovl_func_802C5BA4(dl_ptr)                                   │   ║
║   │     │                                                                │   ║
║   │     │  ┌────────────────────────────────────────────────────────┐   │   ║
║   │     │  │ INSIDE ovl_func_802C5BA4:                              │   │   ║
║   │     │  │                                                        │   │   ║
║   │     │  │  - Check D_801CE63C (boot flag)                        │   │   ║
║   │     │  │  - If first call: clear framebuffer                    │   │   ║
║   │     │  │  - Build display list (~60KB of RDP commands)          │   │   ║
║   │     │  │  - Draw Nintendo logo (fade in/out)                    │   │   ║
║   │     │  │  - Check timer for state transition                    │   │   ║
║   │     │  │  - Return updated dl_ptr                               │   │   ║
║   │     │  │                                                        │   │   ║
║   │     │  └────────────────────────────────────────────────────────┘   │   ║
║   │     ↓                                                                │   ║
║   │  4. DL ptr moved from 0x8011F940 to 0x801208B8 (+0xEF78 bytes)       │   ║
║   │     ↓                                                                │   ║
║   │  5. func_80046BF4 adds sync commands                                 │   ║
║   │     ↓                                                                │   ║
║   │  6. Submit display list to RDP (RT64 renderer)                       │   ║
║   │     ↓                                                                │   ║
║   │  7. osViSwapBuffer → show rendered frame                             │   ║
║   │                                                                      │   ║
║   └──────────────────────────────────────────────────────────────────────┘   ║
║                                                                              ║
║   Probleem: Timer voor state transition werkt niet                           ║
║   → Game blijft in State 5 hangen                                            ║
║   → Boot flag (D_801CE63C) blijft 0                                          ║
║                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════╝
```

---

## BLOK 7: Waar We Zijn vs Waar We Willen Zijn

```
╔══════════════════════════════════════════════════════════════════════════════╗
║  PROGRESS OVERVIEW                                                           ║
╠══════════════════════════════════════════════════════════════════════════════╣
║                                                                              ║
║   BOOT SEQUENCE:                                                             ║
║                                                                              ║
║   [█████████████████████████████████████████████████████████]  100%          ║
║    ROM load, Thread creation, Memory init, Render loop                       ║
║                                                                              ║
║                                                                              ║
║   STATE 5 (Boot/Intro):                                                      ║
║                                                                              ║
║   [█████████████████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░]   50%          ║
║    Overlay code runs, DL generated, maar state transition blocked            ║
║                                                                              ║
║   ◄─── WIJ ZIJN HIER                                                         ║
║                                                                              ║
║   STATE 6 (Logo Animation):                                                  ║
║                                                                              ║
║   [░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░]    0%          ║
║    Niet bereikt - state transition niet werkend                              ║
║                                                                              ║
║                                                                              ║
║   STATE 7 (Title Screen):                                                    ║
║                                                                              ║
║   [░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░]    0%          ║
║    Overlay niet geïmplementeerd (ovl_i1)                                     ║
║                                                                              ║
║                                                                              ║
║   GAMEPLAY:                                                                  ║
║                                                                              ║
║   [░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░]    0%          ║
║    Menu en race overlays niet geïmplementeerd                                ║
║                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════╝
```

---

## Samenvatting: Waar Zijn We Nu?

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│  ✓ ROM loads correctly                                                      │
│  ✓ All 5 threads created and running                                        │
│  ✓ Render loop executing at 60fps                                           │
│  ✓ Game state = 5 (boot)                                                    │
│  ✓ ovl_func_802C5BA4 called every frame                                     │
│  ✓ Display list generated (~60KB per frame)                                 │
│  ✓ RT64 receiving display lists                                             │
│                                                                             │
│  ✗ State doesn't transition from 5 to 6                                     │
│  ✗ Boot flag D_801CE63C stays at 0                                          │
│  ✗ Screen shows black (or N64 logo from separate code path)                 │
│  ✗ Controller input doesn't advance state                                   │
│  ✗ Timer-based transitions not working                                      │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  NEXT STEPS:                                                                │
│                                                                             │
│  1. Debug why state transition from 5→6 doesn't happen                      │
│     - Check timer functions                                                 │
│     - Check func_801EB180 (state setter in codeseg)                         │
│                                                                             │
│  2. Fix boot flag (D_801CE63C) - should change during boot                  │
│                                                                             │
│  3. OR implement ovl_i1 for state 7 (title screen)                          │
│     and manually set state to 7 to bypass boot sequence                     │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

*Gegenereerd: Session 7 - December 2025*
