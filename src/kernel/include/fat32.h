#ifndef FAT32_H
#define FAT32_H

#include "types.h"
#include "ata.h"

typedef struct {
    u8  boot_jmp[3];
    u8  oem_name[8];
    u16 bytes_per_sector;
    u8  sectors_per_cluster;
    u16 reserved_sector_count;
    u8  num_fats;
    u16 root_entry_count;
    u16 total_sectors_16;
    u8  media_type;
    u16 sectors_per_fat_16;
    u16 sectors_per_track;
    u16 num_heads;
    u32 hidden_sectors;
    u32 total_sectors_32;

    // FAT32 Extended BPB
    u32 sectors_per_fat_32;
    u16 ext_flags;
    u16 fs_version;
    u32 root_cluster;
    u16 fs_info;
    u16 backup_boot_sector;
    u8  reserved[12];
    u8  drive_number;
    u8  reserved1;
    u8  boot_signature;
    u32 volume_id;
    u8  volume_label[11];
    u8  fs_type[8];
    u8  boot_code[420];
    u16 boot_sig_2;
} __attribute__((packed)) fat32_bpb_t;

typedef struct {
    u8  name[11];
    u8  attr;
    u8  nt_res;
    u8  creation_time_tenth;
    u16 creation_time;
    u16 creation_date;
    u16 last_acc_date;
    u16 cluster_hi;
    u16 write_time;
    u16 write_date;
    u16 cluster_lo;
    u32 file_size;
} __attribute__((packed)) fat32_dir_entry_t;

#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN    0x02
#define FAT_ATTR_SYSTEM    0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE   0x20
#define FAT_ATTR_LFN       0x0F

void fat32_init(ata_drive_t* drive);
void fat32_ls(void);
void fat32_cat(const char* filename);
int fat32_format(ata_drive_t* drive);
void fat32_cd(const char* path);
void fat32_mkdir(const char* name);
void fat32_touch(const char* name);
void fat32_echo(const char* name, const char* content);
void fat32_rm(const char* name);
void fat32_stat(const char* name);

#endif
