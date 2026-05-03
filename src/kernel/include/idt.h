#ifndef IDT_H
#define IDT_H

#include "types.h"

#define IDT_ENTRIES 256

struct idt_entry {
    u16 base_low;
    u16 sel;
    u8  always0;
    u8  flags;
    u16 base_high;
} __attribute__((packed));

struct idt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));

struct registers {
    u32 ds;
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
    u32 int_no, err_code;
    u32 eip, cs, eflags, useresp, ss;
};

typedef u32 (*irq_handler_t)(u32);

void idt_init(void);
void idt_set_gate(u8 num, u32 base, u16 sel, u8 flags);
void irq_install_handler(int irq, irq_handler_t handler);

u32 isr_handler(u32 esp);
u32 irq_handler(u32 esp);

#endif
