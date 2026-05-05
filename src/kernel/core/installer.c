#include "core/installer.h"
#include "io/kprint.h"
#include "io/terminal.h"
#include "io/keyboard.h"
#include "storage/ata.h"
#include "storage/fat32.h"
#include "core/malloc.h"
#include "io/io.h"
#include "system/timer.h"
#include "core/initrd.h"

// Forward declarations for FAT32 internal helpers needed for installation
extern void _fat32_create_entry(const char* name, u8 attr, u32 first_cluster, u32 size);
extern u32 _fat32_find_free_cluster(void);
extern void _fat32_set_fat_entry(u32 cluster, u32 value);
extern u32 _fat32_cluster_to_lba(u32 cluster);
extern ata_drive_t* _fat32_get_current_drive(void);
extern int _fat32_find_entry(const char* name, void* out_entry);
extern void _fat32_set_current_dir_cluster(u32 c);
extern fat32_bpb_t* _fat32_get_bpb(void);

static void install_file(const char* name, void* data, u32 size) {
    u32 sectors = (size + 511) / 512;
    u32 first_clus = 0;
    u32 prev_clus = 0;
    u8* b_ptr = (u8*)data;

    fat32_bpb_t* bpb = _fat32_get_bpb();
    u32 spc = bpb ? bpb->sectors_per_cluster : 1;
    u32 clusters_needed = (sectors + spc - 1) / spc;

    for (u32 c = 0; c < clusters_needed; c++) {
        u32 clus = _fat32_find_free_cluster();
        if (clus == 0) return;

        if (c == 0) first_clus = clus;
        if (prev_clus != 0) _fat32_set_fat_entry(prev_clus, clus);
        _fat32_set_fat_entry(clus, 0x0FFFFFFF);

        u32 write_sectors = spc;
        if ((c+1)*spc > sectors) write_sectors = sectors - c*spc;

        ata_write_sectors(_fat32_get_current_drive(), _fat32_cluster_to_lba(clus), write_sectors, (u16*)b_ptr);
        b_ptr += write_sectors * 512;
        prev_clus = clus;
    }

    _fat32_create_entry(name, 0x20, first_clus, size);
}

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

static void reboot_os(void) {
    kprint("Rebooting...\n");
    u8 good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);
    // The CPU should reset here, but if it doesn't, we halt.
    asm volatile("hlt"); 
}

int installer_start(u32 boot_drive) {
    kprint_init();
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
    kprint("\nSelection: ");
    
    char choice = wait_for_key();
    terminal_putchar(choice);
    kprint("\n");
    
    if (choice == 'L' || choice == 'l') {
        kprint("Starting Live Mode...\n");
        return 0; // Did not install
    }
    
    if (choice >= '0' && choice <= '3') {
        int idx = choice - '0';
        ata_drive_t* dest = ata_get_drive(idx);
        ata_drive_t* src = ata_get_drive(boot_idx);

        if (idx == boot_idx) {
            kprint("Error: Cannot install onto the source drive!\n");
            wait_for_key();
            return installer_start(boot_drive); // Recurse for a valid choice
        }

        if (!dest || !dest->exists) {
            kprint("Error: Drive not found!\n");
            wait_for_key();
            return installer_start(boot_drive); // Recurse for a valid choice
        }

        kprint("Proceed with installation on "); kprint(dest->name); kprint("? (y/n): ");
        char confirm = wait_for_key();
        terminal_putchar(confirm);
        kprint("\n");
        if (confirm == 'y' || confirm == 'Y') {
            fat32_format(dest); 

            kprint("Installing NoanOS to "); kprint(dest->name); kprint("...\n");
            kprint("Copying 700 sectors (Boot + Kernel + Initrd)...\n");

            u16* buf = (u16*)kmalloc(512);
            u16* bpb_buf = (u16*)kmalloc(512);
            ata_read_sectors(dest, 0, 1, bpb_buf);

            for (int i = 0; i < 700; i++) {
                ata_read_sectors(src, i, 1, buf);
                if (i == 0) {
                    u8* boot_code = (u8*)buf;
                    u8* bpb_data = (u8*)bpb_buf;
                    for (int j = 3; j < 90; j++) boot_code[j] = bpb_data[j];
                }
                ata_write_sectors(dest, i, 1, buf);
                if (i % 32 == 0) { kprint_dec(i); kprint(" "); }
            }

            kfree(buf);
            kfree(bpb_buf);

            kprint("\nFinalizing File System...\n");
            fat32_init(dest);
            
            // Create /bin directory
            fat32_mkdir("/bin");
            
            // Clear LIVE signature on installed disk (sector 255)
            {
                u16* clearbuf = (u16*)kmalloc(512);
                if (clearbuf) {
                    u8* cb = (u8*)clearbuf;
                    for (int _i = 0; _i < 512; _i++) cb[_i] = 0;
                    ata_write_sectors(dest, 255, 1, clearbuf);
                    kfree(clearbuf);
                }
            }

            // Automatically install all files from initrd with NOAN wrapping to /bin
            int file_count = initrd_get_file_count();
            for (int i = 0; i < file_count; i++) {
                char name[32];
                u32 size;
                void* data = initrd_get_file(i, name, &size);
                if (data) {
                    char fullpath[64];
                    fullpath[0] = '/'; fullpath[1] = 'b'; fullpath[2] = 'i'; fullpath[3] = 'n'; fullpath[4] = '/';
                    int k = 0;
                    while(name[k] && k < 32) { fullpath[k+5] = name[k]; k++; }
                    fullpath[k+5] = '\0';
                    
                    kprint("  Installing "); kprint(fullpath); kprint("...\n");

                    // Wrap all binaries (shell and user commands) into NOAN format
                    u32 magic = 0x4E4F414E; // 'NOAN'
                    u32 entry_off = 16;
                    u32 code_size = size;
                    u32 data_size = 0;
                    u8* header = (u8*)kmalloc(16 + size);
                    if (header) {
                        header[0] = (u8)(magic & 0xFF);
                        header[1] = (u8)((magic>>8)&0xFF);
                        header[2] = (u8)((magic>>16)&0xFF);
                        header[3] = (u8)((magic>>24)&0xFF);
                        header[4] = (u8)(entry_off & 0xFF);
                        header[5] = (u8)((entry_off>>8)&0xFF);
                        header[6] = (u8)((entry_off>>16)&0xFF);
                        header[7] = (u8)((entry_off>>24)&0xFF);
                        header[8] = (u8)(code_size & 0xFF);
                        header[9] = (u8)((code_size>>8)&0xFF);
                        header[10] = (u8)((code_size>>16)&0xFF);
                        header[11] = (u8)((code_size>>24)&0xFF);
                        header[12] = (u8)(data_size & 0xFF);
                        header[13] = (u8)((data_size>>8)&0xFF);
                        header[14] = (u8)((data_size>>16)&0xFF);
                        header[15] = (u8)((data_size>>24)&0xFF);
                        for (u32 m = 0; m < (u32)size; m++) header[16 + m] = ((u8*)data)[m];
                        
                        install_file(fullpath, header, 16 + size);
                        kfree(header);
                    } else {
                        install_file(fullpath, data, size);
                    }
                }
            }

            // Reset current dir cluster to root for safety
            {
                fat32_bpb_t* bpb = _fat32_get_bpb();
                _fat32_set_current_dir_cluster(bpb->root_cluster);
            }

            kprint("\nSuccess! NoanOS and all commands are now installed.\n");
            kprint("Press any key to reboot...");
            wait_for_key();
            reboot_os();
            return 1; // Installed
        } else {
            kprint("Installation cancelled.\n");
            wait_for_key();
            return installer_start(boot_drive); // Recurse for a valid choice
        }
    } else {
        kprint("Invalid selection. Starting Live Mode...\n");
        return 0; // Did not install
    }
}
