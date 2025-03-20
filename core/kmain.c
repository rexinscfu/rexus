#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../arch/x86/gdt.h"
#include "../arch/x86/idt.h"
#include "../arch/x86/isr.h"
#include "../arch/x86/pic.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/console.h"
#include "../mem/pmm.h"
#include "../mem/vmm.h"
#include "../proc/process.h"

extern void init_cppcrt();

static void parse_multiboot(uint32_t magic, uint32_t mboot_addr);
static int help_command(int argc, char* argv[]);
static int clear_command(int argc, char* argv[]);
static int info_command(int argc, char* argv[]);
static int echo_command(int argc, char* argv[]);
static int meminfo_command(int argc, char* argv[]);

void kmain(uint32_t magic, uint32_t mboot_addr) {
    // Initialize VGA early for debugging output
    vga_init();
    vga_puts("REXUS Kernel booting...\n");
    
    // Parse multiboot info
    parse_multiboot(magic, mboot_addr);
    
    // Initialize x86-specific hardware
    vga_puts("Initializing GDT...\n");
    gdt_init();
    
    vga_puts("Initializing IDT...\n");
    idt_init();
    
    vga_puts("Setting up ISRs...\n");
    isr_init();
    
    vga_puts("Remapping PIC...\n");
    pic_remap(0x20, 0x28);
    
    // Initialize memory management
    vga_puts("Initializing physical memory manager...\n");
    pmm_init(mboot_addr);
    
    vga_puts("Initializing virtual memory manager...\n");
    vmm_init();
    
    // Initialize C++ runtime if needed
    init_cppcrt();
    
    // Initialize PS/2 keyboard
    vga_puts("Initializing keyboard driver...\n");
    keyboard_init();
    
    // Initialize process management
    vga_puts("Initializing process management...\n");
    process_init();
    
    // Enable interrupts
    vga_puts("Enabling interrupts...\n");
    __asm__ volatile("sti");
    
    vga_puts("REXUS Kernel initialized successfully!\n\n");
    
    // Initialize console
    console_init();
    
    // Register console commands
    console_command_t help_cmd = {
        .name = "help",
        .description = "Display available commands",
        .handler = help_command
    };
    console_register_command(&help_cmd);
    
    console_command_t clear_cmd = {
        .name = "clear",
        .description = "Clear the screen",
        .handler = clear_command
    };
    console_register_command(&clear_cmd);
    
    console_command_t info_cmd = {
        .name = "info",
        .description = "Display system information",
        .handler = info_command
    };
    console_register_command(&info_cmd);
    
    console_command_t echo_cmd = {
        .name = "echo",
        .description = "Display text",
        .handler = echo_command
    };
    console_register_command(&echo_cmd);
    
    console_command_t meminfo_cmd = {
        .name = "meminfo",
        .description = "Display memory information",
        .handler = meminfo_command
    };
    console_register_command(&meminfo_cmd);
    
    // Main kernel loop
    while(1) {
        // Update console (process input)
        console_update();
        
        // Idle CPU when nothing to do
        __asm__ volatile("hlt");
    }
}

static void parse_multiboot(uint32_t magic, uint32_t mboot_addr) {
    // Check multiboot magic
    if (magic != 0x2BADB002) {
        vga_puts("Invalid multiboot magic number!\n");
        for(;;) {
            __asm__ volatile("hlt");
        }
    }
    
    // 'mboot_addr' here is a physical address, but we have identity mapping
    // for low memory at this stage anyway. We'll need to properly handle this
    // when we set up full virtual memory.
    
    // More detailed multiboot info parsing will be added later
    vga_puts("Multiboot information validated.\n");
}

// Command handlers

static int help_command(int argc, char* argv[]) {
    console_puts("REXUS Kernel Commands:\n");
    console_puts("  help     - Display this help text\n");
    console_puts("  clear    - Clear the screen\n");
    console_puts("  info     - Display system information\n");
    console_puts("  echo     - Display text\n");
    console_puts("  meminfo  - Display memory information\n");
    return 0;
}

static int clear_command(int argc, char* argv[]) {
    console_clear();
    return 0;
}

static int info_command(int argc, char* argv[]) {
    console_puts("REXUS Kernel - A specialized kernel for embedded systems\n");
    console_puts("Version: 0.1.0\n");
    console_puts("Architecture: x86\n");
    console_puts("Features: Memory Management, Process Scheduling, Console\n");
    return 0;
}

static int echo_command(int argc, char* argv[]) {
    // Skip the command name
    for (int i = 1; i < argc; i++) {
        console_puts(argv[i]);
        if (i < argc - 1) {
            console_putchar(' ');
        }
    }
    console_putchar('\n');
    return 0;
}

static int meminfo_command(int argc, char* argv[]) {
    uint32_t total_mem = pmm_get_memory_size();
    uint32_t used_mem = pmm_get_used_block_count() * PAGE_SIZE;
    uint32_t free_mem = pmm_get_free_block_count() * PAGE_SIZE;
    
    console_printf("Memory Information:\n");
    console_printf("  Total: %d KB (%d MB)\n", total_mem / 1024, total_mem / 1024 / 1024);
    console_printf("  Used:  %d KB (%d MB)\n", used_mem / 1024, used_mem / 1024 / 1024);
    console_printf("  Free:  %d KB (%d MB)\n", free_mem / 1024, free_mem / 1024 / 1024);
    return 0;
} 