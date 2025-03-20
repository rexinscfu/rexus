#include "vmm.h"
#include "pmm.h"
#include "../arch/x86/isr.h"
#include "../drivers/vga.h"
#include <string.h>

// The kernel's page directory
static page_dir_t* kernel_directory = NULL;

// Current page directory
static page_dir_t* current_directory = NULL;

// Forward declarations for assembly functions
extern void enable_paging(physical_addr_t page_dir);
extern void load_page_directory(physical_addr_t page_dir);

// Flush the TLB entry for a virtual address
void vmm_flush_tlb_entry(virtual_addr_t addr) {
    __asm__ volatile("invlpg (%0)" : : "r" (addr) : "memory");
}

// Page fault handler
static void page_fault(registers_t* regs) {
    uint32_t fault_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r" (fault_addr));
    
    int present = !(regs->err_code & 0x1);
    int rw = regs->err_code & 0x2;
    int us = regs->err_code & 0x4;
    int reserved = regs->err_code & 0x8;
    
    vga_puts("PAGE FAULT at 0x");
    vga_puthex(fault_addr);
    vga_puts(" (");
    
    if (present) vga_puts("not-present ");
    if (rw) vga_puts("read-only ");
    if (us) vga_puts("user-mode ");
    if (reserved) vga_puts("reserved ");
    
    vga_puts(")\n");
    
    for(;;) {
        __asm__ volatile("cli; hlt");
    }
}

// Get a page table from a directory
page_table_t* vmm_get_page_table(page_dir_t* dir, uint32_t idx, bool allocate) {
    if (idx >= 1024) {
        return NULL;
    }
    
    if (dir->entries[idx] & VMM_PRESENT) {
        // Page table already exists
        return (page_table_t*)(dir->entries[idx] & ~0xFFF);
    } else if (allocate) {
        // Allocate a new page table
        void* page_table = pmm_alloc_block();
        if (page_table == NULL) {
            return NULL;
        }
        
        // Clear the table
        memset(page_table, 0, sizeof(page_table_t));
        
        // Add it to the directory
        dir->entries[idx] = ((uint32_t)page_table) | VMM_PRESENT | VMM_WRITABLE | VMM_USER;
        
        return (page_table_t*)page_table;
    }
    
    return NULL;
}

// Initialize virtual memory
void vmm_init(void) {
    // Register page fault handler
    isr_register_handler(14, page_fault);
    
    // Create kernel page directory
    kernel_directory = vmm_create_directory();
    if (!kernel_directory) {
        vga_puts("VMM: Failed to create kernel page directory\n");
        return;
    }
    
    // Identity map the first 4MB (kernel space)
    // This is already done by boot.asm for startup, but now we need to
    // properly set it up in our page directory
    vmm_identity_map(kernel_directory, 0, 4 * 1024 * 1024, VMM_PRESENT | VMM_WRITABLE);
    
    // Identity map the memory where kernel is loaded (typically 1MB+)
    vmm_identity_map(kernel_directory, 1 * 1024 * 1024, 10 * 1024 * 1024, VMM_PRESENT | VMM_WRITABLE);
    
    // Map kernel to higher half (3GB+)
    for (uint32_t i = 0; i < 10 * 1024 * 1024; i += PAGE_SIZE) {
        vmm_map_page(kernel_directory, i, i + 0xC0000000, VMM_PRESENT | VMM_WRITABLE);
    }
    
    // Switch to the kernel directory
    vmm_switch_page_directory(kernel_directory);
    
    vga_puts("VMM: Initialized virtual memory manager\n");
}

// Create a new page directory
page_dir_t* vmm_create_directory(void) {
    page_dir_t* dir = (page_dir_t*)pmm_alloc_block();
    if (!dir) {
        return NULL;
    }
    
    // Clear the directory
    memset(dir, 0, sizeof(page_dir_t));
    
    return dir;
}

// Map a physical page to a virtual address
bool vmm_map_page(page_dir_t* dir, physical_addr_t phys, virtual_addr_t virt, uint32_t flags) {
    // Make sure addresses are page-aligned
    phys &= ~0xFFF;
    virt &= ~0xFFF;
    
    uint32_t pdidx = PAGE_DIR_INDEX(virt);
    uint32_t ptidx = PAGE_TABLE_INDEX(virt);
    
    // Get the page table, create if not exists
    page_table_t* table = vmm_get_page_table(dir, pdidx, true);
    if (!table) {
        return false;
    }
    
    // Set up the page table entry
    table->entries[ptidx] = phys | (flags & 0xFFF);
    
    // Flush TLB for this address if it's in the current directory
    if (dir == current_directory) {
        vmm_flush_tlb_entry(virt);
    }
    
    return true;
}

// Unmap a virtual address
bool vmm_unmap_page(page_dir_t* dir, virtual_addr_t virt) {
    uint32_t pdidx = PAGE_DIR_INDEX(virt);
    uint32_t ptidx = PAGE_TABLE_INDEX(virt);
    
    // Get the page table
    page_table_t* table = vmm_get_page_table(dir, pdidx, false);
    if (!table) {
        return false;
    }
    
    // Clear the entry
    table->entries[ptidx] = 0;
    
    // Flush TLB for this address if it's in the current directory
    if (dir == current_directory) {
        vmm_flush_tlb_entry(virt);
    }
    
    return true;
}

// Get physical address from virtual address
bool vmm_get_mapping(page_dir_t* dir, virtual_addr_t virt, physical_addr_t* phys) {
    uint32_t pdidx = PAGE_DIR_INDEX(virt);
    uint32_t ptidx = PAGE_TABLE_INDEX(virt);
    uint32_t offset = PAGE_OFFSET(virt);
    
    // Get the page table
    page_table_t* table = vmm_get_page_table(dir, pdidx, false);
    if (!table) {
        return false;
    }
    
    // Check if the page is present
    if (!(table->entries[ptidx] & VMM_PRESENT)) {
        return false;
    }
    
    // Get the physical address
    if (phys) {
        *phys = (table->entries[ptidx] & ~0xFFF) + offset;
    }
    
    return true;
}

// Switch to a page directory
void vmm_switch_page_directory(page_dir_t* dir) {
    if (!dir) {
        return;
    }
    
    current_directory = dir;
    
    // Load the page directory
    load_page_directory((physical_addr_t)dir);
}

// Get the current page directory
page_dir_t* vmm_get_current_directory(void) {
    return current_directory;
}

// Free a page directory
void vmm_free_directory(page_dir_t* dir) {
    if (!dir) {
        return;
    }
    
    // Free all page tables
    for (uint32_t i = 0; i < 1024; i++) {
        if (dir->entries[i] & VMM_PRESENT) {
            page_table_t* table = (page_table_t*)(dir->entries[i] & ~0xFFF);
            pmm_free_block(table);
        }
    }
    
    // Free the directory itself
    pmm_free_block(dir);
}

// Identity map a range of physical memory
void vmm_identity_map(page_dir_t* dir, physical_addr_t start, physical_addr_t end, uint32_t flags) {
    start &= ~0xFFF;  // Align to page boundary
    end = (end + 0xFFF) & ~0xFFF;  // Round up to page boundary
    
    for (physical_addr_t addr = start; addr < end; addr += PAGE_SIZE) {
        vmm_map_page(dir, addr, addr, flags);
    }
}

// Clone a page directory (for process creation/fork)
page_dir_t* vmm_clone_directory(page_dir_t* src) {
    // Create a new directory
    page_dir_t* dest = vmm_create_directory();
    if (!dest) {
        return NULL;
    }
    
    // Clone page tables
    for (uint32_t i = 0; i < 1024; i++) {
        if (src->entries[i] & VMM_PRESENT) {
            if (i >= 768) {
                // Entries 768+ (3GB+) are kernel space, just copy them
                dest->entries[i] = src->entries[i];
            } else {
                // User space - create a new page table
                page_table_t* src_table = (page_table_t*)(src->entries[i] & ~0xFFF);
                page_table_t* dest_table = vmm_get_page_table(dest, i, true);
                
                if (!dest_table) {
                    vmm_free_directory(dest);
                    return NULL;
                }
                
                // Copy the page table entries
                for (uint32_t j = 0; j < 1024; j++) {
                    if (src_table->entries[j] & VMM_PRESENT) {
                        // Get the physical address from the source
                        physical_addr_t phys = src_table->entries[j] & ~0xFFF;
                        
                        // If it's writable, we need to copy the page
                        if (src_table->entries[j] & VMM_WRITABLE) {
                            // Allocate a new physical page
                            void* new_page = pmm_alloc_block();
                            if (!new_page) {
                                vmm_free_directory(dest);
                                return NULL;
                            }
                            
                            // Copy the data
                            virtual_addr_t virt = (i << 22) | (j << 12);
                            memcpy(new_page, (void*)virt, PAGE_SIZE);
                            
                            // Map it
                            dest_table->entries[j] = ((uint32_t)new_page) | (src_table->entries[j] & 0xFFF);
                        } else {
                            // Read-only pages can be shared
                            dest_table->entries[j] = src_table->entries[j];
                        }
                    }
                }
            }
        }
    }
    
    return dest;
} 