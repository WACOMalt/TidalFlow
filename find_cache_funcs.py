#!/usr/bin/env python3
"""
Find functions that contain CACHE instructions.
These need to be stubbed as N64Recomp can't handle them.
"""

import struct

def read_rom(rom_path: str) -> bytes:
    with open(rom_path, 'rb') as f:
        return f.read()

def find_cache_funcs(rom_data: bytes, code_start_rom: int, code_end_rom: int, vram_start: int):
    code_size = code_end_rom - code_start_rom
    vram_end = vram_start + code_size

    # Build function boundaries from JAL targets
    jal_targets = set()
    offset = code_start_rom
    while offset < code_end_rom - 4:
        word = struct.unpack('>I', rom_data[offset:offset+4])[0]
        if (word >> 26) == 0x03:
            target = (word & 0x03FFFFFF) << 2
            target_vram = (vram_start & 0xF0000000) | target
            if vram_start <= target_vram < vram_end:
                jal_targets.add(target_vram)
        offset += 4

    jal_targets.add(vram_start)
    sorted_funcs = sorted(jal_targets)

    func_boundaries = {}
    for i, vram in enumerate(sorted_funcs):
        if i + 1 < len(sorted_funcs):
            end = sorted_funcs[i + 1]
        else:
            end = vram_end
        func_boundaries[vram] = end

    # Find functions containing CACHE instruction
    # CACHE opcode: 101111 (0x2F << 26)
    cache_funcs = set()

    offset = code_start_rom
    while offset < code_end_rom - 4:
        word = struct.unpack('>I', rom_data[offset:offset+4])[0]
        current_vram = vram_start + (offset - code_start_rom)

        opcode = word >> 26
        if opcode == 0x2F:  # CACHE instruction
            for func_start, func_end in func_boundaries.items():
                if func_start <= current_vram < func_end:
                    cache_funcs.add(func_start)
                    break

        offset += 4

    return cache_funcs

def main():
    rom_path = "rom/baserom.us.z64"
    CODE_START_ROM = 0x1000
    CODE_END_ROM = 0x8CDB0
    VRAM_START = 0x80046800

    rom_data = read_rom(rom_path)
    cache_funcs = find_cache_funcs(rom_data, CODE_START_ROM, CODE_END_ROM, VRAM_START)

    print(f"Found {len(cache_funcs)} functions with CACHE instructions:")
    for func in sorted(cache_funcs):
        print(f"  func_{func:08X}")

    print("\n# Add to stubs list in waverace.toml:")
    print("stubs = [")
    for func in sorted(cache_funcs):
        print(f'    "func_{func:08X}",')
    print("]")

if __name__ == "__main__":
    main()
