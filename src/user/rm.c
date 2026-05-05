#include "libnoan.h"

void _start(int argc, char** argv) {
    if (argc < 2) {
        print("Usage: rm <file>\n");
        exit();
    }
    _syscall2(SYS_RM, (u32)argv[1], 0);
    exit();
}
