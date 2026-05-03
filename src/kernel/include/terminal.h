#ifndef TERMINAL_H
#define TERMINAL_H

#include "types.h"

void kprint_init(void);
void terminal_scroll(void);
void terminal_putchar(char c);
void terminal_backspace(void);
u8 vga_entry_color(u8 fg, u8 bg);
u16 vga_entry(char c, u8 color);

#endif
