#include "core/pmm.h"

#define PHYS_BLOCKS 8192  // 32MB in 4KB blocks
static u32 phys_bitmap[PHYS_BLOCKS / 32];

void pmm_init(void) {
    for (int i = 0; i < PHYS_BLOCKS / 32; i++) phys_bitmap[i] = 0;
}

u32 pmm_alloc(void) {
    for (int i = 0; i < PHYS_BLOCKS; i++) {
        int idx = i / 32, bit = i % 32;
        if (!(phys_bitmap[idx] & (1 << bit))) {
            phys_bitmap[idx] |= (1 << bit);
            return 0x01000000 + (i * 4096); // Start at 16MB
        }
    }
    return 0;
}

void pmm_free(u32 addr) {
    if (addr < 0x01000000) return;
    u32 i = (addr - 0x01000000) / 4096;
    if (i >= PHYS_BLOCKS) return;
    phys_bitmap[i / 32] &= ~(1 << (i % 32));
}
