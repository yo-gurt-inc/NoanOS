#include "core/usermem.h"
#include "core/errno.h"
#include "cpu/paging.h"
#include "cpu/task.h"
#include "io/serial.h"

/**
 * Check if an address is in kernel space (identity-mapped region).
 * These addresses are always accessible regardless of page directory.
 */
static inline int is_kernel_addr(u32 addr) {
    return addr < 0x10000000; /* First 256MB is kernel space */
}

/**
 * Check if a single page at the given virtual address is mapped
 * in the current process's address space.
 */
static int is_page_mapped(u32 vaddr) {
    process_t* proc = get_current_process();
    if (!proc) {
        serial_puts("[is_page_mapped: no current process]\n");
        return 0;
    }
    
    page_dir_t* dir = proc->page_dir ? proc->page_dir : kernel_page_dir;
    if (!dir) {
        serial_puts("[is_page_mapped: no page dir]\n");
        return 0;
    }
    
    /* Try to get physical address - returns 0 if not mapped */
    u32 phys = paging_get_phys(dir, vaddr);
    
    serial_puts("[is_page_mapped: vaddr=");
    serial_hex(vaddr);
    serial_puts(" phys=");
    serial_hex(phys);
    serial_puts(" mapped=");
    serial_dec(phys != 0);
    serial_puts("]\n");
    
    return (phys != 0);
}

int is_user_addr_valid(u32 addr, u32 len) {
    /* NULL pointer is always invalid */
    if (addr == 0 || len == 0) return 0;
    
    /* Check for overflow */
    if (addr + len < addr) return 0;
    
    /* For now, be permissive: allow any address above 1MB (0x00100000) 
     * This includes:
     * - Kernel space (identity-mapped 0x00100000-0x0FFFFFFF)
     * - ELF load area (typically 0x00200000+)
     * - User space (0x08000000+)
     * 
     * TODO: Implement proper page table walking for validation
     */
    if (addr >= 0x00100000 && addr < 0xC0000000) {
        return 1;
    }
    
    serial_puts("[is_user_addr_valid: rejecting addr=");
    serial_hex(addr);
    serial_puts(" len=");
    serial_dec(len);
    serial_puts("]\n");
    
    return 0;
}

int is_user_string_valid(const char* str, u32 max_len) {
    if (!str) return 0;
    
    u32 addr = (u32)str;
    
    /* Check if the start address is in a valid range */
    if (addr < 0x00100000 || addr >= 0xC0000000) {
        serial_puts("[is_user_string_valid: addr out of range=");
        serial_hex(addr);
        serial_puts("]\n");
        return 0;
    }
    
    /* Find null terminator within max_len */
    for (u32 i = 0; i < max_len; i++) {
        if (str[i] == '\0') return 1;
    }
    
    /* String not null-terminated within max_len */
    serial_puts("[is_user_string_valid: no null terminator within ");
    serial_dec(max_len);
    serial_puts(" bytes]\n");
    return 0;
}

int copy_from_user(void* dst_kernel, const void* src_user, u32 len) {
    if (!dst_kernel || !src_user || len == 0) return EFAULT;
    
    /* Validate source is in accessible memory */
    if (!is_user_addr_valid((u32)src_user, len)) {
        serial_puts("[copy_from_user: invalid src=");
        serial_hex((u32)src_user);
        serial_puts(" len=");
        serial_dec(len);
        serial_puts("]\n");
        return EFAULT;
    }
    
    /* Perform the copy */
    u8* dst = (u8*)dst_kernel;
    const u8* src = (const u8*)src_user;
    for (u32 i = 0; i < len; i++) {
        dst[i] = src[i];
    }
    
    return 0;
}

int copy_to_user(void* dst_user, const void* src_kernel, u32 len) {
    if (!dst_user || !src_kernel || len == 0) return EFAULT;
    
    /* Validate destination is in accessible memory */
    if (!is_user_addr_valid((u32)dst_user, len)) {
        serial_puts("[copy_to_user: invalid dst=");
        serial_hex((u32)dst_user);
        serial_puts(" len=");
        serial_dec(len);
        serial_puts("]\n");
        return EFAULT;
    }
    
    /* Perform the copy */
    u8* dst = (u8*)dst_user;
    const u8* src = (const u8*)src_kernel;
    for (u32 i = 0; i < len; i++) {
        dst[i] = src[i];
    }
    
    return 0;
}

int strncpy_from_user(char* dst_kernel, const char* src_user, u32 max_len) {
    if (!dst_kernel || !src_user || max_len == 0) return EFAULT;
    
    /* Validate the string */
    if (!is_user_string_valid(src_user, max_len)) {
        serial_puts("[strncpy_from_user: invalid string src=");
        serial_hex((u32)src_user);
        serial_puts("]\n");
        return EFAULT;
    }
    
    /* Copy the string */
    u32 i;
    for (i = 0; i < max_len - 1 && src_user[i] != '\0'; i++) {
        dst_kernel[i] = src_user[i];
    }
    dst_kernel[i] = '\0';
    
    return (int)i;
}
