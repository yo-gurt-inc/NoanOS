#include "include/types.h"
#include "include/syscall.h"

static int syscall0(int num) {
    int ret;
    asm volatile("mov %1, %%eax; int $0x80" : "=a"(ret) : "r"(num));
    return ret;
}

static int syscall1(int num, u32 a1) {
    int ret;
    asm volatile("mov %1, %%eax; mov %2, %%ebx; int $0x80" : "=a"(ret) : "r"(num), "r"(a1));
    return ret;
}

static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void shell_main(void) {
    // Note: We can't call kprint directly in user mode, must use syscall
    // But there's a bootstrapping issue here - we need to know if syscalls work
    // Let's test with a simple write to the terminal
    
    syscall1(SYS_PRINT, (u32)"=== SHELL STARTED IN USER MODE ===\n");
    syscall1(SYS_PRINT, (u32)"Welcome to Simple OS (User Mode Ring 3)!\n");
    syscall1(SYS_PRINT, (u32)"> ");

    char cmd[128];
    int len = 0;

    while (1) {
        syscall1(SYS_PRINT, (u32)"[waiting for input]\n");
        int c = syscall0(SYS_READ);
        syscall1(SYS_PRINT, (u32)"[got char: ");
        syscall1(SYS_PRINT, (u32)(u32)c);
        syscall1(SYS_PRINT, (u32)"]\n");
        if (c == 0) {
            syscall0(SYS_YIELD);
            continue;
        }

        if (c == '\n') {
            syscall1(SYS_PUTCHAR, (u32)'\n');
            cmd[len] = '\0';
            
            if (strcmp(cmd, "help") == 0) {
                syscall1(SYS_PRINT, (u32)"User Commands: help, ls, clear, exit\n");
            } else if (strcmp(cmd, "ls") == 0) {
                syscall0(SYS_LS);
            } else if (strcmp(cmd, "clear") == 0) {
                syscall0(SYS_CLEAR);
            } else if (strcmp(cmd, "exit") == 0) {
                syscall0(SYS_EXIT);
                while(1);
            } else if (len > 0) {
                syscall1(SYS_PRINT, (u32)"Unknown command: ");
                syscall1(SYS_PRINT, (u32)cmd);
                syscall1(SYS_PRINT, (u32)"\n");
            }

            len = 0;
            syscall1(SYS_PRINT, (u32)"> ");
        } else if (c == '\b') {
            if (len > 0) {
                len--;
                syscall1(SYS_PUTCHAR, (u32)'\b');
            }
        } else {
            if (len < 127) {
                cmd[len++] = (char)c;
                syscall1(SYS_PUTCHAR, (u32)c);
            }
        }
    }
}
