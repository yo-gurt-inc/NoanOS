#include "storage/noan.h"
#include "storage/fat32.h"
#include "core/malloc.h"
#include "core/panic.h"
#include "io/kprint.h"
#include "cpu/task.h"

// Forward declaration of internal FAT32 helpers
extern int _fat32_find_entry(const char* name, fat32_dir_entry_t* out_entry);
extern int _fat32_find_full_path_file(const char* path, fat32_dir_entry_t* out_entry);

u32 noan_load(const char* cmd_line) {
    char name[64];
    char fallback[72];
    int k = 0;
    
    // Extract program name
    while(cmd_line[k] && (unsigned char)cmd_line[k] > 32 && k < 63) {
        name[k] = cmd_line[k];
        k++;
    }
    name[k] = '\0';

    // Handle ./ prefix
    const char* search_name = name;
    if (name[0] == '.' && name[1] == '/' && name[2] != '\0') {
        search_name = name + 2;
    }

    // Try to find the file
    fat32_dir_entry_t entry;
    
    // Attempt 1: Try the path as-is (handles /bin/ls or bin/ls)
    if (_fat32_find_entry(search_name, &entry)) {
        goto found;
    }
    
    // Check if search_name has a slash in it
    const char* p = search_name;
    while (*p && *p != '/') p++;
    
    if (*p != '\0') {
        // Has a slash - try as full-path file (e.g., "bin/ls" -> "BIN/LS" filename)
        if (_fat32_find_full_path_file(search_name, &entry)) {
            goto found;
        }
    }
    
    // Attempt 2: If simple name (no /), try with /bin/ prefix
    p = search_name;
    while (*p && *p != '/') p++;
    
    if (*p == '\0') {
        // No slash, it's a simple name - try /bin/ prefix
        k = 0;
        fallback[k++] = '/';
        fallback[k++] = 'b';
        fallback[k++] = 'i';
        fallback[k++] = 'n';
        fallback[k++] = '/';
        
        const char* src = search_name;
        while (*src && k < 66) {
            fallback[k++] = *src++;
        }
        fallback[k] = '\0';
        
        if (_fat32_find_entry(fallback, &entry)) {
            search_name = fallback;
            goto found;
        }
        
        // Attempt 3: Try as a full-path filename (e.g., "BIN/LS" stored as one filename)
        if (_fat32_find_full_path_file(fallback, &entry)) {
            search_name = fallback;
            goto found;
        }
    }
    
    return 0;

found:

    if (entry.file_size < sizeof(noan_header_t)) {
        return 0;
    }

    // 1. Read the header into a temporary buffer
    noan_header_t header;
    int header_read = fat32_read(search_name, (char*)&header, sizeof(noan_header_t));
    if (header_read != sizeof(noan_header_t)) {
        return 0;
    }

    if (header.magic != NOAN_MAGIC) {
        return 0;
    }

    // 2. Read the actual binary into the fixed address 0x800000.
    void* temp_buffer = kmalloc(entry.file_size);
    if (!temp_buffer) return 0;

    int read = fat32_read(search_name, (char*)temp_buffer, entry.file_size);
    if (read != (int)entry.file_size) {
        kfree(temp_buffer);
        return 0;
    }

    u8* payload = (u8*)temp_buffer + sizeof(noan_header_t);
    
    // Isolation: Shell stays at 0x800000, others go to 0xA00000
    u32 load_addr = 0xA00000;
    
    // Check if we are loading the shell
    const char* s = search_name;
    int is_shell = 0;
    while (*s) {
        if (s[0] == 's' && s[1] == 'h' && s[2] == 'e' && s[3] == 'l' && s[4] == 'l') {
            is_shell = 1;
            break;
        }
        s++;
    }
    
    if (is_shell) load_addr = 0x800000;

    u8* dest = (u8*)load_addr;
    u32 payload_size = entry.file_size - sizeof(noan_header_t);
    
    for (u32 j = 0; j < payload_size; j++) {
        dest[j] = payload[j];
    }
    kfree(temp_buffer);

    u32 final_entry = load_addr + header.entry_point;
    return final_entry;
}

void noan_execute(u32 entry, const char* cmd_line) {
    process_t* parent = get_current_process();
    int parent_id = parent ? parent->id : 0;

    // 1. Create the task
    process_t* new_proc = task_create((void*)entry, 0x1, parent_id);
    if (!new_proc) return;

    // 2. Parse arguments
    char buf[128];
    char* argv[16];
    int argc = 0;
    int i = 0;
    while(cmd_line[i] && i < 127) { buf[i] = cmd_line[i]; i++; }
    buf[i] = '\0';

    char* p = buf;
    while (*p && argc < 16) {
        while (*p && (unsigned char)*p <= 32) p++;
        if (*p == '\0') break;
        argv[argc++] = p;
        while (*p && (unsigned char)*p > 32) p++;
        if (*p == '\0') break;
        *p++ = '\0';
    }

    // 3. Set up user stack with argc/argv
    u32* ustack_base = (u32*)new_proc->ustack;
    
    // Copy strings to top of user stack
    char* ustrings = (char*)ustack_base - 256;
    u32 ustrings_addr = new_proc->ustack - 256;
    u32 current_off = 0;
    u32 argv_ptrs[16];

    for (int j = 0; j < argc; j++) {
        int len = 0;
        while(argv[j][len]) {
            ustrings[current_off + len] = argv[j][len];
            len++;
        }
        ustrings[current_off + len] = '\0';
        argv_ptrs[j] = ustrings_addr + current_off;
        current_off += len + 1;
    }

    // Set up argv array pointers
    u32* uargv_array = (u32*)(ustrings - 64);
    u32 uargv_array_addr = ustrings_addr - 64;
    for (int j = 0; j < argc; j++) {
        uargv_array[j] = argv_ptrs[j];
    }

    // Final C call frame
    u32* final_esp = (u32*)(uargv_array - 4);
    final_esp[0] = 0xDEADBEEF; // dummy ret
    final_esp[1] = argc;
    final_esp[2] = uargv_array_addr;

    // Update saved ESP in IRET frame on kernel stack
    u32* kstack = (u32*)new_proc->esp;
    kstack[14] = (u32)final_esp;

    // BLOCK the parent (shell) until the child is done
    if (parent) parent->state = TASK_WAITING;
}
