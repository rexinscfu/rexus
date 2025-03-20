#include "gdt.h"
#include <string.h>

// GDT entries
gdt_entry_t gdt_entries[6];  // null, kernel code, kernel data, user code, user data, tss
gdt_ptr_t   gdt_ptr;
tss_entry_t tss_entry;

void gdt_init(void) {
    // Setup GDT pointer
    gdt_ptr.limit = (sizeof(gdt_entry_t) * 6) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;
    
    // Setup TSS
    memset(&tss_entry, 0, sizeof(tss_entry_t));
    tss_entry.ss0 = 0x10;  // Kernel data segment
    tss_entry.esp0 = 0;    // Will be set when needed
    
    // IO map to end of TSS
    tss_entry.iomap_base = sizeof(tss_entry_t);
    
    // GDT Entries
    // Format: index, base, limit, access, granularity
    
    // Null segment - must be zero
    gdt_set_gate(0, 0, 0, 0, 0);
    
    // Kernel code segment - 0x08
    // Base: 0
    // Limit: 0xFFFFF (4GB)
    // Access: 0x9A (present, ring 0, code segment, executable, direction 0, readable)
    // Granularity: 0xCF (4KB pages, 32-bit protected mode)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    
    // Kernel data segment - 0x10
    // Base: 0
    // Limit: 0xFFFFF (4GB)
    // Access: 0x92 (present, ring 0, data segment, non-executable, direction 0, writable)
    // Granularity: 0xCF (4KB pages, 32-bit protected mode)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    
    // User code segment - 0x18
    // Base: 0
    // Limit: 0xFFFFF (4GB)
    // Access: 0xFA (present, ring 3, code segment, executable, direction 0, readable)
    // Granularity: 0xCF (4KB pages, 32-bit protected mode)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    
    // User data segment - 0x20
    // Base: 0
    // Limit: 0xFFFFF (4GB)
    // Access: 0xF2 (present, ring 3, data segment, non-executable, direction 0, writable)
    // Granularity: 0xCF (4KB pages, 32-bit protected mode)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    
    // TSS segment - 0x28
    tss_set_gate(5, 0x10, 0);
    
    // Flush the changes to GDT and TSS
    gdt_flush((uint32_t)&gdt_ptr);
    tss_flush();
}

// Set GDT entry
void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;
    
    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;
    
    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access       = access;
}

// Set TSS entry
void tss_set_gate(int32_t num, uint16_t ss0, uint32_t esp0) {
    uint32_t base = (uint32_t)&tss_entry;
    uint32_t limit = base + sizeof(tss_entry_t);
    
    // As above, first create a normal segment
    gdt_set_gate(num, base, limit, 0xE9, 0x00);
    
    // Update the TSS state
    tss_entry.ss0 = ss0;
    tss_entry.esp0 = esp0;
}

// Set kernel stack for TSS
void tss_set_kernel_stack(uint32_t stack) {
    tss_entry.esp0 = stack;
} 