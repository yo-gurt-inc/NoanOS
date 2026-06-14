#include "shell/noan.h"
#include "shell/commands.h"

int sh_uptime(int argc, char** argv) {
    (void)argc; (void)argv;
    u32 ticks = noan_get_ticks();
    noan_print("System Uptime: ");
    char buf[12]; int i = 0;
    if (ticks == 0) { noan_putchar('0'); }
    else { while (ticks > 0) { buf[i++] = '0' + (ticks % 10); ticks /= 10; } for (int j = i-1; j >= 0; j--) noan_putchar(buf[j]); }
    noan_print(" ticks\n");
    return 0;
}
