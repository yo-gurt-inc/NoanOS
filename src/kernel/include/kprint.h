#ifndef KPRINT_H
#define KPRINT_H

#include "types.h"

void kprint(const char* str);
void kprint_newline(void);
void kprint_hex(u32 value);
void kprint_hex8(u8 value);
void kprint_dec(u32 value);

#endif
