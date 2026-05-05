#include "libnoan.h"

void _start(int argc, char** argv) {
    (void)argc; (void)argv;
    _syscall0(SYS_LS);
    exit();
}
