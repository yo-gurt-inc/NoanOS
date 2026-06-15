#include "core/panic.h"
#include "io/kprint.h"
#include "io/terminal.h"
#include "io/serial.h"

void panic(const char* message) {
    asm volatile("cli");

    serial_puts("\n[KERNEL PANIC] ");
    serial_puts(message);
    serial_puts("\n");

    // Set terminal color to White on Red
    terminal_set_color(vga_entry_color(15, 4)); // 15 = White, 4 = Red
    
    kprint("\n\n");
    kprint("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    kprint("!!                                KERNEL PANIC                                !!\n");
    kprint("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    kprint("\nReason: ");
    kprint(message);
   
    kprint("Please reboot your computer.\n");

    // Halt the CPU
    while (1) {
        asm volatile("hlt");
    }
}
