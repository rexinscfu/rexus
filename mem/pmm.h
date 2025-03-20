#ifndef REXUS_PMM_H
#define REXUS_PMM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Memory constants
#define PAGE_SIZE 4096
#define BLOCKS_PER_BYTE 8
#define BLOCK_SIZE PAGE_SIZE
#define BLOCK_ALIGN BLOCK_SIZE

// Function declarations
void pmm_init(uint32_t mboot_addr);
void* pmm_alloc_block(void);
void pmm_free_block(void* p);
void* pmm_alloc_blocks(size_t size);
void pmm_free_blocks(void* p, size_t size);
size_t pmm_get_memory_size(void);
uint32_t pmm_get_free_block_count(void);
uint32_t pmm_get_block_count(void);
uint32_t pmm_get_used_block_count(void);
void pmm_set_block(uint32_t bit);
void pmm_unset_block(uint32_t bit);
bool pmm_test_block(uint32_t bit);
int32_t pmm_find_first_free_blocks(size_t count);

// Externally available memory map info
extern uint32_t mem_size;
extern uint32_t mem_blocks;
extern uint32_t mem_used_blocks;

#endif /* REXUS_PMM_H */ 