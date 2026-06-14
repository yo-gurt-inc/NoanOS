// fat32_file.c - FAT32 File Operations: cat, read, echo, touch, rm, copy, move, stat
#include "storage/fat32.h"
#include "io/kprint.h"
#include "io/terminal.h"
#include "core/malloc.h"
extern int _fat32_find_entry(const char* name, fat32_dir_entry_t* out_entry);
extern u32 _fat32_get_fat_entry(u32 cluster);
extern u32 _fat32_find_free_cluster(void);
extern void _fat32_set_fat_entry(u32 cluster, u32 value);
extern void _fat32_create_entry(const char* name, u8 attr, u32 first_cluster, u32 size);
extern ata_drive_t* _fat32_get_current_drive(void);
extern fat32_bpb_t* _fat32_get_bpb(void);

// ============================================================================
// PUBLIC API: Display file contents
// ============================================================================
void fat32_cat(const char* name) {
    ata_drive_t* drive = _fat32_get_current_drive();
    if (!drive) return;
    fat32_dir_entry_t entry;
    if (!_fat32_find_entry(name, &entry)) {
        kprint("Error: File not found\n");
        return;
    }
    if (entry.attr & FAT_ATTR_DIRECTORY) {
        kprint("Error: Cannot cat a directory\n");
        return;
    }

    fat32_bpb_t* bpb = _fat32_get_bpb();
    u32 cluster = ((u32)entry.cluster_hi << 16) | entry.cluster_lo;
    u32 size = entry.file_size;
    u16* buf = (u16*)kmalloc(bpb->sectors_per_cluster * 512);
    if (!buf) return;

    while (size > 0 && cluster < 0x0FFFFFF8) {
        ata_read_sectors(drive, _fat32_cluster_to_lba(cluster), bpb->sectors_per_cluster, buf);
        u8* b = (u8*)buf;
        u32 cluster_bytes = bpb->sectors_per_cluster * 512;
        for (u32 i = 0; i < cluster_bytes && size > 0; i++) {
            terminal_putchar(b[i]);
            size--;
        }
        cluster = _fat32_get_fat_entry(cluster);
    }
    kprint("\n");
    kfree(buf);
}

// ============================================================================
// PUBLIC API: Read file into buffer
// ============================================================================
int fat32_read(const char* name, char* buffer, u32 max_len) {
    ata_drive_t* drive = _fat32_get_current_drive();
    if (!drive) return -1;
    fat32_dir_entry_t entry;
    if (!_fat32_find_entry(name, &entry)) return -1;
    if (entry.attr & FAT_ATTR_DIRECTORY) return -1;

    fat32_bpb_t* bpb = _fat32_get_bpb();
    u32 cluster = ((u32)entry.cluster_hi << 16) | entry.cluster_lo;
    u32 size = entry.file_size;
    if (size > max_len) size = max_len;
    
    u32 total_read = 0;
    u16* buf = (u16*)kmalloc(512);
    if (!buf) return -1;

    // Read sector by sector, following the FAT chain
    while (total_read < size && cluster < 0x0FFFFFF8 && cluster >= 2) {
        // Each cluster may have multiple sectors - we need to read all of them

        u32 sectors_per_cluster = bpb ? bpb->sectors_per_cluster : 1;
        if (sectors_per_cluster == 0 || sectors_per_cluster > 128) sectors_per_cluster = 1;
        
        for (u32 sec = 0; sec < sectors_per_cluster && total_read < size; sec++) {
            ata_read_sectors(drive, _fat32_cluster_to_lba(cluster) + sec, 1, buf);
            u8* b = (u8*)buf;
            for (int i = 0; i < 512 && total_read < size; i++) {
                buffer[total_read++] = (char)b[i];
            }
        }
        
        cluster = _fat32_get_fat_entry(cluster);
    }
    
    kfree(buf);
    return total_read;
}

// ============================================================================
// PUBLIC API: Write content to file
// ============================================================================
void fat32_echo(const char* name, const char* content, int flags) {
    ata_drive_t* drive = _fat32_get_current_drive();
    if (!drive) {
        kprint("Error: No drive mounted\n");
        return;
    }
    if (_fat32_find_entry(name, NULL)) {
        //kprint("Error: File already exists\n");
        return;
    }
    u32 clus = _fat32_find_free_cluster();
    if(!clus) {
        kprint("Error: No free clusters\n");
        return;
    }
    _fat32_set_fat_entry(clus, 0x0FFFFFFF);
    
    u16* buf = (u16*)kmalloc(512);
    if (!buf) return;

    for(int i=0; i<256; i++) buf[i] = 0;
    u8* b = (u8*)buf;
    int len = 0;
    while(content[len]) { b[len] = content[len]; len++; }
    
    ata_write_sectors(drive, _fat32_cluster_to_lba(clus), 1, buf);
    _fat32_create_entry(name, FAT_ATTR_ARCHIVE, clus, len);
    kfree(buf);
    if (!(flags & 1)) { kprint("Wrote to "); kprint(name); kprint("\n"); }
}

// ============================================================================
// PUBLIC API: Create empty file
// ============================================================================
void fat32_touch(const char* name) {
    ata_drive_t* drive = _fat32_get_current_drive();
    if (!drive) {
        kprint("Error: No drive mounted\n");
        return;
    }
    if (_fat32_find_entry(name, NULL)) {
        kprint("Error: File already exists\n");
        return;
    }
    _fat32_create_entry(name, FAT_ATTR_ARCHIVE, 0, 0);
    kprint("Created file: "); kprint(name); kprint("\n");
}

// ============================================================================
// PUBLIC API: Delete file
// ============================================================================
void fat32_rm(const char* name, int flags) {
    ata_drive_t* drive = _fat32_get_current_drive();
    if (!drive) return;
    
    u8 fat_name[11];
    _fat32_name_to_83(name, fat_name);

    fat32_bpb_t* bpb = _fat32_get_bpb();
    u16* buf = (u16*)kmalloc(bpb->sectors_per_cluster * 512);
    if (!buf) return;
    
    u32 current_cluster = 0; // Would get this from fat32.c in real implementation
    u32 dir_lba = _fat32_cluster_to_lba(current_cluster);
    ata_read_sectors(drive, dir_lba, bpb->sectors_per_cluster, buf);
    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buf;
    int max_entries = (bpb->sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);

    for (int i = 0; i < max_entries; i++) {
        if (entries[i].name[0] == 0x00) break;
        int match = 1;
        for(int j=0; j<11; j++) if(entries[i].name[j] != fat_name[j]) match = 0;
        
        if (match) {
            u32 cluster = ((u32)entries[i].cluster_hi << 16) | entries[i].cluster_lo;
            
            while (cluster > 0 && cluster < 0x0FFFFFF8) {
                u32 next = _fat32_get_fat_entry(cluster);
                _fat32_set_fat_entry(cluster, 0);
                cluster = next;
            }

            entries[i].name[0] = 0xE5;
            ata_write_sectors(drive, dir_lba, bpb->sectors_per_cluster, buf);
            
            if (!(flags & 1)) { kprint("Deleted: "); kprint(name); kprint("\n"); }
            kfree(buf);
            return;
        }
    }
    if (!(flags & 1)) kprint("No such file or directory \n");
    kfree(buf);
}

// ============================================================================
// PUBLIC API: Copy file
// ============================================================================
void fat32_copy(const char* src, const char* dest) {
    ata_drive_t* drive = _fat32_get_current_drive();
    if (!drive) return;
    fat32_dir_entry_t entry;
    if (!_fat32_find_entry(src, &entry)) {
        kprint("Error: Source file not found\n");
        return;
    }
    if (entry.attr & FAT_ATTR_DIRECTORY) {
        kprint("Error: Cannot copy a directory\n");
        return;
    }

    fat32_bpb_t* bpb = _fat32_get_bpb();
    u32 src_cluster = ((u32)entry.cluster_hi << 16) | entry.cluster_lo;
    u32 size = entry.file_size;

    u32 dest_cluster = _fat32_find_free_cluster();
    if (!dest_cluster) {
        kprint("Error: No free clusters\n");
        return;
    }
    _fat32_set_fat_entry(dest_cluster, 0x0FFFFFFF);
    u32 first_dest_cluster = dest_cluster;

    u16* buf = (u16*)kmalloc(bpb->sectors_per_cluster * 512);
    if (!buf) return;

    u32 remaining = size;
    while (remaining > 0 && src_cluster < 0x0FFFFFF8) {
        ata_read_sectors(drive, _fat32_cluster_to_lba(src_cluster), bpb->sectors_per_cluster, buf);
        ata_write_sectors(drive, _fat32_cluster_to_lba(dest_cluster), bpb->sectors_per_cluster, buf);
        
        u32 cluster_bytes = bpb->sectors_per_cluster * 512;
        remaining -= (remaining > cluster_bytes) ? cluster_bytes : remaining;
        
        if (remaining > 0) {
            src_cluster = _fat32_get_fat_entry(src_cluster);
            u32 next_dest = _fat32_find_free_cluster();
            if (!next_dest) {
                kprint("Error: Disk full during copy\n");
                break; 
            }
            _fat32_set_fat_entry(dest_cluster, next_dest);
            _fat32_set_fat_entry(next_dest, 0x0FFFFFFF);
            dest_cluster = next_dest;
        }
    }

    _fat32_create_entry(dest, FAT_ATTR_ARCHIVE, first_dest_cluster, size);
    kfree(buf);
    kprint("Copied "); kprint(src); kprint(" to "); kprint(dest); kprint("\n");
}

// ============================================================================
// PUBLIC API: Move file
// ============================================================================
void fat32_move(const char* src, const char* dest) {
    fat32_copy(src, dest);
    fat32_rm(src, 0);
}

// ============================================================================
// PUBLIC API: Display file info
// ============================================================================
void fat32_stat(const char* name) {
    ata_drive_t* drive = _fat32_get_current_drive();
    if (!drive) return;
    fat32_dir_entry_t entry;
    if (!_fat32_find_entry(name, &entry)) {
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
