// fat32_file.c - FAT32 File Operations: cat, read, echo, touch, rm, copy, move, stat
#include "storage/fat32.h"
#include "cpu/task.h"
#include "io/kprint.h"
#include "io/terminal.h"
#include "core/malloc.h"
#include <stdbool.h>
extern int _fat32_find_entry(const char* name, fat32_dir_entry_t* out_entry);
extern u32 _fat32_get_fat_entry(u32 cluster);
extern u32 _fat32_find_free_cluster(void);
extern void _fat32_set_fat_entry(u32 cluster, u32 value);
extern void _fat32_create_entry(const char* name, u8 attr, u32 first_cluster, u32 size);
extern ata_drive_t* _fat32_get_current_drive(void);
extern fat32_bpb_t* _fat32_get_bpb(void);
extern u32 _fat32_get_current_dir_cluster(void);

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

    // Prefer shared persistent cluster buffer allocated by fat32_init
    u8* cluster_buf = _fat32_get_cluster_buf();
    u32 cluster_bytes = _fat32_get_cluster_buf_size();

    u32 sectors_per_cluster = bpb ? bpb->sectors_per_cluster : 1;
    if (sectors_per_cluster == 0 || sectors_per_cluster > 128) sectors_per_cluster = 1;
    if (!cluster_buf) {
        /* Fallback: allocate local cluster buffer if persistent one isn't present */
        cluster_bytes = sectors_per_cluster * 512;
        cluster_buf = (u8*)kmalloc(cluster_bytes);
        if (!cluster_buf) return -1;
    }

    // Read cluster-by-cluster using multi-sector ATA reads with simple read-ahead
    u8* readahead_buf = _fat32_get_readahead_buf();
    u32 readahead_size = _fat32_get_readahead_buf_size();
    u32 prefetched_cluster = 0xFFFFFFFF;

    while (total_read < size && cluster < 0x0FFFFFF8 && cluster >= 2) {
        u8* curr_buf = cluster_buf;
        bool used_prefetch = false;

        // If the cluster was prefetched, use readahead buffer
        if (readahead_buf && cluster == prefetched_cluster) {
            curr_buf = readahead_buf; // prefetched data starts at offset 0
            used_prefetch = true;
        } else {
            // Read current cluster normally
            ata_read_sectors(drive, _fat32_cluster_to_lba(cluster), (u8)sectors_per_cluster, (u16*)curr_buf);
        }

        u32 copy_bytes = (size - total_read) < cluster_bytes ? (size - total_read) : cluster_bytes;
        for (u32 i = 0; i < copy_bytes && total_read < size; i++) {
            buffer[total_read++] = (char)curr_buf[i];
        }

        // Determine next cluster in chain
        u32 next_cluster = _fat32_get_fat_entry(cluster);

        // If next cluster looks valid, try to prefetch it into readahead_buf
        if (readahead_buf && next_cluster >= 2 && next_cluster < 0x0FFFFFF8) {
            // Only prefetch if we didn't already have it prefetched
            if (!used_prefetch || prefetched_cluster != next_cluster) {
                // Read next cluster into readahead_buf
                ata_read_sectors(drive, _fat32_cluster_to_lba(next_cluster), (u8)sectors_per_cluster, (u16*)readahead_buf);
                prefetched_cluster = next_cluster;
            }
        } else {
            prefetched_cluster = 0xFFFFFFFF;
        }

        cluster = next_cluster;
    }

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
    u8* buf = (u8*)kmalloc(bpb->sectors_per_cluster * 512);
    if (!buf) return;
    
    u32 current_cluster = _fat32_get_current_dir_cluster();
    u32 dir_lba = _fat32_cluster_to_lba(current_cluster);
    ata_read_sectors(drive, dir_lba, bpb->sectors_per_cluster, (u16*)buf);
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
            ata_write_sectors(drive, dir_lba, bpb->sectors_per_cluster, (u16*)buf);
            
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

// ============================================================================
// INTERNAL: Update file size in the directory entry for a given path
// ============================================================================
static void fat32_update_filesize(const char* path, u32 new_size) {
    ata_drive_t* drive = _fat32_get_current_drive();
    fat32_bpb_t* bpb   = _fat32_get_bpb();
    if (!drive || !bpb) return;

    /* Determine parent directory cluster and the name component */
    u32 dir_cluster = _fat32_get_current_dir_cluster();
    const char* name = path;

    if (path[0] == '/') {
        dir_cluster = bpb->root_cluster;
        name = path + 1;
    }

    /* Navigate to the parent directory of the last path component */
    while (*name) {
        /* find next slash */
        const char* slash = name;
        while (*slash && *slash != '/') slash++;
        if (*slash == '\0') break; /* name now points to the final component */

        /* Navigate into the intermediate directory component */
        char component[64];
        int clen = (int)(slash - name);
        if (clen <= 0) { name = slash + 1; continue; }
        for (int i = 0; i < clen && i < 63; i++) component[i] = name[i];
        component[clen] = '\0';

        u8 fat_name[11];
        _fat32_name_to_83(component, fat_name);
        u8* buf = (u8*)kmalloc(bpb->sectors_per_cluster * 512);
        if (!buf) return;
        ata_read_sectors(drive, _fat32_cluster_to_lba(dir_cluster), bpb->sectors_per_cluster, (u16*)buf);
        fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buf;
        int max_e = (int)((bpb->sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t));
        int found = 0;
        for (int i = 0; i < max_e; i++) {
            if (entries[i].name[0] == 0x00) break;
            if (entries[i].name[0] == 0xE5) continue;
            int match = 1;
            for (int j = 0; j < 11; j++) if (entries[i].name[j] != fat_name[j]) { match = 0; break; }
            if (match && (entries[i].attr & FAT_ATTR_DIRECTORY)) {
                dir_cluster = ((u32)entries[i].cluster_hi << 16) | entries[i].cluster_lo;
                if (dir_cluster == 0) dir_cluster = bpb->root_cluster;
                found = 1; break;
            }
        }
        kfree(buf);
        if (!found) return;
        name = slash + 1;
    }

    /* name now points to the final filename component; update its dir entry */
    if (!*name) return;

    u8 fat_name[11];
    _fat32_name_to_83(name, fat_name);

    u8* buf = (u8*)kmalloc(bpb->sectors_per_cluster * 512);
    if (!buf) return;
    u32 cluster = dir_cluster;
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        u32 lba = _fat32_cluster_to_lba(cluster);
        ata_read_sectors(drive, lba, bpb->sectors_per_cluster, (u16*)buf);
        fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buf;
        int max_e = (int)((bpb->sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t));
        int updated = 0;
        for (int i = 0; i < max_e; i++) {
            if (entries[i].name[0] == 0x00) break;
            if (entries[i].name[0] == 0xE5) continue;
            int match = 1;
            for (int j = 0; j < 11; j++) if (entries[i].name[j] != fat_name[j]) { match = 0; break; }
            if (match) {
                entries[i].file_size = new_size;
                ata_write_sectors(drive, lba, bpb->sectors_per_cluster, (u16*)buf);
                updated = 1; break;
            }
        }
        if (updated) break;
        cluster = _fat32_get_fat_entry(cluster);
    }
    kfree(buf);
}

// ============================================================================
// PUBLIC API: Write from fd_entry_t into a FAT32 file
// Handles cluster chain extension as needed. Returns bytes written or -1.
// ============================================================================
int fat32_write_fd(void* fd_entry_ptr, const char* data, u32 len) {
    fd_entry_t* f = (fd_entry_t*)fd_entry_ptr;
    if (!f || !data || len == 0) return 0;

    ata_drive_t* drive = _fat32_get_current_drive();
    fat32_bpb_t* bpb   = _fat32_get_bpb();
    if (!drive || !bpb) return -1;

    u32 spc      = bpb->sectors_per_cluster;
    u32 clus_sz  = spc * 512;

    /* If the file has no cluster yet (zero-size file from touch), allocate first cluster */
    if (f->fat_cluster == 0) {
        u32 first = _fat32_find_free_cluster();
        if (!first) return -1;
        _fat32_set_fat_entry(first, 0x0FFFFFFF);
        f->fat_cluster = first;
        /* Update directory entry cluster fields — we can't do that here without
         * the path, so the caller (open handler) must store the cluster back.
         * We use a trick: fat32_update_filesize also patches cluster if needed.
         * For now just store in fd and let the first write use it. */
    }

    u32 written = 0;
    u32 pos     = f->offset; /* byte position within file */

    /* Walk the cluster chain to the cluster containing pos */
    u32 cluster = f->fat_cluster;
    u32 cluster_idx = pos / clus_sz;
    u32 prev_cluster = 0;
    for (u32 i = 0; i < cluster_idx; i++) {
        u32 next = _fat32_get_fat_entry(cluster);
        if (next >= 0x0FFFFFF8) {
            /* Need to allocate a new cluster to reach this position */
            u32 new_clus = _fat32_find_free_cluster();
            if (!new_clus) return (written > 0) ? (int)written : -1;
            _fat32_set_fat_entry(cluster, new_clus);
            _fat32_set_fat_entry(new_clus, 0x0FFFFFFF);
            cluster = new_clus;
        } else {
            prev_cluster = cluster;
            cluster = next;
        }
        (void)prev_cluster;
    }

    u8* sector_buf = (u8*)kmalloc(512);
    if (!sector_buf) return -1;

    while (written < len) {
        u32 pos_in_cluster = pos % clus_sz;
        u32 sec_in_cluster = pos_in_cluster / 512;
        u32 pos_in_sector  = pos % 512;
        u32 lba = _fat32_cluster_to_lba(cluster) + sec_in_cluster;

        /* Read-modify-write the sector */
        ata_read_sectors(drive, lba, 1, (u16*)sector_buf);

        u32 space_in_sector = 512 - pos_in_sector;
        u32 to_write = len - written;
        if (to_write > space_in_sector) to_write = space_in_sector;

        for (u32 i = 0; i < to_write; i++)
            sector_buf[pos_in_sector + i] = (u8)data[written + i];

        ata_write_sectors(drive, lba, 1, (u16*)sector_buf);
        written += to_write;
        pos     += to_write;

        /* If we've moved past the current cluster, get/allocate next */
        if (pos % clus_sz == 0 && written < len) {
            u32 next = _fat32_get_fat_entry(cluster);
            if (next >= 0x0FFFFFF8) {
                u32 new_clus = _fat32_find_free_cluster();
                if (!new_clus) break; /* disk full, return partial */
                _fat32_set_fat_entry(cluster, new_clus);
                _fat32_set_fat_entry(new_clus, 0x0FFFFFFF);
                cluster = new_clus;
            } else {
                cluster = next;
            }
        }
    }

    kfree(sector_buf);

    f->offset = pos;
    /* Update the file size in the directory entry if we extended the file */
    if (pos > f->size) {
        f->size = pos;
        if (f->path[0]) fat32_update_filesize(f->path, f->size);
    }
    return (int)written;
}
