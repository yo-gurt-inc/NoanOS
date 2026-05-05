#include "libnoan.h"

void _start(int argc, char** argv) {
    if (argc < 2) {
        print("Usage: touch <file>\n");
        exit();
    }
    _syscall1(SYS_TOUCH, (u32)argv[1]);
    exit();
}
