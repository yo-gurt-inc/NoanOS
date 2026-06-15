#include "shell/noan.h"
#include "shell/commands.h"

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
    {"ls",       sh_ls,       "List directory contents"},
    {"cd",       sh_cd,       "Change directory"},
    {"pwd",      sh_pwd,      "Print working directory"},
    {"mkdir",    sh_mkdir,    "Create directory"},
    {"rm",       sh_rm,       "Remove file or directory"},
    {"cp",       sh_cp,       "Copy file"},
    {"mv",       sh_mv,       "Move/rename file"},
    {"touch",    sh_touch,    "Create empty file"},
    {"cat",      sh_cat,      "Print file contents"},
    {"echo",     sh_echo,     "Print text"},
    {"vim",      sh_vim,      "Simple text editor"},
    {"clear",    sh_clear,    "Clear screen"},
    {"help",     sh_help,     "Show this help message"},
    {"reboot",   sh_reboot,   "Reboot system"},
    {"shutdown", sh_shutdown, "Shutdown system"},
    {"uptime",   sh_uptime,   "Show system uptime"},
    {"panic",    sh_panic,    "Trigger a kernel panic"},
    {"date",     sh_date,     "Show current date and time"},
    {"mem",      sh_mem,      "Show memory usage statistics"},
    {"disks",    sh_disks,    "List detected storage drives"},
    {"stat",     sh_stat,     "Show file or directory information"},
};

int num_commands = sizeof(commands) / sizeof(shell_command_t);

static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int execute_command(const char* cmd_line) {
    int len = 0;
    while (cmd_line[len]) len++;

    char* buf = (char*)noan_malloc(len + 1);
    if (!buf) return -1;
    for (int i = 0; i <= len; i++) buf[i] = cmd_line[i];

    int argc = 0;
    char* p = buf;
    while (*p) {
        while (*p && (unsigned char)*p <= 32) p++;
        if (!*p) break;
        argc++;
        while (*p && (unsigned char)*p > 32) p++;
    }

    if (argc == 0) { noan_free(buf); return 0; }

    char** argv = (char**)noan_malloc((argc + 1) * sizeof(char*));
    if (!argv) { noan_free(buf); return -1; }

    int i = 0; p = buf;
    while (*p && i < argc) {
        while (*p && (unsigned char)*p <= 32) p++;
        if (!*p) break;
        argv[i++] = p;
        while (*p && (unsigned char)*p > 32) p++;
        if (*p) *p++ = '\0';
    }
    argv[i] = (char*)0;

    int ret = -1;
    for (int j = 0; j < num_commands; j++) {
        if (strcmp(argv[0], commands[j].name) == 0) {
            ret = commands[j].handler(argc, argv);
            goto done;
        }
    }

    if (noan_exec(cmd_line) == 0) { ret = 0; goto done; }
    if (noan_exec_elf(argv[0]) >= 0) { ret = 0; goto done; }

    noan_print("Unknown command: ");
    noan_print(argv[0]);
    noan_putchar('\n');

done:
    noan_flush_kb();
    noan_free(argv);
    noan_free(buf);
    return ret;
}

int execute_commands(const char* cmd_line) {
    char buf[128];
    int i = 0;
    for (; cmd_line[i] && i < 127; i++) buf[i] = cmd_line[i];
    buf[i] = '\0';

    char* p = buf, *start = buf;
    int last_ret = 0;
    while (1) {
        if (*p == ';' || *p == '\0') {
            char end = *p; *p = '\0';
            char* check = start;
            while (*check && (unsigned char)*check <= 32) check++;
            if (*check != '\0') last_ret = execute_command(start);
            if (end == '\0') break;
            start = p + 1;
        }
        p++;
    }
    return last_ret;
}
