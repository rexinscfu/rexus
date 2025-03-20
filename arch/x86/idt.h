#ifndef REXUS_IDT_H
#define REXUS_IDT_H

#include <stdint.h>

// IDT entry structure (Interrupt Descriptor Table entry)
typedef struct {
    uint16_t base_low;             // Lower 16 bits of handler function address
    uint16_t selector;             // Kernel segment selector
    uint8_t  always0;              // Must be 0
    uint8_t  flags;                // Flags
    uint16_t base_high;            // Upper 16 bits of handler function address
} __attribute__((packed)) idt_entry_t;

// IDT pointer structure
typedef struct {
    uint16_t limit;                // Size of IDT - 1
    uint32_t base;                 // Address of first IDT entry
} __attribute__((packed)) idt_ptr_t;

// Function declarations
void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags);
extern void idt_load(uint32_t);

// Function pointers for handler delegation
typedef void (*interrupt_handler_t)(void);

#endif /* REXUS_IDT_H */ 