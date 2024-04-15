// Games on the flash memory. Check under the games directory for the
// script that loads the Z80 image files into the flash memory.
struct game_entry {
    const char *name;
    void *addr;         // Address in the flash memory.
    size_t size;        // Length in bytes.
    const uint8_t *map; // Keyboard mapping to use. See keys_config.h.
} GamesTable[] = {
    {"3dshow_demo", (void*)0x1007f100, 24760, keymap_3dshow_demo},
    {"Bmxsim", (void*)0x100851b8, 38872, keymap_bmxsim},
    {"Bombjack", (void*)0x1008e990, 40918, keymap_bombjack},
    {"Ik", (void*)0x10098966, 42854, keymap_ik},
    {"Jetpac", (void*)0x100a30cc, 10848, keymap_jetpac},
    {"Loderunner", (void*)0x100a5b2c, 32181, keymap_loderunner},
    {"Sabre", (void*)0x100ad8e1, 41569, keymap_sabre},
    {"Sanxion", (void*)0x100b7b42, 31430, keymap_sanxion},
    {"Scuba", (void*)0x100bf608, 32213, keymap_scuba},
    {"Skooldaze", (void*)0x100c73dd, 40863, keymap_skooldaze},
    {"Thrust", (void*)0x100d137c, 33938, keymap_thrust},
    {"Valley", (void*)0x100d980e, 44619, keymap_valley},
};
