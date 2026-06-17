#include "cpu/paging.h"
#include "core/malloc.h"
#include "core/panic.h"
#include "core/pmm.h"
#include "io/kprint.h"

static page_dir_t kernel_dir __attribute__((aligned(4096)));
page_dir_t* kernel_page_dir = &kernel_dir;

#define PTE_PRESENT  (1 << 0)
#define PTE_WRITABLE (1 << 1)
#define PTE_USER     (1 << 2)

typedef u32 page_table_t[1024] __attribute__((aligned(4096)));

/* Get page table for a virtual address, create if needed */
static page_table_t* get_page_table(page_dir_t* dir, u32 vaddr, int create) {
    u32 pde_idx = vaddr >> 22;
    u32 pde = (*dir)[pde_idx];
    
    if (!(pde & PDE_PRESENT)) {
        if (!create) return NULL;
        u32 pt_phys = pmm_alloc();
        if (!pt_phys) return NULL;
        (*dir)[pde_idx] = pt_phys | PDE_PRESENT | PDE_WRITABLE | PDE_USER;
        // Zero the page table
        u32* pt = (u32*)pt_phys;
        for (int i = 0; i < 1024; i++) pt[i] = 0;
        return (page_table_t*)pt_phys;
    }
    
    return (page_table_t*)(pde & ~0xFFF);
}

/* Map a virtual page to physical page */
void paging_map_page(page_dir_t* dir, u32 vaddr, u32 paddr, u32 flags) {
    page_table_t* pt = get_page_table(dir, vaddr, 1);
    if (!pt) return;
    u32 pte_idx = (vaddr >> 12) & 0x3FF;
    (*pt)[pte_idx] = (paddr & ~0xFFF) | flags;
}

/* Get physical address for virtual address */
u32 paging_get_phys(page_dir_t* dir, u32 vaddr) {
    u32 pde_idx = vaddr >> 22;
    u32 pde = (*dir)[pde_idx];
    
    if (!(pde & PDE_PRESENT)) return 0;
    if (pde & PDE_4MB) return (pde & ~0x3FFFFF) | (vaddr & 0x3FFFFF);
    
    page_table_t* pt = (page_table_t*)(pde & ~0xFFF);
    u32 pte_idx = (vaddr >> 12) & 0x3FF;
    u32 pte = (*pt)[pte_idx];
    
    if (!(pte & PTE_PRESENT)) return 0;
    return (pte & ~0xFFF) | (vaddr & 0xFFF);
}

void paging_init(void) {
    pmm_init();
    
    // Identity map first 256MB with 4MB pages (kernel space)
    for (u32 i = 0; i < 64; i++) {
        kernel_dir[i] = (i << 22) | PDE_PRESENT | PDE_WRITABLE | PDE_USER | PDE_4MB;
    }
    for (u32 i = 64; i < 1024; i++) kernel_dir[i] = 0;
    
    u32 cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 4);
    asm volatile("mov %0, %%cr4" :: "r"(cr4));
    asm volatile("mov %0, %%cr3" :: "r"(&kernel_dir));
    
    u32 cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1u << 31);
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

page_dir_t* paging_new_dir(void) {
    u8* raw = (u8*)kmalloc(sizeof(page_dir_t) + 4095);
    if (!raw) return NULL;
    page_dir_t* dir = (page_dir_t*)(((u32)raw + 4095) & ~4095u);
    
    // Copy kernel mappings (first 256MB identity mapped)
    for (u32 i = 0; i < 64; i++) (*dir)[i] = kernel_dir[i];
    for (u32 i = 64; i < 1024; i++) (*dir)[i] = 0;
    
    return dir;
}

/* Clone directory with COW - map same physical pages to both */
page_dir_t* paging_clone_dir_cow(page_dir_t* src) {
    page_dir_t* dst = paging_new_dir();
    if (!dst) return NULL;
    
    // For each user page table (above 256MB)
    for (u32 pde_idx = 64; pde_idx < 1024; pde_idx++) {
        u32 pde = (*src)[pde_idx];
        if (!(pde & PDE_PRESENT)) continue;
        
        if (pde & PDE_4MB) {
            // Share 4MB page directly
            (*dst)[pde_idx] = pde;
        } else {
            // Clone page table - create new PT but share physical pages
            u32 new_pt_phys = pmm_alloc();
            if (!new_pt_phys) continue;
            
            page_table_t* src_pt = (page_table_t*)(pde & ~0xFFF);
            page_table_t* dst_pt = (page_table_t*)new_pt_phys;
            
            // Copy all PTEs - both processes share same physical pages
            for (int i = 0; i < 1024; i++) {
                (*dst_pt)[i] = (*src_pt)[i];
            }
            
            (*dst)[pde_idx] = new_pt_phys | PDE_PRESENT | PDE_WRITABLE | PDE_USER;
        }
    }
    
    return dst;
}

/* Copy a page's contents to new physical page */
static void copy_page(u32 src_phys, u32 dst_phys) {
    u8* src = (u8*)src_phys;
    u8* dst = (u8*)dst_phys;
    for (int i = 0; i < 4096; i++) dst[i] = src[i];
}

/* Deep copy user memory (for fork) */
void paging_copy_user_memory(page_dir_t* dst, page_dir_t* src, u32 start, u32 end) {
    for (u32 vaddr = start; vaddr < end; vaddr += 4096) {
        u32 src_phys = paging_get_phys(src, vaddr);
        if (!src_phys) continue;
        
        u32 dst_phys = pmm_alloc();
        if (!dst_phys) continue;
        
        copy_page(src_phys, dst_phys);
        paging_map_page(dst, vaddr, dst_phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    }
}

void paging_free_dir(page_dir_t* dir) {
    (void)dir;
}

void paging_load_dir(page_dir_t* dir) {
    asm volatile("mov %0, %%cr3" :: "r"(dir) : "memory");
}
