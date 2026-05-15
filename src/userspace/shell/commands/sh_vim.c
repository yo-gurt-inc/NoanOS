#include "cpu/syscall.h"
#include "shell/commands.h"
#include "core/types.h"

#define EDITOR_MAX_SIZE 4096
#define EDITOR_WIDTH 80
#define EDITOR_HEIGHT 24

static char text_buffer[EDITOR_MAX_SIZE];
static int cursor_x = 0;
static int cursor_y = 0;
static int text_len = 0;
static int viewport_line = 0;

static void editor_print(const char* s) {
    _syscall1(SYS_PRINT, (u32)s);
}

static void editor_putchar(char c) {
    _syscall1(SYS_PUTCHAR, (u32)c);
}

static void update_screen_cursor(void) {
    // ANSI: \033[y;xH (1-indexed)
    editor_print("\033[");
    
    int display_y = cursor_y - viewport_line + 1;
    if (display_y >= 10) { editor_putchar('0' + (display_y/10)); editor_putchar('0' + (display_y%10)); }
    else editor_putchar('0' + display_y);
    
    editor_putchar(';');
    
    int x = cursor_x + 1;
    if (x >= 10) { editor_putchar('0' + (x/10)); editor_putchar('0' + (x%10)); }
    else editor_putchar('0' + x);
    
    editor_putchar('H');
}

static void refresh_screen(void) {
    _syscall0(SYS_CLEAR);
    editor_print("--- VIM LITE - Ctrl+S: Save, Ctrl+Q: Quit ---\n");
    
    int current_line = 0;
    int display_line = 0;
    
    for (int i = 0; i < text_len && display_line < EDITOR_HEIGHT - 1; i++) {
        if (current_line >= viewport_line && current_line < viewport_line + EDITOR_HEIGHT - 1) {
            editor_putchar(text_buffer[i]);
            if (text_buffer[i] == '\n') {
                display_line++;
                current_line++;
            }
        } else {
            if (text_buffer[i] == '\n') {
                current_line++;
            }
        }
    }
    update_screen_cursor();
}

int sh_vim(int argc, char** argv) {
    const char* filename = (argc > 1) ? argv[1] : "newfile.txt";
    
    for(int i=0; i<EDITOR_MAX_SIZE; i++) text_buffer[i] = 0;
    text_len = 0;
    cursor_x = 0;
    cursor_y = 1; // Row 0 is header

    // Try to load file
    int read_bytes = _syscall3(SYS_READ_FILE, (u32)filename, (u32)text_buffer, EDITOR_MAX_SIZE - 1);
    if (read_bytes > 0) {
        text_len = read_bytes;
        text_buffer[text_len] = '\0';
        
        cursor_x = 0;
        cursor_y = 1;
        viewport_line = 0;
    }

    refresh_screen();

    while (1) {
        int c = 0;
        while ((c = _syscall0(SYS_READ)) == 0) _syscall0(SYS_YIELD);

        if (c == 17) { // Ctrl+Q
            _syscall0(SYS_CLEAR);
            return 0;
        } else if (c == 19) { // Ctrl+S
            _syscall2(SYS_RM, (u32)filename, 0);
            _syscall3(SYS_ECHO_FILE, (u32)filename, (u32)text_buffer, 0);
            editor_print("\nSaved to "); editor_print(filename);
            _syscall0(SYS_YIELD);
            refresh_screen();
        } else if (c == '\033') { // Escape sequence
            c = _syscall0(SYS_READ);
            if (c == '[') {
                c = _syscall0(SYS_READ);
                if (c == 'A') { // Up
                    if (cursor_y > 1) {
                        cursor_y--;
                        if (cursor_y - viewport_line < 1) viewport_line--;
                        refresh_screen();
                    }
                } else if (c == 'B') { // Down
                    int total_lines = 1;
                    for (int i = 0; i < text_len; i++) {
                        if (text_buffer[i] == '\n') total_lines++;
                    }
                    if (cursor_y < total_lines) {
                        cursor_y++;
                        if (cursor_y - viewport_line >= EDITOR_HEIGHT - 1) viewport_line++;
                        refresh_screen();
                    }
                } else if (c == 'C') { // Right
                    if (cursor_x < EDITOR_WIDTH - 1) {
                        cursor_x++;
                        update_screen_cursor();
                    }
                } else if (c == 'D') { // Left
                    if (cursor_x > 0) {
                        cursor_x--;
                        update_screen_cursor();
                    }
                }
            }
        } else if (c == '\b') {
            if (text_len > 0) {
                text_len--;
                text_buffer[text_len] = '\0';
                
                cursor_x = 0;
                cursor_y = 1;
                viewport_line = 0;
                for(int i=0; i<text_len; i++) {
                    if(text_buffer[i] == '\n') {
                        cursor_y++;
                        cursor_x = 0;
                    } else {
                        cursor_x++;
                        if(cursor_x >= EDITOR_WIDTH) {
                            cursor_x = 0;
                            cursor_y++;
                        }
                    }
                }
                refresh_screen();
            }
        } else if (c == '\n' || c == '\r') {
            if (text_len < EDITOR_MAX_SIZE - 1) {
                text_buffer[text_len++] = '\n';
                cursor_x = 0;
                cursor_y++;
                if (cursor_y - viewport_line >= EDITOR_HEIGHT - 1) viewport_line++;
                refresh_screen();
            }
        } else if (c >= 32 && c < 127) {
            if (text_len < EDITOR_MAX_SIZE - 1) {
                text_buffer[text_len++] = (char)c;
                editor_putchar(c);
                cursor_x++;
                if (cursor_x >= EDITOR_WIDTH) {
                    cursor_x = 0;
                    cursor_y++;
                    if (cursor_y - viewport_line >= EDITOR_HEIGHT - 1) viewport_line++;
                }
            }
        }
    }
}
