#include "shell/noan.h"
#include "shell/commands.h"

static void print_padded(u32 val, int digits) {
    char b[12]; int i = 0;
    if (val == 0) { while (digits--) noan_putchar('0'); return; }
    while (val > 0) { b[i++] = '0' + (val % 10); val /= 10; }
    while (i < digits) b[i++] = '0';
    for (int j = i-1; j >= 0; j--) noan_putchar(b[j]);
}

int sh_date(int argc, char** argv) {
    (void)argc; (void)argv;
    u8 h=0, m=0, s=0, d=0, mo=0; u16 y=0;
    noan_get_time(&h, &m, &s);
    noan_get_date(&d, &mo, &y);
    noan_print("Current date: ");
    print_padded(y,4); noan_putchar('-'); print_padded(mo,2); noan_putchar('-'); print_padded(d,2);
    noan_print("  Time: ");
    print_padded(h,2); noan_putchar(':'); print_padded(m,2); noan_putchar(':'); print_padded(s,2);
    noan_putchar('\n');
    return 0;
}
