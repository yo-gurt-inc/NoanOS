#include "cpu/syscall.h"
#include "io/kprint.h"
#include "io/keyboard.h"
#include "core/malloc.h"
#include "cpu/task.h"
#include "storage/fat32.h"
#include "io/terminal.h"
#include "system/power.h"
#include "core/panic.h"
#include "system/rtc.h"
#include "storage/ata.h"
#include "system/timer.h"
#include "storage/noan.h"
#include "core/initrd.h"

extern void syscall_stub(void);

u32 syscall_handler(u32 esp) {
    struct registers* regs = (struct registers*)esp;
    u32 num = regs->eax;
    u32 arg1 = regs->ebx;
    u32 arg2 = regs->ecx;
    u32 arg3 = regs->edx;

    int ret = 0;

    switch (num) {
        case SYS_EXIT:
            process_t* current = get_current_process();
            if (current) {
                current->state = TASK_TERMINATED;
                // Wake up parent if it's waiting
                if (current->parent_id > 0) {
                    process_t* p = get_process_list();
                    process_t* start = p;
                    do {
                        if (p->id == current->parent_id) {
                            if (p->state == TASK_WAITING) {
                                p->state = TASK_READY;
                            }
                            break;
                        }
                        p = p->next;
                    } while (p != start);
                }
            }
            return task_switch(esp);
        case SYS_EXEC: {
            u32 entry = noan_load((const char*)arg1);
            if (entry) {
                keyboard_set_enabled(1);
                noan_execute(entry, (const char*)arg1);
                keyboard_flush();
                ret = 0;
            } else {
                ret = -1;
            }
            break;
        }

        case SYS_PRINT:
            kprint((const char*)arg1);
            break;
        case SYS_READ:
            ret = keyboard_getchar();
            if (ret == 0) {
                process_t* current = get_current_process();
                if (current) current->state = TASK_SLEEPING;
                return task_switch(esp);
            }
            break;
        case SYS_READ_NB:
            /* Non-blocking: returns the next char or 0 if the buffer is empty.
             * Never sleeps, so safe to call in a drain loop. */
            ret = keyboard_getchar();
            break;
        case SYS_FLUSH_KB:
            keyboard_flush();
            break;
        case SYS_KB_ENABLE:
            keyboard_set_enabled((int)arg1);
            break;
        case SYS_MALLOC:
            ret = (u32)kmalloc(arg1);
            break;
        case SYS_FREE:
            kfree((void*)arg1);
            break;
        case SYS_LS:
            fat32_ls();
            break;
        case SYS_CD:
            fat32_cd((const char*)arg1);
            break;
        case SYS_CAT:
            fat32_cat((const char*)arg1);
            break;
        case SYS_PUTCHAR:
            terminal_putchar((char)arg1);
            break;
        case SYS_CLEAR:
            kprint_init();
            break;
        case SYS_YIELD:
            return task_switch(esp);
        case SYS_REBOOT:
            reboot();
            break;
        case SYS_SHUTDOWN:
            shutdown();
            break;
        case SYS_MKDIR:
            fat32_mkdir((const char*)arg1);
            break;
        case SYS_RM:
            fat32_rm((const char*)arg1, (int)arg2);
            break;
        case SYS_TOUCH:
            fat32_touch((const char*)arg1);
            break;
        case SYS_ECHO_FILE:
            fat32_echo((const char*)arg1, (const char*)arg2, (int)arg3);
            break;
        case SYS_CP:
            fat32_copy((const char*)arg1, (const char*)arg2);
            break;
        case SYS_MV:
            fat32_move((const char*)arg1, (const char*)arg2);
            break;
        case SYS_READ_FILE:
            ret = fat32_read((const char*)arg1, (char*)arg2, arg3);
            break;
        case SYS_PANIC:
            panic((const char*)arg1);
            break;
        case SYS_GET_TIME:
            rtc_get_time((u8*)arg1, (u8*)arg2, (u8*)arg3);
            break;
        case SYS_GET_DATE:
            rtc_get_date((u8*)arg1, (u8*)arg2, (u16*)arg3);
            break;
        case SYS_MEM_INFO:
            get_malloc_info((malloc_info_t*)arg1);
            break;
        case SYS_LIST_DISKS:
            ata_list_drives();
            break;
        case SYS_STAT:
            fat32_stat((const char*)arg1);
            break;
        case SYS_GET_TICKS:
            ret = (int)timer_get_ticks();
            break;
        default:
            kprint("Unknown syscall: ");
            kprint_dec(num);
            kprint("\n");
            break;
    }

    regs->eax = (u32)ret;
    
    // Special case: If parent was marked WAITING (by SYS_EXEC), force task switch
    process_t* current = get_current_process();
    if (current && current->state == TASK_WAITING) {
        return task_switch(esp);
    }
    
    return esp;
}

void syscall_init(void) {
    kprint("[Setting up syscall gate 0x80]\n");
    idt_set_gate(0x80, (u32)syscall_stub, 0x08, 0xEE); // Ring 3 accessible
    kprint("[Syscall gate installed]\n");
}
