#include "shell/noan.h"
#include "shell/commands.h"
#include "core/malloc.h"

static void print_num(u32 val) {
    char b[12]; int i = 0;
    if (val == 0) { noan_putchar('0'); return; }
    while (val > 0) { b[i++] = '0' + (val % 10); val /= 10; }
    for (int j = i-1; j >= 0; j--) noan_putchar(b[j]);
}

int sh_mem(int argc, char** argv) {
    (void)argc; (void)argv;
    malloc_info_t info = {0,0,0,0,0};
    noan_mem_info(&info);
    noan_print("Heap Memory Usage:\n");
    noan_print("  Total Size: "); print_num(info.total_size);  noan_print(" bytes\n");
    noan_print("  Used Size:  "); print_num(info.used_size);   noan_print(" bytes\n");
    noan_print("  Free Size:  "); print_num(info.free_size);   noan_print(" bytes\n");
    noan_print("  Blocks:     "); print_num(info.total_blocks); noan_print(" (Free: "); print_num(info.free_blocks); noan_print(")\n");
    return 0;
}
