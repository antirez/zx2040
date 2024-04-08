// Games on the flash memory. Check under the games directory for the
// script that loads the Z80 image files into the flash memory.
struct game_entry {
    const char *name;
    void *addr;         // Address in the flash memory.
    size_t size;        // Length in bytes.
    const uint8_t *map; // Keyboard mapping to use. See keys_config.h.
} GamesTable[] = {
    {"Bmxsim", (void*)0x1007f100, 38872, keymap_bmxsim},
    {"Bombjack", (void*)0x100888d8, 40918, keymap_bombjack},
    {"Ik", (void*)0x100928ae, 42854, keymap_ik},
    {"Jetpac", (void*)0x1009d014, 10848, keymap_jetpac},
    {"Loderunner", (void*)0x1009fa74, 32181, keymap_loderunner},
    {"Scuba", (void*)0x100a7829, 32213, keymap_scuba},
    {"Skooldaze", (void*)0x100af5fe, 40863, keymap_skooldaze},
    {"Thrust", (void*)0x100b959d, 33938, keymap_thrust},
    {"Valley", (void*)0x100c1a2f, 44619, keymap_valley},
};
