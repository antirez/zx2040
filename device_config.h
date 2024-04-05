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

/* ============================= DISPLAY CONFIGURATION =======================
 * Your ST77xx display bus (SPI vs parallel), pins, rate, ...
 */

// Make sure to uncomment only one of the following lines, to select SPI or
// parallel-8 bus.

// #define st77_use_spi
#define st77_use_parallel

// SPI configuration
#ifdef st77_use_spi
#define spi_rate 40000000
#define spi_phase 1
#define spi_polarity 1
#define spi_channel spi0
#define st77_sck 2
#define st77_mosi 3
#define st77_rst 7
#define st77_dc 6
#define st77_cs -1
#endif

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
