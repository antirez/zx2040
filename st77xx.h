#include "hardware/spi.h"

// SPI configuration
#define spi_rate 40000000
#define spi_phase 1
#define spi_polarity 1
#define spi_channel spi0

// Pins configuration
#define st77_sck 2
#define st77_mosi 3
#define st77_rst 7
#define st77_dc 6
#define st77_cs -1
#define st77_bl 8 // Backlight pin

// Display settings
#define st77_width 240
#define st77_height 135
#define st77_landscape 1
#define st77_mirror_x 0
#define st77_mirror_y 1
#define st77_inversion 1
#define st77_offset_x 40
#define st77_offset_y 52

/* Send command and/or data. */
void st77xx_write(uint8_t cmd, void *data, uint32_t datalen) {
    if (st77_cs != -1) gpio_put(st77_cs,0);
    if (cmd != 0) {
        gpio_put(st77_dc,0);
        spi_write_blocking(spi_channel,&cmd,1);
    }
    if (data != NULL) {
        gpio_put(st77_dc,1);
        spi_write_blocking(spi_channel,data,datalen);
    }
    if (st77_cs != -1) gpio_put(st77_cs,1);
}

/* Command without arguments. */
void st77xx_cmd(uint8_t cmd) {
    st77xx_write(cmd,NULL,0);
}

/* Command + 1 byte data argument. */
void st77xx_cmd1(uint8_t cmd, uint8_t val) {
    st77xx_write(cmd,&val,1);
}

/* Write data. */
void st77xx_data(void *data, uint32_t datalen) {
    st77xx_write(0,data,datalen);
}

/* Display initialization. */
void st77xx_init(void) {
    // SPI initializatin.
    gpio_init(st77_dc);
    gpio_set_dir(st77_dc,GPIO_OUT);
    if (st77_rst != -1) {
        gpio_init(st77_rst);
        gpio_set_dir(st77_rst,GPIO_OUT);
    }
    if (st77_cs != -1) {
        gpio_init(st77_cs);
        gpio_set_dir(st77_cs,GPIO_OUT);
    }
    spi_init(spi_channel,spi_rate);
    spi_set_format(spi_channel, 8, spi_polarity, spi_phase, SPI_MSB_FIRST);
    gpio_set_function(st77_sck,GPIO_FUNC_SPI);
    gpio_set_function(st77_mosi,GPIO_FUNC_SPI);

    // Display initialization
    if (st77_rst != -1) {
        gpio_put(st77_rst,1); sleep_ms(50);
        gpio_put(st77_rst,0); sleep_ms(50);
        gpio_put(st77_rst,1); sleep_ms(150);
    }
    st77xx_cmd(0x01); // Software reset
    sleep_ms(50);
    st77xx_cmd(0x11); // Awake from sleep
    sleep_ms(50);

    // Set color mode
    uint8_t colormode = 0x50 | 0x05; // 65k colors | RGB565
    st77xx_cmd1(0x3a,colormode & 0x77);
    sleep_ms(50);

    // Set memory access mode
    uint8_t madctl = 0;
    if (st77_landscape) madctl |= 0x20;
    if (st77_mirror_x) madctl |= 0x40;
    if (st77_mirror_y) madctl |= 0x80;
    st77xx_cmd1(0x36,madctl);
    st77xx_cmd(st77_inversion ? 0x21 : 0x20); // Inversion
    sleep_ms(10);

    st77xx_cmd(0x13); // Normal mode on
    sleep_ms(10);

    st77xx_cmd(0x29); // Display on
    sleep_ms(500);
}

void st77xx_setwin(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    x1 += st77_offset_x;
    y1 += st77_offset_y;
    x2 += st77_offset_x;
    y2 += st77_offset_y;
    uint8_t se[4];
    se[0] = (x1 >> 8) & 0xff;
    se[1] = x1 & 0xff;
    se[2] = (x2 >> 8) & 0xff;
    se[3] = x2 & 0xff;
    st77xx_write(0x2a,se,4);

    se[0] = (y1 >> 8) & 0xff;
    se[1] = y1 & 0xff;
    se[2] = (y2 >> 8) & 0xff;
    se[3] = y2 & 0xff;
    st77xx_write(0x2b,se,4);
    st77xx_cmd(0x2c); // Enter receive buffer data mode.
}

uint16_t st77xx_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t rgb = (r & 0xf8) << 8 | (g & 0xfc) << 3 | b >> 3;
    return (rgb >> 8) | ((rgb & 0xff) << 8);
}

void st77xx_pixel(uint16_t x, uint16_t y, uint16_t c) {
    st77xx_setwin(x,y,x,y);
    st77xx_data(&c,2);
}

void st77xx_pixel_rgb(uint16_t x, uint16_t y, uint32_t rgb) {
    if (x >= st77_width || y >= st77_height) return;
    uint16_t rgb565 = st77xx_rgb565(rgb&0xff,(rgb>>8)&0xff,(rgb>>16)&0xff);
    st77xx_pixel(x,y,rgb565);
}

void st77xx_fill(uint16_t c) {
    st77xx_setwin(0,0,st77_width-1,st77_height-1);
    for (int j = 0; j < st77_width*st77_height; j++)
        st77xx_data(&c,2);
}

void st77xx_update(uint16_t *fb) {
    st77xx_setwin(0,0,st77_width-1,st77_height-1);
    st77xx_data(fb,st77_width*st77_height*2);
}
