#ifndef REXUS_HAL_H
#define REXUS_HAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Architecture detection macros
#if defined(__i386__) || defined(__i686__) || defined(_M_IX86)
    #define REXUS_ARCH_X86
#elif defined(__arm__) || defined(_M_ARM) || defined(__aarch64__)
    #define REXUS_ARCH_ARM
#elif defined(__AVR__)
    #define REXUS_ARCH_AVR
#else
    #error "Unsupported architecture"
#endif

// Forward declarations for architecture-specific structures
typedef struct hal_context hal_context_t;
typedef struct hal_interrupt_state hal_interrupt_state_t;

// HAL initialization
void hal_init(void);

// Memory management
void* hal_alloc_page(void);
void hal_free_page(void* page);
void hal_map_page(void* phys, void* virt, uint32_t flags);
void hal_unmap_page(void* virt);
void hal_flush_tlb(void* addr);

// Interrupt management
void hal_enable_interrupts(void);
void hal_disable_interrupts(void);
hal_interrupt_state_t hal_save_interrupt_state(void);
void hal_restore_interrupt_state(hal_interrupt_state_t state);
void hal_register_interrupt_handler(uint32_t interrupt, void (*handler)(void));

// Timer and clock management
void hal_init_timer(uint32_t frequency);
uint64_t hal_get_system_time(void);
void hal_sleep(uint32_t ms);
void hal_busy_wait(uint32_t us);

// I/O operations
void hal_outb(uint16_t port, uint8_t value);
uint8_t hal_inb(uint16_t port);
void hal_outw(uint16_t port, uint16_t value);
uint16_t hal_inw(uint16_t port);
void hal_outl(uint16_t port, uint32_t value);
uint32_t hal_inl(uint16_t port);

// GPIO operations (for embedded platforms)
void hal_gpio_set_mode(uint32_t pin, uint8_t mode);
void hal_gpio_write(uint32_t pin, bool value);
bool hal_gpio_read(uint32_t pin);
void hal_gpio_toggle(uint32_t pin);

// UART/Serial operations
void hal_uart_init(uint32_t baud_rate);
void hal_uart_putc(char c);
char hal_uart_getc(void);
bool hal_uart_data_available(void);
void hal_uart_flush(void);

// SPI operations
void hal_spi_init(uint32_t clock_div);
uint8_t hal_spi_transfer(uint8_t data);
void hal_spi_chip_select(uint8_t chip, bool select);

// I2C operations
void hal_i2c_init(uint32_t clock_speed);
bool hal_i2c_start(uint8_t address, bool read);
void hal_i2c_stop(void);
bool hal_i2c_write(uint8_t data);
uint8_t hal_i2c_read(bool ack);

// CAN bus operations (for automotive)
void hal_can_init(uint32_t baudrate);
bool hal_can_send(uint32_t id, const uint8_t* data, uint8_t length);
bool hal_can_receive(uint32_t* id, uint8_t* data, uint8_t* length);
bool hal_can_message_available(void);

// ADC operations
void hal_adc_init(void);
uint16_t hal_adc_read(uint8_t channel);
void hal_adc_start_conversion(uint8_t channel);
bool hal_adc_conversion_done(void);

// PWM operations
void hal_pwm_init(uint32_t frequency);
void hal_pwm_set_duty(uint8_t channel, uint8_t duty);
void hal_pwm_enable(uint8_t channel);
void hal_pwm_disable(uint8_t channel);

// Power management
void hal_enter_sleep_mode(uint8_t mode);
void hal_reset(void);
void hal_shutdown(void);

// Architecture-specific operations
void hal_get_platform_info(char* buffer, size_t size);
uint32_t hal_get_cpu_frequency(void);
void hal_idle(void);

#endif /* REXUS_HAL_H */ 