#include "core/initrd.h"
#include "io/kprint.h"
#include "core/malloc.h"

typedef struct {
    char name[32];
    u32 size;
    u32 offset;
} __attribute__((packed)) narc_entry_t;

typedef struct {
    u32 magic;
    u32 count;
    narc_entry_t entries[];
} __attribute__((packed)) narc_header_t;

static narc_header_t* archive = NULL;
static void* shell_entry = NULL;

int initrd_unpack(u32 initrd_addr) {
    archive = (narc_header_t*)initrd_addr;

    if (archive->magic != INITRD_MAGIC) {
        kprint("INITRD: Bad Archive Magic: "); kprint_hex(archive->magic); kprint("\n");
        return -1;
    }
    
    kprint("INITRD: Found "); kprint_dec(archive->count); kprint(" files.\n");

    // The first file is always the shell
    narc_entry_t* shell_meta = &archive->entries[0];
    void* shell_src = (void*)(initrd_addr + shell_meta->offset);
    void* shell_dest = (void*)0x300000;
    
    u8* s = (u8*)shell_src;
    u8* d = (u8*)shell_dest;
    for (u32 i = 0; i < shell_meta->size; i++) d[i] = s[i];
    shell_entry = shell_dest;

    kprint("INITRD: Unpacked shell ("); kprint_dec(shell_meta->size); kprint(" bytes)\n");
    return 0;
}

void* initrd_get_entry(void) {
    return shell_entry;
}

int initrd_get_file_count(void) {
    return archive ? (int)archive->count : 0;
}

void* initrd_get_file(int index, char* name, u32* size) {
    if (!archive || index < 0 || index >= (int)archive->count) return NULL;
    
    narc_entry_t* e = &archive->entries[index];
    if (name) {
        for(int i=0; i<32; i++) name[i] = e->name[i];
    }
    if (size) *size = e->size;
    
    return (void*)((u32)archive + e->offset);
}
