#include "console.h"
#include "vga.h"
#include "keyboard.h"
#include <string.h>
#include <stdarg.h>
#include <stddef.h>

// Input buffer and state
static char input_buffer[CONSOLE_INPUT_BUFFER_SIZE];
static uint32_t input_position = 0;
static bool input_ready = false;

// Command history
static char history_buffer[CONSOLE_MAX_HISTORY][CONSOLE_INPUT_BUFFER_SIZE];
static int history_count = 0;
static int history_position = -1;

// Command table
#define MAX_COMMANDS 32
static console_command_t command_table[MAX_COMMANDS];
static int command_count = 0;

// Current console colors
static console_color_t console_fg = CONSOLE_COLOR_LIGHT_GREY;
static console_color_t console_bg = CONSOLE_COLOR_BLACK;

// Prompt string
static const char* console_prompt = "REXUS> ";

// Keyboard callback for console input
static void console_keyboard_callback(uint16_t key) {
    if (key & KEY_RELEASED) {
        return; // Ignore key releases
    }
    
    // Handle normal keys
    if (!(key & KEY_SPECIAL)) {
        char c = keyboard_scancode_to_ascii(key);
        if (c != 0) {
            if (c == '\n' || c == '\r') {
                // Enter key - process the command
                vga_putchar('\n');
                input_buffer[input_position] = '\0';
                
                // Add to history if not empty
                if (input_position > 0) {
                    // Shift history entries if we've reached max
                    if (history_count == CONSOLE_MAX_HISTORY) {
                        for (int i = 0; i < CONSOLE_MAX_HISTORY - 1; i++) {
                            strcpy(history_buffer[i], history_buffer[i+1]);
                        }
                        history_count--;
                    }
                    
                    // Add to history
                    strcpy(history_buffer[history_count], input_buffer);
                    history_count++;
                }
                
                // Mark input as ready
                input_ready = true;
                
                // Reset history position
                history_position = -1;
            }
            else if (c == '\b') {
                // Backspace
                if (input_position > 0) {
                    input_position--;
                    input_buffer[input_position] = '\0';
                    
                    // Update display
                    vga_putchar('\b');
                    vga_putchar(' ');
                    vga_putchar('\b');
                }
            }
            else if (input_position < CONSOLE_INPUT_BUFFER_SIZE - 1) {
                // Normal character
                input_buffer[input_position++] = c;
                input_buffer[input_position] = '\0';
                vga_putchar(c);
            }
        }
    }
    else {
        // Handle special keys
        switch (key) {
            case KEY_UP:
                // History - previous command
                if (history_count > 0) {
                    if (history_position == -1) {
                        history_position = history_count - 1;
                    }
                    else if (history_position > 0) {
                        history_position--;
                    }
                    
                    // Clear current input
                    while (input_position > 0) {
                        vga_putchar('\b');
                        vga_putchar(' ');
                        vga_putchar('\b');
                        input_position--;
                    }
                    
                    // Copy from history
                    strcpy(input_buffer, history_buffer[history_position]);
                    input_position = strlen(input_buffer);
                    
                    // Display
                    vga_puts(input_buffer);
                }
                break;
                
            case KEY_DOWN:
                // History - next command
                if (history_position != -1) {
                    history_position++;
                    
                    // Clear current input
                    while (input_position > 0) {
                        vga_putchar('\b');
                        vga_putchar(' ');
                        vga_putchar('\b');
                        input_position--;
                    }
                    
                    // If we've reached the end of history, clear the input
                    if (history_position >= history_count) {
                        input_buffer[0] = '\0';
                        history_position = -1;
                    }
                    else {
                        // Copy from history
                        strcpy(input_buffer, history_buffer[history_position]);
                    }
                    
                    input_position = strlen(input_buffer);
                    
                    // Display
                    vga_puts(input_buffer);
                }
                break;
                
            case KEY_HOME:
                // Move cursor to beginning of line
                while (input_position > 0) {
                    vga_putchar('\b');
                    input_position--;
                }
                break;
                
            case KEY_END:
                // Move cursor to end of line
                // Cursor is already at the end in this implementation
                break;
                
            case KEY_LEFT:
                // Move cursor left
                if (input_position > 0) {
                    vga_putchar('\b');
                    input_position--;
                }
                break;
                
            case KEY_RIGHT:
                // Move cursor right
                // Not implemented - would need more complex buffer tracking
                break;
        }
    }
}

// Initialize the console
void console_init(void) {
    // Clear the input buffer
    memset(input_buffer, 0, CONSOLE_INPUT_BUFFER_SIZE);
    input_position = 0;
    input_ready = false;
    
    // Clear the history buffer
    memset(history_buffer, 0, sizeof(history_buffer));
    history_count = 0;
    history_position = -1;
    
    // Clear the command table
    memset(command_table, 0, sizeof(command_table));
    command_count = 0;
    
    // Register keyboard callback
    keyboard_register_callback(console_keyboard_callback);
    
    // Display welcome message
    console_clear();
    console_set_color(CONSOLE_COLOR_LIGHT_CYAN, CONSOLE_COLOR_BLACK);
    console_puts("REXUS Kernel Console\n");
    console_puts("Type 'help' for a list of commands\n\n");
    console_set_color(CONSOLE_COLOR_LIGHT_GREY, CONSOLE_COLOR_BLACK);
    
    // Display prompt
    console_puts(console_prompt);
}

// Update the console (check for input, process commands)
void console_update(void) {
    if (input_ready) {
        // Copy the input buffer
        char cmd[CONSOLE_INPUT_BUFFER_SIZE];
        strcpy(cmd, input_buffer);
        
        // Reset the input buffer
        memset(input_buffer, 0, CONSOLE_INPUT_BUFFER_SIZE);
        input_position = 0;
        input_ready = false;
        
        // Execute the command
        console_execute_command(cmd);
        
        // Display prompt
        console_puts(console_prompt);
    }
}

// Clear the console
void console_clear(void) {
    vga_clear();
}

// Print a string to the console
void console_puts(const char* str) {
    vga_puts(str);
}

// Print a character to the console
void console_putchar(char c) {
    vga_putchar(c);
}

// Set console colors
void console_set_color(console_color_t fg, console_color_t bg) {
    console_fg = fg;
    console_bg = bg;
    vga_set_color(vga_entry_color(fg, bg));
}

// Register a command
void console_register_command(const console_command_t* command) {
    if (command_count < MAX_COMMANDS) {
        command_table[command_count++] = *command;
    }
}

// Parse command arguments
int console_parse_args(char* cmd, char* argv[]) {
    int argc = 0;
    char* token = strtok(cmd, " \t");
    
    while (token != NULL && argc < 16) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }
    
    return argc;
}

// Execute a command
void console_execute_command(const char* command_line) {
    // Copy command line
    char cmd_copy[CONSOLE_INPUT_BUFFER_SIZE];
    strcpy(cmd_copy, command_line);
    
    // Parse arguments
    char* argv[16];
    int argc = console_parse_args(cmd_copy, argv);
    
    if (argc == 0) {
        return; // Empty command
    }
    
    // Find and execute the command
    for (int i = 0; i < command_count; i++) {
        if (strcmp(argv[0], command_table[i].name) == 0) {
            int result = command_table[i].handler(argc, argv);
            
            if (result != 0) {
                console_printf("Command returned error code: %d\n", result);
            }
            
            return;
        }
    }
    
    // Command not found
    console_printf("Unknown command: %s\n", argv[0]);
}

// Read a line from the console
char* console_read_line(void) {
    // Wait for input
    input_ready = false;
    while (!input_ready) {
        __asm__ volatile("hlt");
    }
    
    // Return the input buffer
    return input_buffer;
}

// Check if input is available
bool console_has_input(void) {
    return input_ready;
}

// Get a character from the input buffer
char console_getchar(void) {
    // Wait for a key
    uint16_t key;
    while ((key = keyboard_read_key()) == 0) {
        __asm__ volatile("hlt");
    }
    
    // Convert to ASCII and return
    return keyboard_scancode_to_ascii(key);
}

// Simple printf implementation
void console_printf(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    
    // Very simple printf implementation
    char* dst = buffer;
    const char* src = format;
    
    while (*src) {
        if (*src == '%') {
            src++;
            switch (*src) {
                case 'd': {
                    int val = va_arg(args, int);
                    char num[12];
                    int i = 0;
                    int negative = 0;
                    
                    if (val == 0) {
                        num[i++] = '0';
                    }
                    else {
                        if (val < 0) {
                            negative = 1;
                            val = -val;
                        }
                        
                        while (val > 0) {
                            num[i++] = '0' + (val % 10);
                            val /= 10;
                        }
                        
                        if (negative) {
                            num[i++] = '-';
                        }
                    }
                    
                    while (--i >= 0) {
                        *dst++ = num[i];
                    }
                    break;
                }
                case 'x': {
                    unsigned int val = va_arg(args, unsigned int);
                    char num[9];
                    int i = 0;
                    
                    if (val == 0) {
                        num[i++] = '0';
                    }
                    else {
                        while (val > 0) {
                            int digit = val & 0xF;
                            num[i++] = digit < 10 ? '0' + digit : 'a' + (digit - 10);
                            val >>= 4;
                        }
                    }
                    
                    while (--i >= 0) {
                        *dst++ = num[i];
                    }
                    break;
                }
                case 's': {
                    const char* str = va_arg(args, const char*);
                    while (*str) {
                        *dst++ = *str++;
                    }
                    break;
                }
                case 'c': {
                    char c = va_arg(args, int);
                    *dst++ = c;
                    break;
                }
                case '%': {
                    *dst++ = '%';
                    break;
                }
                default:
                    *dst++ = '%';
                    *dst++ = *src;
                    break;
            }
        }
        else {
            *dst++ = *src;
        }
        
        src++;
    }
    
    *dst = '\0';
    va_end(args);
    
    // Print the formatted string
    console_puts(buffer);
} 