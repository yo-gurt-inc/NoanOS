#include "cpu/syscall.h"
#include "shell/commands.h"

int sh_stat(int argc, char** argv) {
    if (argc < 2) {
        _syscall1(SYS_PRINT, (u32)"Usage: stat <name>\n");
        return 1;
    }
    _syscall1(SYS_STAT, (u32)argv[1]);
    return 0;
}
