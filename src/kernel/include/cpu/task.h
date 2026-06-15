#ifndef TASK_H
#define TASK_H

#include "core/types.h"
#include "cpu/idt.h"
#include "cpu/paging.h"

#define MAX_PROCESSES 16
#define STACK_SIZE 4096
#define MAX_FDS 16

/* File descriptor kinds */
#define FD_FREE    0
#define FD_STDIN   1
#define FD_STDOUT  2
#define FD_STDERR  3
#define FD_FILE    4

typedef struct {
    int kind;           /* FD_* above */
    u32 fat_cluster;    /* first cluster for FD_FILE */
    u32 offset;         /* byte offset for FD_FILE */
    u32 size;           /* file size for FD_FILE */
} fd_entry_t;

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_WAITING,
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
    int parent_id;
    u32 esp;            // Current kernel stack pointer
    u32 kstack;         // Base of kernel stack
    u32 ustack;         // Base of user stack (if any)
    u32 eip;            // Last saved instruction pointer
    u32 cs;             // Last saved code segment
    u32 eflags;         // Last saved eflags
    task_state_t state;
    page_dir_t* page_dir; // This process's page directory (NULL = use kernel)
    fd_entry_t fds[MAX_FDS]; // file descriptor table
    u32 brk_end;             // current heap break (for Linux brk syscall)
    int is_elf;              // 1 = Linux ELF ABI, 0 = native NoanOS ABI
    struct process* next;
} process_t;

void task_init(void);
process_t* task_create(void* entry, u32 flags, int parent_id);
void task_kill_current(void);
void task_yield(void);
u32 task_switch(u32 esp);

process_t* get_current_process(void);
process_t* get_process_list(void);

#endif
