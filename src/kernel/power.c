#include "include/power.h"
#include "include/io.h"
#include "include/kprint.h"

void reboot(void) {
    kprint("Rebooting...\n");
    u8 good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);
}

void shutdown(void) {
    kprint("Shutting down...\n");
    // QEMU shutdown
    outw(0x604, 0x2000);
    // VirtualBox shutdown
    outw(0x4004, 0x3400);
    // Bochs/Older QEMU
    outw(0xB004, 0x2000);
}
