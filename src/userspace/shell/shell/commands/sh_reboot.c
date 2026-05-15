#include "cpu/syscall.h"
#include "shell/commands.h"

int sh_reboot(int argc, char** argv) {
    (void)argc; (void)argv;
    _syscall0(SYS_REBOOT);
    return 0;
}
