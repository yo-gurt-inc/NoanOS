#include "cpu/syscall.h"
#include "shell/commands.h"

int sh_rm(int argc, char** argv) {
    if (argc < 2) {
        _syscall1(SYS_PRINT, (u32)"Usage: rm <name>\n");
        return 1;
    }
    _syscall2(SYS_RM, (u32)argv[1], 0);
    return 0;
}
