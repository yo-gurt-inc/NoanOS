#include "libnoan.h"

void _start(int argc, char** argv) {
    if (argc < 2) {
        print("Usage: rm <file>\n");
        exit();
    }
    const char* path = argv[1];
    if (path[0] == '.' && path[1] == '/') path += 2;
    _syscall2(SYS_RM, (u32)path, 0);
    exit();
}
