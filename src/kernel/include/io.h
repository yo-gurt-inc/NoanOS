#ifndef IO_H
#define IO_H

#include "types.h"

static inline void outb(u16 port, u8 val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static inline u8 inb(u16 port) {
    u8 ret;
    asm volatile ( "inb %1, %0"
                   : "=a"(ret)
                   : "Nd"(port) );
    return ret;
}

static inline u16 inw(u16 port) {
    u16 ret;
    asm volatile ( "inw %1, %0"
                   : "=a"(ret)
                   : "Nd"(port) );
    return ret;
}

static inline void outw(u16 port, u16 val) {
    asm volatile ( "outw %0, %1" : : "a"(val), "Nd"(port) );
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif
