#include "cpu/gdt.h"
#include "io/kprint.h"

struct gdt_entry gdt[7];
struct gdt_ptr gdt_p;
struct tss_entry tss;

extern void gdt_flush(u32);
extern void tss_flush(void);

static void gdt_set_gate(int num, u32 base, u32 limit, u8 access, u8 gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access      = access;
}

static void write_tss(int num, u16 ss0, u32 esp0) {
    u32 base = (u32)&tss;
    u32 limit = base + sizeof(tss);

    gdt_set_gate(num, base, limit, 0xE9, 0x00);

    for (int i = 0; i < (int)sizeof(tss); i++) ((u8*)&tss)[i] = 0;

    tss.ss0 = ss0;
    tss.esp0 = esp0;
    
    // TSS segment values are for hardware task switching (not used)
    // but should be valid. 0x08 = K-Code, 0x10 = K-Data
    tss.cs = 0x08;
    tss.ss = tss.ds = tss.es = tss.fs = tss.gs = 0x10;
}

void tss_set_stack(u32 stack) {
    tss.esp0 = stack;
}

void gdt_init(void) {
    gdt_p.limit = (sizeof(struct gdt_entry) * 7) - 1;
    gdt_p.base  = (u32)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                // Null segment
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Code segment (Ring 0)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Data segment (Ring 0)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User mode code segment (Ring 3)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User mode data segment (Ring 3)
    write_tss(5, 0x10, 0x0);                    // TSS
    gdt_set_gate(6, 0, 0xFFFFF, 0xF2, 0xCF);   // TLS (%gs) slot for musl — base set per-process

    gdt_flush((u32)&gdt_p);
    tss_flush();
}

/* Called from set_thread_area syscall (243) — sets the base of GDT[6] (%gs).
 * musl passes a struct user_desc*; we only need the base_addr field (offset 4). */
void gdt_set_tls(u32 base) {
    gdt_set_gate(6, base, 0xFFFFF, 0xF2, 0xCF);
    /* Reload GDTR so the CPU sees the updated descriptor */
    asm volatile("lgdt %0" :: "m"(gdt_p));
}
