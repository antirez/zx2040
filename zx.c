#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/vreg.h"
#include "st77xx.h"
#include "keys_config.h"

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

/* =============================== Games list =============================== */

// Games on the flash memory. Check under the games directory for the
// script that loads the Z80 image files into the flash memory.
struct game_entry {
    const char *name;
    void *addr;         // Address in the flash memory.
    size_t size;        // Length in bytes.
    const uint8_t *map; // Keyboard mapping to use. See keys_config.h.
} games_table[] = {
    {"Jetpac", (void*)0x1007f100, 10848, keymap_default},
    {"Bombjack", (void*)0x10081b60, 40918, keymap_bombjack},
    {"Thrust", (void*)0x1008bb36, 33938, keymap_thrust},
    {NULL,0,0,NULL} // Terminator.
};

/* ========================== Global state and defines ====================== */

// We are not able to reach 30FPS, but still this is a good amount of
// work for each zx_exec() call. Updating the display takes ~13 milliseconds
// so to do this too often it's not a great idea.
#define FRAME_USEC (33333)

static struct emustate {
    zx_t zx; // The emulator state.

    // We switch betweent wo clocks: one is selected just for zx_exec(), that
    // is the most speed critical code path. For all the other code execution
    // we stay to a lower overclocking mode that is low enough to allow the
    // flash memory to be accessed without issues.
    uint32_t base_clock;
    uint32_t emu_clock;

    // Keymap in use right now. Modified by load_game().
    const uint8_t *CurrentKeymap;

    // Is the game selection / config menu shown?
    int menu_active;
} EMU;

/* ========================== Emulator user interface ======================= */

void draw_char(uint16_t px, uint16_t py, uint8_t c, uint16_t color) {
    c -= 0x20;
    // We use the font in the Spectrum ROM to avoid providing one.
    uint8_t *font = dump_amstrad_zx48k_bin+0x3D00;
    for (int y = 0; y < 8; y++) {
        uint32_t row = font[c*8+y];
        for (int x = 0; x < 8; x++) {
            if (row & 0x80)
                st77xx_pixel(px+x,py+y,color);
            row <<= 1;
        }
    }
}

void draw_string(uint16_t px, uint16_t py, const char *s, uint16_t color) {
    while (*s) {
        draw_char(px,py,s[0],color);
        s++;
        px += 8;
    }
}

void draw_menu_ui(void) {
    draw_string(10,10,"This is just a test!",zxpalette[2]);
}

/* =========================== Emulator implementation ====================== */


// ZX Spectrum palette to RGB565 conversion. We do it at startup to avoid
// burning CPU cycles later.
uint16_t palette_to_565(uint32_t color) {
    return st77xx_rgb565(color & 0xff, (color>>8) & 0xff, (color>>16) & 0xff);
}

// Transfer the Spectrum CRT representation into the ST77xx display.
// We allocate just a scanline of buffer and transfer it one at a time.
//
// Note that the zx.h file included here was modified in order to use
// 4bpp framebuffer to save memory, so each byte in the CRT memory is
// actually two pixels.
void update_display(uint8_t *crt) {
    // crt += 160*121;
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

// This function maps GPIO state to the Spectrum keyboard registers.
// Other than that, certain keys are pressed when a given frame is
// reached, in order to enable the joystick or things like that.
void handle_key_press(zx_t *zx, const uint8_t *keymap, uint32_t ticks) {
    // Handle key presses.
    for (int j = 0; ;j += 3) {
        if (keymap[j] == KEY_END) {
            // End of keymap reached.
            break;
        } else if((keymap[j] == PRESS_AT_TICK ||
                   keymap[j] == RELEASE_AT_TICK) &&
                  keymap[j+1] == ticks)
        {
            // Press/release keys when a given frame is reached.
            if (keymap[j] == PRESS_AT_TICK) {
                zx_key_down(zx,keymap[j+2]);
            } else {
                zx_key_up(zx,keymap[j+2]);
            }
        } else {
            // Map the GPIO status to the ZX Spectrum keyboard
            // registers.
            if (gpio_get(keymap[j])) {
                zx_key_down(zx,keymap[j+1]);
                zx_key_down(zx,keymap[j+2]);
            } else { 
                zx_key_up(zx,keymap[j+1]);
                zx_key_up(zx,keymap[j+2]);
            }
        }
    }
}

// Initialize the Pico and the Spectrum emulator.
void init_emulator(void) {
    // Set default configuration.
    EMU.menu_active = 1;
    EMU.base_clock = 280000;
    EMU.emu_clock = 400000;
    EMU.CurrentKeymap = keymap_default;

    // Overclocking
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    set_sys_clock_khz(EMU.base_clock, true);

    // Pico Init
    stdio_init_all();

    // Display initialization
    st77xx_init();
    st77xx_fill(0xffff);
    for (int x = 0; x < 100; x++)
        st77xx_pixel(x,x,0x0000);

    // Convert palette to RGB565
    for (int j = 0; j < 16; j++)
        zxpalette[j] = palette_to_565(zxpalette[j]);

    // ZX emulator Init
    zx_desc_t zx_desc = {0};
    zx_desc.type = ZX_TYPE_48K;
    zx_desc.joystick_type = ZX_JOYSTICKTYPE_KEMPSTON;
    zx_desc.audio.callback.func = NULL;
    zx_desc.audio.sample_rate = 0;
    zx_desc.roms.zx48k.ptr = dump_amstrad_zx48k_bin;
    zx_desc.roms.zx48k.size = sizeof(dump_amstrad_zx48k_bin);
    zx_init(&EMU.zx, &zx_desc);
}

/* Load the specified game ID. The ID is just the index in the
 * games table. As a side effect, sets the keymap. */
void load_game(int game_id) {
    struct game_entry *g = &games_table[game_id];
    chips_range_t r = {.ptr=g->addr, .size=g->size};
    EMU.CurrentKeymap = g->map;
    zx_quickload(&EMU.zx, r);
}

int main() {
    init_emulator();
    load_game(1);

    uint32_t ticks = 0;
    while (true) {
        absolute_time_t start, end;

        handle_key_press(&EMU.zx, EMU.CurrentKeymap, ticks);

        // Run the Spectrum VM for a few ticks.
        set_sys_clock_khz(EMU.emu_clock, true); sleep_us(50);
        start = get_absolute_time();
        zx_exec(&EMU.zx, FRAME_USEC);
        end = get_absolute_time();
        printf("zx_exec(): %llu us\n",(unsigned long long)end-start);
        set_sys_clock_khz(EMU.base_clock, true); sleep_us(50);

        // Update the display with the current CRT image.
        start = get_absolute_time();
        update_display(EMU.zx.fb);
        end = get_absolute_time();
        printf("update_display(): %llu us\n",(unsigned long long)end-start);

        // Handle the menu.
        if (EMU.menu_active) {
            draw_menu_ui();
        }

        ticks++;
    }
}
