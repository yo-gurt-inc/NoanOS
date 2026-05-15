#include "cpu/syscall.h"
#include "shell/commands.h"

int sh_ls(int argc, char** argv) {
    (void)argc; (void)argv;
    _syscall0(SYS_LS);
    return 0;
}
