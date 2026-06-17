#include "storage/elf.h"
#include "storage/fat32.h"
#include "core/malloc.h"
#include "io/kprint.h"
#include "io/serial.h"
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

    // Calculate total size needed
    u32 min_addr = 0xFFFFFFFF, max_addr = 0;
    for (u16 i = 0; i < eh.e_phnum; i++) {
        elf_ph_t ph;
        if (elf_read_at(&entry, eh.e_phoff + i * sizeof(ph), (u8*)&ph, sizeof(ph)) < (int)sizeof(ph))
            continue;
        if (ph.p_type != PT_LOAD || ph.p_memsz == 0) continue;
        if (ph.p_vaddr < min_addr) min_addr = ph.p_vaddr;
        if (ph.p_vaddr + ph.p_memsz > max_addr) max_addr = ph.p_vaddr + ph.p_memsz;
    }
    
    u32 size = max_addr - min_addr;
    u8* temp = (u8*)kmalloc(size);
    if (!temp) return NULL;
    for (u32 i = 0; i < size; i++) temp[i] = 0;

    // Load segments into temp buffer
    for (u16 i = 0; i < eh.e_phnum; i++) {
        elf_ph_t ph;
        if (elf_read_at(&entry, eh.e_phoff + i * sizeof(ph), (u8*)&ph, sizeof(ph)) < (int)sizeof(ph))
            continue;
        if (ph.p_type != PT_LOAD || ph.p_memsz == 0) continue;

        u32 offset = ph.p_vaddr - min_addr;
        if (ph.p_filesz > 0)
            elf_read_at(&entry, ph.p_offset, temp + offset, ph.p_filesz);
    }

    u32 brk = (max_addr + 0xFFF) & ~0xFFFu;

    process_t* proc = task_create((void*)eh.e_entry, 0x1, 0);
    if (!proc) { kfree(temp); return NULL; }
    proc->brk_end = brk;
    proc->is_elf  = 1;
    
    extern page_dir_t* paging_new_dir(void);
    extern void paging_map_page(page_dir_t*, u32, u32, u32);
    extern u32 pmm_alloc(void);
    
    proc->page_dir = paging_new_dir();
    if (!proc->page_dir) {
        kfree((void*)(proc->kstack - STACK_SIZE));
        kfree((void*)(proc->ustack - STACK_SIZE));
        kfree(proc);
        kfree(temp);
        return NULL;
    }
    
    // Copy from temp buffer to process-specific pages
    for (u32 vaddr = min_addr; vaddr < brk; vaddr += 4096) {
        u32 phys = pmm_alloc();
        if (!phys) continue;
        
        u32 offset = vaddr - min_addr;
        u8* src = temp + offset;
        u8* dst = (u8*)phys;
        u32 copy_len = (offset + 4096 <= size) ? 4096 : (size - offset);
        for (u32 j = 0; j < copy_len; j++) dst[j] = src[j];
        for (u32 j = copy_len; j < 4096; j++) dst[j] = 0;
        
        paging_map_page(proc->page_dir, vaddr, phys, 0x07);
    }

    kfree(temp);
    return proc;
}
