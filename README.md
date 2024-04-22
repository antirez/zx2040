The **ZX2040** is a port of [Andre Weissflog](https://github.com/floooh/chips/) ZX Spectrum emulator to the Raspberry Pico RP2040, packed with a simple UI for game selection and key mapping to make it usable without a keyboard.

This project is specifically designed for the Raspberry Pico and ST77xx based displays. Our reference device is the [Pimoroni Tufty RP2040 display board](https://shop.pimoroni.com/products/tufty-2040?variant=40036912595027), but actually the code can run into any Raspberry Pico equipped with an ST77x display and five buttons connected to five different pins. The buttons work as inputs for the four gaming directions (left, right, top, bottom) and the fire button. Please refer to the *hardware* section for more information.

![Jetpac running on the Pico ZX Spectrum emulator](images/pico_spectrum_emu.jpg)

## Main features

* **Pico -> Spectrum key mapping** with each pin mapped up to two Spectrum keys or Kempstone joystick moves. Each game has its own key map, taking advantage of mapping to make games easier to play on portable devices: for instance Jetpac maps a single key (down key) to up + fire. Key macros are used in order to automatically trigger key presses when given frames are reached, to select the kempstone joystick, skip key redefinition, and other things otherwise impossible with few buttons available on the device.
* A **minimal ST77xx display driver is included**, written specifically for this project. It has just what it is needed to initialize the display and refresh the screen with the Spectrum frame buffer content. It works both with SPI and 8-wires parallel interfaces and is optimized for fast bulk refreshes.
* The emulator has an **UI that allows to select games** into a list, change certain emulation settings and so forth.
* **Easy games upload**, with a script to create a binary image of Z80 games and transfer it into the Pico flaash. **Important:**, I will try to include games for which the owners provided permission, for now you have to copy the Z80 files yourself. Check the list of games for the games that already have a defined keymap.
* **Real time upscaling and downscaling** of video, to use the emulator with displays that are larger or smaller than the Spectrum video output. The emulator is also able to remove borders.
* **Crazy overclocking** to make it work fast enough :D **Warning**: the code must run from the Pico RAM, and not in the memory mapped flash, otherwise it's not possible to go at 400Mhz. This is achieved simply with `pico_set_binary_type(zx copy_to_ram)` in `CMakeList.txt`. There are no problems accessing the flash to load games, because the code down-clocks the CPU when loading games, and then returns at a higher overclocking speeds immediately after.

## Changes made to the original emulator

The fantastic emulator I used as a base for this project was not designed for very small devices. It was rather optimized for the elegance of the implementation (you have self-contained emulated chips that are put together with the set of returned pins states) and very accurate emulation. To make it run on the Pico, I had to modify the emulator in the following ways:

* In order to work with the small amount of RAM available in the RP2040, only the Spectrum 48k version is emulated, the 128k code and allocations were removed. The video decoding was also removed. Now the decoding is performed on the fly in the screen update function of the emulator, by reading directly from the Spectrum video memory (this also provideded a strong speedup).
* The emulator UI itself is rendered directly inside the Spectrum video memory in order to save memory.
* Emulation performances were improved by rewriting video decodincg and modifying the Z80 implementation to cheat a bit (well, a lot): many steps of instruction fetching were combined together, slow instructions executed in less cycles, memory accesses done directly inside the Z80 emulation tick, and so forth. This makes the resulting emulator no longer cycle accurate, but otherwise we could go at best at 60% of the speed of real hardware, which is not enough for a nice gaming experience.
* Audio support was completely rewritten using the Pico second core and double buffering. We have two issues with the RP2040. One is memory. Fortunately there is no need to go from 1 bit music to 16bit samples that will then drive a speaker exactly with 1 bit of actual resolution. It makes sense in the original emulator, since the audio device of a real computer will accept proper 16 bit audio samples, but in the Pico we just drive a pin with a connected speaker. So this repository implements a bitmap audio buffer, reducing the memory usage by a factor of 32. Another major problem is that we are emulating the Spectrum native speed by running without pauses: there is no way to be sure about the exact timing of a full tick (different sequences of instructions run at different speed), and the audio must be played as it is produced (in the original emulator it was assumed that the CPU of the host computer was able to emulate the Spectrum much faster, take the audio buffer, and put the samples in the audio output queue). So I used double buffering, and as the Z80 produces the music we play the other half of the buffer in the other thread, with adaptive timing. The result is recognizable audio even if the quality is not superb.

With this changes, when the Pico is overclocked at 400Mhz (default of this code, **with cpu voltage set to 1.3V**), the emulation speed is more or less the same as a real ZX Spectrum 48K in most games. If you want to go slower (simpler to play games, and certain Picos may not run well at 400Mhz) press the right button when powering up: this will select 300Mhz.

Please note that a few of this changes are particularly "breaking", but they are a needed compromise with performances on the RP2040 and good frame rate. A 20 FPS emulator that runs very smoothly is a nice thing, but breaking the Z80 precise clock breaks certain games. Moreover, the way we plot the video memory instantaneously N times per second is different than what the ULA does: a game may try to "follow" the CRT beam (for example removing the old sprites once it is sure the beam is over a given part). Most games are resilient to these inconsistencies with the original hardware, but when it's an issue, we resort to game specific tuning of the emulator timing parameters.

## Motivations for this project

The ZX Spectrum was the computer where I learned to code when I was a child. Before owning the Spectrum, I used to play with my father's computer, a TI 99/4A, yet the Spectrum was my first real computer, and the one where I wrote my first decent programs. So part of this is nostalgia.

On a more practical way, I wanted an emulator that was a good fit for the Pico and specifically conceived to run with cheap displays, able to easily be adapted to different displays resolutions, MIT licensed, very easy to hack on. So that people can build, enjoy and even sell if they want battery-powered small cheap Spectrum. This can help to make the ZX Spectrum heritage last more in the future.

Finally, I needed to explore a bit more the Pico SDK and its C development experience. Recently I'm doing embedded programming, and the RP2040 is one of the most interesting MCUs out there. Writing this code was extremely helpful to better understand the platform.

## Credits

I want to thank [Andre Weissflog](https://github.com/floooh) for writing the original code and let us dream again. If this project was possible it is 90% because of his work.

## Hardware needed

You need either:

* A Pimoroni Tufty 2040.
* Or, any other suitable Pico + display + 5 buttons combination.

and...

* A piezo speaker if you want audio support.

This project only supports ST77xx displays so far. They are cheap and widespread, and they work well and exist in different qualities: TFT, IPS, and so forth.

A note of warning: it is crucial to be able to refresh the display
fast enough. SPI displays work well if they are not huge, let's say that up to
320x240 max the update latency will not be so terrible.

Parallel 8-lines ST77xx displays are much better, for instance in the Tufty 2040, using upscaling, it is possible to transfer the Spectrum CRT frame buffer to the display in something like ~14 milliseconds, so even refreshing the display ~20 times per second we have 700 milliseconds of CPU time to run the emulator itself.

320x240 displays are particularly good because the Spectrum full visible area including borders is 320x256 pixels, so when borders are enabled this is a nice view. When borders are disabled, it's even better: the bitmap resolution of the Spectrum is 256x192 pixels, it means that using 125% upscaling we match exactly the 320x240 display resolution!

The display should also be big enough if you want a nice play experience. Spectrum games were designed to be displayed in a big TV set, so certain details can be too small if very small displays are used. 2.4" is a nice size. Larger is even better.

## Installation from sources

If you build from sources:

* Install `picotool` (`pip install picotool`, or alike) and the Pico SDK.
* Create a `device_config.h` file in the main directory. For the Pimoroni Tufty 2040 just do `cp devices/tufty2040.h device_config.h`. Otherwise if you have some different board, or you made one by hand with a Pico and an ST77xx display, just check the self-commented example file under the `devices` directory and create a configuration for your setup: it's easy, just define your pins and the display interface (SPI/parallel).
* Transfer the games images on the flash. Enter the `games` directory, add your games `.z80` files there if you want, otherwise there is a just a single demo already included (Note: the games may be under copyright, I'm not responsible in case you use copyrighted material without permission), see the games list for games that already have a keymap defined, also check `keymaps.h` to see how to name the `.z80` file for each game. For instance Jetpac should be named `jetpac.z80`. Put the Pico in boot mode and run the `loadgames.py` Python program: this program will generate the game list header (used in the next step) and will upload the games to the device flash memory.
* Compile with: `mkdir build; cd build; cmake ..; make`.
* If you get compilation errors about keymap files, either the .z80 file was not copyed inside `games` with a name matching the keymap, or a keymap is not defined for that game inside `keymaps.h`.
* Transfer the `zx.uf2` file to your Pico (put it in boot mode pressing the boot button as you power up the device, then drag the file in the `RPI-RP2` drive you see as a USB drive).

## Installation from pre-built images

For the Tufty 2040 I'll add an UF2 file with a few games included. For now I had to remove it, because of games copyright concerns. I need to understand which Spectrum games can be freely distributed.

## Usage

* Select the game and press the fire button to load it. The press the fire button again with the loaded game selected to leave the menu.
* Long press left+right to return back to the menu.
* Start with the left button pressed for more serial debugging and frame counter.
* Start with the right button pressed to boot with a less extreme overclocking (300Mhz instead of 400Mhz). You can adjust it from the menu.

## Games compatibility

This repository has keymaps that work with the following games:

* [Jetpac](https://en.wikipedia.org/wiki/Jetpac)
* [Loderunner](https://en.wikipedia.org/wiki/Lode_Runner)
* [International Karate+](https://en.wikipedia.org/wiki/International_Karate_%2B)
* [Sabre Wulf](https://en.wikipedia.org/wiki/Sabre_Wulf)
* [Sanxion](https://en.wikipedia.org/wiki/Sanxion)
* [Scuba Dive](https://worldofspectrum.org/archive/software/games/scuba-dive-durell-software-ltd)
* [Thrust](https://en.wikipedia.org/wiki/Thrust_\(video_game\))
* [BMX Simulator](https://en.wikipedia.org/wiki/BMX_Simulator)
* [Bombjack](https://en.wikipedia.org/wiki/Bomb_Jack)
* [Skool Daze](https://en.wikipedia.org/wiki/Skool_Daze)
* [Pac-Man](https://marco-leal-zx.itch.io/zx-pacman-arcade) incredible conversion by Marco Leal

## Demos compatibility

* [3D Show](https://worldofspectrum.net/item/0007729/) (19xx), Vektor Graphix.

## Games keymaps

If you have doubts about what keys to use for a given file, make sure to check the **keymaps.h** file inside this repository. There you will find a keymap for each game, with comments telling you what each key does what and why.

## Adding games

1. Copy the game Z80 file into the `games` directory. Use a very short name without special characters.
2. Generate the game table and load the new binary image of the games using the `loadgames.py` script. Before running the script, put the Pico in boot mode.
3. Add a keymap for the game, editing the `keymaps.h` file using the other keymaps as example.
4. Recompile the project.
5. Transfer the new UF2 image to the Pico.

## Using this emulator for commercial purposes

All the code here MIT licensed, so you are free to use this emulator for commercial purposes. Feel free to sell it, put it as example in your boards or whatever you want to do with it. **However please note that games you find in the wild are often copyrighted material** and are not under a free license. Either use free software games with a suitable license (there are new games developed for the Spectrum every year, very cool ones too: pick the ones with a suitable license), or ask permission to the copyright owners. In any case, **you will be responsible for your actions, not me :)**.

About the ZX Spectrum ROM included in this repository, this is copyrighted material, and the current owner is the Sky Group, so if you want to do a commercial product using this code using also the ROM, you need to contact Sky Group. This is what happened so far:
* The original owner was Sinclair, that was sold to Amstrad.
* Amstrad agreed that having the ROMs as parts of NON commercial use was fair use. That's really cool, and one of the resons why Spectrum emulators are legal.
* Then Amstrad was sold to the Sky Group. Apparently the ZX Spectrum Next has official permission from the Sky Group to use the ROM. So the position of the Sky Group is yet very open.
* However if you want to make anything commercial with the ROM, you need an official written permission. If you want to use this emualtor without the ROM, to run a snapshot image that does not use any code inside the ROM, you can just discard the ROM file, and use this emulator for any purpose.
