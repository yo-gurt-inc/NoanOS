#include "shell/commands.h"
#include "core/types.h"
#include "cpu/syscall.h"

// Command prototypes
int sh_ls(int argc, char** argv);
int sh_cd(int argc, char** argv);
int sh_pwd(int argc, char** argv);
int sh_mkdir(int argc, char** argv);
int sh_rm(int argc, char** argv);
int sh_cp(int argc, char** argv);
int sh_mv(int argc, char** argv);
int sh_touch(int argc, char** argv);
int sh_cat(int argc, char** argv);
int sh_echo(int argc, char** argv);
int sh_clear(int argc, char** argv);
int sh_help(int argc, char** argv);
int sh_reboot(int argc, char** argv);
int sh_shutdown(int argc, char** argv);
int sh_vim(int argc, char** argv);
int sh_uptime(int argc, char** argv);
int sh_panic(int argc, char** argv);
int sh_date(int argc, char** argv);
int sh_mem(int argc, char** argv);
int sh_disks(int argc, char** argv);
int sh_stat(int argc, char** argv);

shell_command_t commands[] = {
    {"ls", sh_ls, "List directory contents"},
    {"cd", sh_cd, "Change directory"},
    {"pwd", sh_pwd, "Print working directory"},
    {"mkdir", sh_mkdir, "Create directory"},
    {"rm", sh_rm, "Remove file or directory"},
    {"cp", sh_cp, "Copy file"},
    {"mv", sh_mv, "Move/rename file"},
    {"touch", sh_touch, "Create empty file"},
    {"cat", sh_cat, "Print file contents"},
    {"echo", sh_echo, "Print text"},
    {"vim", sh_vim, "Simple text editor"},
    {"clear", sh_clear, "Clear screen"},
    {"help", sh_help, "Show this help message"},
    {"reboot", sh_reboot, "Reboot system"},
    {"shutdown", sh_shutdown, "Shutdown system"},
    {"uptime", sh_uptime, "Show system uptime"},
    {"panic", sh_panic, "Trigger a kernel panic"},
    {"date", sh_date, "Show current date and time"},
    {"mem", sh_mem, "Show memory usage statistics"},
    {"disks", sh_disks, "List detected storage drives"},
    {"stat", sh_stat, "Show file or directory information"}
};

int num_commands = sizeof(commands) / sizeof(shell_command_t);

static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int execute_command(const char* cmd_line) {
    /* --- Step 1: Copy input into a mutable heap buffer ---
     * cmd_line may be read-only; we need to write '\0' terminators into it.
     * We allocate exactly (len+1) bytes so there is no fixed upper bound on
     * input length beyond what SYS_MALLOC can satisfy. */
    int len = 0;
    while (cmd_line[len]) len++;

    char* buf = (char*)_syscall1(SYS_MALLOC, len + 1);
    if (!buf) return -1;

    for (int i = 0; i <= len; i++) buf[i] = cmd_line[i];

    /* --- Step 2: First pass — count tokens ---
     * We count before allocating argv so the argv array is exactly the right
     * size and can never overflow. */
    int argc = 0;
    char* p = buf;
    while (*p) {
        while (*p && (unsigned char)*p <= 32) p++;   /* skip whitespace */
        if (!*p) break;
        argc++;
        while (*p && (unsigned char)*p > 32) p++;    /* skip token */
    }

    if (argc == 0) {
        _syscall1(SYS_FREE, (u32)buf);
        return 0;
    }

    /* --- Step 3: Allocate argv and do second pass ---
     * argv[argc] is kept as NULL (standard C convention) so any command that
     * iterates until argv[i]==NULL works correctly without knowing argc. */
    char** argv = (char**)_syscall1(SYS_MALLOC, (argc + 1) * sizeof(char*));
    if (!argv) {
        _syscall1(SYS_FREE, (u32)buf);
        return -1;
    }

    int i = 0;
    p = buf;
    while (*p && i < argc) {
        while (*p && (unsigned char)*p <= 32) p++;   /* skip whitespace */
        if (!*p) break;

        argv[i++] = p;                               /* record token start */

        while (*p && (unsigned char)*p > 32) p++;    /* find token end */
        if (*p) *p++ = '\0';                         /* null-terminate token */
    }
    argv[i] = (char*)0;  /* sentinel NULL — argv is now fully standard */

    /* --- Step 4: Dispatch to built-in or external ---
     * argv[0] is the command name; reset state is implicit because buf and
     * argv are freshly allocated on every call. */
    int ret = -1;

    for (int j = 0; j < num_commands; j++) {
        if (strcmp(argv[0], commands[j].name) == 0) {
            ret = commands[j].handler(argc, argv);
            goto done;
        }
    }

    /* Not a built-in — try SYS_EXEC with the original command line. */
    if (_syscall1(SYS_EXEC, (u32)cmd_line) == 0) {
        ret = 0;
        goto done;
    }

    _syscall1(SYS_PRINT, (u32)"Unknown command: ");
    _syscall1(SYS_PRINT, (u32)argv[0]);
    _syscall1(SYS_PUTCHAR, '\n');

done:
    /* --- Step 5: Always free both allocations before returning ---
     * argv pointers all point into buf, so freeing buf first would corrupt
     * argv — free argv first, then buf. */
    _syscall1(SYS_FREE, (u32)argv);
    _syscall1(SYS_FREE, (u32)buf);
    return ret;
}

int execute_commands(const char* cmd_line) {
    char buf[128];
    int i = 0;
    for (; cmd_line[i] && i < 127; i++) buf[i] = cmd_line[i];
    buf[i] = '\0';

    char* p = buf;
    char* start = buf;
    int last_ret = 0;

    while (1) {
        if (*p == ';' || *p == '\0') {
            char end = *p;
            *p = '\0';

            // Check if segment has any non-whitespace content
            char* check = start;
            while (*check && (unsigned char)*check <= 32) check++;
            
            if (*check != '\0') {
                last_ret = execute_command(start);
            }

            if (end == '\0') break;
            start = p + 1;
        }
        p++;
    }

    return last_ret;
}
