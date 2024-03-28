#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/vreg.h"
#include "st77xx.h"

#define CHIPS_IMPL
#include "chips_common.h"
#include "z80.h"
#include "beeper.h"
#include "kbd.h"
#include "clk.h"
#include "mem.h"
#include "zx.h"
#include "zx-roms.h"

/* Modified for even RGB565 conversion. */
static uint32_t zxpalette[16] = {
    0x000000,     // std black
    0xD80000,     // std blue
    0x0000D8,     // std red
    0xD800D8,     // std magenta
    0x00D800,     // std green
    0xD8D800,     // std cyan
    0x00D8D8,     // std yellow
    0xD8D8D8,     // std white
    0x000000,     // bright black
    0xFF0000,     // bright blue
    0x0000FF,     // bright red
    0xFF00FF,     // bright magenta
    0x00FF00,     // bright green
    0xFFFF00,     // bright cyan
    0x00FFFF,     // bright yellow
    0xFFFFFF,     // bright white
};

#define FRAME_USEC (33333)
static zx_t zx;

uint16_t palette_to_565(uint32_t color) {
    return st77xx_rgb565(color & 0xff, (color>>8) & 0xff, (color>>16) & 0xff);
}

void update_display(uint8_t *crt) {
    crt += 160*121;
    uint16_t line[st77_width];
    for (uint32_t y = 0; y < st77_height; y++) {
        for (uint32_t x = 0; x < st77_width; x += 2) {
            uint8_t pixels = crt[x>>1];
            line[x] = zxpalette[(pixels>>4)&0xf];
            line[x+1] = zxpalette[pixels&0xf];
        }
        st77xx_setwin(0, y, st77_width-1, y);
        st77xx_data(line,sizeof(line));
        crt += 160;
    }
}

int main() {
    // Pico Init
    stdio_init_all();
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Display initialization
    st77xx_init();
    st77xx_fill(0xffff);
    for (int x = 0; x < 100; x++)
        st77xx_pixel(x,x,0x0000);

    // Convert palette to RGB565
    for (int j = 0; j < 16; j++)
        zxpalette[j] = palette_to_565(zxpalette[j]);

    // Overclocking
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    sleep_ms(1000);
    set_sys_clock_khz(304000, true);

    // ZX emulator Init
    zx_desc_t zx_desc = {0};
    zx_desc.type = ZX_TYPE_48K;
    zx_desc.joystick_type = ZX_JOYSTICKTYPE_KEMPSTON;
    zx_desc.audio.callback.func = NULL;
    zx_desc.audio.sample_rate = 0;
    zx_desc.roms.zx48k.ptr = dump_amstrad_zx48k_bin;
    zx_desc.roms.zx48k.size = sizeof(dump_amstrad_zx48k_bin);

    zx_init(&zx, &zx_desc);

    int pin_state = 1;
    while (true) {
        gpio_put(LED_PIN, pin_state);
        absolute_time_t start = get_absolute_time();
        zx_exec(&zx, FRAME_USEC);
        absolute_time_t end = get_absolute_time();
        update_display(zx.fb);
        pin_state = !pin_state;
        printf("zx_exec(): %llu us\n",(unsigned long long)end-start);

        static int kbstep = 0, bc = 0;
        if (kbstep == 0) {
            zx_key_down(&zx,'b');
        } else if (kbstep == 1) {
            zx_key_up(&zx,'b');
        } else if (kbstep == 2) {
            zx_key_down(&zx,'0'+bc);
        } else if (kbstep == 3) {
            zx_key_up(&zx,'0'+bc);
            bc++;
            if (bc == 7) bc = 0;
        } else if (kbstep == 4) {
            zx_key_down(&zx,'\r');
        } else if (kbstep == 5) {
            zx_key_up(&zx,'\r');
        }
        kbstep++;
        if (kbstep == 6) kbstep = 0;
    }
}
