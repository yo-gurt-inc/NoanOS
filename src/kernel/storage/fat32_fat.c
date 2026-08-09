// fat32_fat.c - FAT table operations: cluster↔LBA, FAT entry read/write, free cluster search
#include "storage/fat32.h"
#include "core/malloc.h"

extern ata_drive_t* _fat32_get_current_drive(void);
extern fat32_bpb_t* _fat32_get_bpb(void);
extern u32          _fat32_get_fat_start(void);
extern u32          _fat32_get_data_start(void);

u32 _fat32_cluster_to_lba(u32 cluster) {
    fat32_bpb_t* bpb = _fat32_get_bpb();
    return _fat32_get_data_start() + (cluster - 2) * bpb->sectors_per_cluster;
}

// Simple FAT-sector cache to avoid repeated sector reads
#include "core/malloc.h"

static u8* fat_cache_buf = NULL;       // 512 bytes cached FAT sector
static u32 fat_cache_sector = 0xFFFFFFFF;

u32 _fat32_get_fat_entry(u32 cluster) {
    u32 fat_offset = cluster * 4;
    u32 fat_sector = _fat32_get_fat_start() + (fat_offset / 512);
    u32 ent_offset = fat_offset % 512;

    fat32_bpb_t* bpb = _fat32_get_bpb();
    if (!fat_cache_buf) {
        fat_cache_buf = (u8*)kmalloc(512);
        if (!fat_cache_buf) return 0x0FFFFFFF;
        fat_cache_sector = 0xFFFFFFFF;
    }

    if (fat_cache_sector != fat_sector) {
        ata_read_sectors(_fat32_get_current_drive(), fat_sector, 1, (u16*)fat_cache_buf);
        fat_cache_sector = fat_sector;
    }

    u32 entry = *(u32*)(fat_cache_buf + ent_offset);
    return entry & 0x0FFFFFFF;
}

void _fat32_set_fat_entry(u32 cluster, u32 value) {
    fat32_bpb_t* bpb = _fat32_get_bpb();
    u32 fat_offset = cluster * 4;
    u32 fat_sector = _fat32_get_fat_start() + (fat_offset / 512);
    u32 ent_offset = fat_offset % 512;

    if (!fat_cache_buf) {
        fat_cache_buf = (u8*)kmalloc(512);
        if (!fat_cache_buf) return;
        fat_cache_sector = 0xFFFFFFFF;
    }

    if (fat_cache_sector != fat_sector) {
        ata_read_sectors(_fat32_get_current_drive(), fat_sector, 1, (u16*)fat_cache_buf);
        fat_cache_sector = fat_sector;
    }

    *(u32*)(fat_cache_buf + ent_offset) = (value & 0x0FFFFFFF);
    // Write through to all FAT copies
    for (int i = 0; i < bpb->num_fats; i++)
        ata_write_sectors(_fat32_get_current_drive(), fat_sector + (i * bpb->sectors_per_fat_32), 1, (u16*)fat_cache_buf);
}

u32 _fat32_find_free_cluster(void) {
    fat32_bpb_t* bpb = _fat32_get_bpb();
    u32 total_clusters = bpb->total_sectors_32 / bpb->sectors_per_cluster;
    if (!fat_cache_buf) {
        fat_cache_buf = (u8*)kmalloc(512);
        if (!fat_cache_buf) return 0;
        fat_cache_sector = 0xFFFFFFFF;
    }

    u32 current_fat_sector = 0xFFFFFFFF;
    for (u32 i = 2; i < total_clusters; i++) {
        u32 fat_offset = i * 4;
        u32 fat_sector = _fat32_get_fat_start() + (fat_offset / 512);
        u32 ent_offset = fat_offset % 512;
        if (fat_sector != current_fat_sector) {
            ata_read_sectors(_fat32_get_current_drive(), fat_sector, 1, (u16*)fat_cache_buf);
            current_fat_sector = fat_sector;
        }
        if ((*(u32*)(fat_cache_buf + ent_offset) & 0x0FFFFFFF) == 0) {
            return i;
        }
    }
    return 0;
}
