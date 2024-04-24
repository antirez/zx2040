/* ============================= KEYS CONFIGURATION ==========================
 * Here you need to define your Pico pins that will be associated to joystick
 * movements.
 */

// Pimoroni Tufty 2040 keys pins.
#define KEY_LEFT 7
#define KEY_RIGHT 8
#define KEY_FIRE 9
#define KEY_UP 22
#define KEY_DOWN 6

// On most devices, we check if buttons are pressed just reading
// the GPIO, but it is possible to redefine the macro reading the
// value if needed, so that i2c keyboards or joypads can be supported.
#define get_device_button(pin_num) gpio_get(pin_num)

// Speaker pin, if any. We use the pin in PWM mode.
#define SPEAKER_PIN 5

/* ============================= DISPLAY CONFIGURATION ====================== */

#define DEFAULT_DISPLAY_SCALING 125 // Not all values possible. See README.
#define DEFAULT_DISPLAY_BORDERS 0   // 0 = no borders. 1 = borders.


// #define st77_use_spi
#define st77_use_parallel

// Parallel 8 bit configuration
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
#define st77_bl 2

// Display settings
#define st77_width 320
#define st77_height 240
#define st77_landscape 1
#define st77_mirror_x 0
#define st77_mirror_y 1
#define st77_inversion 1
#define st77_offset_x 0
#define st77_offset_y 0
#define st77_isbgr 0

/* === END OF CONFIG === */
