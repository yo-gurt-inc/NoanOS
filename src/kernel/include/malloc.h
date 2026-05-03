#ifndef MALLOC_H
#define MALLOC_H

#include "types.h"

void malloc_init(u32 start_addr, u32 size);
void* kmalloc(size_t size);
void kfree(void* ptr);

// For debugging
void malloc_stats(void);

#endif
