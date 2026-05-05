#include "core/types.h"
#include "cpu/syscall.h"

void _start(int argc, char** argv) {
    if (argc < 2) {
        _syscall1(SYS_PRINT, (u32)"Usage: cat <filename>\n");
        _syscall0(SYS_EXIT);
    }

    const char* filename = argv[1];
    
    // Debug print
    _syscall1(SYS_PRINT, (u32)"[NOAN] cat opening: ");
    _syscall1(SYS_PRINT, (u32)filename);
    _syscall1(SYS_PRINT, (u32)"\n");

    char* buf = (char*)_syscall1(SYS_MALLOC, 4096);
    if (buf) {
        int read = _syscall3(SYS_READ_FILE, (u32)filename, (u32)buf, 4095);
        if (read >= 0) {
            buf[read] = '\0';
            _syscall1(SYS_PRINT, (u32)buf);
            _syscall1(SYS_PUTCHAR, '\n');
        } else {
            _syscall1(SYS_PRINT, (u32)"Error: Could not read file\n");
        }
        _syscall1(SYS_FREE, (u32)buf);
    }

    _syscall0(SYS_EXIT);
}
