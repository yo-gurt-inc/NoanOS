#include "io/serial.h"
#include "io/io.h"

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00); /* Disable interrupts */
    outb(COM1 + 3, 0x80); /* Enable DLAB (set baud rate divisor) */
    outb(COM1 + 0, 0x03); /* Divisor low: 38400 baud */
    outb(COM1 + 1, 0x00); /* Divisor high */
    outb(COM1 + 3, 0x03); /* 8 bits, no parity, one stop bit */
    outb(COM1 + 2, 0xC7); /* Enable FIFO */
    outb(COM1 + 4, 0x03); /* RTS/DSR */
}

void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)); /* Wait for transmit empty */
    outb(COM1, c);
}

void serial_puts(const char* s) {
    for (; *s; s++) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s);
    }
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
