#include "io/kprint.h"
#include "io/terminal.h"
#include "io/io.h"
#include "io/serial.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000
#define MIRROR_BUF_SIZE 512

static u16* terminal_buffer;
static u8 terminal_row;
static u8 terminal_column;
static u8 terminal_color;

// Serial mirror: echo rendered (non-escape) screen output to COM1 so headless
// test runs can observe program output. Lines are buffered and flushed whole
// through the serial TX lock on '\n' (or when the buffer fills / mirror off).
static int serial_mirror_enabled = 0;
static char mirror_buf[MIRROR_BUF_SIZE];
static u32 mirror_len = 0;

static void mirror_flush(void) {
    if (mirror_len > 0) {
        serial_mirror_write(mirror_buf, mirror_len);
        mirror_len = 0;
    }
}

void terminal_set_serial_mirror(int enable) {
    if (enable) {
        mirror_len = 0;
        serial_mirror_enabled = 1;
    } else {
        serial_mirror_enabled = 0;
        mirror_flush();
    }
}

// ANSI parsing state
static int ansi_state = 0;
static int ansi_params[4];
static int ansi_param_idx = 0;

static void update_cursor(int x, int y) {
    u16 pos = y * VGA_WIDTH + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

u8 vga_entry_color(u8 fg, u8 bg) {
    return fg | (bg << 4);
}

u16 vga_entry(char c, u8 color) {
    return ((u16)c) | ((u16)color << 8);
}

void kprint_init(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(7, 0);  // Light grey on black
    terminal_buffer = (u16*)VGA_MEMORY;
    
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
    update_cursor(terminal_column, terminal_row);
    ansi_state = 0;
}

void terminal_scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[y * VGA_WIDTH + x] = terminal_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        const size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
        terminal_buffer[index] = vga_entry(' ', terminal_color);
    }
}

void terminal_set_cursor(u8 row, u8 col) {
    if (row >= VGA_HEIGHT) row = VGA_HEIGHT - 1;
    if (col >= VGA_WIDTH) col = VGA_WIDTH - 1;
    terminal_row = row;
    terminal_column = col;
    update_cursor(terminal_column, terminal_row);
}

void terminal_set_color(u8 color) {
    terminal_color = color;
}

void terminal_putchar(char c) {
    // Serial mirror: only characters that will actually be rendered (not ANSI
    // control sequences) are echoed, as whole lines. '\b' pops the last char
    // instead of appending, '\r' alone is dropped (the '\n' flush ends lines).
    if (serial_mirror_enabled && ansi_state == 0) {
        if (c == '\n') {
            mirror_flush();
            serial_mirror_write("\n", 1);
        } else if (c == '\033') {
            /* escape sequence start; content is skipped until parser resets */
        } else if (c == '\b') {
            if (mirror_len > 0) mirror_len--;
        } else if (c != '\r') {
            if (mirror_len >= MIRROR_BUF_SIZE - 1) mirror_flush();
            mirror_buf[mirror_len++] = c;
        }
    }

    // Basic ANSI escape sequence parser
    if (ansi_state == 0) {
        if (c == '\033') {
            ansi_state = 1;
            return;
        }
    } else if (ansi_state == 1) {
        if (c == '[') {
            ansi_state = 2;
            ansi_params[0] = 0;
            ansi_params[1] = 0;
            ansi_param_idx = 0;
            return;
        }
        ansi_state = 0;
    } else if (ansi_state == 2) {
        if (c >= '0' && c <= '9') {
            ansi_params[ansi_param_idx] = ansi_params[ansi_param_idx] * 10 + (c - '0');
            return;
        } else if (c == ';') {
            if (ansi_param_idx < 3) ansi_param_idx++;
            ansi_params[ansi_param_idx] = 0;
            return;
        } else {
            // Handle command
            switch (c) {
                case 'A': // Up
                    if (terminal_row > 0) terminal_row--;
                    break;
                case 'B': // Down
                    if (terminal_row < VGA_HEIGHT - 1) terminal_row++;
                    break;
                case 'C': // Right
                    if (terminal_column < VGA_WIDTH - 1) terminal_column++;
                    break;
                case 'D': // Left
                    if (terminal_column > 0) terminal_column--;
                    break;
                case 'H': // Set position
                case 'f':
                    terminal_row = (ansi_params[0] > 0) ? ansi_params[0] - 1 : 0;
                    terminal_column = (ansi_params[1] > 0) ? ansi_params[1] - 1 : 0;
                    break;
                case 'J': // Clear screen (simplified)
                    kprint_init();
                    break;
            }
            update_cursor(terminal_column, terminal_row);
            ansi_state = 0;
            return;
        }
    }

    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
        if (terminal_row >= VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT - 1;
        }
    } else if (c == '\r') {
        terminal_column = 0;
    } else if (c == '\b') {
        terminal_backspace();
        return;
    } else {
        const size_t index = terminal_row * VGA_WIDTH + terminal_column;
        terminal_buffer[index] = vga_entry(c, terminal_color);
        terminal_column++;
        
        if (terminal_column >= VGA_WIDTH) {
            terminal_column = 0;
            terminal_row++;
            if (terminal_row >= VGA_HEIGHT) {
                terminal_scroll();
                terminal_row = VGA_HEIGHT - 1;
            }
        }
    }
    update_cursor(terminal_column, terminal_row);
}

void terminal_backspace(void) {
    if (terminal_column > 0) {
        terminal_column--;
    } else if (terminal_row > 0) {
        terminal_row--;
        terminal_column = VGA_WIDTH - 1;
    }
    
    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    terminal_buffer[index] = vga_entry(' ', terminal_color);
    update_cursor(terminal_column, terminal_row);
}
