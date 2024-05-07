/* Copyright (C) 2024 Salvatore Sanfilippo -- All Rights Reserved.
 * This code is released under the MIT license.
 * See the LICENSE file for more info. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/vreg.h"

#include "device_config.h" // Hardware-specific defines for ST77 and keys.
#include "st77xx.h"

// VRAM update tracking function, this is used inside mem.h.
inline void vram_set_dirty_bitmap(uint16_t addr);
inline void vram_set_dirty_attr(uint16_t addr);
inline void vram_force_dirty(void);

#define CHIPS_IMPL
#include "chips_common.h"
#include "mem.h"
#include "z80.h"
#include "kbd.h"
#include "clk.h"
#include "zx.h"
#include "zx-roms.h"

#define DEBUG_MODE 1
#define ZX_DEFAULT_SCANLINE_PERIOD 150

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

struct game_entry {
    char name[8];           // Z80 snapshot filename, first chars.
    void *addr;             // Address in the flash memory.
    uint32_t size;          // Length in bytes.
};

// These are populate during initialization, by scanning the
// flash memory for games images.
struct game_entry *GamesTable;
uint32_t GamesTableSize;

/* ============================== Keymap defines ============================ */

// Kempston joystick key codes: note that this were redefined compared to
// the original zx.h file.
#define KEMPSTONE_FIRE 0xff
#define KEMPSTONE_LEFT 0xfe
#define KEMPSTONE_RIGHT 0xfd
#define KEMPSTONE_DOWN 0xfc
#define KEMPSTONE_UP 0xfb

// "Virtual" pins.
//
// PRESS_AT_TICK is specified when we want a key to be pressed
// after the game starts, when a specific tick (frame) is reached.
// This is often useful in order to select the joystick or for
// similar tasks.
//
// Just specify PRESS_AT_TICK as pin, then the frame number, and
// finally the key.
#define PRESS_AT_TICK   0xfe // Press at the specified frame.
#define RELEASE_AT_TICK 0xfd // Release at the specified frame.
#define KEY_END         0xff // This just marks the end of the key map.

// Extended keymaps allow two device buttons (pins) pressed together to map
// to other Specturm keys. This is useful for games such as Skool Daze
// that have too many keys doing useful things.
// 
// To use this kind of maps, xor KEY_EXT to the first pin, then
// provide as second entry in the row the second pin, and finally
// a single Spectrum key code to trigger.
// 
// IMPORTANT: the extended key maps of a game must be the initial entries,
// before the normal entries. This way we avoid also sensing the keys
// mapped to the single buttons involved.
#define KEY_EXT         0x80

/* ========================== Global state and defines ====================== */

// Don't trust this USEC figure here, since the z80.h file implementation
// is modified to glue together the instruction fetch steps, so we do
// more work per tick.
#define FRAME_USEC (25000)

struct emustate {
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
    uint8_t keymap[3*100];     // 100 map entries... more than enough.
    char *keymap_file;         // Pointer of the keymap description file
                               // inside the flash memory.

    // Is the game selection / config menu shown?
    int menu_active;
    uint32_t menu_left_at_tick; // EMU.tick when the menu was closed.
    int selected_game;          // Game index of currently selected game in
                                // the UI. If less than 0 a settings item is
                                // selected instead.
    int loaded_game;            // Game index of the game currently loaded.
    uint32_t show_border;       // If 0, Spectrum border is not drawn.
    uint32_t scaling;           // Spectrum -> display scaling factor.
    uint32_t brightness;        // Display brightness.

    // Audio related
    uint32_t volume;            // Audio volume. Controls PWM value.
    volatile uint32_t audio_sample_wait; // Wait time (in busy loop cycles)
                                         // between samples when playing back.

    // All our UI graphic primitives are automatically cropped
    // to the area selected by ui_set_crop_area().
    uint16_t ui_crop_x1, ui_crop_x2, ui_crop_y1, ui_crop_y2;

    uint8_t dirty_vram[24]; // Track rows that changed since last update.
    uint8_t last_update_border_color; // Track last border color to update the
                                      // screen border only if it changed.
} EMU;

/* ========================== Emulator user interface ======================= */

// Numerical parameters that it is possible to change using the
// user interface.

#define UI_EVENT_NONE 0
#define UI_EVENT_LOADGAME 1
#define UI_EVENT_CLOCK 2
#define UI_EVENT_BORDER 3
#define UI_EVENT_SCALING 4
#define UI_EVENT_VOLUME 5
#define UI_EVENT_SYNC 6
#define UI_EVENT_BRIGHTNESS 7
#define UI_EVENT_DISMISS 255

const uint32_t SettingsZoomValues[] = {50,75,84,100,112,125,150,200};
const char *SettingsZoomValuesNames[] = {"50%","75%","84%","100%","112%","125%","150%","200%",NULL};
struct UISettingsItem {
    uint32_t event;     // Event reported if setting is changed.
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
    {UI_EVENT_CLOCK,
        "clock", &EMU.emu_clock, 5000, 130000, 600000, NULL, NULL},
    {UI_EVENT_BORDER,
        "border", &EMU.show_border, 1, 0, 1, NULL, NULL},
    {UI_EVENT_SCALING,
        "scaling", &EMU.scaling, 0, 0, 0,
        SettingsZoomValues, SettingsZoomValuesNames,
    },
    {UI_EVENT_VOLUME,
        "volume", &EMU.volume, 1, 0, 20, NULL, NULL},
    {UI_EVENT_BRIGHTNESS,
        "bright", &EMU.brightness, 1, 0, ST77_MAX_BRIGHTNESS, NULL, NULL},
    {UI_EVENT_SYNC,
        "sync",(uint32_t*)&EMU.audio_sample_wait, 5, 0, 1000, NULL, NULL},
    {UI_EVENT_NONE,
        "scan-p", (uint32_t*)&EMU.zx.scanline_period, 1, 10, 500, NULL, NULL}
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
uint32_t settings_change_value(int id, int dir) {
    struct UISettingsItem *si = SettingsList+id;
    if (si->values == NULL) {
        if (si->ptr[0] == si->min && dir == -1) return UI_EVENT_NONE;
        else if (si->ptr[0] == si->max && dir == 1) return UI_EVENT_NONE;
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
        
        if (j == 0 && dir == -1) return UI_EVENT_NONE;
        if (si->values_names[j+1] == NULL && dir == 1) return UI_EVENT_NONE;
        j += dir;
        si->ptr[0] = si->values[j];
    }
    return si->event;
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
    uint8_t *vmem = EMU.zx.ram[EMU.zx.display_ram_bank];

    for (int py = y; py <= y2; py++) {
        for (int px = x; px <= x2; px++) {
            // Don't draw outside the current mask.
            if (px < EMU.ui_crop_x1 ||
                px > EMU.ui_crop_x2 ||
                py < EMU.ui_crop_y1 ||
                py > EMU.ui_crop_y2) continue;

            // Border or inside?
            uint8_t c = (px==x || px==x2 || py==y || py==y2) ? bcolor : color;

            // Write directly into the Spectrum VMEM
            if (px >= 256 || py >= 192) continue; // VMEM limits.
            uint32_t vbyte = ((py & 0xC0)<<5) | ((py & 0x07)<<8) |
                             ((py & 0x38)<<2) | (px>>3);
            uint32_t bit = 1<<(7-(px&7));
            vmem[vbyte] = vmem[vbyte] & (0xff^bit);
            if (c != 0) vmem[vbyte] |= bit;
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
void ui_go_next_prev_game(int dir) {
    EMU.selected_game += dir;
    if (EMU.selected_game == -SettingsListLen-1) {
        EMU.selected_game = GamesTableSize-1;
    } else if (EMU.selected_game == GamesTableSize) {
        EMU.selected_game = -SettingsListLen;
    }
}

// Called when the UI is active. Handle the key presses needed to select
// the game and change the overclock.
//
// Returns 1 is if some event was processed. Otherwise 0.
#define UI_DEBOUNCING_TIME 100000
uint32_t ui_handle_key_press(void) {
    const uint8_t km[] = {
        KEY_LEFT, 0, KEMPSTONE_LEFT,
        KEY_RIGHT, 0, KEMPSTONE_RIGHT,
        KEY_FIRE, 0, KEMPSTONE_FIRE,
        KEY_DOWN, 0, KEMPSTONE_DOWN,
        KEY_UP, 0, KEMPSTONE_UP,
        KEY_END, 0, 0,
    };
    static absolute_time_t last_key_accepted_time = 0;

    // Debouncing
    absolute_time_t now = get_absolute_time();
    if (now - last_key_accepted_time < UI_DEBOUNCING_TIME) return 0;

    uint32_t event = UI_EVENT_NONE; // Event generated by key press, if any.
    int key_pressed = -1;
    for (int j = 0; ;j += 3) {
        if (km[j] == KEY_END) break;
        if (km[j] >= 32) continue; // Skip special codes.
        if (get_device_button(km[j])) {
            key_pressed = km[j+2];
            break;
        }
    }
    if (key_pressed == -1) return UI_EVENT_NONE; // No key pressed right now.

    int value_change_dir = -1;
    switch(key_pressed) {
    case KEMPSTONE_UP: ui_go_next_prev_game(-1); break;
    case KEMPSTONE_DOWN: ui_go_next_prev_game(1); break;
    case KEMPSTONE_RIGHT: value_change_dir = 1; // fall through.
    case KEMPSTONE_LEFT:
        if (EMU.selected_game < 0)
            event = settings_change_value(-EMU.selected_game-1,
                                          value_change_dir);
        break;
    case KEMPSTONE_FIRE:
        if (EMU.selected_game == EMU.loaded_game) {
            // We are forced to reload the game, because the UI
            // messed with the Spectrum video RAM.
            load_game(EMU.selected_game);
            EMU.menu_active = 0;
            EMU.menu_left_at_tick = EMU.tick;
            event = UI_EVENT_DISMISS;
        } else if (EMU.selected_game >= 0) {
            load_game(EMU.selected_game);
            event = UI_EVENT_LOADGAME;
        }
        break;
    }
    last_key_accepted_time = now;
    return event;
}

// Before writing our user interfafce inside the Spectrum video RAM, we need
// to make sure that the colors in the area we will be using will make the
// menu visible.
void ui_set_area_attributes(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint8_t *vmem = EMU.zx.ram[EMU.zx.display_ram_bank];
    for (uint16_t y = y1; y <= y2 && y < 192; y += 8) {
        for (uint16_t x = x1; x <= x2 && x < 256; x += 8) {
            vmem[0x1800+(((y>>3)<<5)|(x>>3))] = 7;
        }
    }
}

// If the menu is active, draw it.
void ui_draw_menu(void) {
    // Draw the menu in the right / top part of the screen.
    int menu_x = 255-100;
    int menu_w = 100;
    int menu_y = 0;
    int menu_h = 192/3*2;
    int font_size = 2;
    menu_h -= menu_h&(8*font_size-1); // Make multiple of font pixel size;
    int vpad = 2;       // Vertical padding of text inside the box.
    menu_h += vpad*2;   // Allow for pixels padding / top bottom.
    menu_h &= ~0x7;     // Make multiple of 8 to avoid color clash.

    ui_fill_box(menu_x, menu_y, menu_w, menu_h, 0, 15);
    ui_set_crop_area(menu_x+1,menu_x+menu_w-2,
                     menu_y+1,menu_y+menu_h-2);

    int first_game = (int)EMU.selected_game - 5;
    int num_settings = (int)SettingsListLen;
    if (first_game < -num_settings) first_game = -num_settings;

    // Make the menu visible.
    ui_set_area_attributes(menu_x,menu_y,menu_x+menu_w-1,menu_y+menu_h-1);

    int y = menu_y+vpad; // Incremented as we write text.
    for (int j = first_game;; j++) {
        if (j >= (int)GamesTableSize || y > menu_y+menu_h) break;

        int color = j >= 0 ? 4 : 6;
        font_size = j >= 0 ? 2 : 1;

        // Highlight the currently selected game, with a box of the color
        // of the font, and the black font (so basically the font is inverted).
        if (j == EMU.selected_game) {
            ui_fill_box(menu_x+2,y,menu_w-2,font_size*8,color,color);
            color = 0;
        }
        if (j < 0) {
            // Show setting item.
            struct UISettingsItem *si = &SettingsList[-j-1];
            char sistr[32];
            settings_to_string(sistr,sizeof(sistr),-j-1);
            ui_draw_string(menu_x+2,y,sistr,color,font_size);
        } else {
            // Show game item.
            ui_draw_string(menu_x+2,y,GamesTable[j].name,color,font_size);
        }
        y += 8*font_size;
    }
    ui_reset_crop_area();
    vram_force_dirty();
}

/* =========================== Emulator implementation ====================== */

// Set a bitmap signaling which part of the screen RAM was touched and
// is yet to be updated on the display. This function is called when the
// address 'addr' is in the range of the VRAM bitmap area.
inline void vram_set_dirty_bitmap(uint16_t addr) {
    uint16_t y = ((addr&0x1800)>>5) | ((addr&0x700)>>8) | ((addr&0xe0)>>2);
    EMU.dirty_vram[y>>3] |= 1 << (y&7);
}

// Like vram_set_dirty_bitmap() but called for addresses in the range
// of the color attributes.
inline void vram_set_dirty_attr(uint16_t addr) {
    // Mark all the 8 rows affected in one operation.
    EMU.dirty_vram[((addr-0x5800)>>5) & 31] = 0xff;
}

inline void vram_reset_dirty(void) {
    memset(EMU.dirty_vram,0,sizeof(EMU.dirty_vram));
}

inline void vram_force_dirty(void) {
    memset(EMU.dirty_vram,0xff,sizeof(EMU.dirty_vram));
    EMU.last_update_border_color = 0xff; // Impossible color: update forced.
}

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
// Valid scaling arguments:
//
// 50: 50% downscaling
// 75: 75% downscaling
// 83: 83% downscaling
// 100 (or any other invalid value): no rescaling
// 112: 112% upscaling
// 125: 125% upscaling
// 150: 150% upscaling
// 200: 200% upscaling
//
// BORDERS:
// If border is false, borders are not drawn at all.
// Useful for small displays or when scaling is used.
void update_display(uint32_t scaling, uint32_t border, uint32_t blink) {
    uint16_t line[st77_width+8]={0}; // 8 pixels more allow us to overflow
                                     // when doing scaling, instead of checking
                                     // (which is costly). Hence width+8.

    // Get a pointer to the Spectrum video memory bank.
    const uint8_t *vmem = EMU.zx.ram[EMU.zx.display_ram_bank];

    // Configure scaling: we duplicate/skip a column/row every N cols/rows.
    uint32_t dup_mask = 0xffff; // no dup/skip.
    uint32_t dup = 1; // Duplicate or skip? Scaling > 100 dup, < 100 skip.
    switch (scaling) {
        // Downscaling
        case 50: dup_mask = 1; dup = 0; break;
        case 75: dup_mask = 3; dup = 0; break;
        case 84: dup_mask = 7; dup = 0; break;
 
        // Upscaling.
        case 112: dup_mask = 7; break;
        case 125: dup_mask = 3; break;
        case 150: dup_mask = 1; break;
        case 200: dup_mask = 0; break;

       // 100 or any other value: no scaling.
        case 100:
        default: scaling = 0; break;
    }

    // Center Spectrum framebuffer into Pico display and
    // set offset to remove borders if needed.
    uint32_t zx_height = ZX_DISPLAY_HEIGHT-64;
    uint32_t zx_width = ZX_DISPLAY_WIDTH-64;
    uint32_t v_border, h_border; // Vertical/horizontal borders in pixels.

    // Adjust virtual Spectrum framebuffer size by scaling.
    if (scaling) {
        if (dup) {
            // scaling > 100%
            zx_height = (zx_height*(dup_mask+2)) / (dup_mask+1);
            zx_width = (zx_width*(dup_mask+2)) / (dup_mask+1);
        } else {
            // scaling < 100%
            zx_height = (zx_height*(dup_mask+1)) / (dup_mask+2);
            zx_width = (zx_width*(dup_mask+1)) / (dup_mask+2);
        }
    }

    // Centering and borders area computation.
    uint32_t xx_start = 0; // Offsets into Spectrum video, for centering.
    uint32_t yy_start = 0;

    // To center, we start checking if our display screen size is smaller
    // than the scaled Spectrum display. Then, after we get the starting
    // x,y offset pixel in the Spectrum VMEM, to correctly center the image
    // we need to resize x/y offset proportionally to the scaling we are using.
    if (st77_width < zx_width) {
        xx_start += (zx_width-st77_width)>>2;

        // Scale offset to video memory actual size.
        xx_start = dup ? (xx_start*(dup_mask+1)) / (dup_mask+2) :
                         (xx_start*(dup_mask+2)) / (dup_mask+1);
        h_border = 0;
    } else {
        h_border = (st77_width-zx_width)>>1;
    }

    if (st77_height < zx_height) {
        yy_start += (zx_height-st77_height)>>1;

        // Scale offset to video memory actual size.
        yy_start = dup ? (yy_start*(dup_mask+1)) / (dup_mask+2) :
                         (yy_start*(dup_mask+2)) / (dup_mask+1);
        v_border = 0;
    } else {
        v_border = (st77_height-zx_height)>>1;
    }

    // Transfer data to the display.
    //
    // Note that we use xx and yy counters other than x and y since
    // we want to duplicate lines every N cols/rows when scaling is
    // used, and when this happens we advance x and y by a pixel more,
    // so we need counters relative to the Spectrum video, not the display.
    uint32_t yy = yy_start;
    const uint8_t *row;
    int update_border = EMU.zx.border_color != EMU.last_update_border_color;
    uint16_t border_color = zxpalette[EMU.zx.border_color];

    // If the border color changed, we need to force a full screen update.
    if (update_border && EMU.show_border) vram_force_dirty();

    for (uint32_t y = 0; y < st77_height; y++) {
        // Handle top / bottom border
        if (EMU.show_border && (y < v_border || yy >= 192)) {
            if (!update_border) continue;
            for (int j = 0; j < st77_width; j++) line[j] = border_color;
            st77xx_setwin(0, y, st77_width-1, y);
            st77xx_data(line,sizeof(line)-16);
            continue;
        } else {
            if (yy >= 192) break; // End of Spectrum bitmap reached.
        }

        // Seek the row in the Spectrum VMEM
        row = vmem + (((yy & 0xC0)<<5) | ((yy & 0x07)<<8) | ((yy & 0x38)<<2));
        uint32_t update_row = (EMU.dirty_vram[yy>>3] & (1<<(yy&7)));
        uint32_t xx = xx_start;

        // We increment x one whole byte at a time, and decode 8 pixels
        // for each iteration.
        uint16_t *l = line;
        for (uint32_t x = 0; x < st77_width && l < line+st77_width; x++) {
            if (EMU.show_border && x < h_border) {
                *l++ = border_color;
                continue;
            }

            uint32_t byte = xx>>3;
            uint8_t attr = vmem[0x1800+(((yy>>3)<<5)|byte)];
            uint16_t fg, bg, aux;

            bg = zxpalette[(attr>>3)&7];
            fg = zxpalette[(attr&7)];

            if (attr&0x80) { // Blink attribute.
                update_row = 1; // With blink we no longer know the state
                                // of the row. Tracking would likely not worth
                                // it.
                if (blink) {
                    aux = fg;
                    fg = bg;
                    bg = aux;
                }
            }
            for (int bit = 7; bit >= 0; bit--) {
                uint16_t pixel_color = (row[byte] & (1<<bit)) ? fg : bg;
                if (((xx+1)&dup_mask) == 0) {
                    if (dup) {
                        l[0] = pixel_color;
                        l[1] = pixel_color;
                        l++;
                    } else  {
                        l--;
                    }
                } else {
                    *l = pixel_color;
                }
                l++;
                xx++;
            }

            // Stop if we reach the end of Spectrum row.
            // Fill the rest with the border color.
            if (xx == 256) {
                while(l < line+st77_width) *l++ = border_color;
                break;
            }
        }

        if (((yy+1)&dup_mask) == 0) {
            // Duplicate/skip row according to scaling mask.
            if (dup) {
                if (update_row) {
                    st77xx_setwin(0, y, st77_width-1, y);
                    st77xx_data(line,sizeof(line)-16);
                }
                y++;
                if (update_row) {
                    st77xx_setwin(0, y, st77_width-1, y);
                    st77xx_data(line,sizeof(line)-16);
                }
            } else {
                y--;    // Skip row.
            }
        } else {
            // If scaling does not affect this line, just
            // write it to the display.
            if (update_row) {
                st77xx_setwin(0, y, st77_width-1, y);
                st77xx_data(line,sizeof(line)-16);
            }
        }

        yy++; // Next row.
    }
    for (int j = 0; j < 24; j++)
        printf("D[%d]: %02x\n", j, EMU.dirty_vram[j]);
    printf("\n");

    vram_reset_dirty();
    EMU.last_update_border_color = EMU.zx.border_color;
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
    uint64_t put_down[4] = {0,0};
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
                printf("Pressing '%c' at frame %d\n",keymap[j+2],ticks);
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
                    if (keymap[j+1]) {
                        put_down_set(keymap[j+1]);
                        zx_key_down(zx,keymap[j+1]);
                    }
                    if (keymap[j+2]) {
                        put_down_set(keymap[j+2]);
                        zx_key_down(zx,keymap[j+2]);
                    }
                } else {
                    // Release.
                    if (!put_down_get(keymap[j+1]) && keymap[j+1])
                        zx_key_up(zx,keymap[j+1]);
                    if (!put_down_get(keymap[j+2]) && keymap[j+2])
                        zx_key_up(zx,keymap[j+2]);
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

// Set the audio volume by altering the PWM counter wrap value.
// The zx.h file will always set the channel level to 1 or 0
// (Z80 audio pin high or low), so the greater the counter value
// the smaller the volume.
void set_volume(uint32_t volume) {
    unsigned int slice_num = pwm_gpio_to_slice_num(SPEAKER_PIN);
    // Volume is in the range 0-20, however the greater the volume
    // the smaller our wrap value should be in order to increase the
    // total duty time.
    pwm_set_wrap(slice_num, 20-volume);
    pwm_set_enabled(slice_num, volume != 0);
}

// Called at startup to seek the games snapshots (Z80 files) and populate
// the games table. Return true if games were found, otherwise zero
// is returned, and the caller knows that the user failed to load games.
int populate_games_list(void) {
    char marker[] = "..2040GAMESBLOB!";
    // Fix the marker. This way the marker string is not
    // part of the program itself, and when we scan the flash
    // looking for games we are sure we find the games and not
    // the program code.
    marker[0] = 'Z';
    marker[1] = 'X';
   
    // Search for games. We aspect the games snapshots to be stored at
    // the start of any page.
    printf("Start scanning...\n");
    uint8_t *p = NULL;
    for (uint32_t offset = 0; offset < 1024*2048; offset += 4096) {
        p = (uint8_t*)(0x10000000|offset);
        if (!memcmp(marker,p,16)) {
            p += 16; // Skip marker.
            printf("Games snapshots found at %p\n", p);
            break;
        }
        p = NULL; // Signal no found.
    }
    if (!p) return 0; // No games in the flash, or wrong alignment.

    // Populate our games table. To start, check the size: the Pico supports
    // realloc() but if we alloc in one pass for the right size, we avoid
    // memory fragmentation and other issues.
    printf("Loading...\n");
    uint8_t *games_table_start = p;
    GamesTableSize = 0;
    while (*p != 0) {
        uint8_t namelen = *p;
        p += 1+namelen;
        uint32_t datalen;
        memcpy(&datalen,p,4);
        p += 4+datalen;
        GamesTableSize++;
    }

    EMU.keymap_file = p+1; // The keymap is at the end of the games snapshots.

    // Now allocate and fill the game table.
    p = games_table_start;
    struct game_entry *ge;
    GamesTable = malloc(sizeof(*ge) * GamesTableSize);
    for (int j = 0; j < GamesTableSize; j++) {
        ge = &GamesTable[j];
        uint8_t namelen = *p;
        uint8_t copylen = namelen;
        if (copylen > sizeof(ge->name)) copylen = sizeof(ge->name)-1;
        p++; // Seek name.
        memcpy(ge->name,p,namelen);
        ge->name[namelen] = 0; // Null term.
        p += namelen; // Skip name
        memcpy(&ge->size,p,4);
        p += 4;
        ge->addr = p;
        p += ge->size;
    }
    return 1;
}

// Initialize the Pico and the Spectrum emulator.
void init_emulator(void) {
    // Set default configuration.
    EMU.debug = 0;
    EMU.menu_active = 0;
    EMU.base_clock = 280000;
    EMU.emu_clock = 400000;
    EMU.tick = 0;
    EMU.keymap[0] = KEY_END; // No keymap at startup. Will be loaded later.
    EMU.keymap_file = NULL;  // Set by populate_games_list().
    EMU.selected_game = 0;
    EMU.show_border = DEFAULT_DISPLAY_BORDERS;
    EMU.scaling = DEFAULT_DISPLAY_SCALING;
    EMU.volume = 20; // 0 to 20 valid values.
    EMU.brightness = ST77_MAX_BRIGHTNESS;
    EMU.audio_sample_wait = 300; // Adjusted dynamically.
    vram_force_dirty(); // Fully update the first frame.
    ui_reset_crop_area();

    // Pico Init
    stdio_init_all();

    // Display initialization. Show a pattern before overclocking.
    // If users are stuck with four colored squares we know what's up.
    st77xx_init();
    st77xx_set_brightness(ST77_MAX_BRIGHTNESS); // Start at max.
    st77xx_fill_box(0,0,40,40,st77xx_rgb565(255,0,0));
    st77xx_fill_box(st77_width-41,0,40,40,st77xx_rgb565(0,255,0));
    st77xx_fill_box(0,st77_height-41,40,40,st77xx_rgb565(0,0,255));
    st77xx_fill_box(st77_width-41,st77_height-41,40,40,st77xx_rgb565(50,50,50));
    st77xx_set_brightness(EMU.brightness); // Go to the default.

    // Overclocking. We start at base clock (low enough to access the
    // flash). After loading the games list from the flash, we go
    // at full speed.
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    set_sys_clock_khz(EMU.base_clock, false); sleep_us(50);

    // Keys pin initialization.
    gpio_init(KEY_LEFT);
    gpio_init(KEY_RIGHT);
    gpio_init(KEY_UP);
    gpio_init(KEY_DOWN);
    gpio_init(KEY_FIRE);
    gpio_set_dir_in_masked(
        (1<<KEY_LEFT) | (1<<KEY_RIGHT) | (1<<KEY_UP) | (1<<KEY_DOWN) |
        (1<<KEY_FIRE));

    // Configure audio pin PWM.
    if (SPEAKER_PIN != -1) {
        gpio_set_function(SPEAKER_PIN, GPIO_FUNC_PWM);
        unsigned int slice_num = pwm_gpio_to_slice_num(SPEAKER_PIN);
        unsigned int pwm_channel = pwm_gpio_to_channel(SPEAKER_PIN);
        set_volume(EMU.volume);
        pwm_set_chan_level(slice_num, pwm_channel, 0);
        pwm_set_enabled(slice_num, true);
    }

    // Convert palette to RGB565.
    for (int j = 0; j < 16; j++)
        zxpalette[j] = palette_to_565(zxpalette[j]);

    // ZX emulator Init.
    zx_desc_t zx_desc = {0};
    zx_desc.type = ZX_TYPE_48K;
    zx_desc.joystick_type = ZX_JOYSTICKTYPE_KEMPSTON;
    zx_desc.roms.zx48k.ptr = dump_amstrad_zx48k_bin;
    zx_desc.roms.zx48k.size = sizeof(dump_amstrad_zx48k_bin);
    zx_init(&EMU.zx, &zx_desc);
    EMU.zx.scanline_period = ZX_DEFAULT_SCANLINE_PERIOD;

    // Enter special mode depending on key presses during power up.
    if (get_device_button(KEY_LEFT)) EMU.debug = 1; // Debugging mode.
    if (get_device_button(KEY_RIGHT)) EMU.emu_clock = 300000; // Less overclock.
}

// Parse line and fill current map.
// This function turns a description in the following format into the
// three (or six) bytes entry representing one mapping in the keymap.
//
// Cases we need to handle:
//
// 1. xul2   Extended keymap (x). ul (up+left) = keypress of 2
// 2. lx     Standard keymap with one key press. l (left) = press x
// 3. rz2    Standard keymap with two presses: r(right) = press z and 2
// 4. l1|l   Kempstone keypresses are |<dir>, |lrud or |f for fire.
// 5. @10:k1 Auto keypresses: at frame 10, press k for 1 frame.
//
// Note that the case "5" will actually write 6 bytes, one for the press
// and one for the release event.
//
// Return the number of bytes filled (frame entries will fill six
// bytes instead of three), or 0 on syntax error.
int keymap_descr_to_row(char *p, uint8_t *map) {
    char buf[16]; // An entry like @1000:|f100 is still just 12 bytes.

    // Let's work on a copy of the entry, stripping everything on the
    // right so that we just have the map line itself.
    int idx = 0;
    while(*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r'
          && idx < (int)sizeof(buf)-1)
    {
        buf[idx++] = *p++;
    }
    buf[idx] = 0;
    
    // We have to set three bytes in total, so do the conversion
    // in three exact steps.
    int ext = 0; // Ext map if true (two buttons + one key).
    int atframe = 0; // Special automatic keypress at frame entry if true.
    int pos = 0; // Position inside the keymap line.
    for (int j = 0; j < 3; j++) {
        // Handle special conditions: extended and atframe entry.
        if (j == 0) {
            if (ext == 0 && buf[0] == 'x') {
                ext = 1;
                pos++;
            } else if (atframe == 0 && buf[0] == '@') {
                atframe = 1;
                map[0] = PRESS_AT_TICK;
                map[3] = RELEASE_AT_TICK;
                pos++;
            }
        }

        if (j == 1 && atframe) {
            // Read the at-frame frame.
            char *frame = buf+pos;
            while (buf[pos] && buf[pos] != ':')
                pos++;
            if (buf[pos] == 0) return 0; // Syntax error.
            buf[pos] = 0; pos++; // Pos points to key to press.
            map[j] = atoi(frame);
        } else if ((j == 0 && !atframe) || (j == 1 && ext)) {
            // We read the button pin if:
            //
            // First keymap byte and is not at-frame entry.
            // Second keymap byte and it is an extended map (has two pins).
            uint8_t pin;
            switch(buf[pos]) {
            case 'l': pin = KEY_LEFT; break;
            case 'r': pin = KEY_RIGHT; break;
            case 'u': pin = KEY_UP; break;
            case 'd': pin = KEY_DOWN; break;
            case 'f': pin = KEY_FIRE; break;
            default: return 0; // Syntax error.
            }
            if (ext && j == 0) pin |= KEY_EXT;
            map[j] = pin;
            pos++;
        } else if (j == 2 || (j == 1 && !ext)) {
            // Read the mapped Spectrum button or joystick move.
            // Note that normal entries map to up to two buttons.
            // The third entry is always a button press.
            // The second entry is a button press if it's not an extended map.

            // Normal maps (one pin, two buttons) may just have a single
            // button. Stop here if that's the case.
            if (j == 2 && buf[pos] == 0) break;

            if (buf[pos] == '|') {
                // Kempstone moves are prefixed by |
                pos++;
                switch(buf[pos]) {
                case 'l': map[j] = KEMPSTONE_LEFT; break;
                case 'r': map[j] = KEMPSTONE_RIGHT; break;
                case 'u': map[j] = KEMPSTONE_UP; break;
                case 'd': map[j] = KEMPSTONE_DOWN; break;
                case 'f': map[j] = KEMPSTONE_FIRE; break;
                default: return 0; // Syntax error.
                }
            } else {
                // Normal keypress. Just take the byte given by the user.
                if (buf[pos] == '~') buf[pos] = ' ';
                map[j] = buf[pos];
            }
            pos++;
            if (atframe) {
                // Populate the release entry too.
                map[5] = buf[2]; // Key to release is the same.
                map[4] = map[1] + atoi(buf+pos); // Release frame.
            }
        }
    }
    return atframe ? 6 : 3;
}

/* This function will parse the keymap file stored in the flash memory
 * (so it must be called with base overclocking, in order to read
 * from flash) and will try to match every entry with the current RAM
 * content in order to find a suitable keymap. */
void get_keymap_for_current_game(int game_id) {
    int got_match = 0; // State telling we got a match with RAM content.
    uint8_t *map = EMU.keymap;
    char *p = EMU.keymap_file;

    int line = 1; // Line number inside keymap file.
    while(1) {
        // Discard comments and empty lines.
        if (*p == '#' || *p == '\r' || *p == '\n' || *p == ' ' || *p == '\t') {
            // If we have a match, a space means our map ended. In this case
            // return.
            if (got_match) {
                *map = KEY_END; // Terminate the map.
                // Log the loaded keymap: useful when things don't
                // work as expected.
                map = EMU.keymap;
                for (int j = 0; j < sizeof(EMU.keymap)/3; j++) {
                    if (map[j*3] == KEY_END) break;
                    printf("map entry %d: %d %d %d\n", j,
                        map[j*3], map[j*3+1], map[j*3+2]);
                }
                return;
            }

            // Otherwise, discard.
            goto next_line;
        }

        // New map. Let's see if we match with it.
        if ((!memcmp(p,"MATCH:",6) && !got_match) ||
            (!memcmp(p,"AND-MATCH",9) && got_match))
        {
            char *end = strchr(p,'\n');
            if (*(end-1) == '\r') end--;
            p = strchr(p,':'); p++;
            unsigned int pattern_len = (end-p);

            // Scan the Spectrum memory for a match.
            int found = 0;
            uint8_t *ram = (uint8_t*)EMU.zx.ram;
            for (uint32_t j = 0; j < 49152-pattern_len; j++) {
                if (ram[j] == p[0] && !memcmp(ram+j,p,pattern_len)) {
                    found = 1;
                    break;
                }
            }

            got_match = found != 0;
            goto next_line;
        }

        // If we reach the default map, just load it.
        if (!memcmp(p,"DEFAULT:",8)) {
            got_match = 1;
            goto next_line;
        }

        // If we don't have a match, skip this map description line.
        if (got_match == 0) goto next_line;

        if (map == EMU.keymap) printf("Loading keymap at line %d\n", line);

        // Keymaps may redefine scanline period for games with special
        // requirements about the CRT timing.
        if (!memcmp(p,"SCANLINE-PERIOD:",16)) {
            int period = atoi(p+16);
            if (period > 0 && period < 500 && EMU.loaded_game != game_id)
                EMU.zx.scanline_period = period;
            goto next_line;
        }

        // Turn this line into three bytes map entry using an helper
        // function.
        int used_bytes;
        used_bytes = keymap_descr_to_row(p,map);
        if (used_bytes == 0) {
            int len = strchr(p,'\n') - p;
            printf("Keymap syntax error at line %d: %.*s\n",line,len,p);
            *map = KEY_END;
            return;
        }
        map += used_bytes; // Go to the next entry;

        if (map > EMU.keymap+sizeof(EMU.keymap)-3) {
            // It is unlikely that a keymap is too long, but let's handle
            // it in case of bugs. The -3 above is there because there are
            // maps that can use two entries (6 bytes instead of 3 bytes).
            map -= 3;
            *map = KEY_END;
            return;
        }

next_line:
        line++;
        if (!memcmp(p,"#END",4)) {
            printf("No default keymap found\n");
            return; // Stop on end of file.
        }
        p = strchr(p,'\n');
        p++;
    }
}

/* Load the specified game ID. The ID is just the index in the
 * games table. As a side effect, sets the keymap. */
void load_game(int game_id) {
    set_sys_clock_khz(EMU.base_clock, false); sleep_us(50);
    struct game_entry *g = &GamesTable[game_id];
    chips_range_t r = {.ptr=g->addr, .size=g->size};
    flush_zx_key_press(&EMU.zx); // Make sure no keys are down.
    EMU.tick = 0;

    // We update the screen from the video memory. Moreover we have Z80
    // clock-related changes. Sometimes games don't have enough time from
    // the vblank interrupt to refresh the screen. Other times they have
    // too much time and start deleting the sprites at the old positions...
    // Here we handle such exceptions.
    //
    // So here we set this default, but the keymap file may change
    // the scanline period for game-specific tweaks using the
    // SCANLINE-PERIOD: directive.
    if (EMU.loaded_game != game_id)
        EMU.zx.scanline_period = ZX_DEFAULT_SCANLINE_PERIOD;

    // Load game and matching keymap (if any)
    zx_quickload(&EMU.zx, r);
    get_keymap_for_current_game(game_id);

    EMU.loaded_game = game_id;
    set_sys_clock_khz(EMU.emu_clock, false); sleep_us(50);
    vram_force_dirty(); // Fully update the screen: we loaded a different
                        // video content.
}

// This thread takes audio data from the main thread emulator context
// and reproduces it on the sound pin.
void core1_play_audio(void) {
    absolute_time_t start, end;
    unsigned int slice_num = pwm_gpio_to_slice_num(SPEAKER_PIN);
    unsigned int pwm_channel = pwm_gpio_to_channel(SPEAKER_PIN);

    // The length of the pause may need to be adjusted when
    // compiling with different compilers. Would be better to
    // write the pause loop later in assembly.
    //
    // Also this pause depends on the sampling rate we do
    // in zx.h.
    int oldlevel = 0;

    while(1) {
        // Wait for new buffer chunk to be available.
        start = get_absolute_time();
        while(EMU.zx.audiobuf_notify == 0);
        end = get_absolute_time();
        if (EMU.debug)
            printf("[playback] waiting %llu [%u]\n",
                end-start, EMU.zx.audiobuf_notify);
        if (end-start == 0) EMU.audio_sample_wait--;
        else if (end-start > 1000) EMU.audio_sample_wait++;

        // Seek the right part of the buffer. We use double buffering
        // splitting the buffer in two. This is needed as memcpy()-ing
        // to our buffer is already a substantian delay, and would not
        // require less memory.
        uint32_t *buf = EMU.zx.audiobuf;
        buf += (EMU.zx.audiobuf_notify-1)*(AUDIOBUF_LEN/2);
        EMU.zx.audiobuf_notify = 0; // Clear notification flag.

        // Play samples.
        start = get_absolute_time();
        for (uint32_t byte = 0; byte < AUDIOBUF_LEN/2; byte++) {
            for (uint32_t bit = 0; bit < 32; bit++) {
                int level = (buf[byte] & (1<<bit)) >> bit;
                if (level != oldlevel) {
                    pwm_set_chan_level(slice_num, pwm_channel, level);
                    oldlevel = level;
                }

                // Wait some time.
                for (volatile int k = 0; k < EMU.audio_sample_wait; k++);

                // Stop if there was a buffer overrun. Very unlikely.
                // if (EMU.zx.audiobuf_notify != 0) goto stoploop;
            }
        }
        stoploop:
        end = get_absolute_time();
        if (EMU.debug)
            printf("[playback] with pause=%u playing took %llu [notify:%u]\n",
                EMU.audio_sample_wait, end-start, EMU.zx.audiobuf_notify);
    }
}

int main() {
    init_emulator();
    st77xx_fill(0);

    // If we fail to populate the game list, the user failed to flash
    // the games image in the flash. Let's make they aware.
    if (populate_games_list() == 0) {
        int frame = 0;
        while(1) {
            frame++;
            zx_exec(&EMU.zx, FRAME_USEC);
            if (frame > 30) {
                ui_draw_string(0,0,"PLEASE LOAD",1,2);
                ui_draw_string(0,20,"GAMES SNAPSHOTS",1,2);
                ui_draw_string(0,40,"CHECK README :)",1,2);
            }
            update_display(EMU.scaling,EMU.show_border,0);
            printf("PLEASE LOAD GAMES SNAPSHOTS\n");
        }
    }

    // Go to full speed and load the first game in the list.
    set_sys_clock_khz(EMU.emu_clock, false); sleep_us(50);
    load_game(EMU.selected_game);

    // Start the audio thread.
    if (SPEAKER_PIN != -1) multicore_launch_core1(core1_play_audio);

    // Our emulation main loop.
    uint32_t blink = 0;
    while (true) {
        absolute_time_t start, zx_exec_time, update_time;

        // Handle key presses on the phisical device. Either translate
        // them to Spectrum keypresses, or if the user interface is
        // active, pass it to the UI handler.
        if (EMU.menu_active) {
            uint32_t ui_event = ui_handle_key_press();
            switch(ui_event) {
            case UI_EVENT_VOLUME:
                set_volume(EMU.volume);
                break;
            case UI_EVENT_BRIGHTNESS:
                st77xx_set_brightness(EMU.brightness);
                break;
            case UI_EVENT_SCALING:
                vram_force_dirty();
                st77xx_fill(0);
                break;
            case UI_EVENT_BORDER:
                vram_force_dirty();
                break;
            case UI_EVENT_CLOCK:
                set_sys_clock_khz(EMU.emu_clock, false);
                break;
            }
        }

        // If the game selection menu is active or just dismissed, we
        // just handle automatic keypresses.
        int kflags = HANDLE_KEYPRESS_ALL;
        if (EMU.menu_active || EMU.tick < EMU.menu_left_at_tick+10)
            kflags = HANDLE_KEYPRESS_MACRO;
        handle_zx_key_press(&EMU.zx, EMU.keymap, EMU.tick, kflags);

        // Run the Spectrum VM for a few ticks.
        start = get_absolute_time();
        zx_exec(&EMU.zx, FRAME_USEC);
        zx_exec_time = get_absolute_time()-start;

        // In debug mode, show the frame number. Useful in order to
        // find the right timing for automatic key presses.
        if (EMU.debug) {
            char buf[32];
            int len = snprintf(buf,sizeof(buf),"%d",(int)EMU.tick);
            ui_set_area_attributes(0,0,16*len,16);
            ui_fill_box(0,0,16*len,16,0,0);
            ui_draw_string(0,0,buf,1,2);
        }

        // Handle the menu.
        if (EMU.menu_active) {
            ui_draw_menu();
        }

        // Update the display with the current CRT image.
        start = get_absolute_time();
        update_display(EMU.scaling,EMU.show_border,blink&0x8);
        update_time = get_absolute_time()-start;
        blink++;

        EMU.tick++;
        printf("display: %llu us, zx(%u): %llu us, FPS: %.1f\n",
            update_time,
            FRAME_USEC, zx_exec_time,
            1000000.0/(float)(zx_exec_time+update_time));
    }
}
