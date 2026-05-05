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
    char buf[256];
    char* argv[32];
    int argc = 0;
    
    // Copy to local buffer
    int i = 0;
    for (; cmd_line[i] && i < 255; i++) buf[i] = cmd_line[i];
    buf[i] = '\0';

    char* p = buf;
    while (*p && argc < 32) {
        // Skip leading whitespace (anything <= 32 is treated as space)
        while (*p && (unsigned char)*p <= 32) p++;
        if (*p == '\0') break;

        argv[argc++] = p;

        // Find end of token
        while (*p && (unsigned char)*p > 32) p++;
        
        if (*p == '\0') break;
        *p++ = '\0'; // Null-terminate and advance
    }

    if (argc == 0) return 0;

    // Try built-in commands first
    for (int j = 0; j < num_commands; j++) {
        if (strcmp(argv[0], commands[j].name) == 0) {
            return commands[j].handler(argc, argv);
        }
    }

    // If not a built-in, try to execute as external binary
    // Pass full command line to SYS_EXEC which handles path parsing and argc/argv setup
    if (_syscall1(SYS_EXEC, (u32)cmd_line) == 0) {
        return 0;  // Success
    }

    // If execution failed - extract just the command name from original cmd_line
    char cmd_name[64];
    int k = 0;
    for (int j = 0; cmd_line[j] && (unsigned char)cmd_line[j] > 32 && k < 63; j++) {
        cmd_name[k++] = cmd_line[j];
    }
    cmd_name[k] = '\0';
    
    _syscall1(SYS_PRINT, (u32)"Unknown command: ");
    _syscall1(SYS_PRINT, (u32)cmd_name);
    _syscall1(SYS_PUTCHAR, '\n');
    return -1;
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
