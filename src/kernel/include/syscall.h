#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"
#include "idt.h"

#define SYS_EXIT    0
#define SYS_PRINT   1
#define SYS_READ    2
#define SYS_MALLOC  3
#define SYS_FREE    4
#define SYS_LS      5
#define SYS_CD      6
#define SYS_CAT     7
#define SYS_PUTCHAR 8
#define SYS_CLEAR   9
#define SYS_YIELD   10

void syscall_init(void);
u32 syscall_handler(u32 esp);

#endif
