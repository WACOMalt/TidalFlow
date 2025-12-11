#!/usr/bin/env python3
"""
Extract functions from Wave Race 64 overlay segments.
The overlay code is loaded at runtime - we need to identify functions for N64Recomp.
"""

import struct
from pathlib import Path

def read_rom(rom_path: str) -> bytes:
    with open(rom_path, 'rb') as f:
        return f.read()

def is_likely_function_start(rom_data: bytes, offset: int) -> bool:
    """Check if this offset looks like a function start (common prologue patterns)"""
    if offset + 8 > len(rom_data):
        return False

    word = struct.unpack('>I', rom_data[offset:offset+4])[0]

    # Common function prologues:
    # addiu sp, sp, -N  (0x27BD.... where N is negative)
    if (word & 0xFFFF0000) == 0x27BD0000:
        imm = word & 0xFFFF
        if imm & 0x8000:  # Negative immediate (stack allocation)
            return True

    # lui at, N (common start)
    if (word >> 26) == 0x0F:  # LUI
        return True

    return False

def find_functions_in_segment(rom_data: bytes, rom_start: int, rom_end: int, vram_start: int):
    """Find function boundaries in a code segment using JAL targets and heuristics"""

    functions = []
    jal_targets = set()

    # Pass 1: Find all JAL targets within the segment
    offset = rom_start
    while offset < rom_end - 4:
        word = struct.unpack('>I', rom_data[offset:offset+4])[0]
        if (word >> 26) == 0x03:  # JAL opcode
            target = (word & 0x03FFFFFF) << 2
            target_vram = (vram_start & 0xF0000000) | target

            # Check if target is within this segment
            vram_end = vram_start + (rom_end - rom_start)
            if vram_start <= target_vram < vram_end:
                jal_targets.add(target_vram)
        offset += 4

    # Also add internal function calls that we detect
    print(f"  Found {len(jal_targets)} JAL targets within segment")

    # Pass 2: Also look for function prologues
    prologue_starts = set()
    offset = rom_start
    while offset < rom_end - 4:
        if is_likely_function_start(rom_data, offset):
            vram = vram_start + (offset - rom_start)
            prologue_starts.add(vram)
        offset += 4

    print(f"  Found {len(prologue_starts)} prologue patterns")

    # Combine: JAL targets are more reliable, use prologues as backup
    all_funcs = jal_targets.copy()

    # Add prologue-detected functions that aren't near JAL targets
    for vram in prologue_starts:
        # Only add if not within 16 bytes of an existing function
        if not any(abs(vram - t) < 16 for t in all_funcs):
            all_funcs.add(vram)

    # Always include segment start
    all_funcs.add(vram_start)

    # Sort and calculate sizes
    sorted_funcs = sorted(all_funcs)
    vram_end = vram_start + (rom_end - rom_start)

    for i, vram in enumerate(sorted_funcs):
        if i + 1 < len(sorted_funcs):
            size = sorted_funcs[i + 1] - vram
        else:
            size = vram_end - vram

        if size > 0:
            functions.append({
                'vram': vram,
                'size': size,
                'name': f'ovl_func_{vram:08X}'
            })

    return functions

def main():
    rom_path = "waverace-recomp/waverace.us.z64"

    # Overlay segments from patches/overlays.c
    RACING_ROM = 0xF7510
    RACING_VRAM = 0x8028DF00
    RACING_ROM_END = 0x123640  # From overlays.c: 0x123640 - 0xf7510

    # ENDING doesn't seem to contain code (looks like data)

    print(f"Reading ROM: {rom_path}")
    rom_data = read_rom(rom_path)

    print(f"\nAnalyzing RACING segment:")
    print(f"  ROM: 0x{RACING_ROM:X} - 0x{RACING_ROM_END:X}")
    print(f"  VRAM: 0x{RACING_VRAM:X}")
    print(f"  Size: 0x{RACING_ROM_END - RACING_ROM:X}")

    functions = find_functions_in_segment(rom_data, RACING_ROM, RACING_ROM_END, RACING_VRAM)

    print(f"\nFound {len(functions)} functions in RACING segment")

    # Print first 20 functions
    print("\nFirst 20 functions:")
    for f in functions[:20]:
        print(f"  {f['name']}: 0x{f['vram']:08X} (size 0x{f['size']:X})")

    # Generate TOML section
    print("\n\n# Add to waverace.syms.toml:")
    print("[[section]]")
    print('name = ".racing"')
    print(f"rom = 0x{RACING_ROM:08X}")
    print(f"vram = 0x{RACING_VRAM:08X}")
    print(f"size = 0x{RACING_ROM_END - RACING_ROM:X}")
    print()
    print("functions = [")
    for f in functions:
        print(f'    {{ name = "{f["name"]}", vram = 0x{f["vram"]:08X}, size = 0x{f["size"]:X} }},')
    print("]")

if __name__ == "__main__":
    main()
