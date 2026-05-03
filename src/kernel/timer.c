#include "include/timer.h"
#include "include/idt.h"
#include "include/io.h"
#include "include/kprint.h"
#include "include/task.h"

static u32 ticks = 0;
static u32 freq = 0;

static u32 timer_callback(u32 esp) {
    ticks++;
    return task_switch(esp);
}

void timer_init(u32 frequency) {
    freq = frequency;
    irq_install_handler(0, timer_callback);

    u32 divisor = 1193182 / frequency;

    outb(0x43, 0x36);
    u8 l = (u8)(divisor & 0xFF);
    u8 h = (u8)((divisor >> 8) & 0xFF);

    outb(0x40, l);
    outb(0x40, h);
}

void timer_sleep(u32 ms) {
    u32 start_ticks = ticks;
    u32 ticks_to_wait = (ms * freq) / 1000;
    while (ticks < start_ticks + ticks_to_wait) {
        asm volatile("hlt");
    }
}

u32 timer_get_ticks(void) {
    return ticks;
}
