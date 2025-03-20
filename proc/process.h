#ifndef REXUS_PROCESS_H
#define REXUS_PROCESS_H

#include <stdint.h>
#include <stdbool.h>
#include "../mem/vmm.h"
#include "../arch/x86/isr.h"

// Process states
typedef enum {
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_TERMINATED
} process_state_t;

// Process priority levels
typedef enum {
    PROCESS_PRIORITY_LOW = 0,
    PROCESS_PRIORITY_NORMAL = 1,
    PROCESS_PRIORITY_HIGH = 2,
    PROCESS_PRIORITY_REAL_TIME = 3
} process_priority_t;

// Process structure
typedef struct process {
    uint32_t pid;                   // Process ID
    char name[32];                  // Process name
    process_state_t state;          // Current state
    process_priority_t priority;    // Process priority
    
    uint32_t esp;                   // Stack pointer
    uint32_t ebp;                   // Base pointer
    uint32_t eip;                   // Instruction pointer
    
    page_dir_t* page_directory;     // Process page directory
    uint32_t stack;                 // Kernel stack location
    uint32_t stack_size;            // Stack size
    
    uint32_t sleep_until;           // Wake time for sleeping processes
    int exit_code;                  // Exit code
    
    struct process* next;           // Next process in queue
} process_t;

// Thread structure
typedef struct {
    uint32_t tid;                   // Thread ID
    process_t* parent;              // Parent process
    uint32_t esp;                   // Stack pointer
    uint32_t ebp;                   // Base pointer
    uint32_t eip;                   // Instruction pointer
    uint32_t stack;                 // Thread stack
    uint32_t stack_size;            // Stack size
    bool is_kernel;                 // Is kernel thread?
} thread_t;

// Function type for process/thread entry points
typedef int (*process_entry_t)(void* arg);

// Process management functions
void process_init(void);
process_t* process_create(const char* name, process_entry_t entry, void* arg, process_priority_t priority);
void process_exit(int code);
process_t* process_get_current(void);
int process_get_pid(void);
void process_sleep(uint32_t ms);
void process_block(process_t* proc);
void process_unblock(process_t* proc);
void process_terminate(process_t* proc);
void process_yield(void);
void process_switch(process_t* next);
void process_scheduler(void);

// Thread functions
thread_t* thread_create(process_t* proc, process_entry_t entry, void* arg, bool is_kernel);
void thread_exit(int code);
thread_t* thread_get_current(void);
void thread_yield(void);

// Interrupt handler for task switching
void process_timer_tick(registers_t* regs);

#endif /* REXUS_PROCESS_H */ 