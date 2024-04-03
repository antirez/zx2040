// Pimoroni Tufty 2040 keys pins.
#define KEY_A 7
#define KEY_B 8
#define KEY_C 9
#define KEY_UP 22
#define KEY_DOWN 6

// Kempton joystick key codes.
#define KEMPTON_FIRE 0x20
#define KEMPTON_LEFT 0x08
#define KEMPTON_RIGHT 0x09
#define KEMPTON_DOWN 0x0a
#define KEMPTON_UP 0x0b

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
 * you want to map keys both to the Kempton joystic codes
 * and to actual keyboard keys useful to select the joystic and
 * start the game. */

// Default keymap, used in all the games where we
// can select the joystick pressing one of
// 1,2,3,4,5.
const uint8_t keymap_default[] = {
    KEY_A, '1', KEMPTON_LEFT,
    KEY_B, '2', KEMPTON_RIGHT,
    KEY_C, '4', KEMPTON_UP,
    KEY_DOWN, '3', KEMPTON_DOWN,
    KEY_UP, '5', KEMPTON_FIRE,
    KEY_END, 0, 0,
};

// Bombjack.
// Here we need to press 'p' to select the joystick.
const uint8_t keymap_bombjack[] = {
    KEY_A, '1', KEMPTON_LEFT,
    KEY_B, '2', KEMPTON_RIGHT,
    KEY_C, 'p', KEMPTON_UP,
    KEY_DOWN, 'p', KEMPTON_DOWN,
    KEY_UP, '5', KEMPTON_FIRE,
    KEY_END, 0, 0,
};

// Thrust.
// Joystick not supported at all. Map to the default keys.
const uint8_t keymap_thrust[] = {
    KEY_A, 'a', KEMPTON_LEFT,
    KEY_B, 's', KEMPTON_RIGHT,
    KEY_C, 'i', 'm',
    KEY_DOWN, 'm', KEMPTON_DOWN,
    KEY_UP, 'p', KEMPTON_UP,
    PRESS_AT_TICK, 10, 'n', // Do you want to redefine the keys? [N]o.
    RELEASE_AT_TICK, 11, 'n',
    KEY_END, 0, 0,
};
