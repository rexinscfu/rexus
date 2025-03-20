#ifndef REXUS_CONSOLE_H
#define REXUS_CONSOLE_H

#include <stdint.h>
#include <stdbool.h>

#define CONSOLE_INPUT_BUFFER_SIZE 256
#define CONSOLE_MAX_HISTORY 10

// Console colors
typedef enum {
    CONSOLE_COLOR_BLACK = 0,
    CONSOLE_COLOR_BLUE = 1,
    CONSOLE_COLOR_GREEN = 2,
    CONSOLE_COLOR_CYAN = 3,
    CONSOLE_COLOR_RED = 4,
    CONSOLE_COLOR_MAGENTA = 5,
    CONSOLE_COLOR_BROWN = 6,
    CONSOLE_COLOR_LIGHT_GREY = 7,
    CONSOLE_COLOR_DARK_GREY = 8,
    CONSOLE_COLOR_LIGHT_BLUE = 9,
    CONSOLE_COLOR_LIGHT_GREEN = 10,
    CONSOLE_COLOR_LIGHT_CYAN = 11,
    CONSOLE_COLOR_LIGHT_RED = 12,
    CONSOLE_COLOR_LIGHT_MAGENTA = 13,
    CONSOLE_COLOR_LIGHT_BROWN = 14,
    CONSOLE_COLOR_WHITE = 15
} console_color_t;

// Command handler function type
typedef int (*console_cmd_handler_t)(int argc, char* argv[]);

// Console command structure
typedef struct {
    const char* name;
    const char* description;
    console_cmd_handler_t handler;
} console_command_t;

// Function declarations
void console_init(void);
void console_update(void);
void console_clear(void);
void console_puts(const char* str);
void console_putchar(char c);
void console_printf(const char* format, ...);
void console_set_color(console_color_t fg, console_color_t bg);
void console_register_command(const console_command_t* command);
void console_execute_command(const char* command_line);
char* console_read_line(void);
int console_parse_args(char* cmd, char* argv[]);

// Get input functions
bool console_has_input(void);
char console_getchar(void);

#endif /* REXUS_CONSOLE_H */ 