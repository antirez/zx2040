#!/usr/bin/env python3

import os
import subprocess

# Base address for the first game in flash memory
base_address = 0x1007f100
offset = 0

# List of .z80 files sorted alphabetically
z80_files = sorted([f for f in os.listdir('.') if f.endswith('.z80')])

# Concatenate files
with open('games.bin', 'wb') as bin_file:
    for z80_file in z80_files:
        with open(z80_file, 'rb') as file:
            data = file.read()
            bin_file.write(data)

# Transfer games.bin to Raspberry Pi Pico
subprocess.run(['picotool', 'load', 'games.bin', '-t', 'bin', '-o', hex(base_address)], check=True)

# Generate games_list.h
with open('games_list.h', 'w') as h_file:
    h_file.write('// Games on the flash memory. Check under the games directory for the\n')
    h_file.write('// script that loads the Z80 image files into the flash memory.\n')
    h_file.write('struct game_entry {\n')
    h_file.write('    const char *name;\n')
    h_file.write('    void *addr;         // Address in the flash memory.\n')
    h_file.write('    size_t size;        // Length in bytes.\n')
    h_file.write('    const uint8_t *map; // Keyboard mapping to use. See keys_config.h.\n')
    h_file.write('} GamesTable[] = {\n')

    for z80_file in z80_files:
        size = os.path.getsize(z80_file)
        name = os.path.splitext(os.path.basename(z80_file))[0].capitalize()
        h_file.write(f'    {{"{name}", (void*){hex(base_address + offset)}, {size}, keymap_{name.lower()}}},\n')
        offset += size

    h_file.write('};\n')

print("Done. games.bin transferred and games_list.h generated.")

