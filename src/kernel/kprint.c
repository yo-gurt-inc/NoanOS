#include "include/kprint.h"
#include "include/terminal.h"

void kprint(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        terminal_putchar(str[i]);
    }
}

void kprint_newline(void) {
    terminal_putchar('\n');
}

void kprint_hex(u32 value) {
    char hex[] = "0123456789ABCDEF";
    kprint("0x");
    for (int i = 7; i >= 0; i--) {
        char digit = hex[(value >> (i * 4)) & 0xF];
        terminal_putchar(digit);
    }
}

void kprint_hex8(u8 value) {
    char hex[] = "0123456789ABCDEF";
    kprint("0x");
    terminal_putchar(hex[(value >> 4) & 0xF]);
    terminal_putchar(hex[value & 0xF]);
}

void kprint_dec(u32 value) {
    char buf[12];
    int i = 0;
    if (value == 0) {
        terminal_putchar('0');
        return;
    }

    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    for (int j = i - 1; j >= 0; j--) {
        terminal_putchar(buf[j]);
    }
}
