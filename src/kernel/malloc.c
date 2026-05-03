#include "include/malloc.h"
#include "include/kprint.h"

struct block_header {
    size_t size;
    int is_free;
    struct block_header* next;
};

#define HEADER_SIZE sizeof(struct block_header)

static struct block_header* free_list = NULL;

void malloc_init(u32 start_addr, u32 size) {
    free_list = (struct block_header*)start_addr;
    free_list->size = size - HEADER_SIZE;
    free_list->is_free = 1;
    free_list->next = NULL;
}

void* kmalloc(size_t size) {
    struct block_header* curr = free_list;
    
    // Align size to 4 bytes
    size = (size + 3) & ~3;

    while (curr) {
        if (curr->is_free && curr->size >= size) {
            // Can we split this block?
            if (curr->size > size + HEADER_SIZE + 4) {
                struct block_header* next_block = (struct block_header*)((u8*)curr + HEADER_SIZE + size);
                next_block->size = curr->size - size - HEADER_SIZE;
                next_block->is_free = 1;
                next_block->next = curr->next;
                
                curr->size = size;
                curr->next = next_block;
            }
            
            curr->is_free = 0;
            return (void*)((u8*)curr + HEADER_SIZE);
        }
        curr = curr->next;
    }
    
    kprint("OUT OF MEMORY!\n");
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;
    
    struct block_header* header = (struct block_header*)((u8*)ptr - HEADER_SIZE);
    header->is_free = 1;
    
    // Coalesce free blocks
    struct block_header* curr = free_list;
    while (curr) {
        if (curr->is_free && curr->next && curr->next->is_free) {
            curr->size += curr->next->size + HEADER_SIZE;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

void malloc_stats(void) {
    struct block_header* curr = free_list;
    int total_blocks = 0;
    int free_blocks = 0;
    size_t total_free_size = 0;
    
    while (curr) {
        total_blocks++;
        if (curr->is_free) {
            free_blocks++;
            total_free_size += curr->size;
        }
        curr = curr->next;
    }
    
    kprint("Memory Stats:\n");
    kprint("  Total blocks: "); kprint_dec(total_blocks); kprint("\n");
    kprint("  Free blocks:  "); kprint_dec(free_blocks); kprint("\n");
    kprint("  Total free:   "); kprint_dec(total_free_size); kprint(" bytes\n");
}
