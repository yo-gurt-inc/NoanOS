#include "cpu/syscall.h"
#include "shell/commands.h"
#include "core/malloc.h"

static void print_mem_num(u32 val) {
    char b[12];
    int i = 0;
    if (val == 0) {
        _syscall1(SYS_PUTCHAR, '0');
        return;
    }
    while (val > 0) {
        b[i++] = '0' + (val % 10);
        val /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        _syscall1(SYS_PUTCHAR, b[j]);
    }
}

int sh_mem(int argc, char** argv) {
    (void)argc; (void)argv;
    malloc_info_t info = {0, 0, 0, 0, 0};
    _syscall1(SYS_MEM_INFO, (u32)&info);

    _syscall1(SYS_PRINT, (u32)"Heap Memory Usage:\n");
    _syscall1(SYS_PRINT, (u32)"  Total Size: "); print_mem_num(info.total_size); _syscall1(SYS_PRINT, (u32)" bytes\n");
    _syscall1(SYS_PRINT, (u32)"  Used Size:  "); print_mem_num(info.used_size); _syscall1(SYS_PRINT, (u32)" bytes\n");
    _syscall1(SYS_PRINT, (u32)"  Free Size:  "); print_mem_num(info.free_size); _syscall1(SYS_PRINT, (u32)" bytes\n");
    _syscall1(SYS_PRINT, (u32)"  Blocks:     "); print_mem_num(info.total_blocks); _syscall1(SYS_PRINT, (u32)" (Free: "); print_mem_num(info.free_blocks); _syscall1(SYS_PRINT, (u32)")\n");

    return 0;
}
