* Ability to center the screen when downscaling is used. If it does not make refreshing much slower, fill the remaining area picking the last border set from the emulator ULA state.
* Use Pico PWM for screen brightness.
* In the Tufty 2040, use the light sensor to adjust screen brightness.
* Allow to change brightness from the menu.
* Many more games with well designed key maps.
* Find a way to put code + game into the same UF2 image.

## User experience

* Make it easier to add games / keymaps without recompiling the project.

## Hardware

* Design ZX Spectrum cover for the Tufty 2040. Provide STL file.
* Try other speakers other than the piezo that the Pi pin can drive but with less horrible frequency response.
* Abstract extended keymaps in the device configuration, so that if there are devices with more keys, we can use them. For instance the Pimoroni PicoSystem has 4 buttons other than the directions.

## Documentation

* Document all the stuff into the device configuration file.
* Document how to change the number of ticks the emulator runs before screen updates in order to match the Spectrum native speed in case of a slow display or less overclocking.
* Document keymaps.
