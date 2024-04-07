// Kempston joystick key codes.
#define KEMPSTONE_FIRE 0x20
#define KEMPSTONE_LEFT 0x08
#define KEMPSTONE_RIGHT 0x09
#define KEMPSTONE_DOWN 0x0a
#define KEMPSTONE_UP 0x0b

// Special codes.
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

// Valley of rain.
const uint8_t keymap_valley[] = {
    KEY_LEFT, 'o', '1',
    KEY_RIGHT, 'p', KEMPSTONE_RIGHT,
    KEY_FIRE, 'm', KEMPSTONE_FIRE,
    KEY_DOWN, 'a', KEMPSTONE_DOWN,
    KEY_UP, 'q', KEMPSTONE_UP,
    KEY_END, 0, 0,
};

// Scuba
const uint8_t keymap_scuba[] = {
    KEY_LEFT, 'z', '3',
    KEY_RIGHT, 'x', '2',
    KEY_FIRE, 'm', '1',
    KEY_DOWN, 'n', KEMPSTONE_DOWN,
    KEY_UP, 'm', KEMPSTONE_UP,
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
