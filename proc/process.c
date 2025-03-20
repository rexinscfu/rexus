#include "process.h"
#include "../arch/x86/gdt.h"
#include "../mem/pmm.h"
#include "../drivers/vga.h"
#include <string.h>

// Forward declarations
static int process_idle(void* arg);

// Process list and current process
static process_t* process_list = NULL;
static process_t* current_process = NULL;

// Next available process ID
static uint32_t next_pid = 1;

// System time (ms)
static uint32_t system_time = 0;

// Initialize process management
void process_init(void) {
    // Register timer tick handler for task switching
    irq_register_handler(IRQ0, process_timer_tick);
    
    // Create idle process (pid 0)
    process_t* idle = (process_t*)pmm_alloc_block();
    memset(idle, 0, sizeof(process_t));
    
    strcpy(idle->name, "idle");
    idle->pid = 0;
    idle->state = PROCESS_STATE_RUNNING;
    idle->priority = PROCESS_PRIORITY_LOW;
    idle->page_directory = vmm_get_current_directory();
    
    // Set up a small kernel stack for the idle process
    idle->stack_size = 4096;
    idle->stack = (uint32_t)pmm_alloc_block();
    idle->esp = idle->stack + idle->stack_size;
    
    // Set initial values for registers
    idle->ebp = idle->esp;
    idle->eip = (uint32_t)&process_idle;
    
    // Add to process list
    process_list = idle;
    current_process = idle;
    
    vga_puts("Process: Initialized process manager\n");
}

// Idle process - runs when nothing else is ready
static int process_idle(void* arg) {
    while (1) {
        __asm__ volatile("hlt");
    }
    return 0;
}

// Create a new process
process_t* process_create(const char* name, process_entry_t entry, void* arg, process_priority_t priority) {
    // Allocate memory for process control block
    process_t* proc = (process_t*)pmm_alloc_block();
    if (!proc) {
        return NULL;
    }
    
    // Clear the structure
    memset(proc, 0, sizeof(process_t));
    
    // Set process properties
    strcpy(proc->name, name);
    proc->pid = next_pid++;
    proc->state = PROCESS_STATE_READY;
    proc->priority = priority;
    
    // Create page directory for the process
    proc->page_directory = vmm_clone_directory(vmm_get_current_directory());
    if (!proc->page_directory) {
        pmm_free_block(proc);
        return NULL;
    }
    
    // Allocate kernel stack
    proc->stack_size = 16384;  // 16 KB stack
    proc->stack = (uint32_t)pmm_alloc_blocks(proc->stack_size / PAGE_SIZE);
    if (!proc->stack) {
        vmm_free_directory(proc->page_directory);
        pmm_free_block(proc);
        return NULL;
    }
    
    // Set up initial stack for the process
    uint32_t* stack = (uint32_t*)(proc->stack + proc->stack_size);
    
    // Push argument
    *--stack = (uint32_t)arg;
    
    // Push fake return address
    *--stack = 0;
    
    // Push initial EFLAGS (interrupts enabled)
    *--stack = 0x202;
    
    // Push initial CS (code segment)
    *--stack = 0x08;  // Kernel code segment
    
    // Push entry point
    *--stack = (uint32_t)entry;
    
    // Push fake registers for the first context switch
    *--stack = 0;  // EAX
    *--stack = 0;  // ECX
    *--stack = 0;  // EDX
    *--stack = 0;  // EBX
    *--stack = 0;  // ESP (old stack pointer, not used)
    *--stack = 0;  // EBP
    *--stack = 0;  // ESI
    *--stack = 0;  // EDI
    
    // Set initial stack pointer
    proc->esp = (uint32_t)stack;
    proc->ebp = proc->esp;
    proc->eip = (uint32_t)entry;
    
    // Add to process list
    process_t* p = process_list;
    while (p->next) {
        p = p->next;
    }
    p->next = proc;
    
    return proc;
}

// Exit current process
void process_exit(int code) {
    if (current_process) {
        current_process->exit_code = code;
        current_process->state = PROCESS_STATE_TERMINATED;
        process_yield();
    }
}

// Get current process
process_t* process_get_current(void) {
    return current_process;
}

// Get current process ID
int process_get_pid(void) {
    return current_process ? current_process->pid : -1;
}

// Sleep for a number of milliseconds
void process_sleep(uint32_t ms) {
    if (current_process) {
        current_process->sleep_until = system_time + ms;
        current_process->state = PROCESS_STATE_BLOCKED;
        process_yield();
    }
}

// Block a process
void process_block(process_t* proc) {
    if (proc) {
        proc->state = PROCESS_STATE_BLOCKED;
        if (proc == current_process) {
            process_yield();
        }
    }
}

// Unblock a process
void process_unblock(process_t* proc) {
    if (proc && proc->state == PROCESS_STATE_BLOCKED) {
        proc->state = PROCESS_STATE_READY;
    }
}

// Terminate a process
void process_terminate(process_t* proc) {
    if (proc) {
        proc->state = PROCESS_STATE_TERMINATED;
        if (proc == current_process) {
            process_yield();
        }
    }
}

// Find next process to run
static process_t* process_get_next(void) {
    if (!process_list) {
        return NULL;
    }
    
    // Start with the process after the current one
    process_t* next = current_process->next;
    if (!next) {
        next = process_list;
    }
    
    // Loop until we find a runnable process
    process_t* start = next;
    do {
        // Check if this process is ready
        if (next->state == PROCESS_STATE_READY) {
            return next;
        }
        
        // Check if this is a sleeping process that should wake up
        if (next->state == PROCESS_STATE_BLOCKED && next->sleep_until && next->sleep_until <= system_time) {
            next->state = PROCESS_STATE_READY;
            return next;
        }
        
        // Check if this is a terminated process that should be cleaned up
        if (next->state == PROCESS_STATE_TERMINATED) {
            // Remove from process list
            process_t* prev = process_list;
            while (prev && prev->next != next) {
                prev = prev->next;
            }
            
            if (prev) {
                prev->next = next->next;
            }
            
            // Free process resources
            if (next->stack) {
                pmm_free_blocks((void*)next->stack, next->stack_size / PAGE_SIZE);
            }
            
            if (next->page_directory) {
                vmm_free_directory(next->page_directory);
            }
            
            // Save the next process
            process_t* temp = next->next;
            if (!temp) {
                temp = process_list;
            }
            
            // Free the process structure
            pmm_free_block(next);
            
            // Move to the next process
            next = temp;
            
            // If we removed the last process, return the idle process
            if (!next) {
                return process_list; // idle
            }
            
            continue;
        }
        
        // Try the next process
        next = next->next;
        if (!next) {
            next = process_list;
        }
    } while (next != start);
    
    // If no process is ready, return the idle process
    return process_list;
}

// Yield to another process
void process_yield(void) {
    // Find next process to run
    process_t* next = process_get_next();
    
    // If no process is ready, run the idle process
    if (!next) {
        next = process_list;
    }
    
    // If we're switching to the same process, do nothing
    if (next == current_process) {
        return;
    }
    
    // Switch to the next process
    process_switch(next);
}

// Switch to a specific process
void process_switch(process_t* next) {
    if (!next) {
        return;
    }
    
    // Save current process state
    process_t* prev = current_process;
    if (prev->state == PROCESS_STATE_RUNNING) {
        prev->state = PROCESS_STATE_READY;
    }
    
    // Set next process as current
    current_process = next;
    current_process->state = PROCESS_STATE_RUNNING;
    
    // Switch address space
    if (prev->page_directory != next->page_directory) {
        vmm_switch_page_directory(next->page_directory);
    }
    
    // Switch kernel stack in TSS
    tss_set_kernel_stack(next->esp);
    
    // Save current ESP/EBP and load new ESP/EBP
    __asm__ volatile(
        "mov %%esp, %0\n"
        "mov %%ebp, %1\n"
        "mov %2, %%esp\n"
        "mov %3, %%ebp\n"
        : "=m"(prev->esp), "=m"(prev->ebp)
        : "m"(next->esp), "m"(next->ebp)
    );
}

// Timer interrupt handler - task switcher
void process_timer_tick(registers_t* regs) {
    // Increment system time (assuming 1000Hz timer)
    system_time++;
    
    // Switch tasks every 10ms (10 ticks)
    if (system_time % 10 == 0) {
        process_yield();
    }
}

// Create a new thread in a process
thread_t* thread_create(process_t* proc, process_entry_t entry, void* arg, bool is_kernel) {
    if (!proc) {
        return NULL;
    }
    
    // Allocate memory for thread control block
    thread_t* thread = (thread_t*)pmm_alloc_block();
    if (!thread) {
        return NULL;
    }
    
    // Clear the structure
    memset(thread, 0, sizeof(thread_t));
    
    // Set thread properties
    thread->tid = next_pid++;  // Use same ID space for now
    thread->parent = proc;
    thread->is_kernel = is_kernel;
    
    // Allocate stack
    thread->stack_size = 16384;  // 16 KB stack
    thread->stack = (uint32_t)pmm_alloc_blocks(thread->stack_size / PAGE_SIZE);
    if (!thread->stack) {
        pmm_free_block(thread);
        return NULL;
    }
    
    // Set up initial stack for the thread
    uint32_t* stack = (uint32_t*)(thread->stack + thread->stack_size);
    
    // Push argument
    *--stack = (uint32_t)arg;
    
    // Push fake return address
    *--stack = 0;
    
    // Set initial stack pointer
    thread->esp = (uint32_t)stack;
    thread->ebp = thread->esp;
    thread->eip = (uint32_t)entry;
    
    return thread;
}

// Exit current thread
void thread_exit(int code) {
    // For now, just exit the whole process
    process_exit(code);
}

// Get current thread
thread_t* thread_get_current(void) {
    // In the current implementation, we don't have separate thread tracking
    return NULL;
}

// Yield to another thread
void thread_yield(void) {
    // For now, just yield the process
    process_yield();
} 