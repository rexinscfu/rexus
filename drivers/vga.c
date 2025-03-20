#include "vga.h"
#include "../arch/x86/io.h"
#include <stddef.h>

static uint16_t* vga_buffer;
static int vga_col;
static int vga_row;
static uint8_t vga_curr_color;

// Helper function to convert from VGA color pair to attribute byte
uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

// Helper function to create a VGA entry with character and color
uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

// Initialize the VGA text mode
void vga_init(void) {
    vga_buffer = (uint16_t*) VGA_ADDR;
    vga_row = 0;
    vga_col = 0;
    vga_curr_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_clear();
}

// Clear the screen
void vga_clear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            const int idx = y * VGA_WIDTH + x;
            vga_buffer[idx] = vga_entry(' ', vga_curr_color);
        }
    }
    vga_row = 0;
    vga_col = 0;
    vga_update_cursor();
}

// Internal function to update hardware cursor position
static void vga_update_cursor(void) {
    uint16_t pos = vga_row * VGA_WIDTH + vga_col;
    
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t) (pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

// Scroll the screen up one line
static void vga_scroll(void) {
    // Move all lines up one
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    
    // Clear the last line
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_curr_color);
    }
    
    vga_row = VGA_HEIGHT - 1;
}

// Put a character on the screen
void vga_putchar(char c) {
    // Handle special characters
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
        if (vga_row >= VGA_HEIGHT) {
            vga_scroll();
        }
        vga_update_cursor();
        return;
    }
    
    if (c == '\r') {
        vga_col = 0;
        vga_update_cursor();
        return;
    }
    
    if (c == '\t') {
        // Tab aligns to 8-character boundary
        vga_col = (vga_col + 8) & ~7;
        if (vga_col >= VGA_WIDTH) {
            vga_col = 0;
            vga_row++;
            if (vga_row >= VGA_HEIGHT) {
                vga_scroll();
            }
        }
        vga_update_cursor();
        return;
    }
    
    // Regular character
    vga_buffer[vga_row * VGA_WIDTH + vga_col] = vga_entry(c, vga_curr_color);
    vga_col++;
    
    // Handle end of line
    if (vga_col >= VGA_WIDTH) {
        vga_col = 0;
        vga_row++;
        if (vga_row >= VGA_HEIGHT) {
            vga_scroll();
        }
    }
    
    vga_update_cursor();
}

// Put a string on the screen
void vga_puts(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        vga_putchar(str[i]);
    }
}

// Set the color for future text
void vga_set_color(uint8_t color) {
    vga_curr_color = color;
}

// Set the cursor position
void vga_set_cursor(int x, int y) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        vga_col = x;
        vga_row = y;
        vga_update_cursor();
    }
}

// Print an integer
void vga_putint(int n) {
    char buf[12]; // Enough for 32-bit int with sign and null terminator
    int i = 0;
    int neg = 0;
    
    if (n == 0) {
        vga_putchar('0');
        return;
    }
    
    if (n < 0) {
        neg = 1;
        n = -n;
    }
    
    while (n != 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    
    if (neg) {
        vga_putchar('-');
    }
    
    while (--i >= 0) {
        vga_putchar(buf[i]);
    }
}

// Print a hexadecimal number
void vga_puthex(uint32_t n) {
    char hex_chars[] = "0123456789ABCDEF";
    vga_puts("0x");
    
    if (n == 0) {
        vga_putchar('0');
        return;
    }
    
    char buf[9]; // 8 hex digits for 32-bit + null
    int i = 0;
    
    while (n != 0) {
        buf[i++] = hex_chars[n & 0xF];
        n >>= 4;
    }
    
    while (--i >= 0) {
        vga_putchar(buf[i]);
    }
} 