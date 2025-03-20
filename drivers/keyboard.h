#ifndef REXUS_KEYBOARD_H
#define REXUS_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

// Keyboard ports
#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64
#define KEYBOARD_COMMAND_PORT 0x64

// Keyboard commands
#define KEYBOARD_CMD_SET_LEDS       0xED
#define KEYBOARD_CMD_ECHO           0xEE
#define KEYBOARD_CMD_GET_ID         0xF2
#define KEYBOARD_CMD_SET_TYPEMATIC  0xF3
#define KEYBOARD_CMD_ENABLE         0xF4
#define KEYBOARD_CMD_RESET_DISABLE  0xF5
#define KEYBOARD_CMD_RESET_ENABLE   0xF6
#define KEYBOARD_CMD_RESET          0xFF

// Special key flags
#define KEY_SPECIAL      0x100
#define KEY_SHIFT        0x200
#define KEY_ALT          0x400
#define KEY_CTRL         0x800
#define KEY_CAPSLOCK     0x1000
#define KEY_NUMLOCK      0x2000
#define KEY_SCROLLLOCK   0x4000
#define KEY_RELEASED     0x8000

// Special keys
#define KEY_ESC       (KEY_SPECIAL | 0x01)
#define KEY_BACKSPACE (KEY_SPECIAL | 0x02)
#define KEY_TAB       (KEY_SPECIAL | 0x03)
#define KEY_ENTER     (KEY_SPECIAL | 0x04)
#define KEY_HOME      (KEY_SPECIAL | 0x05)
#define KEY_END       (KEY_SPECIAL | 0x06)
#define KEY_INSERT    (KEY_SPECIAL | 0x07)
#define KEY_DELETE    (KEY_SPECIAL | 0x08)
#define KEY_PAGE_UP   (KEY_SPECIAL | 0x09)
#define KEY_PAGE_DOWN (KEY_SPECIAL | 0x0A)
#define KEY_LEFT      (KEY_SPECIAL | 0x0B)
#define KEY_RIGHT     (KEY_SPECIAL | 0x0C)
#define KEY_UP        (KEY_SPECIAL | 0x0D)
#define KEY_DOWN      (KEY_SPECIAL | 0x0E)
#define KEY_F1        (KEY_SPECIAL | 0x10)
#define KEY_F2        (KEY_SPECIAL | 0x11)
#define KEY_F3        (KEY_SPECIAL | 0x12)
#define KEY_F4        (KEY_SPECIAL | 0x13)
#define KEY_F5        (KEY_SPECIAL | 0x14)
#define KEY_F6        (KEY_SPECIAL | 0x15)
#define KEY_F7        (KEY_SPECIAL | 0x16)
#define KEY_F8        (KEY_SPECIAL | 0x17)
#define KEY_F9        (KEY_SPECIAL | 0x18)
#define KEY_F10       (KEY_SPECIAL | 0x19)
#define KEY_F11       (KEY_SPECIAL | 0x1A)
#define KEY_F12       (KEY_SPECIAL | 0x1B)

// Keyboard state
typedef struct {
    bool shift_pressed;
    bool alt_pressed;
    bool ctrl_pressed;
    bool caps_lock;
    bool num_lock;
    bool scroll_lock;
} keyboard_state_t;

// Function declarations
void keyboard_init(void);
void keyboard_handler(void);
uint16_t keyboard_get_scancode(void);
char keyboard_scancode_to_ascii(uint16_t scancode);
bool keyboard_is_key_pressed(uint16_t key);
uint16_t keyboard_read_key(void);

// Callback for keyboard events
typedef void (*keyboard_callback_t)(uint16_t key);
void keyboard_register_callback(keyboard_callback_t callback);

#endif /* REXUS_KEYBOARD_H */ 