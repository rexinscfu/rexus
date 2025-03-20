#include "../../core/hal.h"
#include <string.h>
#include <stdio.h>

#ifdef REXUS_ARCH_ARM

#include "stm32f4xx.h"

struct hal_interrupt_state {
    uint32_t primask;
};

void hal_init(void) {
    SystemInit();
    SysTick_Config(SystemCoreClock / 1000);
    __enable_irq();
}

void* hal_alloc_page(void) {
    return NULL;
}

void hal_free_page(void* page) {
}

void hal_map_page(void* phys, void* virt, uint32_t flags) {
}

void hal_unmap_page(void* virt) {
}

void hal_flush_tlb(void* addr) {
    __DSB();
    __ISB();
}

void hal_enable_interrupts(void) {
    __enable_irq();
}

void hal_disable_interrupts(void) {
    __disable_irq();
}

hal_interrupt_state_t hal_save_interrupt_state(void) {
    hal_interrupt_state_t state;
    state.primask = __get_PRIMASK();
    __disable_irq();
    return state;
}

void hal_restore_interrupt_state(hal_interrupt_state_t state) {
    __set_PRIMASK(state.primask);
}

void hal_register_interrupt_handler(uint32_t interrupt, void (*handler)(void)) {
    NVIC_SetVector(interrupt, (uint32_t)handler);
    NVIC_EnableIRQ(interrupt);
}

static volatile uint64_t system_ticks = 0;

void SysTick_Handler(void) {
    system_ticks++;
}

void hal_init_timer(uint32_t frequency) {
    SysTick_Config(SystemCoreClock / frequency);
}

uint64_t hal_get_system_time(void) {
    return system_ticks;
}

void hal_sleep(uint32_t ms) {
    uint64_t target = system_ticks + ms;
    while (system_ticks < target) {
        __WFI();
    }
}

void hal_busy_wait(uint32_t us) {
    uint32_t cycles = (SystemCoreClock / 1000000) * us;
    while (cycles--) {
        __NOP();
    }
}

void hal_gpio_set_mode(uint32_t pin, uint8_t mode) {
    GPIO_TypeDef* port = (GPIO_TypeDef*)(GPIOA_BASE + (pin >> 4) * 0x400);
    uint8_t pin_num = pin & 0xF;
    
    RCC->AHB1ENR |= (1 << (pin >> 4));
    
    port->MODER &= ~(3UL << (pin_num * 2));
    port->MODER |= (mode & 3UL) << (pin_num * 2);
}

void hal_gpio_write(uint32_t pin, bool value) {
    GPIO_TypeDef* port = (GPIO_TypeDef*)(GPIOA_BASE + (pin >> 4) * 0x400);
    uint8_t pin_num = pin & 0xF;
    
    if (value) {
        port->BSRR = (1UL << pin_num);
    } else {
        port->BSRR = (1UL << (pin_num + 16));
    }
}

bool hal_gpio_read(uint32_t pin) {
    GPIO_TypeDef* port = (GPIO_TypeDef*)(GPIOA_BASE + (pin >> 4) * 0x400);
    uint8_t pin_num = pin & 0xF;
    
    return (port->IDR & (1UL << pin_num)) != 0;
}

void hal_gpio_toggle(uint32_t pin) {
    GPIO_TypeDef* port = (GPIO_TypeDef*)(GPIOA_BASE + (pin >> 4) * 0x400);
    uint8_t pin_num = pin & 0xF;
    
    port->ODR ^= (1UL << pin_num);
}

void hal_uart_init(uint32_t baud_rate) {
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    
    USART1->BRR = SystemCoreClock / baud_rate;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void hal_uart_putc(char c) {
    while (!(USART1->SR & USART_SR_TXE));
    USART1->DR = c;
}

char hal_uart_getc(void) {
    while (!(USART1->SR & USART_SR_RXNE));
    return USART1->DR;
}

bool hal_uart_data_available(void) {
    return (USART1->SR & USART_SR_RXNE) != 0;
}

void hal_uart_flush(void) {
    while (!(USART1->SR & USART_SR_TC));
}

void hal_spi_init(uint32_t clock_div) {
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSI | SPI_CR1_SSM |
                ((clock_div & 7) << 3) | SPI_CR1_SPE;
}

uint8_t hal_spi_transfer(uint8_t data) {
    while (!(SPI1->SR & SPI_SR_TXE));
    SPI1->DR = data;
    while (!(SPI1->SR & SPI_SR_RXNE));
    return SPI1->DR;
}

void hal_spi_chip_select(uint8_t chip, bool select) {
    hal_gpio_write(chip, !select);
}

void hal_i2c_init(uint32_t clock_speed) {
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    
    I2C1->CR2 = (SystemCoreClock / 1000000);
    I2C1->CCR = SystemCoreClock / (clock_speed * 2);
    I2C1->TRISE = ((SystemCoreClock / 1000000) + 1);
    I2C1->CR1 |= I2C_CR1_PE;
}

bool hal_i2c_start(uint8_t address, bool read) {
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));
    
    I2C1->DR = (address << 1) | read;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    volatile uint32_t tmp = I2C1->SR2;
    
    return true;
}

void hal_i2c_stop(void) {
    I2C1->CR1 |= I2C_CR1_STOP;
}

bool hal_i2c_write(uint8_t data) {
    while (!(I2C1->SR1 & I2C_SR1_TXE));
    I2C1->DR = data;
    while (!(I2C1->SR1 & I2C_SR1_BTF));
    return true;
}

uint8_t hal_i2c_read(bool ack) {
    if (!ack) {
        I2C1->CR1 &= ~I2C_CR1_ACK;
    }
    
    while (!(I2C1->SR1 & I2C_SR1_RXNE));
    uint8_t data = I2C1->DR;
    
    if (!ack) {
        I2C1->CR1 |= I2C_CR1_ACK;
    }
    
    return data;
}

void hal_adc_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    ADC1->CR2 |= ADC_CR2_ADON;
}

uint16_t hal_adc_read(uint8_t channel) {
    ADC1->SQR3 = channel;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while (!(ADC1->SR & ADC_SR_EOC));
    return ADC1->DR;
}

void hal_adc_start_conversion(uint8_t channel) {
    ADC1->SQR3 = channel;
    ADC1->CR2 |= ADC_CR2_SWSTART;
}

bool hal_adc_conversion_done(void) {
    return (ADC1->SR & ADC_SR_EOC) != 0;
}

void hal_pwm_init(uint32_t frequency) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    
    uint32_t prescaler = SystemCoreClock / frequency / 256;
    TIM2->PSC = prescaler - 1;
    TIM2->ARR = 255;
    TIM2->CR1 |= TIM_CR1_CEN;
}

void hal_pwm_set_duty(uint8_t channel, uint8_t duty) {
    switch (channel) {
        case 0: TIM2->CCR1 = duty; break;
        case 1: TIM2->CCR2 = duty; break;
        case 2: TIM2->CCR3 = duty; break;
        case 3: TIM2->CCR4 = duty; break;
    }
}

void hal_pwm_enable(uint8_t channel) {
    TIM2->CCER |= (1 << (channel * 4));
}

void hal_pwm_disable(uint8_t channel) {
    TIM2->CCER &= ~(1 << (channel * 4));
}

void hal_enter_sleep_mode(uint8_t mode) {
    SCB->SCR = mode;
    __WFI();
}

void hal_reset(void) {
    NVIC_SystemReset();
}

void hal_shutdown(void) {
    __disable_irq();
    PWR->CR |= PWR_CR_PDDS;
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    __WFI();
}

void hal_get_platform_info(char* buffer, size_t size) {
    snprintf(buffer, size, "STM32F4 ARM Cortex-M4 @ %lu MHz", SystemCoreClock / 1000000);
}

uint32_t hal_get_cpu_frequency(void) {
    return SystemCoreClock;
}

void hal_idle(void) {
    __WFI();
}

#endif /* REXUS_ARCH_ARM */ 