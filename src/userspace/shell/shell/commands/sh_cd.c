#include "cpu/syscall.h"
#include "shell/commands.h"

int sh_cd(int argc, char** argv) {
    if (argc < 2) {
        _syscall1(SYS_PRINT, (u32)"Usage: cd <path>\n");
        return 1;
    }
    _syscall1(SYS_CD, (u32)argv[1]);
    return 0;
}
