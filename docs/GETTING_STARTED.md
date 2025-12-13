# Getting Started with N64 Recompilation

Welcome! This guide is for people with **zero prior knowledge** who want to learn about decompilation and recompilation, especially using AI as a learning partner.

---

## What is Decompilation vs Recompilation?

### Decompilation

**Decompilation** is the process of converting compiled machine code back into human-readable source code:

```
Binary (machine code) → Decompiler → C/C++ source code
```

The goal is to understand *what* the original game does, function by function. This creates a **reference** that we can use to understand the game's logic.

### Recompilation

**Recompilation** takes a different approach - it translates the original binary into equivalent code that runs on modern systems:

```
N64 ROM (MIPS) → N64Recomp → C code → Compiler → Native PC executable
```

The key difference:
- **Emulation**: Interprets original code at runtime (slower but more compatible)
- **Recompilation**: Translates code once at build time (faster native execution)

---

## How Does N64Recomp Work?

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         N64RECOMP PIPELINE                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   1. INPUT                    2. ANALYSIS                3. OUTPUT          │
│   ┌─────────┐                 ┌──────────┐               ┌─────────┐        │
│   │  ROM    │    ─────────►   │ Function │   ─────────►  │ funcs_  │        │
│   │  File   │                 │ Discovery│               │   *.c   │        │
│   └─────────┘                 └──────────┘               └─────────┘        │
│       │                           │                           │             │
│   ┌─────────┐                 ┌──────────┐               ┌─────────┐        │
│   │  TOML   │    ─────────►   │ Symbol   │   ─────────►  │ Native  │        │
│   │ Config  │                 │ Matching │               │ Binary  │        │
│   └─────────┘                 └──────────┘               └─────────┘        │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

1. **ROM File**: The original N64 game binary (MIPS machine code)
2. **TOML Config**: Tells N64Recomp about functions, overlays, and special cases
3. **Function Discovery**: N64Recomp identifies functions in the binary
4. **Symbol Matching**: Connects functions to their names and addresses
5. **C Code Output**: Each function becomes equivalent C code
6. **Native Binary**: Compiled with RT64 for modern GPU rendering

---

## Key Concepts

### Memory Addresses

N64 games use addresses like `0x80046800`:
- `0x80XXXXXX` = Cached RDRAM (main memory)
- `0xA0XXXXXX` = Uncached RDRAM
- `0xB0XXXXXX` = Cartridge ROM

### Overlays

Games load different code modules to the same memory address:
```
State 5: overlay_boot loaded at 0x802C5800
State 6: overlay_logo loaded at 0x802C5800
State 2: overlay_menu loaded at 0x802C5800
```

Each overlay has different code, but shares the same address space!

### Display Lists

N64 graphics are command-based:
```c
gSPVertex(gfx++, vertices, 32, 0);  // Load vertices
gSP2Triangles(gfx++, 0,1,2, 0, 3,4,5, 0);  // Draw triangles
gDPSetPrimColor(gfx++, 0, 0, 255, 0, 0, 255);  // Set color
```

RT64 interprets these commands and renders with modern GPU features.

---

## Using AI for Recompilation

### Why AI Helps

1. **Pattern Recognition**: AI can spot common N64/libultra patterns
2. **Code Analysis**: AI can trace through complex assembly/C code
3. **Documentation**: AI can explain what functions do
4. **Debugging**: AI can hypothesize about crashes and bugs

### Effective AI Workflow

1. **Provide Context**
   ```
   "I'm working on Wave Race 64 recompilation. The game crashes
   when calling func_8008FB74. Here's the assembly..."
   ```

2. **Share Error Messages**
   ```
   "The game outputs this debug log before crashing:
   [DL] Segment 8 = 0x80316800
   [CRASH] Invalid memory access at 0x10011F944"
   ```

3. **Ask Specific Questions**
   ```
   "Why would an N64 pointer 0x8011F940 cause an access at 0x10011F944?"
   ```

4. **Iterate on Fixes**
   ```
   "The fix didn't work. Now I see this output..."
   ```

### Key Lesson from This Project

**Document everything!** We maintain session logs that capture:
- What was tried
- What worked/didn't work
- Root cause analysis
- Key insights

This creates institutional knowledge that survives context limits.

---

## Getting Started Checklist

### 1. Understand the Original Game
- [ ] Play the game in an emulator
- [ ] Observe game states (boot, logo, menu, gameplay)
- [ ] Note any special effects or behaviors

### 2. Study Existing Work
- [ ] Check if a decompilation exists
- [ ] Read through related recomp projects (Zelda64Recomp, etc.)
- [ ] Understand the N64 architecture basics

### 3. Set Up Your Environment
- [ ] Install WSL2 (Windows) or use Linux
- [ ] Install CMake, Ninja, Clang
- [ ] Clone N64Recomp and build it

### 4. Start Small
- [ ] Get the game to boot (show anything on screen)
- [ ] Fix crashes one at a time
- [ ] Document each fix in a session log

### 5. Use AI Effectively
- [ ] Provide context and error messages
- [ ] Ask for analysis, not just solutions
- [ ] Iterate based on results

---

## Common Challenges

### "Function Not Found"
The function isn't defined in your symbol file. Add it:
```toml
{ name = "func_800XXXXX", vram = 0x800XXXXX }
```

### "Jump Table Error"
The VRAM address for an overlay is wrong. Check the decompilation for the correct address.

### "Game Hangs"
Usually a message queue deadlock. Check:
- Is osRecvMesg blocking forever?
- Are external messages being delivered?

### "Graphics Corruption"
Often a segment addressing issue:
- Check segment setup commands
- Verify segment base addresses

---

## Resources

### Tools
- [N64Recomp](https://github.com/N64Recomp/N64Recomp) - Static recompiler
- [RT64](https://github.com/rt64/rt64) - N64 graphics renderer
- [Ghidra](https://ghidra-sre.org/) - Reverse engineering tool

### Reference Projects
- [Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp) - Zelda OoT/MM
- [PerfectDarkRecomp](https://github.com/N64Recomp/PerfectDarkRecomp) - Perfect Dark

### Documentation
- [N64 Programming Manual](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro-man.htm)
- [N64brew Wiki](https://n64brew.dev/)

---

## Next Steps

1. Read our [Build Guide](BUILD_GUIDE.md) to set up the project
2. Check [Lessons Learned](LESSONS_LEARNED.md) for key insights
3. Browse [Session Logs](sessions/) to see our debugging journey

Happy recompiling!
