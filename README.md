This repository is a port of [https://github.com/floooh/chips/](Andre Weissflog) ZX Spectrum emulator to the Raspberry Pico RP2040, packed with a simple UI for game selection and key mapping to make it usable without a keyboard.

This project is specifically designed for the Raspberry Pico and ST77xx based displays. Our reference device is the [Pimoroni Tufty RP2040 display board](https://shop.pimoroni.com/products/tufty-2040?variant=40036912595027), but actually the code can run into any Raspberry Pico equipped with an ST77x display and five buttons connected to five different pins. The buttons work as inputs for the four gaming directions (left, right, top, bottom) and the fire button. Please refer to the *hardware* section for more information.

## Main features

* Pico -> Spectrum key mapping with each pin mapped up to two Specturm keys or Kempstone joystick moves. Each game has its own key map, taking advantage of mapping to make games easier to play on portable devices: for instance Jetpac maps a single key (down key) to up + fire. Key macros are used in order to automatically trigger key presses when given frames are reached, to select the kempstone joystick, skip key redefinition, and other things otherwise impossible with few buttons available on the device.
* A minimal ST77xx display driver is included, written specifically for this project. It has just what it is needed to initialize the display and refresh the screen with the Spectrum framebuffer content. It works both with SPI and 8-wires parallel interfaces and is optimized for fast bulk refreshes.
* The emulator has an UI that allows to select a game into a list, change certain emulation settings and so forth.
* Multiple games included, with a script to easily added more (see section about adding games). **Important**, I included copyrighted games hoping that's fair-use, since these games are no longer sold. If you are the copyright owner and want the game to be removed, please open an issue or write me an email at *antirez at google mail service*.
* Real time upscaling and downscaling of video, to use the emulator with displays that are larger or smaller than the Spectrum video output. The emulator is also able to remove borders.
* Crazy overclocking to make it work fast enough :D **Warning**: the code must run from the Pico RAM, and not in the memory mapped flash, otherwise it's not possible to go at 400Mhz. This is achieved simply with `pico_set_binary_type(zx copy_to_ram)` in `CMakeList.txt`. There are no problems accessing the flash to load games, because the code downclocks the CPU when loading games, and then returns at a higher overclocking speeds immediately after.

## Changes made to the original emulator

The fantastic emulator I used as a base for this project was not designed for very small devices. It was rather optimized for the elegance of the implementation (you have self-contained emulated chips that are put together with the set of returned pins states) and very accurate emulation. To make it run on the Pico, I had to modify the emulator in the following ways:

* In order to work with the small amount of RAM available in the RP2040, only the Spectrum 48k version is emulated, the 128k code was removed. The video code was changed to use a much smaller CRT framebuffer, where each byte holds two pixels (4bpp color).
* The emulator UI is rendered directly on the emulator framebuffer in order to save memory.
* Emulation performances were improved by rewriting small parts of the code that renders the ZX Spectrum VMEM into the CRT framebuffer (ULA emulation) and modifying the Z80 implementation to cheat a bit (instruction fetching combines three steps into one, slow instructions executed in less cycles and so on. Otherwise we could go at max at 60% speed of real hardware). On the scanline decoding code, there is a fast path for drawing the borders and and a few more changes to improve performances.
* Audio support was completely rewritten using the Pico second core and double buffering. We have two issues with the RP2040. One is memory. Fortunately there is no need to go from 1 bit music to 16bit samples that will then drive a speaker exactly with 1 bit of actual resolution. It makes sense in the original emulator, since the audio device of a real computer will accept proprer 16 bit audio samples, but in the Pico we just drive a pin with a connected speaker. So this repository implements a bitmap audio buffer, reducing the memory usage by a factor of 32. Another major problem is that we are emulating the Spectrum native speed by running without pauses: there is no way to be sure about the exact timing of a full tick (different sequences of instructions run at different speed), and the audio must be played as it is produced (in the original emulator it was assumed that the CPU of the host computer was able to emulate the Spectrum much faster, take the audio buffer, and put the samples in the audio output queue). So I used double buffering, and as the Z80 produces the music we play the other half of the buffer in the other thread, with adaptive timing. The result is recognizable audio even if the quality is not superb.

With this changes, when the Pico is overclocked at 400Mhz (default of this code, **with cpu voltage set to 1.3V**), the emulation speed is more or less the same as a real ZX Specturm 48K in most games. If you want to go slower (simpler to play games, and certain Picos may not run well at 400Mhz) press the right button when powering up: this will select 300Mhz.

## Motivactions for this project

The ZX Spectrum was the computer where I learned to code when I was a child. Before owning the Spectrum, I used to play with my father's computer, a TI 99/4A, yet the Spectrum was my first real computer, and the one where I wrote my first decent programs. So part of this is nostalgia.

On a more practical way, I wanted an emulatort that was a good fit for the Pico and specifically conceived to run with cheap displays, able to easily be adapted to different displays resolutions, MIT licensed, very easy to hack on. So that people can build, enjoy and even sell if they want battery-powered small cheap Spectrum. This can help to make the ZX Spectrum heritage last more in the future.

Finally, I needed to explore a bit more the Pico SDK and its C development experience. Recently I'm doing embedded programming, and the RP2040 is one of the most interesting MCUs out there. Writing this code was extremely helpful to better understand the platform.

## Credits

I want to thank [Andre Weissflog](https://github.com/floooh) for writing the original code and let us dream again.

## Hardware needed

You need either:

* A Pimoroni Tufty 2040.
* Or ... any other suitable Pico + display + 5 buttons combination.
* A piezo speaker if you want audio.

This project only supports ST77xx displays so far. They are cheap and widespread, and work well.

A note of warning: it is crucial to be able to refresh the display
fast enough. SPI displays work well if they are not huge, let's say that up to
320x240 max the update latency will not be so terrible.

Parallel 8-lines ST77xx displays are much better, for instance in the Tufty 2040, using upscaling, it is possible to transfer the Spectrum CRT framebuffer to the display in something like ~14 milliseconds, so even refreshing the display 20 times per second we have 700 milliseconds of CPU time to run the emulator itself.

320x240 displays are particularly good because the Spectrum full visible area including borders is 320x256 pixels, but if we remove borders, so when borders are enabled this is a nice view. When borders are disabled, it's even better: the bitmap resolution of the Spectrum is 256x192 pixels, it means that using 125% upscaling we match exactly the 320x240 display resolution!

## Installation from sources

If you build from sources:

* Create a `device_config.h` file in the main directory. For the Pimoroni Tufty 2040 just do `cp devices/tufty2040.h device_config.h`. Otherwise if you have some different board, or you made one by hand with a Pico and an ST77xx display, just check the examples under the `devices` directory and create a configuration: it's easy, just define your pins and display interface (SPI/parallel).
* Compile with: `mkdir build; cd build; cmake ..; make`.
* Transfer the `zx.uf2` file to your Pico (put it in boot mode pressing the boot button as you power up the device, then drag the file in the `RPI-RP2` drive you see as a USB drive).
* Transfer the games images on the flash. Enter the `games` directory, put the Pico in boot mode (again) and run the `loadgames.py` Python program. Note that you need `picotool` installed (`pip install picotool`, or alike) to run it.

## Installation from pre-built images

If you have a Tufty 2040, you can just grab one of the images under the `uf2` directory in this repository and flash your device. Done.

## Usage

## Included games

... add key maps for each game ...

## Adding games

## Porting to other Pico setups
