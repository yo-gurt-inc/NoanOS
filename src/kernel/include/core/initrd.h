#ifndef INITRD_H
#define INITRD_H

#include "core/types.h"

#define INITRD_MAGIC 0x4352414E // "NARC"

int initrd_unpack(u32 initrd_addr);
void* initrd_get_entry(void);

int initrd_get_file_count(void);
void* initrd_get_file(int index, char* name, u32* size);

#endif
