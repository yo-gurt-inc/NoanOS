#include "cpu/task.h"
#include "core/malloc.h"
#include "cpu/gdt.h"
#include "io/kprint.h"

static process_t* process_list = NULL;
static process_t* current_process = NULL;
static int next_pid = 1;

void task_init(void) {
    current_process = (process_t*)kmalloc(sizeof(process_t));
    current_process->id = next_pid++;
    current_process->parent_id = 0;
    current_process->state = TASK_RUNNING;
    
    // Allocate proper kernel stack for the idle task
    void* k_ptr = kmalloc(STACK_SIZE);
    if (k_ptr) {
        current_process->kstack = (u32)k_ptr + STACK_SIZE;
    } else {
        current_process->kstack = 0x9C00;
    }
    
    current_process->ustack = 0;
    current_process->next = current_process;
    process_list = current_process;
}

process_t* task_create(void* entry, u32 flags, int parent_id) {
    process_t* proc = (process_t*)kmalloc(sizeof(process_t));
    if (!proc) return NULL;
    
    proc->id = next_pid++;
    proc->parent_id = parent_id;
    proc->state = TASK_READY;
    
    void* k_ptr = kmalloc(STACK_SIZE);
    if (!k_ptr) {
        kfree(proc);
        return NULL;
    }
    proc->kstack = (u32)k_ptr + STACK_SIZE;
    
    void* u_ptr = kmalloc(STACK_SIZE);
    if (!u_ptr) {
        kfree(k_ptr);
        kfree(proc);
        return NULL;
    }
    proc->ustack = (u32)u_ptr + STACK_SIZE;
    
    // Initialize tracking fields
    proc->eip = (u32)entry;
    proc->cs = (flags & 0x1) ? 0x1B : 0x08;
    proc->eflags = 0x202;

    // Zero out the user stack
    u8* u_stack_ptr = (u8*)u_ptr;
    for(int i=0; i<STACK_SIZE; i++) u_stack_ptr[i] = 0;

    u32* stack = (u32*)proc->kstack;

    // --- PUSH THE IRET FRAME ---
    u16 ss = (flags & 0x1) ? 0x23 : 0x10;
    u32 esp_val = (flags & 0x1) ? proc->ustack : (u32)stack;
    
    *(--stack) = ss;
    *(--stack) = esp_val;
    *(--stack) = 0x202; // EFLAGS
    *(--stack) = (flags & 0x1) ? 0x1B : 0x08; // CS
    *(--stack) = (u32)entry; // EIP

    // --- PUSH DUMMY ERROR CODE & INT NO ---
    *(--stack) = 0; // err_code
    *(--stack) = 0; // int_no

    // --- PUSH REGISTERS FOR PUSHA ---
    *(--stack) = 0; // EAX
    *(--stack) = 0; // ECX
    *(--stack) = 0; // EDX
    *(--stack) = 0; // EBX
    *(--stack) = 0; // ESP (dummy)
    *(--stack) = 0; // EBP
    *(--stack) = 0; // ESI
    *(--stack) = 0; // EDI

    // --- PUSH DS ---
    *(--stack) = (flags & 0x1) ? 0x23 : 0x10;

    proc->esp = (u32)stack;

    // Add to list
    proc->next = process_list->next;
    process_list->next = proc;

    return proc;
}

void task_kill_current(void) {
    if (!current_process) return;
    
    process_t* proc = current_process;
    proc->state = TASK_TERMINATED;
    
    kprint("[Process ");
    kprint_dec(proc->id);
    kprint(" killed]\n");
}

u32 task_switch(u32 esp) {
    if (!current_process) return esp;

    // Save current task's state
    current_process->esp = esp;
    struct registers* regs = (struct registers*)esp;
    current_process->eip = regs->eip;
    current_process->cs = regs->cs;
    current_process->eflags = regs->eflags;

    // Move to next task in the circular list
    process_t* next = current_process->next;
    
    // Find next runnable task
    process_t* start = next;
    int iterations = 0;
    while ((next->state == TASK_TERMINATED || next->state == TASK_SLEEPING || next->state == TASK_WAITING) && iterations < 100) {
        next = next->next;
        iterations++;
        if (next == start) {
            if (current_process->state == TASK_RUNNING) return esp;
            break; 
        }
    }

    // PRIORITY: If we picked PID 1 (kernel idle), but there are other runnable tasks, skip PID 1
    if (next->id == 1 && next->next != next) {
        process_t* check = next->next;
        while (check != next) {
            if (check->state != TASK_TERMINATED && check->state != TASK_SLEEPING && check->state != TASK_WAITING && check->id != 1) {
                next = check;
                break;
            }
            check = check->next;
        }
    }

    if (next == current_process && current_process->state == TASK_RUNNING) {
        return esp;
    }
    
    current_process = next;
    current_process->state = TASK_RUNNING;

    tss_set_stack(current_process->kstack);
    return next->esp;
}

void task_yield(void) {
    asm volatile("int $0x20");
}

process_t* get_current_process(void) {
    return current_process;
}

process_t* get_process_list(void) {
    return process_list;
}
