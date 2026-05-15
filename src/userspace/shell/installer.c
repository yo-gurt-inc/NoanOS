#include "shell/installer.h"
#include "io/kprint.h"
#include "io/terminal.h"
#include "io/keyboard.h"
#include "storage/ata.h"
#include "storage/fat32.h"
#include "core/malloc.h"
#include "io/io.h"
#include "system/timer.h"

static char wait_for_key(void) {
    char c = 0;
    while (!(c = keyboard_getchar())) {
        asm volatile("hlt");
    }
    return c;
}

static char wait_for_key_timeout(int seconds) {
    u32 start_ticks = timer_get_ticks();
    u32 timeout_ticks = seconds * 100; // 100Hz

    while (timer_get_ticks() < start_ticks + timeout_ticks) {
        char c = keyboard_getchar();
        if (c) return c;
        asm volatile("hlt");
    }
    return 0; // Timeout
}

static void reboot(void) {
    kprint("Rebooting...\n");
    u8 good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);
    
    // Fallback if keyboard controller doesn't work (triple fault)
    asm volatile("lidt %0; int3" : : "m"((u32[2]){0,0}));
}

void installer_start(u32 boot_drive) {
    kprint_init(); // Clear screen
    kprint("======================================\n");
    kprint("         NoanOS Installation Menu     \n");
    kprint("======================================\n\n");
    
    int boot_idx = (boot_drive >= 0x80) ? (boot_drive - 0x80) : 0;

    kprint("Detected Drives:\n");
    for (int i = 0; i < 4; i++) {
        ata_drive_t* d = ata_get_drive(i);
        if (d && d->exists) {
            kprint(" ["); kprint_dec(i); kprint("] ");
            kprint(d->name);
            if (i == boot_idx) kprint(" (SOURCE/LIVE DISK)");
            else kprint(" (TARGET HDD)");
            kprint("\n");
        }
    }
    
    kprint("\nOptions:\n");
    kprint(" [0-3] Select TARGET drive to INSTALL OS\n");
    kprint(" [L]   Run LIVE MODE (Skip Installation)\n");
    kprint("\nDefaulting to LIVE MODE in 10 seconds...\n");
    kprint("Selection: ");
    
    char choice = wait_for_key_timeout(10);
    if (choice == 0) {
        kprint("L (Timeout)\n");
        choice = 'L';
    } else {
        terminal_putchar(choice);
        kprint("\n");
    }
    
    if (choice == 'L' || choice == 'l') {
        kprint("Starting Live Mode...\n");
        return;
    }
    
    if (choice >= '0' && choice <= '3') {
        int idx = choice - '0';
        ata_drive_t* dest = ata_get_drive(idx);
        ata_drive_t* src = ata_get_drive(boot_idx);

        if (idx == boot_idx) {
            kprint("Error: Cannot install onto the source drive!\n");
            kprint("Please select a different drive.\n");
            kprint("Press any key to return to menu...");
            wait_for_key();
            installer_start(boot_drive);
            return;
        }

        if (!dest || !dest->exists) {
            kprint("Error: Drive not found!\n");
            kprint("Press any key to return to menu...");
            wait_for_key();
            installer_start(boot_drive);
            return;
        }

        kprint("WARNING: All data on "); kprint(dest->name); kprint(" will be LOST!\n");
        kprint("Proceed with installation? (y/n): ");
        char confirm = wait_for_key();
        terminal_putchar(confirm);
        kprint("\n");
        if (confirm == 'y' || confirm == 'Y') {
            // 1. Format the destination drive with a clean FAT32 structure
            fat32_format(dest); 

            kprint("Installing Simple OS to "); kprint(dest->name); kprint("...\n");
            kprint("Copying 128 sectors from "); kprint(src->name); kprint("...\n");

            u16* buf = (u16*)kmalloc(512);
            u16* bpb_buf = (u16*)kmalloc(512);

            // Save the BPB we just created in sector 0 of destination
            ata_read_sectors(dest, 0, 1, bpb_buf);

            for (int i = 0; i < 128; i++) {
                // Read from source (Live Disk)
                ata_read_sectors(src, i, 1, buf);

                if (i == 0) {
                    // For sector 0, we must preserve the BPB fields
                    // Bytes 3-89 in FAT32 are the BPB
                    u8* boot_code = (u8*)buf;
                    u8* bpb_data = (u8*)bpb_buf;
                    for (int j = 3; j < 90; j++) {
                        boot_code[j] = bpb_data[j];
                    }
                }

                // Write to destination (HDD)
                ata_write_sectors(dest, i, 1, buf);

                if (i % 16 == 0) {
                   kprint_dec(i);
                   kprint(" ");
                }
            }

            kprint("\nVerifying Installation...\n");
            ata_read_sectors(dest, 0, 1, buf);

            // Check for AA55 signature at the end of boot sector
            if (buf[255] == 0xAA55) {
                kprint("Installation verified.\n");
            } else {
                kprint("Warning: Verification signature mismatch!\n");
            }

            kfree(buf);
            kfree(bpb_buf);

            kprint("\nSuccess! NoanOS is now installed.\n");
            kprint("Press any key to reboot...");
            wait_for_key();
            reboot();
        }
    } else {
        kprint("Invalid selection. Starting Live Mode...\n");
    }
}
