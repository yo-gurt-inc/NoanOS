#include "include/elf.h"
#include "include/fat32.h"
#include "include/malloc.h"
#include "include/kprint.h"
#include "include/task.h"

int elf_load(const char* filename) {
    // This is a simplified ELF loader.
    // In a real OS, we'd read the file into a buffer and parse it.
    // Since we don't have a full VFS/file read yet (just cat), 
    // we'll assume the file is loaded into memory or use a placeholder.
    
    kprint("ELF Loader: Loading "); kprint(filename); kprint("\n");
    
    // Placeholder: In a real implementation, you'd do:
    // 1. Open file
    // 2. Read ELF header
    // 3. Loop through program headers
    // 4. For each PT_LOAD, allocate memory and copy data
    // 5. task_create(header->e_entry, 1); // 1 for User Mode
    
    return 0;
}
