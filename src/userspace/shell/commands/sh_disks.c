#include "cpu/syscall.h"
#include "shell/commands.h"

int sh_disks(int argc, char** argv) {
    (void)argc; (void)argv;
    _syscall0(SYS_LIST_DISKS);
    return 0;
}
