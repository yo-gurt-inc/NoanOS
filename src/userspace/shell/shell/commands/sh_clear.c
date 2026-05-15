#include "cpu/syscall.h"
#include "shell/commands.h"

int sh_clear(int argc, char** argv) {
    (void)argc; (void)argv;
    _syscall0(SYS_CLEAR);
    return 0;
}
