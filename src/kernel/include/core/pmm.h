#ifndef PMM_H
#define PMM_H

#include "core/types.h"

void pmm_init(void);
u32 pmm_alloc(void);
void pmm_free(u32 addr);

#endif
