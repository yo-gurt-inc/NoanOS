#include "include/ata.h"
#include "include/io.h"
#include "include/kprint.h"

static ata_drive_t drives[4];

void kprint_init_str(char* dest, const char* src);

static void ata_wait_bsy(u16 base) {
    while (inb(base + ATA_REG_STATUS) & ATA_STATUS_BSY);
}

static void ata_wait_drq(u16 base) {
    while (!(inb(base + ATA_REG_STATUS) & ATA_STATUS_DRQ));
}

int ata_identify(ata_drive_t* drive) {
    outb(drive->base + ATA_REG_DRV_HEAD, drive->master ? 0xA0 : 0xB0);
    outb(drive->base + ATA_REG_SECCOUNT, 0);
    outb(drive->base + ATA_REG_LBA_LO, 0);
    outb(drive->base + ATA_REG_LBA_MID, 0);
    outb(drive->base + ATA_REG_LBA_HI, 0);
    outb(drive->base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    u8 status = inb(drive->base + ATA_REG_STATUS);
    if (status == 0) return 0; // Drive doesn't exist

    ata_wait_bsy(drive->base);

    // Check if it's an ATAPI drive
    u8 mid = inb(drive->base + ATA_REG_LBA_MID);
    u8 hi = inb(drive->base + ATA_REG_LBA_HI);
    if (mid == 0x14 && hi == 0xEB) return 0; // ATAPI
    if (mid == 0x3C && hi == 0xC3) return 0; // SATA

    ata_wait_drq(drive->base);

    // Read 256 words of identification data
    for (int i = 0; i < 256; i++) {
        u16 data = inw(drive->base + ATA_REG_DATA);
        (void)data; // We could parse this for model/serial/size
    }

    return 1;
}

void ata_init(void) {
    // Primary Master
    drives[0].base = ATA_PRIMARY_IO_BASE;
    drives[0].master = 1;
    for(int i=0; i<32; i++) drives[0].name[i] = 0;
    kprint_init_str(drives[0].name, "Primary Master");
    drives[0].exists = ata_identify(&drives[0]);

    // Primary Slave
    drives[1].base = ATA_PRIMARY_IO_BASE;
    drives[1].master = 0;
    kprint_init_str(drives[1].name, "Primary Slave");
    drives[1].exists = ata_identify(&drives[1]);

    // Secondary Master
    drives[2].base = ATA_SECONDARY_IO_BASE;
    drives[2].master = 1;
    kprint_init_str(drives[2].name, "Secondary Master");
    drives[2].exists = ata_identify(&drives[2]);

    // Secondary Slave
    drives[3].base = ATA_SECONDARY_IO_BASE;
    drives[3].master = 0;
    kprint_init_str(drives[3].name, "Secondary Slave");
    drives[3].exists = ata_identify(&drives[3]);
}

void kprint_init_str(char* dest, const char* src) {
    int i = 0;
    while(src[i]) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = 0;
}

void ata_list_drives(void) {
    kprint("Detected Drives:\n");
    for (int i = 0; i < 4; i++) {
        if (drives[i].exists) {
            kprint(" ["); kprint_dec(i); kprint("] ");
            kprint(drives[i].name);
            kprint("\n");
        }
    }
}

ata_drive_t* ata_get_drive(int idx) {
    if (idx >= 0 && idx < 4) return &drives[idx];
    return NULL;
}

void ata_read_sectors(ata_drive_t* drive, u32 lba, u8 count, u16* buf) {
    outb(drive->base + ATA_REG_DRV_HEAD, (drive->master ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F));
    outb(drive->base + ATA_REG_ERROR, 0x00);
    outb(drive->base + ATA_REG_SECCOUNT, count);
    outb(drive->base + ATA_REG_LBA_LO, (u8)lba);
    outb(drive->base + ATA_REG_LBA_MID, (u8)(lba >> 8));
    outb(drive->base + ATA_REG_LBA_HI, (u8)(lba >> 16));
    outb(drive->base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    io_wait();

    for (int j = 0; j < count; j++) {
        ata_wait_bsy(drive->base);
        ata_wait_drq(drive->base);
        for (int i = 0; i < 256; i++) {
            buf[j * 256 + i] = inw(drive->base + ATA_REG_DATA);
        }
    }
}

void ata_write_sectors(ata_drive_t* drive, u32 lba, u8 count, u16* buf) {
    outb(drive->base + ATA_REG_DRV_HEAD, (drive->master ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F));
    outb(drive->base + ATA_REG_SECCOUNT, count);
    outb(drive->base + ATA_REG_LBA_LO, (u8)lba);
    outb(drive->base + ATA_REG_LBA_MID, (u8)(lba >> 8));
    outb(drive->base + ATA_REG_LBA_HI, (u8)(lba >> 16));
    outb(drive->base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    io_wait();

    for (int j = 0; j < count; j++) {
        ata_wait_bsy(drive->base);
        ata_wait_drq(drive->base);
        for (int i = 0; i < 256; i++) {
            outw(drive->base + ATA_REG_DATA, buf[j * 256 + i]);
        }
    }
    
    // Wait for the drive to finish processing the write
    ata_wait_bsy(drive->base);
}
