#ifndef REXUS_IO_H
#define REXUS_IO_H

#include <stdint.h>

// Write a byte to an I/O port
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port) : "memory");
}

// Read a byte from an I/O port
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

// Write a word to an I/O port
static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port) : "memory");
}

// Read a word from an I/O port
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

// Write a double word to an I/O port
static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port) : "memory");
}

// Read a double word from an I/O port
static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

// I/O wait - small delay for I/O operations
static inline void io_wait(void) {
    outb(0x80, 0);
}

// Read multiple bytes from an I/O port
static inline void insw(uint16_t port, void* addr, uint32_t count) {
    __asm__ volatile ("rep insw" : "+D"(addr), "+c"(count) : "d"(port) : "memory");
}

// Write multiple bytes to an I/O port
static inline void outsw(uint16_t port, const void* addr, uint32_t count) {
    __asm__ volatile ("rep outsw" : "+S"(addr), "+c"(count) : "d"(port) : "memory");
}

#endif /* REXUS_IO_H */ 