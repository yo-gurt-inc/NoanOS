#include "cpu/syscall.h"
#include "shell/commands.h"

int sh_shutdown(int argc, char** argv) {
    (void)argc; (void)argv;
    _syscall0(SYS_SHUTDOWN);
    return 0;
}
