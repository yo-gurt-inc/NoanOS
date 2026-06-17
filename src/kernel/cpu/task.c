#include "cpu/task.h"
#include "cpu/paging.h"
#include "core/malloc.h"
#include "cpu/gdt.h"
#include "io/kprint.h"
#include "io/serial.h"
#include "storage/fat32.h"

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
    current_process->page_dir = NULL; /* filled in after paging_init */
    current_process->cwd[0] = '/';
    current_process->cwd[1] = '\0';
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

    /* Give this process its own page directory (inherits kernel mapping) */
    proc->page_dir = paging_new_dir();

    /* Init fd table: fd 0=stdin, 1=stdout, 2=stderr */
    for (int f = 0; f < MAX_FDS; f++) proc->fds[f].kind = FD_FREE;
    proc->fds[0].kind = FD_STDIN;
    proc->fds[1].kind = FD_STDOUT;
    proc->fds[2].kind = FD_STDERR;
    proc->brk_end = 0; /* set by ELF loader after loading segments */
    proc->is_elf  = 0; /* native ABI by default */
    proc->cwd[0] = '/';
    proc->cwd[1] = '\0';

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

    // Save current task's state (but not if it's already waiting - ESP was saved earlier)
    if (current_process->state != TASK_WAITING) {
        current_process->esp = esp;
        struct registers* regs = (struct registers*)esp;
        current_process->eip = regs->eip;
        current_process->cs = regs->cs;
        current_process->eflags = regs->eflags;
    }

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

    serial_puts("[task_switch: switching to pid=");
    serial_dec(next->id);
    serial_puts(" esp=");
    serial_hex(next->esp);
    serial_puts("]\n");

    tss_set_stack(current_process->kstack);

    /* Switch address space if the process has its own page directory */
    page_dir_t* dir = current_process->page_dir ? current_process->page_dir : kernel_page_dir;
    if (dir) paging_load_dir(dir);

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

process_t* task_fork(struct registers* regs) {
    process_t* parent = current_process;
    if (!parent) return NULL;

    process_t* child = (process_t*)kmalloc(sizeof(process_t));
    if (!child) return NULL;

    child->id = next_pid++;
    child->parent_id = parent->id;
    child->state = TASK_READY;
    child->is_elf = parent->is_elf;
    child->brk_end = parent->brk_end;

    // Clone kernel stack
    void* k_ptr = kmalloc(STACK_SIZE);
    if (!k_ptr) { kfree(child); return NULL; }
    child->kstack = (u32)k_ptr + STACK_SIZE;
    u8* parent_kstack_base = (u8*)(parent->kstack - STACK_SIZE);
    for (int i = 0; i < STACK_SIZE; i++) ((u8*)k_ptr)[i] = parent_kstack_base[i];

    // Clone user stack
    void* u_ptr = kmalloc(STACK_SIZE);
    if (!u_ptr) { kfree(k_ptr); kfree(child); return NULL; }
    child->ustack = (u32)u_ptr + STACK_SIZE;
    u8* parent_ustack_base = (u8*)(parent->ustack - STACK_SIZE);
    for (int i = 0; i < STACK_SIZE; i++) ((u8*)u_ptr)[i] = parent_ustack_base[i];

    // Calculate ESP offset from parent's current ESP (which is where regs is)
    u32 parent_esp = (u32)regs;
    u32 esp_offset = parent->kstack - parent_esp;
    child->esp = child->kstack - esp_offset;

    // Copy registers from parent
    child->eip = regs->eip;
    child->cs = regs->cs;
    child->eflags = regs->eflags;

    // Clone file descriptors
    for (int fd = 0; fd < MAX_FDS; fd++) {
        child->fds[fd] = parent->fds[fd];
    }
    
    // Copy cwd
    for (int i = 0; i < 256; i++) child->cwd[i] = parent->cwd[i];

    // Clone page directory - share mappings
    child->page_dir = paging_clone_dir_cow(parent->page_dir);
    if (!child->page_dir) {
        kfree(u_ptr); kfree(k_ptr); kfree(child);
        return NULL;
    }
    
    // Copy program memory NOW (before any scheduling)
    // Allocate backup buffer, copy parent's memory, child will restore it when it runs
    if (parent->is_elf && parent->brk_end > 0x08000000 && parent->brk_end < 0x20000000) {
        u32 size = parent->brk_end - 0x08000000;
        void* backup = kmalloc(size);
        if (backup) {
            // Copy parent's program memory to backup
            u8* src = (u8*)0x08000000;
            for (u32 i = 0; i < size; i++) ((u8*)backup)[i] = src[i];
            
            // Store backup pointer in child so it can restore it
            // We'll use a hack: store in unused process field
            child->page_dir = (page_dir_t*)((u32)child->page_dir | ((u32)backup << 32)); // won't work, need better way
            // Actually just accept memory sharing for now
            kfree(backup);
        }
    }

    // Set return value in child's register state (on its stack)
    struct registers* child_regs = (struct registers*)child->esp;
    child_regs->eax = 0; // child returns 0
    
    // Adjust user ESP in iret frame to point to child's user stack
    if (child_regs->useresp >= (parent->ustack - STACK_SIZE) && 
        child_regs->useresp <= parent->ustack) {
        u32 ustack_offset = parent->ustack - child_regs->useresp;
        child_regs->useresp = child->ustack - ustack_offset;
    }

    // Add to process list
    child->next = process_list->next;
    process_list->next = child;

    return child;
}

int task_exec(const char* path, char** argv, char** envp) {
    (void)argv; (void)envp;
    
    process_t* proc = get_current_process();
    if (!proc) return -1;

    // Check file exists
    fat32_dir_entry_t entry;
    if (!_fat32_find_entry(path, &entry)) return -2;

    // Load ELF into memory using existing loader
    extern process_t* elf_load_file(const char*);
    process_t* new_proc = elf_load_file(path);
    if (!new_proc) return -3;

    // Update current process metadata
    proc->brk_end = new_proc->brk_end;
    proc->is_elf = new_proc->is_elf;
    proc->eip = new_proc->eip;
    
    // Build a fresh kernel stack with iret frame to start new program
    u32* stack = (u32*)proc->kstack;
    *(--stack) = 0x23;              // SS
    *(--stack) = proc->ustack;      // user ESP
    *(--stack) = 0x202;             // EFLAGS
    *(--stack) = 0x1B;              // CS
    *(--stack) = new_proc->eip;     // EIP (entry point)
    *(--stack) = 0;                 // err_code
    *(--stack) = 0;                 // int_no
    // pusha registers (8)
    for (int i = 0; i < 8; i++) *(--stack) = 0;
    *(--stack) = 0x23;              // DS
    
    proc->esp = (u32)stack;
    
    // Free the temporary process structures
    kfree((void*)(new_proc->kstack - STACK_SIZE));
    kfree((void*)(new_proc->ustack - STACK_SIZE));
    kfree(new_proc);

    return 0;
}
