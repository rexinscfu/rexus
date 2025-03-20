#include "../../core/hal.h"
#include <string.h>
#include <stdio.h>

#ifdef REXUS_ARCH_AVR
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>

// Placeholder for dynamic memory management on AVR
// Real implementation would use a custom allocator
#define AVR_HEAP_SIZE 4096
static uint8_t avr_heap[AVR_HEAP_SIZE];
static size_t avr_heap_used = 0;

// Interrupt state structure for AVR
struct hal_interrupt_state {
    uint8_t sreg;
};

// Initialize the HAL for AVR
void hal_init(void) {
    // Initialize heap
    memset(avr_heap, 0, AVR_HEAP_SIZE);
    avr_heap_used = 0;
    
    // Other AVR-specific initialization
    
    // Enable interrupts
    sei();
}

// Memory management functions for AVR (simplified)
void* hal_alloc_page(void) {
    // AVR doesn't have MMU for paging, so we allocate from our heap
    if (avr_heap_used + 256 <= AVR_HEAP_SIZE) {
        void* ptr = &avr_heap[avr_heap_used];
        avr_heap_used += 256; // Use 256-byte "pages" for AVR
        return ptr;
    }
    return NULL;
}

void hal_free_page(void* page) {
    // This simple allocator doesn't support freeing individual pages
    // A real implementation would use a proper memory manager
}

void hal_map_page(void* phys, void* virt, uint32_t flags) {
    // AVR doesn't have MMU, so mapping is a no-op
    // The physical and virtual addresses are the same
}

void hal_unmap_page(void* virt) {
    // AVR doesn't have MMU, so unmapping is a no-op
}

void hal_flush_tlb(void* addr) {
    // AVR doesn't have TLB, so flushing is a no-op
}

// Interrupt management
void hal_enable_interrupts(void) {
    sei();
}

void hal_disable_interrupts(void) {
    cli();
}

hal_interrupt_state_t hal_save_interrupt_state(void) {
    hal_interrupt_state_t state;
    state.sreg = SREG;
    return state;
}

void hal_restore_interrupt_state(hal_interrupt_state_t state) {
    SREG = state.sreg;
}

// AVR interrupt vectors are hard-coded, so we use a lookup table
typedef void (*avr_interrupt_handler_t)(void);
static avr_interrupt_handler_t avr_interrupt_handlers[32] = {0};

// Map from our generic interrupt numbers to AVR vectors
void hal_register_interrupt_handler(uint32_t interrupt, void (*handler)(void)) {
    if (interrupt < 32) {
        avr_interrupt_handlers[interrupt] = handler;
    }
}

// Timer and system time
static volatile uint64_t system_ticks = 0;

// Timer overflow interrupt
ISR(TIMER0_OVF_vect) {
    system_ticks++;
}

void hal_init_timer(uint32_t frequency) {
    // Configure Timer0 for system time
    // This is a simplified example - real code would calculate proper values
    
    // Set prescaler to 64
    TCCR0B = (1 << CS01) | (1 << CS00);
    
    // Enable overflow interrupt
    TIMSK0 = (1 << TOIE0);
}

uint64_t hal_get_system_time(void) {
    // Convert ticks to milliseconds based on timer frequency
    // For a 16MHz AVR with prescaler 64, each tick is 4us
    // 250 ticks = 1ms
    return system_ticks / 250;
}

void hal_sleep(uint32_t ms) {
    uint64_t target = hal_get_system_time() + ms;
    while (hal_get_system_time() < target) {
        // Use AVR sleep mode to save power
        sleep_mode();
    }
}

void hal_busy_wait(uint32_t us) {
    // Use AVR delay functions
    _delay_us(us);
}

// I/O port operations - AVR has different I/O mechanisms
void hal_outb(uint16_t port, uint8_t value) {
    // Map virtual port to AVR port registers
    // This is a simplified implementation
    volatile uint8_t* portx = (volatile uint8_t*)(0x20 + port);
    *portx = value;
}

uint8_t hal_inb(uint16_t port) {
    // Map virtual port to AVR port registers
    // This is a simplified implementation
    volatile uint8_t* portx = (volatile uint8_t*)(0x20 + port);
    return *portx;
}

// 16-bit and 32-bit I/O not supported on AVR, stubbed
void hal_outw(uint16_t port, uint16_t value) { }
uint16_t hal_inw(uint16_t port) { return 0; }
void hal_outl(uint16_t port, uint32_t value) { }
uint32_t hal_inl(uint16_t port) { return 0; }

// GPIO operations for AVR
#define GPIO_MODE_INPUT 0
#define GPIO_MODE_OUTPUT 1
#define GPIO_MODE_INPUT_PULLUP 2

void hal_gpio_set_mode(uint32_t pin, uint8_t mode) {
    uint8_t port = (pin >> 4) & 0xF;
    uint8_t pin_num = pin & 0xF;
    
    volatile uint8_t* ddr = (volatile uint8_t*)(0x21 + (port * 3)); // DDRx
    volatile uint8_t* port_reg = (volatile uint8_t*)(0x22 + (port * 3)); // PORTx
    
    switch (mode) {
        case GPIO_MODE_INPUT:
            *ddr &= ~(1 << pin_num);
            *port_reg &= ~(1 << pin_num); // No pull-up
            break;
        case GPIO_MODE_OUTPUT:
            *ddr |= (1 << pin_num);
            break;
        case GPIO_MODE_INPUT_PULLUP:
            *ddr &= ~(1 << pin_num);
            *port_reg |= (1 << pin_num); // Enable pull-up
            break;
    }
}

void hal_gpio_write(uint32_t pin, bool value) {
    uint8_t port = (pin >> 4) & 0xF;
    uint8_t pin_num = pin & 0xF;
    
    volatile uint8_t* port_reg = (volatile uint8_t*)(0x22 + (port * 3)); // PORTx
    
    if (value) {
        *port_reg |= (1 << pin_num);
    } else {
        *port_reg &= ~(1 << pin_num);
    }
}

bool hal_gpio_read(uint32_t pin) {
    uint8_t port = (pin >> 4) & 0xF;
    uint8_t pin_num = pin & 0xF;
    
    volatile uint8_t* pin_reg = (volatile uint8_t*)(0x20 + (port * 3)); // PINx
    
    return (*pin_reg & (1 << pin_num)) != 0;
}

void hal_gpio_toggle(uint32_t pin) {
    uint8_t port = (pin >> 4) & 0xF;
    uint8_t pin_num = pin & 0xF;
    
    volatile uint8_t* pin_reg = (volatile uint8_t*)(0x20 + (port * 3)); // PINx
    
    // AVR has atomic toggle with PIN register write
    *pin_reg = (1 << pin_num);
}

// UART operations for AVR
void hal_uart_init(uint32_t baud_rate) {
    // Calculate UBRR value for the given baud rate (assuming 16MHz clock)
    uint16_t ubrr = F_CPU / 16 / baud_rate - 1;
    
    // Set baud rate
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    
    // Enable receiver and transmitter
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    
    // Set frame format: 8 data bits, 1 stop bit, no parity
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void hal_uart_putc(char c) {
    // Wait for transmit buffer to be empty
    while (!(UCSR0A & (1 << UDRE0)));
    
    // Send character
    UDR0 = c;
}

char hal_uart_getc(void) {
    // Wait for data
    while (!(UCSR0A & (1 << RXC0)));
    
    // Read character
    return UDR0;
}

bool hal_uart_data_available(void) {
    return (UCSR0A & (1 << RXC0)) != 0;
}

void hal_uart_flush(void) {
    // Wait until transmission complete
    while (!(UCSR0A & (1 << TXC0)));
}

// SPI operations for AVR
void hal_spi_init(uint32_t clock_div) {
    // Configure SPI pins
    // SS (PB2) as output
    DDRB |= (1 << DDB2);
    // MOSI (PB3) as output
    DDRB |= (1 << DDB3);
    // MISO (PB4) as input
    DDRB &= ~(1 << DDB4);
    // SCK (PB5) as output
    DDRB |= (1 << DDB5);
    
    // Enable SPI, Master mode, set clock rate
    SPCR = (1 << SPE) | (1 << MSTR) | ((clock_div & 0x03) << SPR0);
    
    // If highest speed selected (SPI2X)
    if (clock_div & 0x04) {
        SPSR = (1 << SPI2X);
    }
}

uint8_t hal_spi_transfer(uint8_t data) {
    // Start transmission
    SPDR = data;
    
    // Wait for transmission complete
    while (!(SPSR & (1 << SPIF)));
    
    // Return received data
    return SPDR;
}

void hal_spi_chip_select(uint8_t chip, bool select) {
    // Simple implementation for a single CS pin (SS/PB2)
    if (select) {
        PORTB &= ~(1 << PB2); // Active low
    } else {
        PORTB |= (1 << PB2);
    }
}

// I2C operations for AVR (TWI interface)
void hal_i2c_init(uint32_t clock_speed) {
    // Calculate TWI bit rate prescaler
    uint8_t twbr = ((F_CPU / clock_speed) - 16) / 2;
    
    // Set bit rate
    TWBR = twbr;
    
    // Enable TWI
    TWCR = (1 << TWEN);
}

bool hal_i2c_start(uint8_t address, bool read) {
    // Send START condition
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    
    // Wait for TWINT flag set
    while (!(TWCR & (1 << TWINT)));
    
    // Check if START was sent successfully
    if ((TWSR & 0xF8) != 0x08) {
        return false;
    }
    
    // Send address with read/write bit
    TWDR = (address << 1) | (read ? 1 : 0);
    TWCR = (1 << TWINT) | (1 << TWEN);
    
    // Wait for TWINT flag set
    while (!(TWCR & (1 << TWINT)));
    
    // Check if address was acknowledged
    return (TWSR & 0xF8) == (read ? 0x40 : 0x18);
}

void hal_i2c_stop(void) {
    // Send STOP condition
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

bool hal_i2c_write(uint8_t data) {
    // Send data
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    
    // Wait for TWINT flag set
    while (!(TWCR & (1 << TWINT)));
    
    // Check if data was acknowledged
    return (TWSR & 0xF8) == 0x28;
}

uint8_t hal_i2c_read(bool ack) {
    // Control byte with or without ACK
    TWCR = (1 << TWINT) | (1 << TWEN) | (ack ? (1 << TWEA) : 0);
    
    // Wait for TWINT flag set
    while (!(TWCR & (1 << TWINT)));
    
    // Return received data
    return TWDR;
}

// CAN bus operations - not directly supported on ATmega, would need external controller
void hal_can_init(uint32_t baudrate) {
    // Not implemented on AVR ATmega
}

bool hal_can_send(uint32_t id, const uint8_t* data, uint8_t length) {
    // Not implemented on AVR ATmega
    return false;
}

bool hal_can_receive(uint32_t* id, uint8_t* data, uint8_t* length) {
    // Not implemented on AVR ATmega
    return false;
}

bool hal_can_message_available(void) {
    // Not implemented on AVR ATmega
    return false;
}

// ADC operations for AVR
void hal_adc_init(void) {
    // Set ADC reference to AVCC
    ADMUX = (1 << REFS0);
    
    // Enable ADC and set prescaler to 128 (125kHz at 16MHz)
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t hal_adc_read(uint8_t channel) {
    // Select ADC channel
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    
    // Start conversion
    ADCSRA |= (1 << ADSC);
    
    // Wait for conversion to complete
    while (ADCSRA & (1 << ADSC));
    
    // Return result
    return ADC;
}

void hal_adc_start_conversion(uint8_t channel) {
    // Select ADC channel
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    
    // Start conversion
    ADCSRA |= (1 << ADSC);
}

bool hal_adc_conversion_done(void) {
    // Check if conversion is complete
    return (ADCSRA & (1 << ADSC)) == 0;
}

// PWM operations for AVR using Timer1
void hal_pwm_init(uint32_t frequency) {
    // Configure Timer1 for PWM
    // Fast PWM mode, 8-bit
    TCCR1A = (1 << WGM10);
    
    // Calculate prescaler based on frequency
    uint8_t prescaler = 0;
    
    if (frequency <= 30) {
        // Prescaler 1024
        prescaler = (1 << CS12) | (1 << CS10);
    } else if (frequency <= 250) {
        // Prescaler 256
        prescaler = (1 << CS12);
    } else if (frequency <= 2000) {
        // Prescaler 64
        prescaler = (1 << CS11) | (1 << CS10);
    } else if (frequency <= 8000) {
        // Prescaler 8
        prescaler = (1 << CS11);
    } else {
        // Prescaler 1
        prescaler = (1 << CS10);
    }
    
    // Set prescaler
    TCCR1B = (TCCR1B & ~((1 << CS12) | (1 << CS11) | (1 << CS10))) | prescaler;
}

void hal_pwm_set_duty(uint8_t channel, uint8_t duty) {
    // Set duty cycle (0-255)
    switch (channel) {
        case 0: // OC1A (PB1)
            OCR1A = duty;
            break;
        case 1: // OC1B (PB2)
            OCR1B = duty;
            break;
    }
}

void hal_pwm_enable(uint8_t channel) {
    // Configure channels as output
    DDRB |= (1 << (1 + channel)); // PB1 or PB2
    
    // Enable PWM output
    switch (channel) {
        case 0: // OC1A (PB1)
            TCCR1A |= (1 << COM1A1);
            break;
        case 1: // OC1B (PB2)
            TCCR1A |= (1 << COM1B1);
            break;
    }
}

void hal_pwm_disable(uint8_t channel) {
    // Disable PWM output
    switch (channel) {
        case 0: // OC1A (PB1)
            TCCR1A &= ~(1 << COM1A1);
            break;
        case 1: // OC1B (PB2)
            TCCR1A &= ~(1 << COM1B1);
            break;
    }
}

// Power management for AVR
void hal_enter_sleep_mode(uint8_t mode) {
    // Set sleep mode
    set_sleep_mode(mode);
    
    // Enable sleep
    sleep_enable();
    
    // Enter sleep mode
    sleep_cpu();
    
    // Execution resumes here after wake-up
    sleep_disable();
}

void hal_reset(void) {
    // Reset using watchdog
    wdt_enable(WDTO_15MS);
    while (1) { }
}

void hal_shutdown(void) {
    // Simplest form of shutdown on AVR is to enter sleep mode
    // with all peripherals disabled
    
    // Disable all peripherals
    ADCSRA = 0; // Disable ADC
    power_all_disable(); // Disable all modules
    
    // Enter power-down sleep mode
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_cpu();
    
    // Should never reach here
    while (1) { }
}

// Architecture-specific operations
void hal_get_platform_info(char* buffer, size_t size) {
    // Get AVR info
    snprintf(buffer, size, "AVR ATmega MCU @ %d MHz", F_CPU / 1000000);
}

uint32_t hal_get_cpu_frequency(void) {
    return F_CPU;
}

void hal_idle(void) {
    // Use AVR sleep mode for idle
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_mode();
}

#endif /* REXUS_ARCH_AVR */ 