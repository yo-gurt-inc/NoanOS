#include "storage/elf.h"
#include "storage/fat32.h"
#include "core/malloc.h"
#include "io/kprint.h"
#include "io/serial.h"
#include "cpu/task.h"

// Simple local memcpy to avoid stdlib headers and size_t conflicts
static void local_memcpy(u8* dst, const u8* src, u32 n) {
    for (u32 i = 0; i < n; i++) dst[i] = src[i];
}

/*
 * Read `len` bytes from a FAT32 file at byte offset `file_off` into `dest`.
 * Uses a single 512-byte stack buffer — no large heap allocation.
 */
// Read entire file starting at the directory entry into dest (up to max_len)
static int elf_read_all(fat32_dir_entry_t* entry, u8* dest, u32 max_len) {
    ata_drive_t* drive = _fat32_get_current_drive();
    fat32_bpb_t* bpb   = _fat32_get_bpb();
    if (!drive || !bpb) return -1;

    u32 spc        = bpb->sectors_per_cluster;
    u32 cluster_sz = spc * 512;

    u32 cluster = ((u32)entry->cluster_hi << 16) | entry->cluster_lo;
    u32 remaining = entry->file_size;
    if (remaining > max_len) remaining = max_len;

    u32 bytes_written = 0;

    // Prefer shared persistent cluster buffer
    u8* cluster_buf = _fat32_get_cluster_buf();
    u32 cluster_bytes = _fat32_get_cluster_buf_size();
    u32 sectors_per_cluster = spc;
    if (sectors_per_cluster == 0 || sectors_per_cluster > 128) sectors_per_cluster = 1;
    if (!cluster_buf) {
        cluster_bytes = sectors_per_cluster * 512;
        cluster_buf = (u8*)kmalloc(cluster_bytes);
        if (!cluster_buf) return -1;
    }

    while (remaining > 0 && cluster >= 2 && cluster < 0x0FFFFFF8) {
        // Read full cluster
        ata_read_sectors(drive, _fat32_cluster_to_lba(cluster), (u8)sectors_per_cluster, (u16*)cluster_buf);
        u32 to_copy = (remaining > cluster_bytes) ? cluster_bytes : remaining;
        for (u32 i = 0; i < to_copy; i++) dest[bytes_written + i] = cluster_buf[i];
        bytes_written += to_copy;
        remaining -= to_copy;
        if (remaining == 0) break;
        cluster = _fat32_get_fat_entry(cluster);
    }
    return (int)bytes_written;
}

process_t* elf_load_file(const char* path) {
    fat32_dir_entry_t entry;
    if (!_fat32_find_entry(path, &entry)) {
        kprint("elf_load: not found: "); kprint(path); kprint("\n");
        return NULL;
    }

    // Read full file into memory to avoid many small reads
    u32 file_size = entry.file_size;
    u8* file_buf = (u8*)kmalloc(file_size);
    if (!file_buf) return NULL;
    for (u32 i = 0; i < file_size; i++) file_buf[i] = 0;
    if (elf_read_all(&entry, file_buf, file_size) < (int)file_size) {
        kfree(file_buf);
        kprint("elf_load: read error\n");
        return NULL;
    }

    // Parse ELF header from file_buf
    if (file_size < sizeof(elf_header_t)) { kfree(file_buf); kprint("elf_load: file too small\n"); return NULL; }
    elf_header_t* peh = (elf_header_t*)file_buf;
    if (peh->e_ident[0] != 0x7F || peh->e_ident[1] != 'E' || peh->e_ident[2] != 'L' || peh->e_ident[3] != 'F') {
        kfree(file_buf); kprint("elf_load: bad magic\n"); return NULL;
    }
    if (peh->e_machine != 3) { kfree(file_buf); kprint("elf_load: not i386\n"); return NULL; }

    // Calculate memory size from program headers
    u32 min_addr = 0xFFFFFFFF, max_addr = 0;
    for (u16 i = 0; i < peh->e_phnum; i++) {
        if (peh->e_phoff + (u32)(i+1) * sizeof(elf_ph_t) > file_size) break;
        elf_ph_t ph;
        local_memcpy((u8*)&ph, file_buf + peh->e_phoff + i * sizeof(ph), sizeof(ph));
        if (ph.p_type != PT_LOAD || ph.p_memsz == 0) continue;
        if (ph.p_vaddr < min_addr) min_addr = ph.p_vaddr;
        if (ph.p_vaddr + ph.p_memsz > max_addr) max_addr = ph.p_vaddr + ph.p_memsz;
    }

    u32 size = (max_addr > min_addr) ? (max_addr - min_addr) : 0;
    if (size == 0) { kfree(file_buf); kprint("elf_load: no loadable segments\n"); return NULL; }

    u8* temp = (u8*)kmalloc(size);
    if (!temp) { kfree(file_buf); return NULL; }
    for (u32 i = 0; i < size; i++) temp[i] = 0;

    // Copy loadable segments from file_buf into temp
    for (u16 i = 0; i < peh->e_phnum; i++) {
        if (peh->e_phoff + (u32)(i+1) * sizeof(elf_ph_t) > file_size) break;
        elf_ph_t ph;
        local_memcpy((u8*)&ph, file_buf + peh->e_phoff + i * sizeof(ph), sizeof(ph));
        if (ph.p_type != PT_LOAD || ph.p_memsz == 0) continue;
        u32 offset = ph.p_vaddr - min_addr;
        if (ph.p_filesz > 0 && ph.p_offset + ph.p_filesz <= file_size) {
            local_memcpy(temp + offset, file_buf + ph.p_offset, ph.p_filesz);
        }
    }

    // Entry point
    serial_puts("[ELF: e_entry=");
    serial_hex(peh->e_entry);
    serial_puts("]\n");

    process_t* proc = task_create((void*)peh->e_entry, 0x1, 0);
    if (!proc) { kfree(temp); return NULL; }
    
    serial_puts("[ELF: proc->eip after task_create=");
    serial_hex(proc->eip);
    serial_puts("]\n");
    u32 brk = (max_addr + 0xFFF) & ~0xFFFu;
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
