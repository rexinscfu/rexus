#include "pmm.h"
#include "../drivers/vga.h"
#include "../arch/x86/isr.h"
#include <string.h>

// Multiboot structure for memory information
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} __attribute__((packed)) multiboot_info_t;

typedef struct {
    uint32_t size;
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
} __attribute__((packed)) mmap_entry_t;

// Memory map
static uint32_t *pmm_memory_map = 0;
static uint32_t pmm_memory_map_size = 0;

// Memory tracking variables
uint32_t mem_size = 0;
uint32_t mem_blocks = 0;
uint32_t mem_used_blocks = 0;

// Actual maximum address of physical memory
static uint32_t mem_max_addr = 0;

// Set a bit in the memory map (mark as used)
void pmm_set_block(uint32_t bit) {
    pmm_memory_map[bit / 32] |= (1 << (bit % 32));
    mem_used_blocks++;
}

// Unset a bit in the memory map (mark as free)
void pmm_unset_block(uint32_t bit) {
    pmm_memory_map[bit / 32] &= ~(1 << (bit % 32));
    mem_used_blocks--;
}

// Test if a bit is set
bool pmm_test_block(uint32_t bit) {
    return pmm_memory_map[bit / 32] & (1 << (bit % 32));
}

// Find first free block(s)
int32_t pmm_find_first_free_blocks(size_t count) {
    if (count == 0) {
        return -1;
    }
    
    if (count == 1) {
        // Find a single free block
        for (uint32_t i = 0; i < mem_blocks; i++) {
            if (!pmm_test_block(i)) {
                return i;
            }
        }
    } else {
        // Find consecutive free blocks
        uint32_t free_blocks = 0;
        int32_t first_free = -1;
        
        for (uint32_t i = 0; i < mem_blocks; i++) {
            if (!pmm_test_block(i)) {
                // Found a free block
                if (free_blocks == 0) {
                    first_free = i;
                }
                
                free_blocks++;
                
                if (free_blocks == count) {
                    return first_free;
                }
            } else {
                free_blocks = 0;
                first_free = -1;
            }
        }
    }
    
    return -1; // No free blocks found
}

// Initialize the PMM with multiboot info
void pmm_init(uint32_t mboot_addr) {
    multiboot_info_t* mboot_info = (multiboot_info_t*)mboot_addr;
    
    // Check if memory map is available
    if (!(mboot_info->flags & 0x40)) {
        vga_puts("PMM: No memory map provided by bootloader!\n");
        return;
    }
    
    // Parse memory map to find max memory address
    mmap_entry_t* mmap = (mmap_entry_t*)mboot_info->mmap_addr;
    mmap_entry_t* mmap_end = (mmap_entry_t*)((uint32_t)mboot_info->mmap_addr + mboot_info->mmap_length);
    
    while (mmap < mmap_end) {
        // Type 1 is available memory
        if (mmap->type == 1) {
            uint32_t region_end = (uint32_t)(mmap->base_addr + mmap->length);
            if (region_end > mem_max_addr) {
                mem_max_addr = region_end;
            }
        }
        
        // Go to the next entry (entries can have varying sizes)
        mmap = (mmap_entry_t*)((uint32_t)mmap + mmap->size + sizeof(uint32_t));
    }
    
    // Calculate total memory size in blocks
    mem_size = mem_max_addr;
    mem_blocks = mem_size / BLOCK_SIZE;
    
    // Allocate memory for the bitmap
    pmm_memory_map_size = mem_blocks / BLOCKS_PER_BYTE;
    if (mem_blocks % BLOCKS_PER_BYTE != 0) {
        pmm_memory_map_size++;
    }
    
    // Align the memory map to a block boundary
    pmm_memory_map = (uint32_t*)((mem_max_addr + BLOCK_ALIGN - 1) & ~(BLOCK_ALIGN - 1));
    
    // Clear the memory map (mark all blocks as free)
    memset(pmm_memory_map, 0, pmm_memory_map_size);
    
    // Mark blocks used by the kernel and the PMM bitmap as used
    uint32_t kernel_end = (uint32_t)pmm_memory_map + pmm_memory_map_size;
    for (uint32_t i = 0; i < kernel_end / BLOCK_SIZE; i++) {
        pmm_set_block(i);
    }
    
    // Mark unavailable regions as used
    mmap = (mmap_entry_t*)mboot_info->mmap_addr;
    while (mmap < mmap_end) {
        if (mmap->type != 1) {
            // This region is reserved
            uint32_t block_start = (uint32_t)mmap->base_addr / BLOCK_SIZE;
            uint32_t block_end = ((uint32_t)mmap->base_addr + (uint32_t)mmap->length + BLOCK_SIZE - 1) / BLOCK_SIZE;
            
            for (uint32_t i = block_start; i < block_end && i < mem_blocks; i++) {
                pmm_set_block(i);
            }
        }
        
        mmap = (mmap_entry_t*)((uint32_t)mmap + mmap->size + sizeof(uint32_t));
    }
    
    vga_puts("PMM: Initialized, ");
    vga_putint(mem_size / 1024 / 1024);
    vga_puts(" MB, ");
    vga_putint(mem_blocks);
    vga_puts(" blocks, ");
    vga_putint(mem_blocks - mem_used_blocks);
    vga_puts(" free\n");
}

// Allocate a single physical memory block
void* pmm_alloc_block(void) {
    if (mem_used_blocks >= mem_blocks) {
        return 0; // Out of memory
    }
    
    int32_t free_block = pmm_find_first_free_blocks(1);
    if (free_block == -1) {
        return 0; // No free blocks despite counter saying otherwise
    }
    
    pmm_set_block(free_block);
    return (void*)(free_block * BLOCK_SIZE);
}

// Free a single physical memory block
void pmm_free_block(void* p) {
    uint32_t addr = (uint32_t)p;
    uint32_t block = addr / BLOCK_SIZE;
    
    if (block >= mem_blocks) {
        return; // Address out of range
    }
    
    pmm_unset_block(block);
}

// Allocate multiple contiguous physical memory blocks
void* pmm_alloc_blocks(size_t size) {
    if (mem_used_blocks + size > mem_blocks) {
        return 0; // Not enough memory
    }
    
    int32_t starting_block = pmm_find_first_free_blocks(size);
    if (starting_block == -1) {
        return 0; // No contiguous space available
    }
    
    // Mark all the blocks as used
    for (uint32_t i = 0; i < size; i++) {
        pmm_set_block(starting_block + i);
    }
    
    return (void*)(starting_block * BLOCK_SIZE);
}

// Free multiple contiguous physical memory blocks
void pmm_free_blocks(void* p, size_t size) {
    uint32_t addr = (uint32_t)p;
    uint32_t block = addr / BLOCK_SIZE;
    
    for (uint32_t i = 0; i < size && (block + i) < mem_blocks; i++) {
        pmm_unset_block(block + i);
    }
}

// Get total memory size
size_t pmm_get_memory_size(void) {
    return mem_size;
}

// Get number of free blocks
uint32_t pmm_get_free_block_count(void) {
    return mem_blocks - mem_used_blocks;
}

// Get total block count
uint32_t pmm_get_block_count(void) {
    return mem_blocks;
}

// Get used block count
uint32_t pmm_get_used_block_count(void) {
    return mem_used_blocks;
} 