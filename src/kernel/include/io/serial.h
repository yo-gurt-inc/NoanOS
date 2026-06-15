#ifndef SERIAL_H
#define SERIAL_H

#include "core/types.h"

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char* s);
void serial_hex(u32 v);
void serial_dec(u32 v);

#endif
