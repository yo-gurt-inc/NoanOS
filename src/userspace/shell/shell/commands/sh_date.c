#include "cpu/syscall.h"
#include "shell/commands.h"

static void print_num(u32 val, int digits) {
    char b[12];
    int i = 0;
    if (val == 0) {
        for(int j=0; j<digits; j++) _syscall1(SYS_PUTCHAR, '0');
        return;
    }
    while (val > 0) {
        b[i++] = '0' + (val % 10);
        val /= 10;
    }
    while(i < digits) b[i++] = '0';
    for (int j = i - 1; j >= 0; j--) {
        _syscall1(SYS_PUTCHAR, b[j]);
    }
}

int sh_date(int argc, char** argv) {
    (void)argc; (void)argv;
    u8 h = 0, m = 0, s = 0;
    u8 d = 0, mo = 0;
    u16 y = 0;

    _syscall3(SYS_GET_TIME, (u32)&h, (u32)&m, (u32)&s);
    _syscall3(SYS_GET_DATE, (u32)&d, (u32)&mo, (u32)&y);

    _syscall1(SYS_PRINT, (u32)"Current date: ");
    print_num(y, 4);
    _syscall1(SYS_PUTCHAR, '-');
    print_num(mo, 2);
    _syscall1(SYS_PUTCHAR, '-');
    print_num(d, 2);
    
    _syscall1(SYS_PRINT, (u32)"  Time: ");
    print_num(h, 2);
    _syscall1(SYS_PUTCHAR, ':');
    print_num(m, 2);
    _syscall1(SYS_PUTCHAR, ':');
    print_num(s, 2);
    _syscall1(SYS_PUTCHAR, '\n');

    return 0;
}
