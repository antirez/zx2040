This repository is a port of [https://github.com/floooh/chips/](Andre Weissflog) ZX Spectrum emulator to the Raspberry Pico RP2040, packed with a simple UI for game selection and key mapping to make it usable without a keyboard.

Right now this repository especially targets the Pimoroni Tufty RP2040+display board, but actually the code can run in any Raspberry Pico equipped with an ST77x display and five buttons connected to five different pints, working as inputs for the four gaming directions (left, right, top, bottom) and the fire button. Please refer to the *hardware* section for more information.

A few key information about this project:

* The emulator was modified in order to work with the small amount of RAM available in the RP2040. Only the Spectrum 48k version is emulated. The video code was modified to work with a smaller internal framebuffer, where each byte holds two pixels (4bpp color). The emulator UI is rendered directly on the emulator framebuffer in order to save memory.
* Emulation performances were improved by rewriting small parts of the code that renders the ZX Spectrum VMEM into the CRT framebuffer and modifying the Z80 implementation to cheat a bit (instruction fetching combines three steps into one, slow instructions executed in less cycles and so on. Otherwise we could go at max at 60% speed of real hardware). On the scanline decoding code, there is a fast path for drawing the borders and and a few more changes to improve performances. When the Pico is overclocked at 400Mhz (default of this code, **with cpu voltage set to 1.3V**), the emulation speed is more or less the same as a real ZX Specturm 48K in most games. If you want to go slower (simpler to play games, and certain Picos may not run well at 400Mhz) press the right button when powering up: this will select 300Mhz.
* Audio was rewritten from scratch: we have two issues with the RP2040. One is memory. Fortunately there is no need to go from 1 bit music to 16bit samples that will then drive a speaker exactly with 1 bit of actual resolution. It makes sense in the original emulator, since the audio device of a real computer will accept proprer 16 bit audio, but in the pico we just drive a pin with a connected speaker. So this repository implements a bitmap audio buffer, reducing the memory usage by a factor of 16. Another major problem is that we are emulating the Spectrum native speed by running without pauses: there is no way to be sure about the exact timing of a full tick (different sequences of instructions run at different speed), and the audio must be played as it is produced (in the original emulator it was assumed that the CPU of the host computer was able to emulate the Spectrum much faster, take the audio buffer, and put the samples in the audio output queue). So I used double buffering, and as the Z80 produces the music we play the other half of the buffer, with adaptive timing. The result is recognizable audio even if the quality is not superb.
* The code implements Pico -> Spectrum key mapping with each pin mapped to two Specturm keys or Kempstone joystick moves. Each game has its own key map, taking advantage of mapping to make games easier to play on portable devices: for instance Jetpac maps a single key (down key) to up + fire. Key map macros are used in order to automatically press keys when given frames are reached, to select the kempstone joystick, skip key redefinition, and other things otherwise impossible with few buttons available on the device. Maybe soon or later I'll implement a software keyboard too, but we are already at the limits of used RAM.
* A minimal ST77xx display driver is included, written specifically for this project. It has just what we need to initialize the display and refresh the screen with the Spectrum framebuffer content. It works both with SPI and 8-wires parallel interfaces and is optimized for fast bulk refreshes.
* The emulator has an UI that allows to select a game into a list, change certain emulation settings and so forth.
* Multiple games included. **Important**, I included copyrighted games hoping that's fair-use, since these games are no longer sold. If you are the copyright owner and want the game to be removed, please open an issue or write me an email at *antirez at google mail service*.
* **Warning**: the code must run from the Pico RAM, and not in the memory mapped flash, otherwise it's not possible to go at 400Mhz. This is achieved simply with `pico_set_binary_type(zx copy_to_ram)` in `CMakeList.txt`. There are no problems accessing the flash to load games, because the code downclocks the CPU when loading games, and then returns at a higher overclocking speeds immediately after.

## Motivactions for this project

The ZX Spectrum was the computer where I learned to code when I was a child. Before owning the Spectrum, I used to play with my father's computer, a TI 99/4A, yet the Spectrum was my first real computer, and the one where I wrote my first decent programs. So part of this is nostalgia.

However there are dozens of Spectrum emulators available, and there are a few even for the Pico. Well: I wanted a physical and portable device, and one that was very cheap to build (Picos and ST77xx displays are very cheap). I have the feeling that most people rarely power-on emulators. Something with the right form factor, that you can bring with you on vacation (or even better, donate to a child), has a different feeling.

About the other Pico emulators, I found two: one had very problematic licensing issues (the Z80 implementation is not open source, but was used regardless). The other was a bit too complex, didn't ship with the Z80 implementation, and apparently it was designed for Pico boards with an actual keyboard. My goal was to take care of each game, designing key maps that allow to play with just five button mapped, and in general to provide a easy-to-run and build project. Also I wanted all the code to be under the MIT license, since I care about spreading the 80s microcomputers culture, and this way everybody can grab this code and do whatever they want with it, including selling small Spectrum emulators.

Another driver for this project was that I needed to explore a bit more the Pico SDK and its C development experience. Recently I'm doing embedded programming, and the RP2040 is one of the most interesting MCUs out there.

Hopefully, thanks to the awesome Spectrum implementation written by Andre, and my hopefully clear modifications and UI code, this repository will be very simple to try, understand, modify.

## Hardware

## Installation from sources

If you build from sources:

* Create a `device_config.h` file in the main directory. For the Pimoroni Tufty2040 just do `cp devices/tufty2040.h device_config.h`. Otherwise if you have some different board, or you made one by hand with a Pico and an ST77xx display, just check the examples under the `device` directory and create a configuration: it's easy, just define your pins and display interface (SPI/parallel).
* Compile with: `mkdir build; cd build; cmake ..; make`.
* Transfer the zx.uf2 file to your Pico (put it in boot mode pressing the boot button as you power up the device, then drag the file in the `RPI-RP2` drive you see as USB disk).
* Transfer the games images on the flash. Enter the `games` directory, put the Pico in boot mode (again) and run the `loadgames.py` Python program. Note that you need `picotool` installed (pip install picotool, or alike).

## Installation from pre-built images

## Displays update latency

## Usage

## Included games

... add key maps for each game ...

## Adding games

## Porting to other Pico setups
