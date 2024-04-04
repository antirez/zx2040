#!/bin/bash

cat     jetpac.z80.1007f100 \
        bombjack.z80.10081b60 \
        thrust.z80.1008bb36 \
        loderunner.z80.10093fc8 \
        > games.bin
picotool load games.bin -t bin -o 1007f100
rm -f games.bin
