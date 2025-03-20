#include "../../core/hal.h"
#include "io.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "isr.h"
#include "../../mem/pmm.h"
#include "../../mem/vmm.h"
#include "../../drivers/vga.h"
#include <string.h>
#include <stdio.h>

// Interrupt state structure for x86
struct hal_interrupt_state {
    uint32_t eflags;
};

// Initialize the HAL for x86
void hal_init(void) {
    // Already initialized in kmain for x86:
    // - GDT, IDT, ISR, PIC, PMM, VMM are handled there
    vga_puts("HAL: Initialized x86 Hardware Abstraction Layer\n");
}

// Memory management functions (wrap existing PMM/VMM)
void* hal_alloc_page(void) {
    return pmm_alloc_block();
}

void hal_free_page(void* page) {
    pmm_free_block(page);
}

void hal_map_page(void* phys, void* virt, uint32_t flags) {
    page_dir_t* dir = vmm_get_current_directory();
    vmm_map_page(dir, (physical_addr_t)phys, (virtual_addr_t)virt, flags);
}

void hal_unmap_page(void* virt) {
    page_dir_t* dir = vmm_get_current_directory();
    vmm_unmap_page(dir, (virtual_addr_t)virt);
}

void hal_flush_tlb(void* addr) {
    vmm_flush_tlb_entry((virtual_addr_t)addr);
}

// Interrupt management
void hal_enable_interrupts(void) {
    __asm__ volatile("sti");
}

void hal_disable_interrupts(void) {
    __asm__ volatile("cli");
}

hal_interrupt_state_t hal_save_interrupt_state(void) {
    hal_interrupt_state_t state;
    __asm__ volatile(
        "pushfl\n"
        "popl %0\n"
        : "=r"(state.eflags)
    );
    return state;
}

void hal_restore_interrupt_state(hal_interrupt_state_t state) {
    __asm__ volatile(
        "pushl %0\n"
        "popfl\n"
        :
        : "r"(state.eflags)
    );
}

void hal_register_interrupt_handler(uint32_t interrupt, void (*handler)(void)) {
    // Adapt to our ISR system
    isr_register_handler(interrupt, (isr_handler_t)handler);
}

// Timer and system time
static uint64_t system_ticks = 0;
static uint32_t ticks_per_ms = 0;

static void timer_handler(registers_t* regs) {
    system_ticks++;
}

void hal_init_timer(uint32_t frequency) {
    uint32_t divisor = 1193180 / frequency;
    ticks_per_ms = frequency / 1000;
    
    // Set PIT mode 3 (square wave generator)
    outb(0x43, 0x36);
    
    // Set divisor
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
    
    // Register handler
    irq_register_handler(IRQ0, timer_handler);
    
    vga_puts("HAL: Initialized system timer\n");
}

uint64_t hal_get_system_time(void) {
    return system_ticks / ticks_per_ms;
}

void hal_sleep(uint32_t ms) {
    uint64_t target_ticks = system_ticks + (ms * ticks_per_ms);
    while (system_ticks < target_ticks) {
        __asm__ volatile("hlt");
    }
}

void hal_busy_wait(uint32_t us) {
    // Busy wait using I/O port reads (approximate timing)
    for (uint32_t i = 0; i < us; i++) {
        // Each inb takes about 1 microsecond on a typical x86 system
        inb(0x80);
    }
}

// I/O port operations (direct mapping to x86 I/O functions)
void hal_outb(uint16_t port, uint8_t value) {
    outb(port, value);
}

uint8_t hal_inb(uint16_t port) {
    return inb(port);
}

void hal_outw(uint16_t port, uint16_t value) {
    outw(port, value);
}

uint16_t hal_inw(uint16_t port) {
    return inw(port);
}

void hal_outl(uint16_t port, uint32_t value) {
    outl(port, value);
}

uint32_t hal_inl(uint16_t port) {
    return inl(port);
}

// GPIO operations - not directly applicable to x86, stubbed
void hal_gpio_set_mode(uint32_t pin, uint8_t mode) {
    // Not implemented for x86
}

void hal_gpio_write(uint32_t pin, bool value) {
    // Not implemented for x86
}

bool hal_gpio_read(uint32_t pin) {
    // Not implemented for x86
    return false;
}

void hal_gpio_toggle(uint32_t pin) {
    // Not implemented for x86
}

// UART/Serial operations for x86 COM ports
#define COM1 0x3F8

void hal_uart_init(uint32_t baud_rate) {
    uint16_t divisor = 115200 / baud_rate;
    
    // Disable interrupts
    outb(COM1 + 1, 0x00);
    
    // Set divisor latch
    outb(COM1 + 3, 0x80);
    
    // Set divisor
    outb(COM1 + 0, divisor & 0xFF);
    outb(COM1 + 1, (divisor >> 8) & 0xFF);
    
    // 8 bits, no parity, 1 stop bit
    outb(COM1 + 3, 0x03);
    
    // Enable FIFO, clear, 14-byte threshold
    outb(COM1 + 2, 0xC7);
    
    // Enable IRQs, RTS/DSR set
    outb(COM1 + 4, 0x0B);
    
    vga_puts("HAL: Initialized UART serial port\n");
}

void hal_uart_putc(char c) {
    // Wait for transmit buffer to be empty
    while ((inb(COM1 + 5) & 0x20) == 0);
    
    // Send character
    outb(COM1, c);
}

char hal_uart_getc(void) {
    // Wait for data
    while ((inb(COM1 + 5) & 0x01) == 0);
    
    // Read character
    return inb(COM1);
}

bool hal_uart_data_available(void) {
    return (inb(COM1 + 5) & 0x01) != 0;
}

void hal_uart_flush(void) {
    // Wait until transmission buffer is empty
    while ((inb(COM1 + 5) & 0x20) == 0);
}

// SPI operations - not commonly used in x86, stubbed for compatibility
void hal_spi_init(uint32_t clock_div) {
    // Not implemented for x86
}

uint8_t hal_spi_transfer(uint8_t data) {
    // Not implemented for x86
    return 0;
}

void hal_spi_chip_select(uint8_t chip, bool select) {
    // Not implemented for x86
}

// I2C operations - can be implemented via x86 PCH/SMBus, stubbed for now
void hal_i2c_init(uint32_t clock_speed) {
    // Not implemented for x86
}

bool hal_i2c_start(uint8_t address, bool read) {
    // Not implemented for x86
    return false;
}

void hal_i2c_stop(void) {
    // Not implemented for x86
}

bool hal_i2c_write(uint8_t data) {
    // Not implemented for x86
    return false;
}

uint8_t hal_i2c_read(bool ack) {
    // Not implemented for x86
    return 0;
}

// CAN bus operations - not directly applicable to x86 without hardware, stubbed
void hal_can_init(uint32_t baudrate) {
    // Not implemented for x86
}

bool hal_can_send(uint32_t id, const uint8_t* data, uint8_t length) {
    // Not implemented for x86
    return false;
}

bool hal_can_receive(uint32_t* id, uint8_t* data, uint8_t* length) {
    // Not implemented for x86
    return false;
}

bool hal_can_message_available(void) {
    // Not implemented for x86
    return false;
}

// ADC operations - not directly applicable to x86, stubbed
void hal_adc_init(void) {
    // Not implemented for x86
}

uint16_t hal_adc_read(uint8_t channel) {
    // Not implemented for x86
    return 0;
}

void hal_adc_start_conversion(uint8_t channel) {
    // Not implemented for x86
}

bool hal_adc_conversion_done(void) {
    // Not implemented for x86
    return true;
}

// PWM operations - not directly applicable to x86, stubbed
void hal_pwm_init(uint32_t frequency) {
    // Not implemented for x86
}

void hal_pwm_set_duty(uint8_t channel, uint8_t duty) {
    // Not implemented for x86
}

void hal_pwm_enable(uint8_t channel) {
    // Not implemented for x86
}

void hal_pwm_disable(uint8_t channel) {
    // Not implemented for x86
}

// Power management
void hal_enter_sleep_mode(uint8_t mode) {
    // Simple implementation for x86 - just halt
    __asm__ volatile("hlt");
}

void hal_reset(void) {
    // Reset via keyboard controller
    outb(0x64, 0xFE);
}

void hal_shutdown(void) {
    // Not fully implemented for x86 - would require ACPI
    vga_puts("HAL: Shutdown requested, halting CPU\n");
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

// Architecture-specific operations
void hal_get_platform_info(char* buffer, size_t size) {
    // Get basic CPU info from CPUID
    uint32_t eax, ebx, ecx, edx;
    char vendor[13];
    
    __asm__ volatile(
        "cpuid"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "a" (0)
    );
    
    // Extract vendor string
    *((uint32_t*)vendor) = ebx;
    *((uint32_t*)(vendor + 4)) = edx;
    *((uint32_t*)(vendor + 8)) = ecx;
    vendor[12] = '\0';
    
    // Get family/model info
    __asm__ volatile(
        "cpuid"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "a" (1)
    );
    
    uint8_t family = (eax >> 8) & 0xF;
    uint8_t model = (eax >> 4) & 0xF;
    
    snprintf(buffer, size, "x86 CPU: %s Family %d Model %d", vendor, family, model);
}

uint32_t hal_get_cpu_frequency(void) {
    // Not reliable without CPUID extended features
    // Returns a reasonable default for modern x86 CPUs
    return 2000000000; // 2 GHz
}

void hal_idle(void) {
    __asm__ volatile("hlt");
} 