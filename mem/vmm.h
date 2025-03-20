#ifndef REXUS_VMM_H
#define REXUS_VMM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Page directory/table entry flags
#define VMM_PRESENT        0x01
#define VMM_WRITABLE       0x02
#define VMM_USER           0x04
#define VMM_WRITE_THROUGH  0x08
#define VMM_CACHE_DISABLE  0x10
#define VMM_ACCESSED       0x20
#define VMM_DIRTY          0x40
#define VMM_PAGE_SIZE      0x80
#define VMM_GLOBAL         0x100

// Virtual memory constants
#define PAGE_DIR_INDEX(x) (((x) >> 22) & 0x3FF)
#define PAGE_TABLE_INDEX(x) (((x) >> 12) & 0x3FF)
#define PAGE_OFFSET(x) ((x) & 0xFFF)

typedef uint32_t page_dir_entry_t;
typedef uint32_t page_table_entry_t;
typedef uint32_t virtual_addr_t;
typedef uint32_t physical_addr_t;

// Page table structure (1024 entries, each mapping 4KB)
typedef struct {
    page_table_entry_t entries[1024];
} page_table_t;

// Page directory structure (1024 entries, each pointing to a page table)
typedef struct {
    page_dir_entry_t entries[1024];
} page_dir_t;

// Function declarations
void vmm_init(void);
bool vmm_map_page(page_dir_t* dir, physical_addr_t phys, virtual_addr_t virt, uint32_t flags);
bool vmm_unmap_page(page_dir_t* dir, virtual_addr_t virt);
bool vmm_get_mapping(page_dir_t* dir, virtual_addr_t virt, physical_addr_t* phys);
void vmm_switch_page_directory(page_dir_t* dir);
page_dir_t* vmm_get_current_directory(void);
page_dir_t* vmm_create_directory(void);
void vmm_free_directory(page_dir_t* dir);
void vmm_identity_map(page_dir_t* dir, physical_addr_t start, physical_addr_t end, uint32_t flags);
page_table_t* vmm_get_page_table(page_dir_t* dir, uint32_t idx, bool allocate);

// Page fault handler
void page_fault_handler(void);

// Clone a directory (for fork/process creation)
page_dir_t* vmm_clone_directory(page_dir_t* src);

// Flush a TLB entry
void vmm_flush_tlb_entry(virtual_addr_t addr);

#endif /* REXUS_VMM_H */ 