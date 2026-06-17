#ifndef PAGING_H
#define PAGING_H

#include "core/types.h"

/* Each entry in the page directory (4MB PSE pages)
 *  Bit 0 : Present
 *  Bit 1 : Read/Write
 *  Bit 2 : User/Supervisor (0 = kernel only)
 *  Bit 7 : Page Size (1 = 4MB)
 */
#define PDE_PRESENT  (1 << 0)
#define PDE_WRITABLE (1 << 1)
#define PDE_USER     (1 << 2)
#define PDE_4MB      (1 << 7)

/* A page directory is 1024 × 4-byte entries, covering 4GB in 4MB chunks */
#define PD_ENTRIES 1024

typedef u32 page_dir_t[PD_ENTRIES];

/*
 * paging_init()  – build the kernel page directory and enable paging.
 *                  Must be called before any code that touches user memory.
 *
 * paging_new_dir()  – allocate a fresh page directory for a new process,
 *                     pre-seeded with the kernel identity mapping.
 *
 * paging_free_dir() – release a page directory (does NOT free mapped pages).
 *
 * paging_load_dir() – switch CR3 to the given page directory.
 */
void        paging_init(void);
page_dir_t* paging_new_dir(void);
page_dir_t* paging_clone_dir_cow(page_dir_t* src);
void        paging_free_dir(page_dir_t* dir);
void        paging_load_dir(page_dir_t* dir);

/* The kernel's own page directory */
extern page_dir_t* kernel_page_dir;

#endif
