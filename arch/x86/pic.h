#ifndef REXUS_PIC_H
#define REXUS_PIC_H

#include <stdint.h>

// PIC I/O ports
#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1

// PIC commands
#define PIC_EOI         0x20    // End of interrupt command
#define PIC_READ_IRR    0x0A    // Read Interrupt Request Register
#define PIC_READ_ISR    0x0B    // Read In-Service Register

// ICW1 commands
#define ICW1_ICW4       0x01    // ICW4 will be sent
#define ICW1_SINGLE     0x02    // Single (cascade) mode
#define ICW1_INTERVAL4  0x04    // Call address interval 4 (8)
#define ICW1_LEVEL      0x08    // Level triggered (edge) mode
#define ICW1_INIT       0x10    // Initialization

// ICW4 commands
#define ICW4_8086       0x01    // 8086/88 mode
#define ICW4_AUTO       0x02    // Auto EOI
#define ICW4_BUF_SLAVE  0x08    // Buffered mode/slave
#define ICW4_BUF_MASTER 0x0C    // Buffered mode/master
#define ICW4_SFNM       0x10    // Special fully nested mode

// Function declarations
void pic_remap(uint8_t offset1, uint8_t offset2);
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);
uint16_t pic_get_irr(void);
uint16_t pic_get_isr(void);

#endif /* REXUS_PIC_H */ 