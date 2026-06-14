#include "shell/noan.h"
#include "shell/commands.h"

void shell_main(void);

void _start(void) {
    shell_main();
    noan_exit();
}

#define shell_print(s) noan_print(s)
#define shell_putchar(c) noan_putchar(c)

char current_path[256] = "/";

#define MAX_HISTORY 20
static char history[MAX_HISTORY][128];
static int history_count = 0;
static int history_index = -1;
static const char* history_file = "/.sh_hist";

static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}



static void strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

static void strcat(char* dest, const char* src) {
    while (*dest) dest++;
    while ((*dest++ = *src++));
}

static size_t strlen(const char* s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

static int shell_getchar(void) {
    int c;
    while ((c = noan_getchar()) == 0);
    return c;
}

static void load_history(void) {
    char* buf = (char*)(noan_malloc(4096));
    if (!buf) return;

    int bytes = noan_read_file(history_file, buf, 4095);
    if (bytes <= 0) {
        noan_free(buf);
        return;
    }
    buf[bytes] = '\0';

    char* p = buf;
    while (*p && history_count < MAX_HISTORY) {
        char* start = p;
        while (*p && *p != '\n' && *p != '\r') p++;
        char end_char = *p;
        *p = '\0';
        if (strlen(start) > 0) {
            strcpy(history[history_count++], start);
        }
        if (end_char != '\0') p++;
        while (*p && (*p == '\n' || *p == '\r')) p++;
    }
    noan_free(buf);
}

static void save_history(void) {
    char* buf = (char*)(noan_malloc(4096));
    if (!buf) return;

    buf[0] = '\0';
    for (int i = 0; i < history_count; i++) {
        strcat(buf, history[i]);
        strcat(buf, "\n");
    }
    noan_rm(history_file, 1);
    noan_write_file(history_file, buf, 1);
    noan_free(buf);
}

static void add_history(const char* cmd) {
    if (strlen(cmd) == 0) return;
    if (history_count > 0 && strcmp(cmd, history[history_count - 1]) == 0) return;

    if (history_count < MAX_HISTORY) {
        strcpy(history[history_count++], cmd);
    } else {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(history[i], history[i + 1]);
        }
        strcpy(history[MAX_HISTORY - 1], cmd);
    }
    save_history();
}

void shell_main(void) {
    load_history();
    shell_print("=== NoanOS Shell (User Mode) ===\n");
    shell_print("Type 'help' for commands.\n");

    char cmd[128];
    for(int i=0; i<128; i++) cmd[i] = 0;
    int len = 0;

    shell_print(current_path);
    shell_print(" > ");

    while (1) {
        int c = shell_getchar();

        if (c == '\n' || c == '\r') {
            shell_putchar('\n');
            cmd[len] = '\0';
            
            if (len > 0) {
                execute_commands(cmd);
                add_history(cmd);
                history_index = -1;
            }

            for (int i = 0; i < 128; i++) cmd[i] = 0;
            len = 0;
            history_index = -1;
            shell_print(current_path);
            shell_print(" > ");
        } else if (c == '\b') {
            if (len > 0) {
                len--;
                shell_putchar('\b');
            }
        } else if (c == '\033') { // Arrow Keys
            // Only proceed if it's really an arrow key sequence ([A or [B)
            int c2 = shell_getchar();
            if (c2 == '[') {
                int c3 = shell_getchar();
                if (c3 == 'A') { // Up
                    if (history_count > 0 && history_index < history_count - 1) {
                        // Clear current line
                        for (int i = 0; i < len; i++) shell_putchar('\b');
                        
                        history_index++;
                        strcpy(cmd, history[history_count - 1 - history_index]);
                        len = strlen(cmd);
                        shell_print(cmd);
                    }
                } else if (c3 == 'B') { // Down
                    if (history_index > -1) {
                        // Clear current line
                        for (int i = 0; i < len; i++) shell_putchar('\b');
                        
                        history_index--;
                        if (history_index == -1) {
                            cmd[0] = '\0';
                            len = 0;
                        } else {
                            strcpy(cmd, history[history_count - 1 - history_index]);
                            len = strlen(cmd);
                            shell_print(cmd);
                        }
                    }
                }
            }
        } else if (c >= 32 && c < 127) {
            if (len < 127) {
                cmd[len++] = (char)c;
                shell_putchar(c);
            }
        }
    }
}
