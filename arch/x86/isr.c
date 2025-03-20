#include "isr.h"
#include "pic.h"
#include "io.h"
#include "../../drivers/vga.h"
#include <string.h>

isr_handler_t interrupt_handlers[256];

// Messages for CPU exceptions
static const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

void isr_init(void) {
    // Clear interrupt handlers array
    memset(interrupt_handlers, 0, sizeof(isr_handler_t) * 256);
}

// This gets called from our ASM interrupt handler stub
void isr_handler(registers_t *regs) {
    // Handle CPU exceptions specifically (interrupts 0-31)
    if (regs->int_no < 32) {
        vga_puts("EXCEPTION: ");
        vga_puts(exception_messages[regs->int_no]);
        vga_puts(" (");
        vga_putint(regs->int_no);
        vga_puts(")\n");
        
        vga_puts("System halted.\n");
        for(;;) {
            __asm__ volatile("cli; hlt");
        }
    }
    
    // Call the registered handler if available
    if (interrupt_handlers[regs->int_no] != 0) {
        isr_handler_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    } else {
        // If no handler exists, at least report the unhandled interrupt
        vga_puts("Unhandled interrupt: ");
        vga_putint(regs->int_no);
        vga_puts("\n");
    }
}

// This gets called from our ASM IRQ handler stub
void irq_handler(registers_t *regs) {
    // Send an EOI (End of Interrupt) signal to the PICs
    if (regs->int_no >= 40) {
        // If it came from the slave PIC (IRQ8-15), send to both
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
    
    // Call the registered handler if available
    if (interrupt_handlers[regs->int_no] != 0) {
        isr_handler_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    }
}

// Register a handler for a specific ISR
void isr_register_handler(uint8_t n, isr_handler_t handler) {
    interrupt_handlers[n] = handler;
}

// Register a handler for a specific IRQ (convenience wrapper)
void irq_register_handler(uint8_t n, isr_handler_t handler) {
    isr_register_handler(n + 32, handler);
} 