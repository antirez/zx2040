// Example device config for a stand alone Raspberry Pico
// connected to a cheap Aliexpress ST7735 display.

/* ============================= KEYS CONFIGURATION ==========================
 * Here you need to define your Pico pins that will be associated to joystick
 * movements.
 */

// Pimoroni Tufty 2040 keys pins.
#define KEY_LEFT 10
#define KEY_RIGHT 11
#define KEY_FIRE 12
#define KEY_UP 13
#define KEY_DOWN 14

// On most devices, we check if buttons are pressed just reading
// the GPIO, but it is possible to redefine the macro reading the
// value if needed, so that i2c keyboards or joypads can be supported.
#define get_device_button(pin_num) gpio_get(pin_num)

// Speaker pin, if any (otherwise set it to -1). We use the pin in PWM mode.
#define SPEAKER_PIN -1

/* ============================= DISPLAY CONFIGURATION =======================
 * Your ST77xx display bus (SPI vs parallel), pins, rate, ...
 */

#define DEFAULT_DISPLAY_SCALING 100  // Not all values possible. See README.
#define DEFAULT_DISPLAY_BORDERS 1    // 0 = no borders. 1 = borders.

// Make sure to uncomment only one of the following lines, to select SPI or
// parallel-8 bus.

#define st77_use_spi
// #define st77_use_parallel

// SPI configuration (the one used by this configuration)
#ifdef st77_use_spi
#define spi_rate 200000000  // Crazy rate, but works.
#define spi_phase 1
#define spi_polarity 1
#define spi_channel spi0
#define st77_sck 2
#define st77_mosi 3
#define st77_rst 7
#define st77_dc 6
#define st77_cs -1
#endif

// Parallel 8 bit configuration (not used)
#ifdef st77_use_parallel
#define pio_channel pio0
#define st77_cs 10
#define st77_dc 11
#define st77_wr 12
#define st77_rd 13
#define st77_d0 14 // d1=d0+1, d2=d0+2, ...
#define st77_rst -1
#endif

// Backlight pin
#define st77_bl 8

// Display settings
#define st77_width 240
#define st77_height 135
#define st77_landscape 1
#define st77_mirror_x 0
#define st77_mirror_y 1
#define st77_inversion 1
#define st77_offset_x 40    // Play with x/y offset to center the image.
#define st77_offset_y 52    // Maybe start with 0 and 0.
