#ifndef SYSCALL_H
#define SYSCALL_H

#include "core/types.h"
#include "cpu/idt.h"

#define SYS_EXIT      0
#define SYS_PRINT     1
#define SYS_READ      2
#define SYS_MALLOC    3
#define SYS_FREE      4
#define SYS_LS        5
#define SYS_CD        6
#define SYS_CAT       7
#define SYS_PUTCHAR   8
#define SYS_CLEAR     9
#define SYS_YIELD     10
#define SYS_REBOOT    11
#define SYS_SHUTDOWN  12
#define SYS_MKDIR     13
#define SYS_RM        14
#define SYS_TOUCH     15
#define SYS_ECHO_FILE 16
#define SYS_CP        17
#define SYS_MV        18
#define SYS_READ_FILE 19
#define SYS_PANIC     20
#define SYS_GET_TIME  21
#define SYS_GET_DATE  22
#define SYS_MEM_INFO  23
#define SYS_LIST_DISKS 24
#define SYS_STAT      25
#define SYS_GET_TICKS 26
#define SYS_EXEC      27
#define SYS_READ_NB   28   /* Non-blocking read: returns 0 immediately if no key waiting */
#define SYS_FLUSH_KB  29   /* Discard all buffered keypresses */
#define SYS_KB_ENABLE 30   /* 0 = discard input during command, 1 = re-enable + flush */
#define SYS_EXEC_ELF  31   /* load+run an ELF binary from FAT32: arg1=path */

void syscall_init(void);
u32 syscall_handler(u32 esp);

// User-side syscall wrappers
static inline int _syscall0(int num) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num) : "memory");
    return ret;
}

static inline int _syscall1(int num, u32 a1) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1) : "memory");
    return ret;
}

static inline int _syscall2(int num, u32 a1, u32 a2) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2) : "memory");
    return ret;
}

static inline int _syscall3(int num, u32 a1, u32 a2, u32 a3) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2), "d"(a3) : "memory");
    return ret;
}

#endif
