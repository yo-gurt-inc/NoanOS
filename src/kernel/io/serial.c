#include "io/serial.h"
#include "io/io.h"

#define COM1 0x3F8

/* Interrupt masking for atomic serial payloads.
 *
 * All multi-byte output (serial_puts / serial_mirror_write) is emitted with
 * interrupts disabled for the whole payload. The 100 Hz timer task_switch
 * chatter and userland syscalls both write to COM1 from different contexts;
 * without this, lines tear mid-string. A spinlock would deadlock instead:
 * the timer IRQ fires while a process holds it and prints from IRQ context,
 * spinning forever. Masking is safe here: it is single-CPU, nothing spins
 * waiting, and every caller either already has IF clear (IRQ entries) or
 * restores the saved flags afterwards.
 */
static inline u32 irq_save(void) {
    u32 flags;
    asm volatile("pushfl; popl %0" : "=r"(flags));
    asm volatile("cli");
    return flags;
}

static inline void irq_restore(u32 flags) {
    asm volatile("pushl %0; popfl" :: "r"(flags));
}

void serial_init(void) {
    outb(COM1 + 1, 0x00); /* Disable interrupts */
    outb(COM1 + 3, 0x80); /* Enable DLAB (set baud rate divisor) */
    outb(COM1 + 0, 0x01); /* Divisor low: 115200 baud */
    outb(COM1 + 1, 0x00); /* Divisor high */
    outb(COM1 + 3, 0x03); /* 8 bits, no parity, one stop bit */
    outb(COM1 + 2, 0xC7); /* Enable FIFO */
    outb(COM1 + 4, 0x03); /* RTS/DSR */
}

/* Raw transmit of one byte; no masking (callers that need atomicity use the
 * whole-payload functions). */
void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)); /* Wait for transmit empty */
    outb(COM1, c);
}

void serial_puts(const char* s) {
    u32 flags = irq_save();
    for (; *s; s++) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s);
    }
    irq_restore(flags);
}

void serial_mirror_write(const char* buf, u32 len) {
    u32 flags = irq_save();
    for (u32 i = 0; i < len; i++) {
        if (buf[i] == '\n') serial_putc('\r');
        serial_putc(buf[i]);
    }
    irq_restore(flags);
}

void serial_hex(u32 v) {
    const char h[] = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 7; i >= 0; i--)
        serial_putc(h[(v >> (i * 4)) & 0xF]);
}

void serial_dec(u32 v) {
    char buf[11];
    int i = 0;
    if (!v) { serial_putc('0'); return; }
    while (v) { buf[i++] = '0' + v % 10; v /= 10; }
    while (i--) serial_putc(buf[i]);
}
