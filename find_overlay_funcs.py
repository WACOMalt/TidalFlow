#!/usr/bin/env python3
"""
Find all functions that call overlay addresses (0x801xxxxx, 0x84xxxxxx, etc.)
These functions cannot be statically recompiled and must be ignored.
"""

import struct
from pathlib import Path

def read_rom(rom_path: str) -> bytes:
    with open(rom_path, 'rb') as f:
        return f.read()

def find_overlay_callers(rom_data: bytes, code_start_rom: int, code_end_rom: int, vram_start: int):
    """Find functions that call addresses outside the main code segment"""

    code_size = code_end_rom - code_start_rom
    vram_end = vram_start + code_size

    # First, build a map of function boundaries from JAL targets
    jal_targets = set()
    offset = code_start_rom
    while offset < code_end_rom - 4:
        word = struct.unpack('>I', rom_data[offset:offset+4])[0]
        if (word >> 26) == 0x03:  # JAL opcode
            target = (word & 0x03FFFFFF) << 2
            target_vram = (vram_start & 0xF0000000) | target
            if vram_start <= target_vram < vram_end:
                jal_targets.add(target_vram)
        offset += 4

    jal_targets.add(vram_start)
    sorted_funcs = sorted(jal_targets)

    # Build function boundaries
    func_boundaries = {}
    for i, vram in enumerate(sorted_funcs):
        if i + 1 < len(sorted_funcs):
            end = sorted_funcs[i + 1]
        else:
            end = vram_end
        func_boundaries[vram] = end

    # Now find which functions call overlay addresses
    overlay_callers = set()
    overlay_targets = {}

    offset = code_start_rom
    while offset < code_end_rom - 4:
        word = struct.unpack('>I', rom_data[offset:offset+4])[0]
        current_vram = vram_start + (offset - code_start_rom)

        if (word >> 26) == 0x03:  # JAL opcode
            target = (word & 0x03FFFFFF) << 2
            target_vram = (vram_start & 0xF0000000) | target

            # Check if target is outside main code segment
            if target_vram < vram_start or target_vram >= vram_end:
                # Find which function contains this instruction
                for func_start, func_end in func_boundaries.items():
                    if func_start <= current_vram < func_end:
                        overlay_callers.add(func_start)
                        if func_start not in overlay_targets:
                            overlay_targets[func_start] = []
                        overlay_targets[func_start].append(target_vram)
                        break

        offset += 4

    return overlay_callers, overlay_targets

def main():
    rom_path = "rom/baserom.us.z64"

    CODE_START_ROM = 0x1000
    CODE_END_ROM = 0x8CDB0
    VRAM_START = 0x80046800

    print(f"Reading ROM: {rom_path}")
    rom_data = read_rom(rom_path)

    print("Finding functions that call overlay addresses...")
    overlay_callers, overlay_targets = find_overlay_callers(
        rom_data, CODE_START_ROM, CODE_END_ROM, VRAM_START
    )

    print(f"\nFound {len(overlay_callers)} functions that call overlay code:")
    for func in sorted(overlay_callers):
        targets = overlay_targets.get(func, [])
        print(f"  func_{func:08X} -> {', '.join(f'0x{t:08X}' for t in targets[:3])}")

    # Generate ignored list for TOML
    print("\n\n# Add to waverace.toml ignored list:")
    print("ignored = [")
    for func in sorted(overlay_callers):
        print(f'    "func_{func:08X}",')
    print("]")

if __name__ == "__main__":
    main()
