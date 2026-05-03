#ifndef TIMER_H
#define TIMER_H

#include "types.h"

void timer_init(u32 frequency);
void timer_sleep(u32 ms);
u32 timer_get_ticks(void);

#endif
