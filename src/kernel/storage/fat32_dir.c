// fat32_dir.c - FAT32 Directory Operations: ls, cd, mkdir
#include "storage/fat32.h"
#include "io/kprint.h"
#include "io/terminal.h"
#include "core/malloc.h"
extern int _fat32_find_entry(const char* name, fat32_dir_entry_t* out_entry);
extern u32 _fat32_get_current_dir_cluster(void);
extern void _fat32_set_current_dir_cluster(u32 c);
extern ata_drive_t* _fat32_get_current_drive(void);
extern fat32_bpb_t* _fat32_get_bpb(void);
extern u32 _fat32_find_free_cluster(void);
extern void _fat32_set_fat_entry(u32 cluster, u32 value);
extern void _fat32_create_entry(const char* name, u8 attr, u32 first_cluster, u32 size);

// ============================================================================
// PUBLIC API: List files in current directory
// ============================================================================
void fat32_ls(void) {
    ata_drive_t* drive = _fat32_get_current_drive();
    if (!drive) {
        kprint("Error: No drive mounted\n");
        return;
    }
    
    fat32_bpb_t* bpb = _fat32_get_bpb();
    u32 cluster = _fat32_get_current_dir_cluster();
    u16* buf = (u16*)kmalloc(bpb->sectors_per_cluster * 512);
    if (!buf) return;

    ata_read_sectors(drive, _fat32_cluster_to_lba(cluster), bpb->sectors_per_cluster, buf);
    
    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buf;
    int max_entries = (bpb->sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);
    
    int count = 0;
    for (int i = 0; i < max_entries; i++) {
        if (entries[i].name[0] == 0x00) break;
        if (entries[i].name[0] == 0xE5) continue;
        if (entries[i].attr == FAT_ATTR_LFN) continue;
        
        count++;
        // Print filename (up to 8 chars)
        for (int j = 0; j < 8; j++) {
            if (entries[i].name[j] != ' ') terminal_putchar(entries[i].name[j]);
        }
        // Print extension if it exists (up to 3 chars)
        if (entries[i].name[8] != ' ') {
            terminal_putchar('.');
            for (int j = 8; j < 11; j++) {
                if (entries[i].name[j] != ' ') terminal_putchar(entries[i].name[j]);
            }
        }
        
        // Print type indicator
        if (entries[i].attr & FAT_ATTR_DIRECTORY) {
            kprint("/");
        }
        
        // Print file size for regular files
        if (!(entries[i].attr & FAT_ATTR_DIRECTORY)) {
            kprint(" ("); kprint_dec(entries[i].file_size); kprint("B)");
        }
        kprint("\n");
    }
    if (count == 0) {
        kprint("\n");
    }
    kfree(buf);
}

// ============================================================================
// PUBLIC API: Change directory
// ============================================================================
void fat32_cd(const char* name) {
    ata_drive_t* drive = _fat32_get_current_drive();
    if (!drive) return;
    
    if (name[0] == '/' && name[1] == '\0') {
        fat32_bpb_t* bpb = _fat32_get_bpb();
        _fat32_set_current_dir_cluster(bpb->root_cluster);
        kprint("Returned to root\n");
        return;
    }

    u8 fat_name[11];
    _fat32_name_to_83(name, fat_name);

    fat32_bpb_t* bpb = _fat32_get_bpb();
    u16* buf = (u16*)kmalloc(bpb->sectors_per_cluster * 512);
    if (!buf) return;

    u32 current_cluster = _fat32_get_current_dir_cluster();
    ata_read_sectors(drive, _fat32_cluster_to_lba(current_cluster), bpb->sectors_per_cluster, buf);
    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buf;
    int max_entries = (bpb->sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);

    for (int i = 0; i < max_entries; i++) {
        int match = 1;
        for(int j=0; j<11; j++) if(entries[i].name[j] != fat_name[j]) match = 0;
        if(match && (entries[i].attr & FAT_ATTR_DIRECTORY)) {
            u32 new_cluster = ((u32)entries[i].cluster_hi << 16) | entries[i].cluster_lo;
            if (new_cluster == 0) new_cluster = bpb->root_cluster;
            _fat32_set_current_dir_cluster(new_cluster);
            //kprint("Changed directory to "); kprint(name); kprint("\n");
            kfree(buf);
            return;
        }
    }
    kprint("Error: No such file or directory \n");
    kfree(buf);
}

// ============================================================================
// PUBLIC API: Create a directory
// ============================================================================
void fat32_mkdir(const char* name) {
    ata_drive_t* drive = _fat32_get_current_drive();
    if (!drive) {
        kprint("Error: No drive mounted\n");
        return;
    }
    if (_fat32_find_entry(name, NULL)) {
        kprint("Error: File or directory already exists\n");
        return;
    }

    u32 clus = _fat32_find_free_cluster();
    if(!clus) {
        kprint("Error: No free clusters found\n");
        return;
    }
    _fat32_set_fat_entry(clus, 0x0FFFFFFF);
    _fat32_create_entry(name, FAT_ATTR_DIRECTORY, clus, 0);

    fat32_bpb_t* bpb = _fat32_get_bpb();
    u16* buf = (u16*)kmalloc(bpb->sectors_per_cluster * 512);
    if (!buf) return;

    for(int i=0; i<bpb->sectors_per_cluster*256; i++) buf[i] = 0;
    fat32_dir_entry_t* dot = (fat32_dir_entry_t*)buf;
    _fat32_name_to_83(".", dot[0].name); dot[0].attr = FAT_ATTR_DIRECTORY;
    dot[0].cluster_hi = (u16)(clus >> 16); dot[0].cluster_lo = (u16)(clus & 0xFFFF);
    _fat32_name_to_83("..", dot[1].name); dot[1].attr = FAT_ATTR_DIRECTORY;
    u32 current = _fat32_get_current_dir_cluster();
    u32 parent = (current == bpb->root_cluster) ? 0 : current;
    dot[1].cluster_hi = (u16)(parent >> 16); dot[1].cluster_lo = (u16)(parent & 0xFFFF);
    
    ata_write_sectors(drive, _fat32_cluster_to_lba(clus), bpb->sectors_per_cluster, buf);
    kfree(buf);
    //kprint("Created directory: "); kprint(name); kprint("\n");
}
