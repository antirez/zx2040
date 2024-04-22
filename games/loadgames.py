#!/usr/bin/env python3

import os, subprocess, struct

# Base address for the first game in flash memory
# WARNING: must be multiple of 4096, since the emulator
# only scans at multiples of 4096 to find the start
# address.
base_address = 0x1007f000
offset = 0

# List of .z80 files sorted alphabetically
z80_files = sorted([f for f in os.listdir('z80') if f.endswith('.z80')])

# Concatenate files with headers
with open('games.bin', 'wb') as bin_file:
    # The emulator will search for this string to find the
    # start of the games section, and to detect if there user
    # failed to update the games at all (and in such case, it
    # will display a message).
    bin_file.write(b'ZX2040GAMESBLOB!')

    for z80_file in z80_files:
        # Get the filename without the extension
        name_part = z80_file.split('.')[0]
        name_len = len(name_part)
        
        # Open the source .z80 file
        with open("z80/"+z80_file, 'rb') as file:
            data = file.read()
            data_size = len(data)
            
            # Write the header: uint8_t for name length, name as bytes,
			# uint32_t for data size
            header = struct.pack(f'<B{name_len}sI', name_len, name_part.encode(), data_size)
            bin_file.write(header)
            # Write the data
            bin_file.write(data)
    
    # Write the end-of-game marker: zero length filename to indicate
	# no more games.
    bin_file.write(b'\x00')

    # Write keymaps at the end.
    keymap = open("keymaps.txt", 'rb')
    data = keymap.read()
    data_size = len(data)
    bin_file.write(data);

print(">>> games.bin generated.")

# Transfer games.bin to Raspberry Pi Pico
try:
    subprocess.run(['picotool', 'load', 'games.bin', '-t', 'bin', '-o', hex(base_address)], check=True)
except Exception as e:
    print(e)
    exit(1)

print(">>> games.bin transferred.")
