#include "shell/noan.h"
#include "shell/commands.h"

#define EDITOR_MAX_SIZE 4096
#define EDITOR_WIDTH    80
#define EDITOR_HEIGHT   24

static char text_buffer[EDITOR_MAX_SIZE];
static int cursor_x = 0, cursor_y = 0, text_len = 0, viewport_line = 0;

static void update_screen_cursor(void) {
    noan_print("\033[");
    int y = cursor_y - viewport_line + 1;
    if (y >= 10) { noan_putchar('0' + y/10); noan_putchar('0' + y%10); } else noan_putchar('0' + y);
    noan_putchar(';');
    int x = cursor_x + 1;
    if (x >= 10) { noan_putchar('0' + x/10); noan_putchar('0' + x%10); } else noan_putchar('0' + x);
    noan_putchar('H');
}

static void refresh_screen(void) {
    noan_clear();
    noan_print("--- VIM LITE - Ctrl+S: Save, Ctrl+Q: Quit ---\n");
    int cur_line = 0, disp_line = 0;
    for (int i = 0; i < text_len && disp_line < EDITOR_HEIGHT - 1; i++) {
        if (cur_line >= viewport_line && cur_line < viewport_line + EDITOR_HEIGHT - 1) {
            noan_putchar(text_buffer[i]);
            if (text_buffer[i] == '\n') { disp_line++; cur_line++; }
        } else {
            if (text_buffer[i] == '\n') cur_line++;
        }
    }
    update_screen_cursor();
}

static int editor_read(void) {
    int c = 0;
    while ((c = noan_getchar()) == 0) noan_yield();
    return c;
}

int sh_vim(int argc, char** argv) {
    const char* filename = (argc > 1) ? argv[1] : "newfile.txt";
    for (int i = 0; i < EDITOR_MAX_SIZE; i++) text_buffer[i] = 0;
    text_len = 0; cursor_x = 0; cursor_y = 1; viewport_line = 0;

    int n = noan_read_file(filename, text_buffer, EDITOR_MAX_SIZE - 1);
    if (n > 0) { text_len = n; text_buffer[n] = '\0'; }

    refresh_screen();

    while (1) {
        int c = editor_read();

        if (c == 17) { // Ctrl+Q
            noan_clear();
            return 0;
        } else if (c == 19) { // Ctrl+S
            noan_rm(filename, 1);
            noan_write_file(filename, text_buffer, 0);
            noan_print("\nSaved to "); noan_print(filename);
            noan_yield();
            refresh_screen();
        } else if (c == '\033') {
            int c2 = editor_read();
            if (c2 == '[') {
                int c3 = editor_read();
                if (c3 == 'A' && cursor_y > 1) {
                    cursor_y--;
                    if (cursor_y - viewport_line < 1) viewport_line--;
                    refresh_screen();
                } else if (c3 == 'B') {
                    int lines = 1;
                    for (int i = 0; i < text_len; i++) if (text_buffer[i] == '\n') lines++;
                    if (cursor_y < lines) {
                        cursor_y++;
                        if (cursor_y - viewport_line >= EDITOR_HEIGHT - 1) viewport_line++;
                        refresh_screen();
                    }
                } else if (c3 == 'C' && cursor_x < EDITOR_WIDTH - 1) {
                    cursor_x++; update_screen_cursor();
                } else if (c3 == 'D' && cursor_x > 0) {
                    cursor_x--; update_screen_cursor();
                }
            }
        } else if (c == '\b') {
            if (text_len > 0) {
                text_buffer[--text_len] = '\0';
                cursor_x = 0; cursor_y = 1; viewport_line = 0;
                for (int i = 0; i < text_len; i++) {
                    if (text_buffer[i] == '\n') { cursor_y++; cursor_x = 0; }
                    else if (++cursor_x >= EDITOR_WIDTH) { cursor_x = 0; cursor_y++; }
                }
                refresh_screen();
            }
        } else if (c == '\n' || c == '\r') {
            if (text_len < EDITOR_MAX_SIZE - 1) {
                text_buffer[text_len++] = '\n';
                cursor_x = 0; cursor_y++;
                if (cursor_y - viewport_line >= EDITOR_HEIGHT - 1) viewport_line++;
                refresh_screen();
            }
        } else if (c >= 32 && c < 127) {
            if (text_len < EDITOR_MAX_SIZE - 1) {
                text_buffer[text_len++] = (char)c;
                noan_putchar(c);
                if (++cursor_x >= EDITOR_WIDTH) {
                    cursor_x = 0; cursor_y++;
                    if (cursor_y - viewport_line >= EDITOR_HEIGHT - 1) viewport_line++;
                }
            }
        }
    }
}
