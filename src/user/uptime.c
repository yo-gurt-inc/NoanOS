#include "libnoan.h"

void _start(int argc, char** argv) {
    (void)argc; (void)argv;
    int ticks = _syscall0(SYS_GET_TICKS);
    print("Uptime: ");
    print_dec(ticks / 100);
    print(" seconds\n");
    exit();
}
