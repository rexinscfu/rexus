#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../arch/x86/gdt.h"
#include "../arch/x86/idt.h"
#include "../arch/x86/isr.h"
#include "../arch/x86/pic.h"
#include "../drivers/vga.h"
#include "../mem/pmm.h"
#include "../mem/vmm.h"

extern void init_cppcrt();

static void parse_multiboot(uint32_t magic, uint32_t mboot_addr);

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
    
    // Enable interrupts
    vga_puts("Enabling interrupts...\n");
    __asm__ volatile("sti");
    
    vga_puts("REXUS Kernel initialized successfully!\n");
    
    for(;;) {
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