// Kempston joystick key codes.
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

// Extended keymaps allow two device buttons (pins) to map to
// other Specturm keys. This is useful for games such as Skool Daze
// that have too many keys doing useful things, but where the nature
// of the game don't make likely we press multiple keys for error.
// 
// To use this kind of maps, xor KEY_EXT to the first pin, then
// provide as second entry in the row the second pin, and finally
// a single Spectrum key code to trigger.
// 
// IMPORTANT: the extended key maps of a game must be the initial entries,
// before the normal entries. This way we avoid also sensing the keys
// mapped to the single buttons involved.
#define KEY_EXT         0x80

/* Each row is: pin, keycode_1, keycode_2.
 *
 * So keys when a button is pressed logically two
 * Spectrum buttons are pressed. This is useful because often
 * you want to map keys both to the Kempston joystic codes
 * and to actual keyboard keys useful to select the joystic and
 * start the game. */

// Default keymap. Loaded at startup, but actually
// right now every game requires to have a specific map.
const uint8_t keymap_default[] = {
    KEY_LEFT, '1', KEMPSTONE_LEFT,
    KEY_RIGHT, '2', KEMPSTONE_RIGHT,
    KEY_FIRE, '4', KEMPSTONE_FIRE,
    KEY_DOWN, '3', KEMPSTONE_DOWN,
    KEY_UP, '5', KEMPSTONE_UP,
    KEY_END, 0, 0,
};

// Jetpack. Select joystick with 4 at startup. Down key does not block
// but provides an up+fire combo which is useful to actually play the
// game when keys are badly placed such as in the Pimoroni Tufty 2040.
const uint8_t keymap_jetpac[] = {
    KEY_LEFT, '1', KEMPSTONE_LEFT,
    KEY_RIGHT, '2', KEMPSTONE_RIGHT,
    KEY_FIRE, '4', KEMPSTONE_FIRE,
    KEY_DOWN, KEMPSTONE_FIRE, KEMPSTONE_UP,
    KEY_UP, '5', KEMPSTONE_UP,
    PRESS_AT_TICK, 10, '4', // Select joystick.
    RELEASE_AT_TICK, 11, '4',
    KEY_END, 0, 0,
};

// Bombjack.
// Here we need to press 'p' to select the joystick.
const uint8_t keymap_bombjack[] = {
    KEY_LEFT, '1', KEMPSTONE_LEFT,
    KEY_RIGHT, '2', KEMPSTONE_RIGHT,
    KEY_FIRE, 'p', KEMPSTONE_UP,
    KEY_DOWN, 'p', KEMPSTONE_DOWN,
    KEY_UP, '5', KEMPSTONE_FIRE,
    KEY_END, 0, 0,
};

// Thrust.
// Joystick not supported at all. Map to the default keys.
const uint8_t keymap_thrust[] = {
    KEY_LEFT, 'a', KEMPSTONE_LEFT,
    KEY_RIGHT, 's', KEMPSTONE_RIGHT,
    KEY_FIRE, 'i', 'm',
    KEY_DOWN, 'm', KEMPSTONE_DOWN,
    KEY_UP, 'p', KEMPSTONE_UP,
    PRESS_AT_TICK, 20, 'n', // Do you want to redefine the keys? [N]o.
    RELEASE_AT_TICK, 21, 'n',
    KEY_END, 0, 0,
};

// Loderunner.
// Select joystick at startup.
const uint8_t keymap_loderunner[] = {
    KEY_LEFT, '1', KEMPSTONE_LEFT,
    KEY_RIGHT, '2', KEMPSTONE_RIGHT,
    KEY_FIRE, '0', KEMPSTONE_FIRE,
    KEY_DOWN, '3', KEMPSTONE_DOWN,
    KEY_UP, '5', KEMPSTONE_UP,
    PRESS_AT_TICK, 20, '0', // Leave splash screen.
    RELEASE_AT_TICK, 21, '0',
    PRESS_AT_TICK, 70, '2', // Select joystick.
    RELEASE_AT_TICK, 71, '2',
    KEY_END, 0, 0,
};

// IK+
// Skip splash screen and credits.
// Select joystick at startup.
// Say "N" to second player.
const uint8_t keymap_ik[] = {
    KEY_LEFT, '1', KEMPSTONE_LEFT,
    KEY_RIGHT, '2', KEMPSTONE_RIGHT,
    KEY_FIRE, '5', KEMPSTONE_FIRE,
    KEY_DOWN, '3', KEMPSTONE_DOWN,
    KEY_UP, '4', KEMPSTONE_UP,
    PRESS_AT_TICK, 20, '0', // Leave splash screen.
    RELEASE_AT_TICK, 21, '0',
    PRESS_AT_TICK, 30, '0', // Leave credits screen.
    RELEASE_AT_TICK, 31, '0',
    PRESS_AT_TICK, 40, '5', // Player 1 select joystick.
    RELEASE_AT_TICK, 41, '5',
    PRESS_AT_TICK, 50, 'n', // No player 2.
    RELEASE_AT_TICK, 51, 'n',
    KEY_END, 0, 0,
};

// Scuba
// We redefine the keys at startup using macros.
//
// The the user must select the level from 1 to 4.
// Level 1 is selected with just fire.
// Level 2 pressing together up and left.
// Level 3 up and right.
// Level 4 up and fire.
const uint8_t keymap_scuba[] = {
    // Extended key maps must be the initial entries.
    KEY_UP|KEY_EXT, KEY_LEFT, '2',  // Level 2
    KEY_UP|KEY_EXT, KEY_RIGHT, '3', // Level 3
    KEY_UP|KEY_EXT, KEY_FIRE, '4',  // Level 4

    KEY_LEFT, 'x', 0,
    KEY_RIGHT, 'z', 0,
    KEY_FIRE, 'm', '1',
    KEY_DOWN, 'n', KEMPSTONE_DOWN,
    KEY_UP, '1', '1',       // Start game at level 1.
    PRESS_AT_TICK, 10, 'k', // Redefine keys
    RELEASE_AT_TICK, 11, 'k',
    PRESS_AT_TICK, 14, 'z', // Redefine left
    RELEASE_AT_TICK, 15, 'z',
    PRESS_AT_TICK, 16, 'x', // Redefine right
    RELEASE_AT_TICK, 17, 'x',
    PRESS_AT_TICK, 18, 'm', // Redefine accelerate
    RELEASE_AT_TICK, 19, 'm',
    PRESS_AT_TICK, 20, 'n', // Redefine decelerate
    RELEASE_AT_TICK, 21, 'n',
    KEY_END, 0, 0,
};

// BMX simulator
// Here we just need a way to start the game. There is no joystick
// support apparently, so we map the default keyboard keys.
const uint8_t keymap_bmxsim[] = {
    KEY_LEFT, '6', 0,               // 6 = left
    KEY_RIGHT, '7', 0,              // 7 = right
    KEY_FIRE, '0', 0,               // 0 = accelerate.
    KEY_DOWN, ' ', 0,               // Space = Exit demo.
    KEY_UP, 's', 0,                 // S = Start game.
    KEY_END, 0, 0,
};

// Skooldaze. Here there are too many keys, so we resort
// to extended mapping, using two keys pressed at the same time
// to map to other actions.
const uint8_t keymap_skooldaze[] = {
    // Extended key maps must be the initial entries.
    KEY_UP|KEY_EXT, KEY_LEFT, 's',  // Up + left = sit
    KEY_UP|KEY_EXT, KEY_RIGHT, 'l', // Up + right = leap
    KEY_UP|KEY_EXT, KEY_FIRE, 'j',  // Up + fire = jump
    KEY_UP|KEY_EXT, KEY_DOWN, 'n',  // [N]o to usign your names.
    KEY_LEFT|KEY_EXT, KEY_FIRE, 'u',  // [U]nderstand.

    KEY_LEFT, 'o', KEMPSTONE_LEFT,
    KEY_RIGHT, 'p', KEMPSTONE_RIGHT,
    KEY_FIRE, 'f', KEMPSTONE_FIRE,  // Fire catapult.
    KEY_DOWN, 'a', KEMPSTONE_DOWN,
    KEY_UP, 'q', KEMPSTONE_UP,
    KEY_END, 0, 0,
};

// Sabre Wulf
//
// We don't select the joystick automatically with a macro
// since this stops the music, which may bring good memories.
//
// To select the joystick, press the fire button.
// The game starts pressing 0, which is mapped to the up button.
const uint8_t keymap_sabre[] = {
    KEY_LEFT, 0, KEMPSTONE_LEFT,
    KEY_RIGHT, 0, KEMPSTONE_RIGHT,
    KEY_FIRE, '4', KEMPSTONE_FIRE,  // 4 = Select kempstone joystick.
    KEY_DOWN, '0', KEMPSTONE_DOWN,  // 0 = start game.
    KEY_UP, 0, KEMPSTONE_UP,        // 0 = start game.
    KEY_END, 0, 0,
};

// Sanxon
const uint8_t keymap_sanxion[] = {
    KEY_LEFT, 0, KEMPSTONE_LEFT,
    KEY_RIGHT, 0, KEMPSTONE_RIGHT,
    KEY_FIRE, '1', KEMPSTONE_FIRE,  // 1 starts the game.
    KEY_DOWN, 0, KEMPSTONE_DOWN,
    KEY_UP, 0, KEMPSTONE_UP,
    PRESS_AT_TICK, 40, '3',         // Select Kempstone automatically.
    RELEASE_AT_TICK, 43, '3',
    KEY_END, 0, 0,
};

// 3D show demo
#define keymap_3dshow_demo keymap_default
