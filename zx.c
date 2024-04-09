/* Copyright (C) 2024 Salvatore Sanfilippo -- All Rights Reserved.
 * This code is released under the MIT license.
 * See the LICENSE file for more info. */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/vreg.h"

#include "device_config.h" // Hardware-specific defines for ST77 and keys.
#include "st77xx.h"
#include "keymaps.h"

#define CHIPS_IMPL
#include "chips_common.h"
#include "z80.h"
#include "beeper.h"
#include "kbd.h"
#include "clk.h"
#include "mem.h"
#include "zx.h"
#include "zx-roms.h"

#define DEBUG_MODE 1

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

void load_game(int game_id);

/* =============================== Games list =============================== */

#include "games/games_list.h"
#define GamesTableSize (sizeof(GamesTable)/sizeof(GamesTable[0]))

/* ========================== Global state and defines ====================== */

// Don't trust this USEC figure here, since the z80.h file implementation
// is modified to glue together the instruction fetch steps, so we do
// more work per tick.
#define FRAME_USEC (30000)

static struct emustate {
    zx_t zx;    // The emulator state.
    int debug;  // Debugging mode

    // We switch betweent wo clocks: one is selected just for zx_exec(), that
    // is the most speed critical code path. For all the other code execution
    // we stay to a lower overclocking mode that is low enough to allow the
    // flash memory to be accessed without issues.
    uint32_t base_clock;
    uint32_t emu_clock;

    uint32_t tick; // Frame number since last game load.

    // Keymap in use right now. Modified by load_game().
    const uint8_t *current_keymap;

    // Is the game selection / config menu shown?
    int menu_active;
    uint32_t menu_left_at_tick; // EMU.tick when the menu was closed.
    int current_game;           // Game index in the menu. If less than 0
                                // a settings item is selected instead.
    uint32_t show_border;       // If 0, Spectrum border is not drawn.
    uint32_t scaling;           // Spectrum -> display scaling factor.

    // All our UI graphic primitives are automatically cropped
    // to the area selected by ui_set_crop_area().
    uint16_t ui_crop_x1, ui_crop_x2, ui_crop_y1, ui_crop_y2;
} EMU;

/* ========================== Emulator user interface ======================= */

// Numerical parameters that it is possible to change using the
// user interface.

const uint32_t SettingsZoomValues[] = {100,112,125,150};
const char *SettingsZoomValuesNames[] = {"100%","112%","125%","150%",NULL};
struct UISettingsItem {
    const char *name;   // Name of the setting.
    uint32_t *ptr;      // Pointer to the variable of the setting.
    uint32_t step;      // Incremnet/decrement pressing right/left.
    uint32_t min;       // Minimum value alllowed.
    uint32_t max;       // Maximum value allowed.
    const uint32_t *values; // If not NULL, discrete values the variable can
                            // assume.
    const char **values_names; // If not NULL, the name to display for the
                               // values array. If values is defined, this
                               // must be defined as well.
} SettingsList[] = {
    {"clock", &EMU.emu_clock, 5000, 130000, 600000, NULL, NULL},
    {"border", &EMU.show_border, 1, 0, 1, NULL, NULL},
    {"zoom", &EMU.scaling, 0, 0, 0,
        SettingsZoomValues, SettingsZoomValuesNames,
    }
};

#define SettingsListLen (sizeof(SettingsList)/sizeof(SettingsList[0]))

// Convert the setting 'id' name and current value into a string
// to show as menu item.
void settings_to_string(char *buf, size_t buflen, int id) {
    if (SettingsList[id].values == NULL) {
        snprintf(buf,buflen,"%s:%u",
            SettingsList[id].name,
            SettingsList[id].ptr[0]);
    } else {
        int j = 0;
        while(SettingsList[id].values_names[j]) {
            if (SettingsList[id].values[j] == SettingsList[id].ptr[0])
                break;
            j++;
        }
        snprintf(buf,buflen,"%s:%s",
            SettingsList[id].name,
            SettingsList[id].values_names[j] ?
            SettingsList[id].values_names[j] : "?");
    }
}

// Change the specified setting ID value to the next/previous
// value. If we are already at the min or max value, nothing is
// done.
//
// 'dir' shoild be 1 (next value) or -1 (previous value).
void settings_change_value(int id, int dir) {
    struct UISettingsItem *si = SettingsList+id;
    if (si->values == NULL) {
        if (si->ptr[0] == si->min && dir == -1) return;
        else if (si->ptr[0] == si->max && dir == 1) return;
        si->ptr[0] += si->step * dir;
        if (si->ptr[0] < si->min) si->ptr[0] = si->min;
        else if (si->ptr[0] > si->max) si->ptr[0] = si->max;
    } else {
        int j = 0;
        while (si->values_names[j]) {
            if (si->values[j] == si->ptr[0]) break;
            j++;
        }

        // In case of non standard value found, recover
        // setting the first valid value.
        if (si->values_names[j] == NULL) {
            j = 0;
            si->ptr[0] = si->values[0];
        }
        
        if (j == 0 && dir == -1) return;
        if (si->values_names[j+1] == NULL && dir == 1) return;
        j += dir;
        si->ptr[0] = si->values[j];
    }
}

// Set the draw window of the ui_* functions. This is useful in order
// to limit drawing the menu inside its area, without doing too many
// calculations about font sizes and such.
void ui_set_crop_area(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2) {
    EMU.ui_crop_x1 = x1;
    EMU.ui_crop_x2 = x2;
    EMU.ui_crop_y1 = y1;
    EMU.ui_crop_y2 = y2;
}

// Allow to draw everywhere on the screen. Called after we finished
// updating a specific area to restore the normal state.
void ui_reset_crop_area(void) {
    ui_set_crop_area(0,st77_width-1,0,st77_height-1);
}

// This function writes a box (with the specified border, if given) directly
// inside the ZX Spectrum CRT framebuffer. We use this primitive to draw our
// UI, this way when we refresh the emulator framebuffer copying it to our
// phisical display, the UI is also rendered.
//
// bcolor and color are from 0 to 15, and use the Spectrum palette (sorry :D).
// bcolor is the color of the border. If you don't want a border, just use
// bcolor the same as color.
void ui_fill_box(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color, uint8_t bcolor) {
    uint16_t x2 = x+width-1;
    uint16_t y2 = y+height-1;
    uint8_t *crt = EMU.zx.fb;
    for (int py = y; py <= y2; py++) {
        for (int px = x; px <= x2; px++) {
            // Don't draw outside the current mask.
            if (px < EMU.ui_crop_x1 ||
                px > EMU.ui_crop_x2 ||
                py < EMU.ui_crop_y1 ||
                py > EMU.ui_crop_y2) continue;

            uint8_t *p = crt + py*160 + (px>>1);
            // Border or inside?
            uint8_t c = (px==x || px==x2 || py==y || py==y2) ? bcolor : color;

            // CRT FB is 4 bit per pixel.
            if (px&1)
                *p = (*p&0xf0) | c;
            else
                *p = (*p&0x0f) | (c<<4);
        }
    }
}

// Draw a character on the screen.
// We use the font in the Spectrum ROM to avoid providing one.
// Size is the size multiplier.
void ui_draw_char(uint16_t px, uint16_t py, uint8_t c, uint8_t color, uint8_t size) {
    c -= 0x20; // The Spectrum ROM font starts from ASCII 0x20 char.
    uint8_t *font = dump_amstrad_zx48k_bin+0x3D00;
    for (int y = 0; y < 8; y++) {
        uint32_t row = font[c*8+y];
        for (int x = 0; x < 8; x++) {
            if (row & 0x80)
                ui_fill_box(px+x*size,py+y*size,size,size,color,color);
            row <<= 1;
        }
    }
}

// Draw the string 's' using the ROM font by calling ui_draw_char().
// Size is the font size multiplier. 1 = 8x8 font, 2 = 16x16, ...
void ui_draw_string(uint16_t px, uint16_t py, const char *s, uint8_t color, uint8_t size) {
    while (*s) {
        ui_draw_char(px,py,s[0],color,size);
        s++;
        px += 8*size;
    }
}

// Load the prev/next game in the list (dir = -1 / 1).
void ui_change_game(int dir) {
    EMU.current_game += dir;
    if (EMU.current_game == -SettingsListLen-1) {
        EMU.current_game = GamesTableSize-1;
    } else if (EMU.current_game == GamesTableSize) {
        EMU.current_game = -SettingsListLen;
    }
    // Negative indexes are settings items. The game
    // list starts at index 0.
    if (EMU.current_game >= 0) load_game(EMU.current_game);
}

// Called when the UI is active. Handle the key presses needed to select
// the game and change the overclock.
#define UI_DEBOUNCING_TIME 100000
void ui_handle_key_press(void) {
    const uint8_t *km = keymap_default;
    static absolute_time_t last_key_accepted_time = 0;

    // Debouncing
    absolute_time_t now = get_absolute_time();
    if (now - last_key_accepted_time < UI_DEBOUNCING_TIME) return;

    int event = -1;
    for (int j = 0; ;j += 3) {
        if (km[j] == KEY_END) break;
        if (km[j] >= 32) continue; // Skip special codes.
        if (get_device_button(km[j])) {
            event = km[j+2];
            break;
        }
    }
    if (event == -1) return; // No key pressed right now.

    int value_change_dir = -1;
    switch(event) {
    case KEMPSTONE_UP: ui_change_game(-1); break;
    case KEMPSTONE_DOWN: ui_change_game(1); break;
    case KEMPSTONE_RIGHT: value_change_dir = 1; // fall through.
    case KEMPSTONE_LEFT:
        if (EMU.current_game < 0)
            settings_change_value(-EMU.current_game-1,value_change_dir);
        break;
    case KEMPSTONE_FIRE:
        EMU.menu_active = 0;
        EMU.menu_left_at_tick = EMU.tick;
        break;
    }
    last_key_accepted_time = now;
}

// If the menu is active, draw it.
void ui_draw_menu(void) {
    // Draw the menu in the right / top part of the screen.
    int font_size = 2;
    int menu_x = st77_width/2;
    int menu_w = st77_width/2-5;
    int menu_y = 32; // Skip border in case it's not displayed.
    int menu_h = (st77_height/3*2); // Use 2/3 of height.
    menu_h -= menu_h&(8*font_size-1); // Make multiple of font pixel size;
    int vpad = 2;       // Vertical padding of text inside the box.
    menu_h += vpad*2;   // Allow for pixels padding / top bottom.
    int listlen = (menu_h-vpad*2)/(8*font_size); // Number of items we can list.

    ui_fill_box(menu_x, menu_y, menu_w, menu_h, 0, 15);
    ui_set_crop_area(menu_x+1,menu_x+menu_w-2,
                     menu_y+1,menu_y+menu_h-2);

    int count = 0;
    int first_game = (int)EMU.current_game - listlen + 1;
    if (first_game < -SettingsListLen) first_game = -SettingsListLen;
    for (int j = first_game;; j++) {
        if (j >= (int)GamesTableSize || count >= listlen) break;

        int color = j >= 0 ? 4 : 6;
        // Highlight the currently selected game, with a box of the color
        // of the font, and the black font (so basically the font is inverted).
        if (j == EMU.current_game) {
            ui_fill_box(menu_x+2,menu_y+2+count*(8*font_size),menu_w-2,
                        font_size*8,color,color);
            color = 0;
        }
        if (j < 0) {
            // Show setting item.
            struct UISettingsItem *si = &SettingsList[-j-1];
            char sistr[32];
            settings_to_string(sistr,sizeof(sistr),-j-1);
            ui_draw_string(menu_x+2,menu_y+2+count*(8*font_size),   
                sistr,color,font_size);
        } else {
            // Show game item.
            ui_draw_string(menu_x+2,menu_y+2+count*(8*font_size),   
                GamesTable[j].name,color,font_size);
        }
        count++;
    }
    ui_reset_crop_area();
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
//
// SCALING:
// This function supports scaling: it means that it is able to transfer
// an overscaled Spectrum image to the ST77xx dispaly. This is useful in order
// to accomodate different display sizes.
//
//
// Valid scaling arguments:
//
// 100: no scaling
// 112: 112% scaling
// 125: 125% scaling
// 150: 150% scaling
//
// BORDERS:
// If border is false, borders are not drawn at all.
// Useful for small displays or when scaling is used.
void update_display(uint32_t scaling, uint32_t border) {
    uint16_t line[st77_width+1]={0}; // One pixel more allow us to overflow
                                     // when doing scaling, instead of checking
                                     // (which is costly). Hence width+1.

    uint8_t *crt = EMU.zx.fb;

    // Configure scaling: we duplicate a column/row every N cols/rows.
    uint32_t x_dup_mask = 0xffff; // no dup.
    uint32_t y_dup_mask = 0xffff; // no dup.
    switch (scaling) {
        case 150: x_dup_mask = 0; y_dup_mask = 1; break;
        case 125: x_dup_mask = 1; y_dup_mask = 3; break;
        case 112: x_dup_mask = 3; y_dup_mask = 7; break;
        case 100:
        default: scaling = 0; break;
    }

    // Center Spectrum framebuffer into Pico display and
    // set offset to remove borders if needed.
    if (!border) crt += 160*32;
    uint32_t xx_start = border ? 0 : 16; // 16*2 = 32 (4 bit per pixel).
    uint32_t zx_height = ZX_DISPLAY_HEIGHT - 64*(!border);
    uint32_t zx_width = ZX_DISPLAY_WIDTH - 64*(!border);
    if (scaling) {
        // Adjust virtual Spectrum framebuffer size by scaling.
        zx_height = (zx_height*(y_dup_mask+2)) / (y_dup_mask+1);
        zx_width = (zx_width*(y_dup_mask+2)) / (y_dup_mask+1);
    }

    if (st77_height < zx_height)
        crt += 160*((zx_height-st77_height)>>1);
    if (st77_width < zx_width)
        xx_start += (zx_width-st77_width)>>2;

    // Transfer data to the display.
    //
    // Note that we use xx and yy counters other than x and y since
    // we want to duplicate lines every N cols/rows when scaling is
    // used, and when this happens we advance x and y by a pixel more,
    // so we need counters relative to the Spectrum video, not the display.
    uint32_t yy = 0;
    for (uint32_t y = 0; y < st77_height; y++) {
        uint8_t *p = crt;
        int xx = xx_start;
        for (uint32_t x = 0; x < st77_width && xx < 160; x += 2) {
            line[x] = zxpalette[(p[xx]>>4)&0xf];
            line[x+1] = zxpalette[p[xx]&0xf];
            // Duplicate pixel according to scaling mask.
            if (((xx+1)&x_dup_mask) == 0) {
                line[x+2] = line[x+1];
                x++;
            }
            xx++;
        }
        st77xx_setwin(0, y, st77_width-1, y);
        st77xx_data(line,sizeof(line)-2); // -2 to account for the extra pixel
                                          // allocated above.
        // Duplicating row according to scaling mask.
        if (((yy+1)&y_dup_mask) == 0) {
            y++;
            st77xx_setwin(0, y, st77_width-1, y);
            st77xx_data(line,sizeof(line)-2);
        }
        crt += 160; yy++; // Next row.
        if (crt >= EMU.zx.fb+ZX_FRAMEBUFFER_SIZE_BYTES) break;
    }
}

// This function maps GPIO state to the Spectrum keyboard registers.
// Other than that, certain keys are pressed when a given frame is
// reached, in order to enable the joystick or things like that.
#define HANDLE_KEYPRESS_MACRO 1
#define HANDLE_KEYPRESS_PIN 2
#define HANDLE_KEYPRESS_ALL (HANDLE_KEYPRESS_MACRO|HANDLE_KEYPRESS_PIN)
void handle_zx_key_press(zx_t *zx, const uint8_t *keymap, uint32_t ticks, int flags) {
    // This 128 bit bitmap remembers what keys we put down
    // during this call. This is useful as sometimes key maps
    // have multiple keys mapped to the same Spectrum key, and if
    // some phisical key put down some Spectrum key, we don't want
    // a successive mapping to up it up.
    uint64_t put_down[2] = {0,0};
    #define put_down_set(keycode) put_down[keycode>>6] |= (1ULL<<(keycode&63))
    #define put_down_get(keycode) (put_down[keycode>>6] & (1ULL<<(keycode&63)))

    for (int j = 0; ;j += 3) {
        if (keymap[j] == KEY_END) {
            // End of keymap reached.
            break;
        } else if ((keymap[j] == PRESS_AT_TICK ||
                    keymap[j] == RELEASE_AT_TICK) &&
                  keymap[j+1] == ticks)
        {
            // Press/release keys when a given frame is reached.
            if (!(flags & HANDLE_KEYPRESS_MACRO)) continue;
            if (keymap[j] == PRESS_AT_TICK) {
                zx_key_down(zx,keymap[j+2]);
            } else {
                zx_key_up(zx,keymap[j+2]);
            }
        } else {
            // Map the GPIO status to the ZX Spectrum keyboard
            // registers.
            if (!(flags & HANDLE_KEYPRESS_PIN)) continue;
            if (!(keymap[j] & KEY_EXT)) {
                // Normal key maps: Pico pin -> two Spectrum keys.
                if (get_device_button(keymap[j])) {
                    put_down_set(keymap[j+1]);
                    put_down_set(keymap[j+2]);
                    zx_key_down(zx,keymap[j+1]);
                    zx_key_down(zx,keymap[j+2]);
                } else {
                    if (!put_down_get(keymap[j+1])) zx_key_up(zx,keymap[j+1]);
                    if (!put_down_get(keymap[j+2])) zx_key_up(zx,keymap[j+2]);
                }
            } else {
                // Extended key maps: two Pico pins -> one Spectrum key.
                if (get_device_button(keymap[j]&0x7f) &&
                    get_device_button(keymap[j+1]))
                {
                    put_down_set(keymap[j+2]);
                    zx_key_down(zx,keymap[j+2]);
                    return; // Return ASAP before processing normal keys.
                } else {
                    if (!put_down_get(keymap[j+2])) zx_key_up(zx,keymap[j+2]);
                }
            }
        }
    }

    // Detect long press of left+right to return back in
    // game selection mode.
    {
        #define LEFT_RIGHT_LONG_PRESS_FRAMES 30
        static int left_right_frames = 0;
        if (get_device_button(KEY_LEFT) && get_device_button(KEY_RIGHT)) {
            left_right_frames++;
            if (left_right_frames == LEFT_RIGHT_LONG_PRESS_FRAMES)
                EMU.menu_active = 1;
        } else {
            left_right_frames = 0;
        }
    }
}

// Clear all keys. Useful when we switch game, to make sure that no
// key downs are left from the previous keymap.
void flush_zx_key_press(zx_t *zx) {
    for (int j = 0; j < KBD_MAX_KEYS; j++) zx_key_up(zx,j);
}

// Initialize the Pico and the Spectrum emulator.
void init_emulator(void) {
    // Set default configuration.
    EMU.debug = 0;
    EMU.menu_active = 1;
    EMU.base_clock = 280000;
    EMU.emu_clock = 400000;
    EMU.tick = 0;
    EMU.current_keymap = keymap_default;
    EMU.current_game = 0;
    EMU.show_border = 1;
    EMU.scaling = 100;
    ui_reset_crop_area();

    // Pico Init
    stdio_init_all();

    // Display initialization. Show a pattern before overclocking.
    // If users are stuck with four colored squares we know what's up.
    st77xx_init();
    st77xx_fill_box(0,0,40,40,st77xx_rgb565(255,0,0));
    st77xx_fill_box(st77_width-41,0,40,40,st77xx_rgb565(0,255,0));
    st77xx_fill_box(0,st77_height-41,40,40,st77xx_rgb565(0,0,255));
    st77xx_fill_box(st77_width-41,st77_height-41,40,40,st77xx_rgb565(50,50,50));

    // Overclocking
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    set_sys_clock_khz(EMU.base_clock, false);

    // Keys pin initialization
    gpio_init(KEY_LEFT);
    gpio_init(KEY_RIGHT);
    gpio_init(KEY_UP);
    gpio_init(KEY_DOWN);
    gpio_init(KEY_FIRE);
    gpio_set_dir_in_masked(
        (1<<KEY_LEFT) | (1<<KEY_RIGHT) | (1<<KEY_UP) | (1<<KEY_DOWN) |
        (1<<KEY_FIRE));

    // Enter special mode depending on key presses during power up.
    if (get_device_button(KEY_LEFT)) EMU.debug = 1; // Debugging mode.
    if (get_device_button(KEY_RIGHT)) EMU.emu_clock = 300000; // Less overclock.

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
    struct game_entry *g = &GamesTable[game_id];
    chips_range_t r = {.ptr=g->addr, .size=g->size};
    flush_zx_key_press(&EMU.zx); // Make sure no keys are down.
    EMU.current_keymap = g->map;
    EMU.tick = 0;
    zx_quickload(&EMU.zx, r);
}

int main() {
    init_emulator();
    st77xx_fill(0);
    load_game(EMU.current_game);

    while (true) {
        absolute_time_t start, end;

        // Handle key presses on the phisical device. Either translate
        // them to Spectrum keypresses, or if the user interface is
        // active, pass it to the UI handler.
        if (EMU.menu_active) {
            ui_handle_key_press();
        }

        // If the game selection menu is active or just dismissed, we
        // just handle automatic keypresses.
        int kflags = HANDLE_KEYPRESS_ALL;
        if (EMU.menu_active || EMU.tick < EMU.menu_left_at_tick+10)
            kflags = HANDLE_KEYPRESS_MACRO;
        handle_zx_key_press(&EMU.zx, EMU.current_keymap, EMU.tick, kflags);

        // Run the Spectrum VM for a few ticks.
        set_sys_clock_khz(EMU.emu_clock, false); sleep_us(50);
        start = get_absolute_time();
        zx_exec(&EMU.zx, FRAME_USEC);
        end = get_absolute_time();
        printf("zx_exec(): %llu us\n",(unsigned long long)end-start);
        set_sys_clock_khz(EMU.base_clock, false); sleep_us(50);

        // Handle the menu.
        if (EMU.menu_active) {
            ui_draw_menu();
        }

        // In debug mode, show the frame number. Useful in order to
        // find the right timing for automatic key presses.
        if (EMU.debug) {
            char buf[32];
            snprintf(buf,sizeof(buf),"%d",(int)EMU.tick);
            ui_draw_string(10,10,buf,3,2);
        }

        // Update the display with the current CRT image.
        start = get_absolute_time();
        update_display(EMU.scaling,EMU.show_border);
        end = get_absolute_time();
        printf("update_display(): %llu us\n",(unsigned long long)end-start);
        printf("scanline_y: %d\n",EMU.zx.scanline_y);

        EMU.tick++;
    }
}
