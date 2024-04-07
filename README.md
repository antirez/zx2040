This repository is a port of [https://github.com/floooh/chips/](Andre Weissflog) ZX Spectrum emulator to the Raspberry Pico RP2040, packed with a simple UI for game selection and key mapping to make it usable without a keyboard.

Right now this repository especially targets the Pimoroni Tufty RP2040+display board, but actually the code can run in any Raspberry Pico equipped with an ST77x display and five buttons connected to five different pints, working as inputs for the four gaming directions (left, right, top, bottom) and the fire button.

A few key information about this project:

* The emulator was modified in order to work with the small amount of RAM available in the RP2040. Only the Spectrum 48k version is emulated. The video code was modified to work with a smaller internal framebuffer, where each byte holds two pixels (4bpp color).
* Emulation performances were improved by rewriting small parts of the code that renders the ZX Spectrum VMEM into the CRT framebuffer and modifying the Z80 implementation to cheat a bit (instruction fetching combines three steps into one.. otherwise we could go at max at 60% speed of real hardware). There is a fast path for the borders and and a few more changes to improve performances. When the Pico is overclocked at 400Mhz (default), the emulation speed is the same as a real ZX Specturm 48K. If you want to go slower (simpler to play games, and certain Picos may not run well at 400Mhz) press the right button when powering up: this will select 300Mhz.
* Audio was disabled, for now. In the future I want to write a beeper emulator that directly triggers the Pico PWM, so that we don't pay any emulation cost for the sound feature.
* The code implements Pico -> Spectrum key mapping with each pin mapped to two Specturm keys or Kempstone joystick moves. Each game has its own key map. Key map macros are used in order to automatically press keys when given frames are reached, to select the kempstone joystick, skip key redefinition, and other things otherwise impossible with five buttons.
* A minimal ST77xx display driver is included, written specifically for this project. It has just what we need to initialize the display and refresh the screen with the Spectrum framebuffer content. It works both with SPI and 8-wires parallel interfaces.
* The emulator has an UI needed that allow to select a game into a list.
* Multiple games included. **Important**, I included copyrighted games hoping that's fair-use, since these games are no longer sold. If you are the copyright owner and want the game to be removed, please open an issue or write me an email at *antirez at google mail service*.
* **Warning**: the code must run from the Pico RAM, and not in the memory mapped flash, otherwise it's not possible to go at 400Mhz. This is achieved simply with `pico_set_binary_type(zx copy_to_ram)` in `CMakeList.txt`. There are no problems accessing the flash to load games, because the code overclocks to 400Mhz only to tick the emulator code, and then returns at a lower overclocking speed to do other operations.

## Installation
## Usage
## Included games
## Adding games
## Porting to other Pico setups
