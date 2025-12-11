# Wave Race 64 Recompilation - Development Workflow

This document describes the workflow for working on the Wave Race 64 recompilation project.

## Project Structure

```
wave-race-64-recomp-claude-code-opus45/
├── waverace-recomp/           # Main project directory
│   ├── build/                 # Build output (Linux executables)
│   ├── lib/N64ModernRuntime/  # Runtime library (ultramodern, librecomp)
│   ├── RecompiledFuncs/       # Auto-generated recompiled MIPS functions
│   ├── src/game/              # Game-specific stubs and patches
│   ├── waverace.toml          # N64Recomp configuration
│   └── waverace.syms.toml     # Symbol definitions
├── N64Recomp/                 # N64Recomp tool
└── waverace64/                # LLONSIT decomp reference (optional)
```

## Building

**IMPORTANT**: This project builds with Linux/WSL, not native Windows.

### Build Commands (from waverace-recomp directory)

```bash
# Full rebuild
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && cmake --build build -j 24"

# Rebuild specific target
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && cmake --build build --target WaveRace64Recompiled -j 24"

# Clean rebuild
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && rm -rf build && cmake -B build -G Ninja && cmake --build build -j 24"
```

### Running the Game

```bash
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && ./build/WaveRace64Recompiled"
```

With timeout for debugging:
```bash
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && timeout 30 ./build/WaveRace64Recompiled 2>&1 | tail -200"
```

## Key Files to Edit

### Runtime Library (lib/N64ModernRuntime/)

| File | Purpose |
|------|---------|
| `ultramodern/src/events.cpp` | Event handling (SP, DP, VI, SI events) |
| `ultramodern/src/mesgqueue.cpp` | Message queue implementation |
| `ultramodern/src/threads.cpp` | Thread scheduling |
| `librecomp/src/ultra_translation.cpp` | OS function translations (PI, threads, etc.) |
| `librecomp/src/pi.cpp` | Peripheral Interface (PI) DMA handling |
| `librecomp/src/sp.cpp` | RSP task handling |

### Game-Specific (waverace-recomp/)

| File | Purpose |
|------|---------|
| `src/game/waverace_stubs.cpp` | Game stubs and entrypoint |
| `waverace.syms.toml` | Symbol address mappings |
| `waverace.toml` | N64Recomp configuration |
| `RecompiledFuncs/*.c` | Auto-generated (add debug prints here) |

## Debugging Workflow

### 1. Add Debug Prints to Recompiled Functions

Edit files in `RecompiledFuncs/` to add fprintf statements:

```c
RECOMP_FUNC void func_80097EC8(uint8_t* rdram, recomp_context* ctx) {
    fprintf(stderr, "[DEBUG] >>> func_80097EC8 called\n");
    // ... existing code ...
}
```

### 2. Add Debug Prints to Runtime

Edit files in `lib/N64ModernRuntime/` for OS-level debugging:

```cpp
extern "C" void osRecvMesg_recomp(uint8_t* rdram, recomp_context* ctx) {
    fprintf(stderr, "[DEBUG-RT] osRecvMesg_recomp(mq=0x%08X, msg=0x%08X, flags=%d)...\n",
        (uint32_t)ctx->r4, (uint32_t)ctx->r5, (int)ctx->r6);
    // ... existing code ...
}
```

### 3. Filter Debug Output

```bash
# Show specific patterns
wsl bash -c "... && ./build/WaveRace64Recompiled 2>&1 | grep -E '(RENDER|osSpTask|PI Manager)'"

# Show context around matches
wsl bash -c "... && ./build/WaveRace64Recompiled 2>&1 | grep -A5 'func_8004A130'"
```

## Common Issues and Fixes

### 1. Thread Blocking on Empty Queue

**Symptom**: Thread blocks indefinitely on osRecvMesg

**Solution**: Pre-seed event queues in `events.cpp`:

```cpp
case OS_EVENT_SP:
    {
        static bool sp_first_time = true;
        if (sp_first_time && mq_ != NULLPTR) {
            sp_first_time = false;
            osSendMesg(PASS_RDRAM mq_, msg, OS_MESG_NOBLOCK);
        }
        events_context.sp.msg = msg;
        events_context.sp.mq = mq_;
    }
    break;
```

### 2. PI DMA Issues (PATH 2)

**Symptom**: DMA reads return garbage or block

**Solution**: Handle PI Manager requests in `osSendMesg_recomp`:

```cpp
if (mq == (uint32_t)recomp::cart_handle) {
    // Process DMA inline using MEM_W macro
    gpr msg = ctx->r5;  // Keep as gpr, not uint32_t!
    // ... read OSIoMesg fields ...
    recomp::do_rom_read(rdram, dramAddr, physical_addr, size);
}
```

### 3. MEM_W Macro Issues

**Important**: MEM_W expects sign-extended 64-bit gpr values:

```cpp
// WRONG:
uint32_t msg = (uint32_t)ctx->r5;  // Loses sign extension
uint32_t val = MEM_W(0, msg);      // Returns garbage

// CORRECT:
gpr msg = ctx->r5;                 // Preserves sign extension
uint32_t val = MEM_W(0, msg);      // Works correctly
```

## Reference Resources

### LLONSIT Wave Race 64 Decomp

GitHub: https://github.com/LLONSIT/waverace-64

Key file: `symbol_addrs.txt` - Contains function addresses for US ROM

### Important Addresses (US ROM)

- `game_dma_copy` = 0x80097EC8
- `osPiStartDma` = 0x800CA370
- `osCreatePiManager` = 0x800C6B80
- `osSpTaskLoad` = 0x800C615C
- `osSpTaskStartGo` = 0x800C62BC
- `game_thread_entry` = 0x80047530
- `render_thread_entry` = 0x80046DA0

### ROM Information

- Wave Race 64 US (NWRE)
- SHA1: 508dfc2d4caa42b6f6de5263d0aed5e44ac7966a
- Note: Wave Race does NOT use osCartRomInit

## Message Queue Reference

| Queue Address | Purpose | Event |
|--------------|---------|-------|
| 0x80154130 | Unified event queue | VI (0x19), SP (0x17), DP (0x18) |
| 0x80154100 | Render ready queue | 0x29 ("ready to render") |
| 0x801540D0 | SI completion queue | OS_EVENT_SI |
| 0x801540B8 | DMA completion queue | PI DMA completions |

## Message Types

- 0x17 = SP task complete
- 0x18 = DP complete
- 0x19 = VI retrace
- 0x29 = Ready to render
- 0x1F = Sync signal
