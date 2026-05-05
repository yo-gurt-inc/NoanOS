#include "libnoan.h"

void _start(int argc, char** argv) {
    if (argc < 2) {
        print("Usage: mkdir <dirname>\n");
        exit();
    }
    _syscall1(SYS_MKDIR, (u32)argv[1]);
    exit();
}
