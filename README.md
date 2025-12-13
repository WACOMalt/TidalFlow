# Wave Race 64 Recompilation

[![N64Recomp](https://img.shields.io/badge/Tool-N64Recomp-blue)](https://github.com/N64Recomp/N64Recomp)
[![Game](https://img.shields.io/badge/Game-Wave%20Race%2064-green)](https://en.wikipedia.org/wiki/Wave_Race_64)
[![AI Assisted](https://img.shields.io/badge/AI%20Assisted-Claude%20Code-orange)](https://claude.ai)

A static recompilation of **Wave Race 64** (N64, 1996) to native PC, using [N64Recomp](https://github.com/N64Recomp/N64Recomp) and [RT64](https://github.com/rt64/rt64). This project demonstrates **AI-assisted reverse engineering and recompilation** - showing how modern AI tools can help tackle complex low-level programming tasks.

---

## New to Decomp/Recomp? Start Here!

If you're new to decompilation or recompilation, check out our **[Getting Started Guide](docs/GETTING_STARTED.md)** - designed for people with zero prior knowledge who want to learn decomp/recomp using AI as a learning partner.

---

## What is This?

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        N64 STATIC RECOMPILATION                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   Original ROM          N64Recomp Tool          Native PC Binary            │
│   ┌─────────┐           ┌──────────┐           ┌─────────────┐              │
│   │  MIPS   │  ──────►  │ Static   │  ──────►  │    x86_64   │              │
│   │ Binary  │           │ Recomp   │           │   + RT64    │              │
│   └─────────┘           └──────────┘           └─────────────┘              │
│                                                                             │
│   The ROM is translated at build time, not emulated at runtime!             │
│   RT64 provides modern GPU rendering of N64 graphics commands.              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Static Recompilation** converts the original MIPS machine code into equivalent C code, which is then compiled to run natively on modern systems. This is different from emulation - the game code actually runs as native x86_64 instructions.

---

## Project Status

| Component | Status | Details |
|-----------|--------|---------|
| Main Code (1024 functions) | ✅ Complete | 0x80046800 - 0x800D25B0 |
| Overlay 0x801E (245 functions) | ✅ Working | Boot/logo overlay |
| Overlay ovl_i0 (Menu) | ✅ Working | State 2 menu intro |
| Game Threads | ✅ Working | Threads 1,3,4,5 all running |
| Message Queues | ✅ Fixed | External queue deadlock resolved |
| Display List Processing | ✅ Working | RT64 processes ~6500 cmds/frame |
| Segment 8 Addressing | ✅ Fixed | Runtime override in RT64 |
| G_TRI1 Dual Format | ✅ Fixed | Both F3D and F3DEX2 supported |
| Nintendo Logo | ✅ Renders | State 6 graphics working |
| State Machine | ✅ States 5→6→2 | Boot → Logo → Menu |
| Current Progress | 🟡 State 2 | Running 549+ frames stable |
| Controller Input | ⚪ Simulated | Real input not yet implemented |

### State Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                    WAVE RACE 64 STATE FLOW                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  STATE 5  ──(1 frame)──►  STATE 6  ──(14 frames)──►  STATE 2       │
│  (Boot 1)                 (Logo)                     (Menu/ovl_i0)  │
│  WORKING                  WORKING                    WORKING!       │
│                                                                     │
│  STATE 2  ──(button/timer)──►  STATE 3  ──►  STATE 7               │
│  ovl_i0                        ovl_i0         ovl_i1                │
│  WORKING                       NOT REACHED    NOT REACHED           │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Documentation

| Document | Description |
|----------|-------------|
| [Getting Started](docs/GETTING_STARTED.md) | For newcomers to decomp/recomp |
| [Build Guide](docs/BUILD_GUIDE.md) | How to build the project |
| [N64Recomp Guide](docs/N64RECOMP_GUIDE.md) | Configuration reference |
| [Lessons Learned](docs/LESSONS_LEARNED.md) | Key insights from debugging |
| [Session Logs](docs/sessions/) | Detailed session documentation |

---

## Project Structure

```
wave-race-64-recomp/
├── README.md                    # This file
├── docs/                        # Documentation
│   ├── GETTING_STARTED.md       # Newcomer guide
│   ├── BUILD_GUIDE.md           # Build instructions
│   ├── N64RECOMP_GUIDE.md       # N64Recomp configuration
│   ├── LESSONS_LEARNED.md       # Key debugging insights
│   └── sessions/                # Session logs (SESSION_*.md)
│
├── waverace-recomp/             # Main recompilation project
│   ├── waverace.toml            # N64Recomp configuration
│   ├── waverace.syms.toml       # Function/symbol definitions
│   ├── src/game/
│   │   └── waverace_stubs.cpp   # Custom stub implementations
│   ├── RecompiledFuncs/         # Generated C code (from N64Recomp)
│   └── lib/
│       ├── N64ModernRuntime/    # OS emulation layer
│       └── rt64/                # GPU rendering (RT64)
│
└── N64Recomp/                   # N64Recomp tool (submodule)
```

---

## Related Projects

| Project | Description |
|---------|-------------|
| [Wave-Race-64 Decomp](https://github.com/chrisking1981/Wave-Race-64) | Decompilation of Wave Race 64 (used as reference) |
| [sly1-recomp](https://github.com/chrisking1981/sly1-recomp) | PS2 Sly Cooper recompilation (similar AI-assisted approach) |
| [N64Recomp](https://github.com/N64Recomp/N64Recomp) | The static recompilation tool |
| [RT64](https://github.com/rt64/rt64) | Modern N64 graphics renderer |

---

## Prerequisites

- **Windows 10/11** with WSL2 (Ubuntu)
- **CMake** 3.20+
- **Ninja** build system
- **Clang** compiler (in WSL)
- **Wave Race 64 ROM** (NTSC-U, `waverace.us.z64`)
  - SHA1: `C4E3D6B9E15C5CD1A7CE32C5B45E2EDA78F65D16`

---

## Quick Start

1. **Clone the repository:**
   ```bash
   git clone --recursive https://github.com/chrisking1981/Wave-Race-64-recomp.git
   cd Wave-Race-64-recomp
   ```

2. **Place your ROM:**
   ```bash
   cp /path/to/waverace.us.z64 waverace-recomp/
   ```

3. **Build N64Recomp tool:**
   ```bash
   cd N64Recomp
   cmake -B build -G Ninja
   cmake --build build
   cd ..
   ```

4. **Run N64Recomp on the ROM:**
   ```bash
   cd waverace-recomp
   ../N64Recomp/build/N64Recomp waverace.toml
   ```

5. **Build the game:**
   ```bash
   cmake -B build -G Ninja
   cmake --build build -j$(nproc)
   ```

6. **Run:**
   ```bash
   ./build/WaveRace64Recompiled
   ```

See [Build Guide](docs/BUILD_GUIDE.md) for detailed instructions.

---

## Bugs We Fixed in N64ModernRuntime/RT64

During development, we discovered and fixed several issues:

1. **External Message Queue Deadlock** (Session 17)
   - Game threads waiting for messages from non-game threads would deadlock
   - Fix: Drain external message queue in blocking wait loops

2. **Segment 8 Address Override** (Session 12)
   - Wave Race sets segment 8 to wrong address
   - Fix: Runtime override in RT64's G_MOVEWORD handler

3. **G_TRI1 Dual Format Support** (Session 14)
   - Wave Race uses both F3D and F3DEX2 triangle formats
   - Fix: Detect format based on w1 high byte

4. **Display List Null Termination** (Session 18)
   - Some DLs end without G_ENDDL command
   - Fix: Detect consecutive null commands as end-of-DL

5. **Sign Extension for N64 Pointers** (Session 26)
   - Custom stubs returning N64 pointers must sign-extend to 64-bit
   - Fix: `ctx->r2 = (gpr)(int32_t)pointer;`

---

## AI-Assisted Development

This project demonstrates how AI can assist with complex reverse engineering tasks. The entire recompilation effort has been done in collaboration with **Claude Code** (Claude Opus 4.5), showing that:

- AI can help navigate complex codebases
- AI can assist with debugging low-level issues
- AI can learn from documentation and apply patterns
- Human + AI collaboration accelerates progress

### How We Work

1. **AI reads the documentation** - Session logs, lessons learned, decomp reference
2. **AI proposes fixes** - Based on analysis of code and symptoms
3. **Human tests and validates** - Runs the game, provides feedback
4. **AI documents findings** - Creates session logs for future reference

This workflow is documented in detail to help others learn how to effectively use AI for similar projects.

---

## Contributing

We welcome contributions! Here's what's needed:

- [ ] Real controller input implementation (SDL mapping)
- [ ] State 2→3 transition investigation
- [ ] ovl_i1/i2/i3 overlay support
- [ ] Audio implementation
- [ ] Remove remaining stubs where possible

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

---

## License

This project does not include any copyrighted game assets. You must provide your own legally obtained ROM file. The recompilation code is provided for educational and research purposes.

The tools and libraries used have their own licenses:
- N64Recomp: MIT License
- RT64: MIT License
- N64ModernRuntime: MIT License

---

## Credits

- **Chris King** ([@chrisking1981](https://github.com/chrisking1981)) - Project lead
- **Claude Code** (Claude Opus 4.5) - AI-assisted development
- **N64Recomp Team** - The amazing recompilation tool
- **RT64 Team** - Modern N64 graphics rendering
- **Wave Race 64 Decomp Community** - Reference decompilation

---

## Session History

This project has progressed through 26 sessions of AI-assisted development:

| Session | Focus | Outcome |
|---------|-------|---------|
| 1-11 | Initial setup, build fixes | Project foundation |
| 12 | Segment 8 runtime fix | Nintendo logo visible |
| 13 | Crash fix | State 6 stable |
| 14 | G_TRI1 dual format | Triangle rendering fixed |
| 15 | ovl_i0 stubs | State 2 support added |
| 16 | Scheduler blocking | Issue documented |
| 17 | External message queue | Deadlock fixed |
| 18 | RT64 null memory | DL termination fixed |
| 19 | Status review | Documentation cleanup |
| 20-21 | State flow analysis | Boot sequence understood |
| 22 | State 5→6 transition | Boot to logo working |
| 23 | Fade counter analysis | Logo timing verified |
| 24 | State 6→2 transition | Logo to menu working |
| 25 | DMA crash fix | Overlay loading stable |
| 26 | Sign extension fix | **549+ frames stable!** |

---

*This project shows that with AI assistance, complex reverse engineering and recompilation projects are more accessible than ever. Whether you're a seasoned developer or just starting out, we hope this inspires you to explore the fascinating world of game preservation and modification.*
