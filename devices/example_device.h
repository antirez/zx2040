// This configuration file is an example with comment sections
// explaining how to create a configuration file for a new device.

// We assume a Raspberry Pico RP2040 + any ST77xx display.

/* ============================= KEYS CONFIGURATION ==========================
 * Here you need to define your Pico pins that will be associated to joystick
 * movements. These pins should be connected to buttons (switches) so that
 * the Pico reads the pin at level 1 when the button is pressed.
 * However redefininig get_device_button() you can invert the logic if needed.
 */

#define KEY_LEFT    10  // Pico pin 10
#define KEY_RIGHT   11  // Pico pin 11
#define KEY_FIRE    12  // Pico pin 12
#define KEY_UP      13  // Pico pin 13
#define KEY_DOWN    14  // Pico pin 14

// If you have buttons that will drive the pin to ground, you want
// to initialize the GPIOs in pull-up mode. Uncomment the following.
// Please also check the get_device_button() define below as you
// will also need some tweak there.

#if 0
#define KEY_LEFT_PULLUP
#define KEY_RIGHT_PULLUP
#define KEY_UP_PULLUP
#define KEY_DOWN_PULLUP
#define KEY_FIRE_PULLUP
#endif

/* On most devices, we check if buttons are pressed just reading
 * the GPIO, but it is possible to redefine the macro reading the
 * value if needed, so that i2c keyboards or joypads can be supported.
 * This is also useful if you have buttons that are low when pressed
 * instead of the default (high when pressed). */

#define get_device_button(pin_num) gpio_get(pin_num)

// For example, if you have buttons that drive GPIOs to ground,
// you may want to invert the return value:
// #define get_device_button(pin_num) (!gpio_get(pin_num))

/* ================================ SPEAKER PIN ==============================
 * If you want audio support, can connect a piezo speaker to some pin.
 * Alternatively any speaker that can be driven by the Pico pin output
 * current, is fine. Ideally a transistor could be used in order to amplify
 * the output of the pin.
 *
 * Speaker pin set to the right pin if any.
 * Otherwise it should be set to -1.
 * Note that the speaker pin will use PWM. */

#define SPEAKER_PIN -1

/* ============================= DISPLAY CONFIGURATION =======================
 * Your need an ST77xx-based display, like the ST7789 or ST7735.
 * We supporth both 8-lines parrallel bus and the much more common SPI.
 */

// First of all, select your bus type, SPI or parallel.
// Make sure to uncomment only one of the following lines.

#define st77_use_spi
// #define st77_use_parallel

// If your display is an SPI display, fill the configuratin here.
// If you can't see anything try a lower SPI data rate.
// Also play with polaity and phase.
#ifdef st77_use_spi
#define spi_rate 200000000  // Crazy rate, but works.
#define spi_phase 1         // 1 or 0
#define spi_polarity 1      // 1 or 0
#define spi_channel spi0
#define st77_sck 2          // Sometimes called SCL or clock.
#define st77_mosi 3         // Sometimes called SDA.
#define st77_rst 7          // -1 if your display lacks a reset pin.
#define st77_dc 6
#define st77_cs -1          // -1 if your display lacks a CS pin.

/* Software SPI:
 *
 * If you want to go fast with SPI transfers (less display update latency)
 * you can use the big banging software SPI implementation. I suggest first
 * testing the emulator with the standard hardware SPI, then checking the
 * Pico serial output to see the update time, and switch to bit banging if
 * needed. */
// #define st77_spi_bb
#endif

// For parallel 8 lines display, fill the configuration here:
#ifdef st77_use_parallel
#define pio_channel pio0
#define st77_cs 10
#define st77_dc 11
#define st77_wr 12
#define st77_rd 13
#define st77_d0 14 // d1=d0+1, d2=d0+2, ...
#define st77_rst -1
#endif

// These kind of displays require a backlight in order for the user
// to see the image. This is often marked in the display as LED0 or BL
// or something like that.
//
// Please note that if no backlight pin is configured, you will likely
// not see any image even if the rest of the configuration is correct.
#define st77_bl 8

// ST77xx display settings.
#define st77_width 240      // Display width and height.
#define st77_height 135     // Check your display for the correct order.
#define st77_landscape 1    // Portrait or landscape. You want landscape.
#define st77_mirror_x 0     // X mirroring, if needed.
#define st77_mirror_y 1     // Y mirroring, if needed.
#define st77_inversion 1    // If colors are inverted, toggle this to 0 or 1.
#define st77_offset_x 0     // Image not centered? Play with this offset.
#define st77_offset_y 0     // Note: offsets may depend on landscape/mirroring.
#define st77_isbgr 0        // Set to 1 if it's a BGR and not RGB display.
                            // In practical terms, if colors look "inverted"
                            // (yellow is cyan and the contrary) set to 1.

/* =========================== SCREEN RENDERING CONFIG =======================
 * Here you can set how the Spectrum video memory is rendered on your display.
 * You can select the scaling level and if to visualize or not the border.
 *
 * If your display is smaller than the Spectrum visible screen, you need
 * downscaling, otherwise you need upscaling.
 *
 * When borders are used, the Spectrum CRT resolution that gets rendered
 * on your display is 320x256. Without borders only the bitmap area is
 * drawn: 256x192. If your display is 320x240 the best thing to do is to
 * disable borders and use an upscaling of 125, so that the 256x192 bitmap
 * area gets scaled exactly to 320x240.
 *
 * Scaling factors supported: 50%, 75%, 84%, 100%, 112%, 125%, 150%, 200%.
 */
#define DEFAULT_DISPLAY_SCALING 100  // Not all values possible, see above.
#define DEFAULT_DISPLAY_BORDERS 1    // 0 = no borders. 1 = borders.

// Partial updates make the emulator MUCH faster. The sound timing may be
// a bit less stable, but if your display is slow to update, it's recommended
// to enable it.
#define DEFAULT_DISPLAY_PARTIAL_UPDATE 1

// That's it! Copy the modified file as 'device_config.h' in the root
// directory and recompile it.
