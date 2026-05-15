#ifndef COMMANDS_H
#define COMMANDS_H

#include "core/types.h"

typedef int (*command_handler_t)(int argc, char** argv);

typedef struct {
    const char* name;
    command_handler_t handler;
    const char* help;
} shell_command_t;

void commands_init(void);
int execute_command(const char* cmd_line);
int execute_commands(const char* cmd_line);

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
int sh_date(int argc, char** argv);
int sh_mem(int argc, char** argv);
int sh_disks(int argc, char** argv);
int sh_stat(int argc, char** argv);
int sh_uptime(int argc, char** argv);
int sh_panic(int argc, char** argv);

#endif
