#include "keyboard.h"
#include "../arch/x86/io.h"
#include "../arch/x86/isr.h"
#include "vga.h"
#include <string.h>

// Keyboard state
static keyboard_state_t keyboard_state;

// Keyboard buffer
#define KEYBOARD_BUFFER_SIZE 64
static uint16_t keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static uint32_t keyboard_buffer_head = 0;
static uint32_t keyboard_buffer_tail = 0;

// Callback function
static keyboard_callback_t keyboard_callback = NULL;

// US QWERTY keyboard layout scancode to ASCII conversion tables
static const char scancode_to_ascii_lower[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0
};

static const char scancode_to_ascii_upper[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0
};

// Initialize keyboard
void keyboard_init(void) {
    // Register keyboard IRQ handler
    irq_register_handler(IRQ1, keyboard_handler);
    
    // Initialize keyboard state
    memset(&keyboard_state, 0, sizeof(keyboard_state_t));
    keyboard_state.num_lock = true;  // NumLock on by default
    
    // Initialize buffer
    memset(keyboard_buffer, 0, sizeof(keyboard_buffer));
    keyboard_buffer_head = 0;
    keyboard_buffer_tail = 0;
    
    vga_puts("Keyboard: Initialized PS/2 keyboard driver\n");
}

// Add key to buffer
static void keyboard_buffer_enqueue(uint16_t key) {
    // Calculate next position
    uint32_t next = (keyboard_buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    
    // Check if buffer is full
    if (next == keyboard_buffer_tail) {
        return; // Buffer full
    }
    
    // Add key to buffer
    keyboard_buffer[keyboard_buffer_head] = key;
    keyboard_buffer_head = next;
    
    // Call the callback if registered
    if (keyboard_callback) {
        keyboard_callback(key);
    }
}

// Get key from buffer
uint16_t keyboard_read_key(void) {
    // Check if buffer is empty
    if (keyboard_buffer_head == keyboard_buffer_tail) {
        return 0; // No key available
    }
    
    // Get key from buffer
    uint16_t key = keyboard_buffer[keyboard_buffer_tail];
    keyboard_buffer_tail = (keyboard_buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    
    return key;
}

// Process a scancode and update keyboard state
static void keyboard_process_scancode(uint8_t scancode) {
    static bool extended = false;
    uint16_t key = 0;
    bool keyup = false;
    
    // Check for extended scancode
    if (scancode == 0xE0) {
        extended = true;
        return;
    }
    
    // Check for key release (bit 7 set)
    if (scancode & 0x80) {
        keyup = true;
        scancode &= 0x7F; // Clear bit 7
    }
    
    // Process normal keys
    if (!extended && scancode < sizeof(scancode_to_ascii_lower)) {
        // Convert scancode to key
        key = scancode_to_ascii_lower[scancode];
        
        // Handle special keys
        switch (scancode) {
            case 0x2A: // Left shift
            case 0x36: // Right shift
                keyboard_state.shift_pressed = !keyup;
                key = KEY_SHIFT;
                break;
                
            case 0x1D: // Left control
                keyboard_state.ctrl_pressed = !keyup;
                key = KEY_CTRL;
                break;
                
            case 0x38: // Left alt
                keyboard_state.alt_pressed = !keyup;
                key = KEY_ALT;
                break;
                
            case 0x3A: // Caps lock
                if (!keyup) {
                    keyboard_state.caps_lock = !keyboard_state.caps_lock;
                }
                key = KEY_CAPSLOCK;
                break;
                
            case 0x45: // Num lock
                if (!keyup) {
                    keyboard_state.num_lock = !keyboard_state.num_lock;
                }
                key = KEY_NUMLOCK;
                break;
                
            case 0x46: // Scroll lock
                if (!keyup) {
                    keyboard_state.scroll_lock = !keyboard_state.scroll_lock;
                }
                key = KEY_SCROLLLOCK;
                break;
        }
    }
    // Process extended keys
    else if (extended) {
        extended = false;
        
        switch (scancode) {
            case 0x1D: // Right control
                keyboard_state.ctrl_pressed = !keyup;
                key = KEY_CTRL;
                break;
                
            case 0x38: // Right alt
                keyboard_state.alt_pressed = !keyup;
                key = KEY_ALT;
                break;
                
            case 0x47: // Home
                key = KEY_HOME;
                break;
                
            case 0x48: // Up arrow
                key = KEY_UP;
                break;
                
            case 0x49: // Page up
                key = KEY_PAGE_UP;
                break;
                
            case 0x4B: // Left arrow
                key = KEY_LEFT;
                break;
                
            case 0x4D: // Right arrow
                key = KEY_RIGHT;
                break;
                
            case 0x4F: // End
                key = KEY_END;
                break;
                
            case 0x50: // Down arrow
                key = KEY_DOWN;
                break;
                
            case 0x51: // Page down
                key = KEY_PAGE_DOWN;
                break;
                
            case 0x52: // Insert
                key = KEY_INSERT;
                break;
                
            case 0x53: // Delete
                key = KEY_DELETE;
                break;
        }
    }
    
    // Mark key as released if necessary
    if (keyup) {
        key |= KEY_RELEASED;
    }
    
    // Add key to buffer if it's a valid key
    if (key != 0 && !keyup) {
        keyboard_buffer_enqueue(key);
    }
}

// Keyboard interrupt handler
void keyboard_handler(void) {
    // Read scancode from keyboard
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    // Process the scancode
    keyboard_process_scancode(scancode);
}

// Convert scancode to ASCII
char keyboard_scancode_to_ascii(uint16_t scancode) {
    // Handle special keys
    if (scancode & KEY_SPECIAL) {
        return 0; // Not an ASCII character
    }
    
    // Get the base ascii value
    char ascii = scancode_to_ascii_lower[scancode & 0xFF];
    
    // Apply shift/caps lock
    bool shift = (keyboard_state.shift_pressed || 
                 (keyboard_state.caps_lock && (ascii >= 'a' && ascii <= 'z')));
    
    if (shift) {
        ascii = scancode_to_ascii_upper[scancode & 0xFF];
    }
    
    return ascii;
}

// Check if a key is pressed
bool keyboard_is_key_pressed(uint16_t key) {
    // Remove flags
    key &= ~KEY_RELEASED;
    
    // Check for modifier keys
    switch (key) {
        case KEY_SHIFT:
            return keyboard_state.shift_pressed;
        case KEY_ALT:
            return keyboard_state.alt_pressed;
        case KEY_CTRL:
            return keyboard_state.ctrl_pressed;
        case KEY_CAPSLOCK:
            return keyboard_state.caps_lock;
        case KEY_NUMLOCK:
            return keyboard_state.num_lock;
        case KEY_SCROLLLOCK:
            return keyboard_state.scroll_lock;
        default:
            // Not a modifier key
            break;
    }
    
    // For other keys, we would need to track the key state
    // This is a simplified version that only tracks modifiers
    return false;
}

// Register a callback for keyboard events
void keyboard_register_callback(keyboard_callback_t callback) {
    keyboard_callback = callback;
} 