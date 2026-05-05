#ifndef LIBNOAN_H
#define LIBNOAN_H

#include "core/types.h"
#include "cpu/syscall.h"

static inline void print(const char* s) { _syscall1(SYS_PRINT, (u32)s); }
static inline void putchar(char c) { _syscall1(SYS_PUTCHAR, (u32)c); }
static inline void exit(void) { _syscall0(SYS_EXIT); }
static inline void* malloc(u32 size) { return (void*)_syscall1(SYS_MALLOC, size); }
static inline void free(void* ptr) { _syscall1(SYS_FREE, (u32)ptr); }
static inline int read_file(const char* path, char* buf, u32 len) { return _syscall3(SYS_READ_FILE, (u32)path, (u32)buf, len); }

static inline void print_dec(u32 value) {
    char buf[12];
    int i = 0;
    if (value == 0) { putchar('0'); return; }
    while (value > 0) { buf[i++] = '0' + (value % 10); value /= 10; }
    for (int j = i - 1; j >= 0; j--) putchar(buf[j]);
}

#endif
