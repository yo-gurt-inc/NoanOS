// fat32.c - FAT32 core: module state, init, format
// FAT table ops → fat32_fat.c
// Path/entry ops → fat32_path.c
// Directory ops  → fat32_dir.c
// File ops       → fat32_file.c
#include "storage/fat32.h"
#include "io/kprint.h"
#include "io/terminal.h"
#include "core/malloc.h"

// ============================================================================
// MODULE STATE
// ============================================================================
static fat32_bpb_t   bpb;
static ata_drive_t*  current_drive      = NULL;
static u32           fat_start_sector   = 0;
static u32           data_start_sector  = 0;
static u32           current_dir_cluster = 0;

// ============================================================================
// ACCESSORS (used by fat32_fat.c, fat32_path.c, fat32_dir.c, fat32_file.c)
// ============================================================================
u32          _fat32_get_fat_start(void)          { return fat_start_sector; }
u32          _fat32_get_data_start(void)         { return data_start_sector; }
u32          _fat32_get_current_dir_cluster(void){ return current_dir_cluster; }
void         _fat32_set_current_dir_cluster(u32 c){ current_dir_cluster = c; }
ata_drive_t* _fat32_get_current_drive(void)      { return current_drive; }
fat32_bpb_t* _fat32_get_bpb(void)               { return &bpb; }

// ============================================================================
// PUBLIC API: Initialize FAT32 from a drive
// ============================================================================
void fat32_init(ata_drive_t* drive) {
    if (!drive || !drive->exists) return;
    current_drive = drive;

    u16* buf = (u16*)kmalloc(512);
    ata_read_sectors(drive, 0, 1, buf);
    u8* b = (u8*)buf;
    for (int i = 0; i < 512; i++) ((u8*)&bpb)[i] = b[i];
    kfree(buf);

    fat_start_sector  = bpb.reserved_sector_count;
    data_start_sector = bpb.reserved_sector_count + (bpb.num_fats * bpb.sectors_per_fat_32);
    current_dir_cluster = bpb.root_cluster;

    kprint("FAT32 Initialized on "); kprint(drive->name); kprint("\n");
    kprint("  OEM Name: ");
    for (int i = 0; i < 8; i++) terminal_putchar(bpb.oem_name[i]);
    kprint("\n");
    kprint("  Total Sectors: "); kprint_dec(bpb.total_sectors_32); kprint("\n");
}

// ============================================================================
// PUBLIC API: Format a drive as FAT32
// ============================================================================
int fat32_format(ata_drive_t* drive) {
    if (!drive || !drive->exists) return 0;
    kprint("Formatting "); kprint(drive->name); kprint(" as FAT32...\n");

    u16* buf = (u16*)kmalloc(512);
    u8* b = (u8*)buf;
    for (int i = 0; i < 512; i++) b[i] = 0;

    fat32_bpb_t* f = (fat32_bpb_t*)buf;
    f->boot_jmp[0] = 0xEB; f->boot_jmp[1] = 0x3C; f->boot_jmp[2] = 0x90;
    for (int i = 0; i < 8; i++) f->oem_name[i]    = "SIMPLEOS"[i];
    f->bytes_per_sector   = 512;
    f->sectors_per_cluster = 8;
    f->reserved_sector_count = 704;
    f->num_fats           = 2;
    f->media_type         = 0xF8;
    f->total_sectors_32   = 20480;
    f->sectors_per_fat_32 = 160;
    f->root_cluster       = 2;
    f->boot_signature     = 0x29;
    f->volume_id          = 0x12345678;
    for (int i = 0; i < 11; i++) f->volume_label[i] = "SIMPLE OS  "[i];
    for (int i = 0; i < 8;  i++) f->fs_type[i]      = "FAT32   "[i];
    f->boot_sig_2         = 0xAA55;

    kprint(" Writing Boot Sector...\n");
    ata_write_sectors(drive, 0, 1, buf);

    kprint(" Clearing FAT Tables...\n");
    for (int i = 0; i < 512; i++) b[i] = 0;
    u32* fat = (u32*)buf;
    fat[0] = 0x0FFFFFF8; fat[1] = 0x0FFFFFFF; fat[2] = 0x0FFFFFFF;
    ata_write_sectors(drive, 704,       1, buf);
    ata_write_sectors(drive, 704 + 160, 1, buf);

    fat[0] = 0; fat[1] = 0; fat[2] = 0;
    for (int i = 1; i < 160; i++) {
        ata_write_sectors(drive, 704 + i,       1, buf);
        ata_write_sectors(drive, 704 + 160 + i, 1, buf);
    }

    kprint(" Creating Root Directory...\n");
    for (int i = 0; i < 512; i++) b[i] = 0;
    /* Zero all sectors in root cluster (SPC=8) */
    u32 root_lba = 704 + (2 * 160); /* data_start + (cluster2-2)*SPC */
    for (int s = 0; s < 8; s++) {
        ata_write_sectors(drive, root_lba + s, 1, buf);
    }
    /* Verify: read back sector 0 of root cluster and check it's zero */
    {
        u16* check = (u16*)kmalloc(512);
        if (check) {
            ata_read_sectors(drive, root_lba, 1, check);
            u8* cb = (u8*)check;
            int dirty = 0;
            for (int i = 0; i < 512; i++) if (cb[i]) { dirty = 1; break; }
            kprint(dirty ? "[WARN] root cluster not zeroed!\n" : "[OK] root cluster zeroed\n");
            kfree(check);
        }
    }

    kfree(buf);
    kprint("Format Complete!\n");
    return 1;
}
