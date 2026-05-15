#include "cpu/syscall.h"
#include "shell/commands.h"

int sh_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        // Simple redirection check
        if (argv[i][0] == '>' && argv[i+1]) {
            // This is actually handled by the shell parser usually
            // but for now we'll just skip it here as shell.c handles it.
            break;
        }
        _syscall1(SYS_PRINT, (u32)argv[i]);
        if (i < argc - 1) _syscall1(SYS_PUTCHAR, ' ');
    }
    _syscall1(SYS_PUTCHAR, '\n');
    return 0;
}
