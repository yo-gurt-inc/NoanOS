#include "include/fat32.h"
#include "include/kprint.h"
#include "include/terminal.h"
#include "include/malloc.h"

static fat32_bpb_t bpb;
static ata_drive_t* current_drive = NULL;
static u32 data_start_sector;
static u32 fat_start_sector;
static u32 current_dir_cluster;

static u32 get_fat_entry(u32 cluster);
static void set_fat_entry(u32 cluster, u32 value);
static u32 find_free_cluster(void);
static void name_to_83(const char* name, u8* dest);
static void create_entry(const char* name, u8 attr, u32 first_cluster, u32 size);
static int find_entry(const char* name, fat32_dir_entry_t* out_entry);

void fat32_init(ata_drive_t* drive) {
    if (!drive || !drive->exists) return;
    
    current_drive = drive;
    u16* buf = (u16*)kmalloc(512);
    ata_read_sectors(drive, 0, 1, buf);
    
    u8* byte_buf = (u8*)buf;
    for (int i = 0; i < 512; i++) {
        ((u8*)&bpb)[i] = byte_buf[i];
    }
    
    fat_start_sector = bpb.reserved_sector_count;
    data_start_sector = bpb.reserved_sector_count + (bpb.num_fats * bpb.sectors_per_fat_32);
    current_dir_cluster = bpb.root_cluster;
    
    kprint("FAT32 Initialized on ");
    kprint(drive->name);
    kprint("\n");
    kprint("  OEM Name: ");
    for(int i=0; i<8; i++) terminal_putchar(bpb.oem_name[i]);
    kprint("\n");
    kprint("  Total Sectors: "); kprint_dec(bpb.total_sectors_32); kprint("\n");
    
    kfree(buf);
}

int fat32_format(ata_drive_t* drive) {
    if (!drive || !drive->exists) return 0;
    
    kprint("Formatting "); kprint(drive->name); kprint(" as FAT32...\n");
    
    u16* buf = (u16*)kmalloc(512);
    u8* b = (u8*)buf;
    for(int i=0; i<512; i++) b[i] = 0;
    
    // Minimal BPB for 10MB drive
    fat32_bpb_t* f = (fat32_bpb_t*)buf;
    f->boot_jmp[0] = 0xEB; f->boot_jmp[1] = 0x3C; f->boot_jmp[2] = 0x90;
    for(int i=0; i<8; i++) f->oem_name[i] = "SIMPLEOS"[i];
    f->bytes_per_sector = 512;
    f->sectors_per_cluster = 1; // 512 byte clusters for small drive
    f->reserved_sector_count = 128;
    f->num_fats = 2;
    f->media_type = 0xF8;
    f->total_sectors_32 = 20480; // 10MB
    f->sectors_per_fat_32 = 160; 
    f->root_cluster = 2;
    f->boot_signature = 0x29;
    f->volume_id = 0x12345678;
    for(int i=0; i<11; i++) f->volume_label[i] = "SIMPLE OS  "[i];
    for(int i=0; i<8; i++) f->fs_type[i] = "FAT32   "[i];
    f->boot_sig_2 = 0xAA55;
    
    kprint(" Writing Boot Sector...\n");
    ata_write_sectors(drive, 0, 1, buf);
    
    kprint(" Clearing FAT Tables...\n");
    for(int i=0; i<512; i++) b[i] = 0;
    // FAT[0] = Media Type, FAT[1] = EOC, FAT[2] = Root Dir EOC
    u32* fat = (u32*)buf;
    fat[0] = 0x0FFFFFF8;
    fat[1] = 0x0FFFFFFF;
    fat[2] = 0x0FFFFFFF;
    
    ata_write_sectors(drive, 128, 1, buf); // FAT1
    ata_write_sectors(drive, 128 + 160, 1, buf); // FAT2
    
    // Clear rest of FAT
    fat[0] = 0; fat[1] = 0; fat[2] = 0;
    for(int i=1; i<160; i++) {
        ata_write_sectors(drive, 128 + i, 1, buf);
        ata_write_sectors(drive, 128 + 160 + i, 1, buf);
    }
    
    kprint(" Creating Root Directory...\n");
    for(int i=0; i<512; i++) b[i] = 0;
    ata_write_sectors(drive, 128 + (2 * 160), 1, buf); // Cluster 2
    
    kfree(buf);
    kprint("Format Complete!\n");
    return 1;
}

static u32 cluster_to_lba(u32 cluster) {
    return data_start_sector + (cluster - 2) * bpb.sectors_per_cluster;
}

void fat32_ls(void) {
    if (!current_drive) {
        kprint("Error: No drive mounted\n");
        return;
    }
    
    u32 cluster = current_dir_cluster;
    u16* buf = (u16*)kmalloc(bpb.sectors_per_cluster * 512);
    if (!buf) return;

    ata_read_sectors(current_drive, cluster_to_lba(cluster), bpb.sectors_per_cluster, buf);
    
    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buf;
    int max_entries = (bpb.sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);
    
    int count = 0;
    for (int i = 0; i < max_entries; i++) {
        if (entries[i].name[0] == 0x00) break;
        if (entries[i].name[0] == 0xE5) continue;
        if (entries[i].attr == FAT_ATTR_LFN) continue;
        
        count++;
        for (int j = 0; j < 8; j++) {
            if (entries[i].name[j] != ' ') terminal_putchar(entries[i].name[j]);
        }
        if (entries[i].name[8] != ' ') {
            terminal_putchar('.');
            for (int j = 8; j < 11; j++) {
                if (entries[i].name[j] != ' ') terminal_putchar(entries[i].name[j]);
            }
        }
        
        if (entries[i].attr & FAT_ATTR_DIRECTORY) {
            kprint(" [DIR]");
        } else {
            kprint(" ("); kprint_dec(entries[i].file_size); kprint(" bytes)");
        }
        kprint("\n");
    }
    if (count == 0) {
        kprint("(Directory empty)\n");
    }
    kfree(buf);
}

static u32 get_fat_entry(u32 cluster) {
    u32 fat_offset = cluster * 4;
    u32 fat_sector = fat_start_sector + (fat_offset / 512);
    u32 ent_offset = fat_offset % 512;

    u16* buf = (u16*)kmalloc(512);
    if (!buf) return 0x0FFFFFFF;

    ata_read_sectors(current_drive, fat_sector, 1, buf);
    u32 entry = *(u32*)((u8*)buf + ent_offset);
    kfree(buf);
    return entry & 0x0FFFFFFF;
}

static void set_fat_entry(u32 cluster, u32 value) {
    u32 fat_offset = cluster * 4;
    u32 fat_sector = fat_start_sector + (fat_offset / 512);
    u32 ent_offset = fat_offset % 512;

    u16* buf = (u16*)kmalloc(512);
    if (!buf) return;

    ata_read_sectors(current_drive, fat_sector, 1, buf);
    *(u32*)((u8*)buf + ent_offset) = (value & 0x0FFFFFFF);
    
    // Write to all FAT copies
    for(int i=0; i<bpb.num_fats; i++) {
        ata_write_sectors(current_drive, fat_sector + (i * bpb.sectors_per_fat_32), 1, buf);
    }
    kfree(buf);
}

static u32 find_free_cluster(void) {
    u32 total_clusters = bpb.total_sectors_32 / bpb.sectors_per_cluster;
    u16* buf = (u16*)kmalloc(512);
    if (!buf) return 0;

    u32 current_fat_sector = 0xFFFFFFFF;

    for (u32 i = 2; i < total_clusters; i++) {
        u32 fat_offset = i * 4;
        u32 fat_sector = fat_start_sector + (fat_offset / 512);
        u32 ent_offset = fat_offset % 512;

        if (fat_sector != current_fat_sector) {
            ata_read_sectors(current_drive, fat_sector, 1, buf);
            current_fat_sector = fat_sector;
        }

        u32 entry = *(u32*)((u8*)buf + ent_offset);
        if ((entry & 0x0FFFFFFF) == 0) {
            kfree(buf);
            return i;
        }
    }
    kfree(buf);
    return 0;
}

static void name_to_83(const char* name, u8* dest) {
    for(int i=0; i<11; i++) dest[i] = ' ';
    int i = 0, j = 0;
    while(name[i] && name[i] != '.' && j < 8) {
        char c = name[i++];
        if(c >= 'a' && c <= 'z') c -= 32;
        dest[j++] = c;
    }
    if(name[i] == '.') {
        i++;
        j = 8;
        while(name[i] && j < 11) {
            char c = name[i++];
            if(c >= 'a' && c <= 'z') c -= 32;
            dest[j++] = c;
        }
    }
}

void fat32_cd(const char* name) {
    if (!current_drive) return;
    if (name[0] == '/' && name[1] == '\0') {
        current_dir_cluster = bpb.root_cluster;
        kprint("Returned to root\n");
        return;
    }

    u8 fat_name[11];
    name_to_83(name, fat_name);

    u16* buf = (u16*)kmalloc(bpb.sectors_per_cluster * 512);
    if (!buf) return;

    ata_read_sectors(current_drive, cluster_to_lba(current_dir_cluster), bpb.sectors_per_cluster, buf);
    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buf;
    int max_entries = (bpb.sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);

    for (int i = 0; i < max_entries; i++) {
        int match = 1;
        for(int j=0; j<11; j++) if(entries[i].name[j] != fat_name[j]) match = 0;
        if(match && (entries[i].attr & FAT_ATTR_DIRECTORY)) {
            current_dir_cluster = ((u32)entries[i].cluster_hi << 16) | entries[i].cluster_lo;
            if (current_dir_cluster == 0) current_dir_cluster = bpb.root_cluster;
            kprint("Changed directory to "); kprint(name); kprint("\n");
            kfree(buf);
            return;
        }
    }
    kprint("Error: Directory not found\n");
    kfree(buf);
}

static void create_entry(const char* name, u8 attr, u32 first_cluster, u32 size) {
    u16* buf = (u16*)kmalloc(bpb.sectors_per_cluster * 512);
    if (!buf) return;

    ata_read_sectors(current_drive, cluster_to_lba(current_dir_cluster), bpb.sectors_per_cluster, buf);
    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buf;
    int max_entries = (bpb.sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);

    for (int i = 0; i < max_entries; i++) {
        if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
            name_to_83(name, entries[i].name);
            entries[i].attr = attr;
            entries[i].cluster_hi = (u16)(first_cluster >> 16);
            entries[i].cluster_lo = (u16)(first_cluster & 0xFFFF);
            entries[i].file_size = size;
            ata_write_sectors(current_drive, cluster_to_lba(current_dir_cluster), bpb.sectors_per_cluster, buf);
            kfree(buf);
            return;
        }
    }
    kfree(buf);
}

static int find_entry(const char* name, fat32_dir_entry_t* out_entry) {
    u8 fat_name[11];
    name_to_83(name, fat_name);

    u16* buf = (u16*)kmalloc(bpb.sectors_per_cluster * 512);
    if (!buf) return 0;
    
    ata_read_sectors(current_drive, cluster_to_lba(current_dir_cluster), bpb.sectors_per_cluster, buf);
    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buf;
    int max_entries = (bpb.sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);

    for (int i = 0; i < max_entries; i++) {
        if (entries[i].name[0] == 0x00) break;
        if (entries[i].name[0] == 0xE5) continue;
        
        int match = 1;
        for(int j=0; j<11; j++) if(entries[i].name[j] != fat_name[j]) match = 0;
        if (match) {
            if (out_entry) *out_entry = entries[i];
            kfree(buf);
            return 1;
        }
    }
    kfree(buf);
    return 0;
}

void fat32_cat(const char* name) {
    if (!current_drive) return;
    fat32_dir_entry_t entry;
    if (!find_entry(name, &entry)) {
        kprint("Error: File not found\n");
        return;
    }
    if (entry.attr & FAT_ATTR_DIRECTORY) {
        kprint("Error: Cannot cat a directory\n");
        return;
    }

    u32 cluster = ((u32)entry.cluster_hi << 16) | entry.cluster_lo;
    u32 size = entry.file_size;
    u16* buf = (u16*)kmalloc(512);
    if (!buf) return;

    while (size > 0 && cluster < 0x0FFFFFF8) {
        ata_read_sectors(current_drive, cluster_to_lba(cluster), 1, buf);
        u8* b = (u8*)buf;
        for (int i = 0; i < 512 && size > 0; i++) {
            terminal_putchar(b[i]);
            size--;
        }
        cluster = get_fat_entry(cluster);
    }
    kprint("\n");
    kfree(buf);
}

void fat32_stat(const char* name) {
    if (!current_drive) return;
    fat32_dir_entry_t entry;
    if (!find_entry(name, &entry)) {
        kprint("Error: Not found\n");
        return;
    }

    kprint("File: "); kprint(name); kprint("\n");
    kprint("  Size:    "); kprint_dec(entry.file_size); kprint(" bytes\n");
    kprint("  Cluster: "); kprint_hex(((u32)entry.cluster_hi << 16) | entry.cluster_lo); kprint("\n");
    kprint("  Attr:    "); kprint_hex(entry.attr); 
    if (entry.attr & FAT_ATTR_DIRECTORY) kprint(" (DIR)");
    kprint("\n");
}

void fat32_rm(const char* name) {
    if (!current_drive) return;
    
    // We need to find the entry AND its index in the current directory sector
    u8 fat_name[11];
    name_to_83(name, fat_name);

    u16* buf = (u16*)kmalloc(bpb.sectors_per_cluster * 512);
    if (!buf) return;
    
    u32 dir_lba = cluster_to_lba(current_dir_cluster);
    ata_read_sectors(current_drive, dir_lba, bpb.sectors_per_cluster, buf);
    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buf;
    int max_entries = (bpb.sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);

    for (int i = 0; i < max_entries; i++) {
        if (entries[i].name[0] == 0x00) break;
        int match = 1;
        for(int j=0; j<11; j++) if(entries[i].name[j] != fat_name[j]) match = 0;
        
        if (match) {
            u32 cluster = ((u32)entries[i].cluster_hi << 16) | entries[i].cluster_lo;
            
            // Clear FAT chain
            while (cluster > 0 && cluster < 0x0FFFFFF8) {
                u32 next = get_fat_entry(cluster);
                set_fat_entry(cluster, 0);
                cluster = next;
            }

            // Mark entry as deleted (0xE5)
            entries[i].name[0] = 0xE5;
            ata_write_sectors(current_drive, dir_lba, bpb.sectors_per_cluster, buf);
            
            kprint("Deleted: "); kprint(name); kprint("\n");
            kfree(buf);
            return;
        }
    }
    kprint("Error: Not found\n");
    kfree(buf);
}

void fat32_mkdir(const char* name) {
    if (!current_drive) {
        kprint("Error: No drive mounted\n");
        return;
    }
    if (find_entry(name, NULL)) {
        kprint("Error: File or directory already exists\n");
        return;
    }

    u32 clus = find_free_cluster();
    if(!clus) {
        kprint("Error: No free clusters found\n");
        return;
    }
    set_fat_entry(clus, 0x0FFFFFFF);
    create_entry(name, FAT_ATTR_DIRECTORY, clus, 0);

    // Init directory clusters with . and ..
    u16* buf = (u16*)kmalloc(bpb.sectors_per_cluster * 512);
    if (!buf) return;

    for(int i=0; i<bpb.sectors_per_cluster*256; i++) buf[i] = 0;
    fat32_dir_entry_t* dot = (fat32_dir_entry_t*)buf;
    name_to_83(".", dot[0].name); dot[0].attr = FAT_ATTR_DIRECTORY;
    dot[0].cluster_hi = (u16)(clus >> 16); dot[0].cluster_lo = (u16)(clus & 0xFFFF);
    name_to_83("..", dot[1].name); dot[1].attr = FAT_ATTR_DIRECTORY;
    u32 parent = (current_dir_cluster == bpb.root_cluster) ? 0 : current_dir_cluster;
    dot[1].cluster_hi = (u16)(parent >> 16); dot[1].cluster_lo = (u16)(parent & 0xFFFF);
    
    ata_write_sectors(current_drive, cluster_to_lba(clus), bpb.sectors_per_cluster, buf);
    kfree(buf);
    kprint("Created directory: "); kprint(name); kprint("\n");
}

void fat32_touch(const char* name) {
    if (!current_drive) {
        kprint("Error: No drive mounted\n");
        return;
    }
    if (find_entry(name, NULL)) {
        kprint("Error: File already exists\n");
        return;
    }
    create_entry(name, FAT_ATTR_ARCHIVE, 0, 0);
    kprint("Created file: "); kprint(name); kprint("\n");
}

void fat32_echo(const char* name, const char* content) {
    if (!current_drive) {
        kprint("Error: No drive mounted\n");
        return;
    }
    if (find_entry(name, NULL)) {
        kprint("Error: File already exists\n");
        return;
    }
    u32 clus = find_free_cluster();
    if(!clus) {
        kprint("Error: No free clusters\n");
        return;
    }
    set_fat_entry(clus, 0x0FFFFFFF);
    
    u16* buf = (u16*)kmalloc(512);
    if (!buf) return;

    for(int i=0; i<256; i++) buf[i] = 0;
    u8* b = (u8*)buf;
    int len = 0;
    while(content[len]) { b[len] = content[len]; len++; }
    
    ata_write_sectors(current_drive, cluster_to_lba(clus), 1, buf);
    create_entry(name, FAT_ATTR_ARCHIVE, clus, len);
    kfree(buf);
    kprint("Wrote to "); kprint(name); kprint("\n");
}
