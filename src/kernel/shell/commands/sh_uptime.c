#include "cpu/syscall.h"
#include "shell/commands.h"

int sh_uptime(int argc, char** argv) {
    (void)argc; (void)argv;
    u32 ticks = (u32)_syscall0(SYS_GET_TICKS);
    
    _syscall1(SYS_PRINT, (u32)"System Uptime: ");
    
    char buf[12];
    int i = 0;
    if (ticks == 0) {
        _syscall1(SYS_PUTCHAR, '0');
    } else {
        while (ticks > 0) {
            buf[i++] = '0' + (ticks % 10);
            ticks /= 10;
        }
        for (int j = i - 1; j >= 0; j--) {
            _syscall1(SYS_PUTCHAR, buf[j]);
        }
    }
    
    _syscall1(SYS_PRINT, (u32)" ticks\n");
    return 0;
}
