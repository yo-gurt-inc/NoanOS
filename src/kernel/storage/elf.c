#include "storage/elf.h"
#include "storage/fat32.h"
#include "core/malloc.h"
#include "io/kprint.h"
#include "cpu/task.h"

/*
 * Read `len` bytes from a FAT32 file at byte offset `file_off` into `dest`.
 * Uses a single 512-byte stack buffer — no large heap allocation.
 */
static int elf_read_at(fat32_dir_entry_t* entry, u32 file_off, u8* dest, u32 len) {
    ata_drive_t* drive = _fat32_get_current_drive();
    fat32_bpb_t* bpb   = _fat32_get_bpb();
    if (!drive || !bpb) return -1;

    u32 spc        = bpb->sectors_per_cluster;
    u32 cluster_sz = spc * 512;

    /* Walk FAT chain to cluster containing file_off */
    u32 cluster     = ((u32)entry->cluster_hi << 16) | entry->cluster_lo;
    u32 cluster_idx = file_off / cluster_sz;
    for (u32 i = 0; i < cluster_idx && cluster < 0x0FFFFFF8; i++)
        cluster = _fat32_get_fat_entry(cluster);

    u32 bytes_read = 0;
    u8 buf[512];

    while (bytes_read < len && cluster >= 2 && cluster < 0x0FFFFFF8) {
        u32 pos_in_cluster = (file_off + bytes_read) % cluster_sz;
        u32 sec_in_cluster = pos_in_cluster / 512;
        u32 pos_in_sector  = (file_off + bytes_read) % 512;

        ata_read_sectors(drive, _fat32_cluster_to_lba(cluster) + sec_in_cluster, 1, (u16*)buf);

        u32 can_copy = 512 - pos_in_sector;
        if (can_copy > len - bytes_read) can_copy = len - bytes_read;
        for (u32 i = 0; i < can_copy; i++)
            dest[bytes_read + i] = buf[pos_in_sector + i];
        bytes_read += can_copy;

        if ((file_off + bytes_read) % cluster_sz == 0)
            cluster = _fat32_get_fat_entry(cluster);
    }
    return (int)bytes_read;
}

process_t* elf_load_file(const char* path) {
    fat32_dir_entry_t entry;
    if (!_fat32_find_entry(path, &entry)) {
        kprint("elf_load: not found: "); kprint(path); kprint("\n");
        return NULL;
    }

    elf_header_t eh;
    if (elf_read_at(&entry, 0, (u8*)&eh, sizeof(eh)) < (int)sizeof(eh)) {
        kprint("elf_load: read error\n");
        return NULL;
    }
    if (eh.e_ident[0] != 0x7F || eh.e_ident[1] != 'E' ||
        eh.e_ident[2] != 'L'  || eh.e_ident[3] != 'F') {
        kprint("elf_load: bad magic\n");
        return NULL;
    }
    if (eh.e_machine != 3) { kprint("elf_load: not i386\n"); return NULL; }

    u32 brk = 0;
    for (u16 i = 0; i < eh.e_phnum; i++) {
        elf_ph_t ph;
        if (elf_read_at(&entry, eh.e_phoff + i * sizeof(ph), (u8*)&ph, sizeof(ph)) < (int)sizeof(ph))
            continue;
        if (ph.p_type != PT_LOAD || ph.p_memsz == 0) continue;

        if (ph.p_filesz > 0)
            elf_read_at(&entry, ph.p_offset, (u8*)ph.p_vaddr, ph.p_filesz);

        /* Zero BSS */
        u8* bss = (u8*)(ph.p_vaddr + ph.p_filesz);
        for (u32 j = 0; j < ph.p_memsz - ph.p_filesz; j++) bss[j] = 0;

        u32 end = ph.p_vaddr + ph.p_memsz;
        if (end > brk) brk = end;
    }

    brk = (brk + 0xFFF) & ~0xFFFu;

    process_t* proc = task_create((void*)eh.e_entry, 0x1, 0);
    if (!proc) { kprint("elf_load: task_create failed\n"); return NULL; }
    proc->brk_end = brk;
    proc->is_elf  = 1;

    kprint("elf_load: "); kprint(path);
    kprint(" entry="); kprint_hex(eh.e_entry); kprint("\n");
    return proc;
}
