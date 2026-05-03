#ifndef ATA_H
#define ATA_H

#include "types.h"

#define ATA_PRIMARY_IO_BASE 0x1F0
#define ATA_SECONDARY_IO_BASE 0x170

#define ATA_REG_DATA 0
#define ATA_REG_ERROR 1
#define ATA_REG_FEATURES 1
#define ATA_REG_SECCOUNT 2
#define ATA_REG_LBA_LO 3
#define ATA_REG_LBA_MID 4
#define ATA_REG_LBA_HI 5
#define ATA_REG_DRV_HEAD 6
#define ATA_REG_STATUS 7
#define ATA_REG_COMMAND 7

#define ATA_STATUS_BSY  0x80
#define ATA_STATUS_DRDY 0x40
#define ATA_STATUS_DF   0x20
#define ATA_STATUS_DSC  0x10
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_CORR 0x04
#define ATA_STATUS_IDX  0x02
#define ATA_STATUS_ERR  0x01

#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY          0xEC

typedef struct {
    u16 base;
    u16 master;
    char name[32];
    int exists;
} ata_drive_t;

void ata_init(void);
int ata_identify(ata_drive_t* drive);
void ata_read_sectors(ata_drive_t* drive, u32 lba, u8 count, u16* buf);
void ata_write_sectors(ata_drive_t* drive, u32 lba, u8 count, u16* buf);
void ata_list_drives(void);
ata_drive_t* ata_get_drive(int idx);

#endif
