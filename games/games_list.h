// Games on the flash memory. Check under the games directory for the
// script that loads the Z80 image files into the flash memory.
struct game_entry {
    const char *name;
    void *addr;         // Address in the flash memory.
    size_t size;        // Length in bytes.
    const uint8_t *map; // Keyboard mapping to use. See keys_config.h.
} GamesTable[] = {
    {"Bombjack", (void*)0x1007f100, 40918, keymap_bombjack},
    {"Jetpac", (void*)0x100890d6, 10848, keymap_jetpac},
    {"Loderunner", (void*)0x1008bb36, 32181, keymap_loderunner},
    {"Thrust", (void*)0x100938eb, 33938, keymap_thrust},
};
