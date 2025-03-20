#ifndef REXUS_GDT_H
#define REXUS_GDT_H

#include <stdint.h>

// GDT entry structure (Global Descriptor Table entry)
typedef struct {
    uint16_t limit_low;           // Lower 16 bits of limit
    uint16_t base_low;            // Lower 16 bits of base
    uint8_t base_middle;          // Next 8 bits of base
    uint8_t access;               // Access flags
    uint8_t granularity;          // Granularity flags + upper 4 bits of limit
    uint8_t base_high;            // Last 8 bits of base
} __attribute__((packed)) gdt_entry_t;

// GDT pointer structure
typedef struct {
    uint16_t limit;               // Size of GDT - 1
    uint32_t base;                // Address of first GDT entry
} __attribute__((packed)) gdt_ptr_t;

// TSS (Task State Segment) structure
typedef struct {
    uint32_t prev_tss;            // Previous TSS
    uint32_t esp0;                // ESP for ring 0
    uint32_t ss0;                 // SS for ring 0
    uint32_t esp1;                // ESP for ring 1
    uint32_t ss1;                 // SS for ring 1
    uint32_t esp2;                // ESP for ring 2
    uint32_t ss2;                 // SS for ring 2
    uint32_t cr3;                 // Page directory base
    uint32_t eip;                 // Saved EIP
    uint32_t eflags;              // Saved EFLAGS
    uint32_t eax;                 // Saved EAX
    uint32_t ecx;                 // Saved ECX
    uint32_t edx;                 // Saved EDX
    uint32_t ebx;                 // Saved EBX
    uint32_t esp;                 // Saved ESP
    uint32_t ebp;                 // Saved EBP
    uint32_t esi;                 // Saved ESI
    uint32_t edi;                 // Saved EDI
    uint32_t es;                  // Saved ES
    uint32_t cs;                  // Saved CS
    uint32_t ss;                  // Saved SS
    uint32_t ds;                  // Saved DS
    uint32_t fs;                  // Saved FS
    uint32_t gs;                  // Saved GS
    uint32_t ldt;                 // Saved LDT segment selector
    uint16_t trap;                // Debug trap flag
    uint16_t iomap_base;          // I/O map base address
} __attribute__((packed)) tss_entry_t;

// Function declarations
void gdt_init(void);
void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);
void tss_set_gate(int32_t num, uint16_t ss0, uint32_t esp0);
void tss_set_kernel_stack(uint32_t stack);
extern void gdt_flush(uint32_t);
extern void tss_flush(void);

#endif /* REXUS_GDT_H */ 