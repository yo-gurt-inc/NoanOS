#ifndef FAT32_H
#define FAT32_H

#include "core/types.h"
#include "storage/ata.h"

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
void fat32_echo(const char* name, const char* content, int flags);
void fat32_rm(const char* name, int flags);
void fat32_stat(const char* name);
void fat32_copy(const char* src, const char* dest);
void fat32_move(const char* src, const char* dest);
int fat32_read(const char* name, char* buffer, u32 max_len);

/* Internal accessors shared across fat32_*.c modules */
u32          _fat32_get_fat_start(void);
u32          _fat32_get_data_start(void);
u32          _fat32_get_current_dir_cluster(void);
void         _fat32_set_current_dir_cluster(u32 c);
ata_drive_t* _fat32_get_current_drive(void);
fat32_bpb_t* _fat32_get_bpb(void);
u32          _fat32_cluster_to_lba(u32 cluster);
u32          _fat32_get_fat_entry(u32 cluster);
void         _fat32_set_fat_entry(u32 cluster, u32 value);
u32          _fat32_find_free_cluster(void);
void         _fat32_name_to_83(const char* name, u8* dest);
int          _fat32_find_entry(const char* name, fat32_dir_entry_t* out_entry);
void         _fat32_create_entry(const char* name, u8 attr, u32 first_cluster, u32 size);
int          _fat32_find_full_path_file(const char* path, fat32_dir_entry_t* out_entry);

#endif
