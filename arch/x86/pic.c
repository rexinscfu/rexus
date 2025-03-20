#include "pic.h"
#include "io.h"

// Helper function to read from the PIC
static uint16_t pic_get_register(uint8_t command) {
    // Send command to both PICs
    outb(PIC1_COMMAND, command);
    outb(PIC2_COMMAND, command);
    
    // Read the values
    uint8_t a = inb(PIC1_COMMAND);
    uint8_t b = inb(PIC2_COMMAND);
    
    // Combine them
    return (b << 8) | a;
}

// Get the Interrupt Request Register
uint16_t pic_get_irr(void) {
    return pic_get_register(PIC_READ_IRR);
}

// Get the In-Service Register
uint16_t pic_get_isr(void) {
    return pic_get_register(PIC_READ_ISR);
}

// Remap the PICs to avoid conflicts with CPU exceptions
void pic_remap(uint8_t offset1, uint8_t offset2) {
    // Save masks
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    
    // Start initialization sequence in cascade mode
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    
    // Set vector offsets
    outb(PIC1_DATA, offset1);
    io_wait();
    outb(PIC2_DATA, offset2);
    io_wait();
    
    // Tell PICs about their cascade relationship
    outb(PIC1_DATA, 4);          // Tell Master PIC that there is a slave at IRQ2 (0000 0100)
    io_wait();
    outb(PIC2_DATA, 2);          // Tell Slave PIC its cascade identity (0000 0010)
    io_wait();
    
    // Set operation mode to 8086 mode
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();
    
    // Restore saved masks
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

// Set a specific IRQ mask (disable IRQ)
void pic_set_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        // IRQs 0-7 go to Master PIC
        port = PIC1_DATA;
    } else {
        // IRQs 8-15 go to Slave PIC
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) | (1 << irq);
    outb(port, value);
}

// Clear a specific IRQ mask (enable IRQ)
void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        // IRQs 0-7 go to Master PIC
        port = PIC1_DATA;
    } else {
        // IRQs 8-15 go to Slave PIC
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

// Disable all interrupts by masking all IRQs
void pic_disable(void) {
    // Set all bits in both data ports
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
} 