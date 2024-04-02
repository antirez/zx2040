#define key_left_pin 7
#define key_right_pin 8
#define key_up_pin 9
#define key_fire_pin 22
#define key_down_pin 6

/* Each row is: pin, key, joystick_code */
const uint8_t key_map[] = {
    7, '1', 0x8,    // left
    8, '2', 0x9,    // right
    6, '3', 0xa,    // down
    9, '4', 0xb,    // up
    22, '5', 0x20,  // fire
};
