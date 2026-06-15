#include "cpu/paging.h"
#include "core/malloc.h"
#include "core/panic.h"

/* The kernel page directory must be 4096-byte aligned (CR3 requirement).
 * Using a static array guarantees alignment via __attribute__((aligned)). */
static page_dir_t kernel_dir __attribute__((aligned(4096)));

page_dir_t* kernel_page_dir = &kernel_dir;

/* Identity-map the first IDENTITY_MB with 4MB PSE pages.
 * Must cover: BIOS/VGA (0), kernel (1MB), initrd (2MB),
 *             malloc heap (4MB), shell (8MB), user progs (10MB). */
#define IDENTITY_4MB_ENTRIES 4   /* 0–4MB, 4–8MB, 8–12MB, 12–16MB = 16MB */

static void fill_kernel_map(page_dir_t* dir) {
    for (u32 i = 0; i < IDENTITY_4MB_ENTRIES; i++) {
        (*dir)[i] = (i << 22) | PDE_PRESENT | PDE_WRITABLE | PDE_USER | PDE_4MB;
    }
    for (u32 i = IDENTITY_4MB_ENTRIES; i < PD_ENTRIES; i++) {
        (*dir)[i] = 0;
    }
}

void paging_init(void) {
    fill_kernel_map(&kernel_dir);

    /* Enable PSE (4MB pages) in CR4 */
    u32 cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 4);
    asm volatile("mov %0, %%cr4" :: "r"(cr4));

    /* Load page directory */
    asm volatile("mov %0, %%cr3" :: "r"(&kernel_dir));

    /* Enable paging in CR0 */
    u32 cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1u << 31);
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

page_dir_t* paging_new_dir(void) {
    /* Allocate from heap — must be 4KB-aligned.
     * Over-allocate by 4095 bytes so we can align the pointer manually. */
    u8* raw = (u8*)kmalloc(sizeof(page_dir_t) + 4095);
    if (!raw) return NULL;

    /* Align up to 4096 */
    page_dir_t* dir = (page_dir_t*)(((u32)raw + 4095) & ~4095u);

    fill_kernel_map(dir);
    return dir;
}

void paging_free_dir(page_dir_t* dir) {
    /* We can't easily recover the original raw pointer after alignment,
     * so for now just leave it (no per-process free needed until we have
     * a proper aligned allocator). */
    (void)dir;
}

void paging_load_dir(page_dir_t* dir) {
    asm volatile("mov %0, %%cr3" :: "r"(dir) : "memory");
}
