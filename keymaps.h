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

// Default keymap, used in all the games where we
// can select the joystick pressing one of
// 1,2,3,4,5.
const uint8_t keymap_default[] = {
    KEY_LEFT, '1', KEMPSTONE_LEFT,
    KEY_RIGHT, '2', KEMPSTONE_RIGHT,
    KEY_FIRE, '4', KEMPSTONE_FIRE,
    KEY_DOWN, '3', KEMPSTONE_DOWN,
    KEY_UP, '5', KEMPSTONE_UP,
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
    PRESS_AT_TICK, 10, 'n', // Do you want to redefine the keys? [N]o.
    RELEASE_AT_TICK, 11, 'n',
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

