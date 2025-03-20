#ifndef REXUS_ISR_H
#define REXUS_ISR_H

#include <stdint.h>

// This struct gets pushed to the stack when an interrupt occurs
typedef struct {
    uint32_t ds;                  // Data segment selector
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha
    uint32_t int_no, err_code;    // Interrupt number and error code
    uint32_t eip, cs, eflags, useresp, ss; // Pushed by the processor automatically
} __attribute__((packed)) registers_t;

// ISR handler function typedef
typedef void (*isr_handler_t)(registers_t *);

// Function declarations
void isr_init(void);
void isr_register_handler(uint8_t n, isr_handler_t handler);
void irq_register_handler(uint8_t n, isr_handler_t handler);

// Commonly used ISR/IRQ numbers
#define IRQ0  32 // Timer
#define IRQ1  33 // Keyboard
#define IRQ2  34 // Cascade for 8259A Slave controller
#define IRQ3  35 // Serial port 2
#define IRQ4  36 // Serial port 1
#define IRQ5  37 // LPT2 or sound card
#define IRQ6  38 // Floppy disk controller
#define IRQ7  39 // LPT1
#define IRQ8  40 // RTC
#define IRQ9  41 // Legacy SCSI or NIC
#define IRQ10 42 // Available
#define IRQ11 43 // Available
#define IRQ12 44 // PS/2 Mouse
#define IRQ13 45 // FPU
#define IRQ14 46 // Primary ATA
#define IRQ15 47 // Secondary ATA

#endif /* REXUS_ISR_H */ 