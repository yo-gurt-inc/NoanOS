#ifndef TASK_H
#define TASK_H

#include "types.h"
#include "idt.h"

#define MAX_PROCESSES 16
#define STACK_SIZE 4096

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_TERMINATED
} task_state_t;

typedef struct {
    u32 esp, ebp, eip;
    u32 edi, esi, ebx, edx, ecx, eax;
    u32 eflags;
    u32 cs, ss, ds, es, fs, gs;
} task_registers_t;

typedef struct process {
    int id;
    u32 esp;            // Current kernel stack pointer
    u32 kstack;         // Base of kernel stack
    u32 ustack;         // Base of user stack (if any)
    task_state_t state;
    struct process* next;
} process_t;

void task_init(void);
void task_create(void* entry, u32 flags);
void task_yield(void);
u32 task_switch(u32 esp);

process_t* get_current_process(void);

#endif
