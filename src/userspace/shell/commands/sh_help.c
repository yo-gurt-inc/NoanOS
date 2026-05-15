#include "cpu/syscall.h"
#include "shell/commands.h"

extern shell_command_t commands[];
extern int num_commands;

int sh_help(int argc, char** argv) {
    (void)argc; (void)argv;
    _syscall1(SYS_PRINT, (u32)"Available commands:\n");
    for (int i = 0; i < num_commands; i++) {
        _syscall1(SYS_PRINT, (u32)"  ");
        _syscall1(SYS_PRINT, (u32)commands[i].name);
        _syscall1(SYS_PRINT, (u32)" - ");
        _syscall1(SYS_PRINT, (u32)commands[i].help);
        _syscall1(SYS_PUTCHAR, '\n');
    }
    return 0;
}
