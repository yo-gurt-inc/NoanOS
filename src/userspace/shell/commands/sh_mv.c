#include "cpu/syscall.h"
#include "shell/commands.h"

int sh_mv(int argc, char** argv) {
    if (argc < 3) {
        _syscall1(SYS_PRINT, (u32)"Usage: mv <src> <dest>\n");
        return 1;
    }
    _syscall2(SYS_MV, (u32)argv[1], (u32)argv[2]);
    return 0;
}
