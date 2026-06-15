// fat32_path.c - Path/entry operations: 8.3 name conversion, find, create entries
#include "storage/fat32.h"
#include "core/malloc.h"

extern ata_drive_t* _fat32_get_current_drive(void);
extern fat32_bpb_t* _fat32_get_bpb(void);
extern u32          _fat32_get_current_dir_cluster(void);
extern u32          _fat32_cluster_to_lba(u32 cluster);
extern u32          _fat32_find_free_cluster(void);
extern void         _fat32_set_fat_entry(u32 cluster, u32 value);

void _fat32_name_to_83(const char* name, u8* dest) {
    for (int i = 0; i < 11; i++) dest[i] = ' ';
    int i = 0, j = 0;
    while (name[i] && name[i] != '.' && j < 8) {
        char c = name[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        dest[j++] = c;
    }
    if (name[i] == '.') {
        i++; j = 8;
        while (name[i] && j < 11) {
            char c = name[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            dest[j++] = c;
        }
    }
}

int _fat32_find_entry(const char* name, fat32_dir_entry_t* out_entry) {
    fat32_bpb_t* bpb = _fat32_get_bpb();
    u32 search_cluster = _fat32_get_current_dir_cluster();
    const char* remaining = name;

    if (name[0] == '/') { search_cluster = bpb->root_cluster; remaining = name + 1; }

    while (remaining && *remaining) {
        const char* next_slash = remaining;
        int component_len = 0;
        while (*next_slash && *next_slash != '/') { next_slash++; component_len++; }

        if (component_len == 0) {
            if (*next_slash == '/') remaining = next_slash + 1;
            else break;
            continue;
        }

        char component[64];
        for (int i = 0; i < component_len && i < 63; i++) component[i] = remaining[i];
        component[component_len] = '\0';

        u8 fat_name[11];
        _fat32_name_to_83(component, fat_name);

        u8* buf = (u8*)kmalloc(bpb->sectors_per_cluster * 512);
        if (!buf) return 0;
        ata_read_sectors(_fat32_get_current_drive(), _fat32_cluster_to_lba(search_cluster), bpb->sectors_per_cluster, (u16*)buf);
        fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buf;
        int max_entries = (bpb->sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);

        int found = 0;
        fat32_dir_entry_t found_entry;
        for (int i = 0; i < max_entries; i++) {
            if (entries[i].name[0] == 0x00) break;
            if (entries[i].name[0] == 0xE5) continue;
            int match = 1;
            for (int j = 0; j < 11; j++) if (entries[i].name[j] != fat_name[j]) { match = 0; break; }
            if (match) { found_entry = entries[i]; found = 1; break; }
        }
        kfree(buf);
        if (!found) return 0;
        if (*next_slash == '\0') { if (out_entry) *out_entry = found_entry; return 1; }
        if (!(found_entry.attr & FAT_ATTR_DIRECTORY)) return 0;
        search_cluster = ((u32)found_entry.cluster_hi << 16) | found_entry.cluster_lo;
        if (search_cluster == 0) search_cluster = bpb->root_cluster;
        remaining = next_slash + 1;
    }
    return 0;
}

void _fat32_create_entry(const char* name, u8 attr, u32 first_cluster, u32 size) {
    fat32_bpb_t* bpb = _fat32_get_bpb();
    u32 target_dir_cluster = _fat32_get_current_dir_cluster();
    const char* path = name;

    if (name[0] == '/') { target_dir_cluster = bpb->root_cluster; path = name + 1; }

    char component[64];
    while (*path) {
        int component_len = 0;
        while (path[component_len] && path[component_len] != '/') component_len++;
        if (component_len == 0) { if (*path == '/') path++; else break; continue; }

        for (int i = 0; i < component_len && i < 63; i++) component[i] = path[i];
        component[component_len] = '\0';

        u8 fat_name[11];
        _fat32_name_to_83(component, fat_name);
        u8* buf = (u8*)kmalloc(bpb->sectors_per_cluster * 512);
        if (!buf) return;
        ata_read_sectors(_fat32_get_current_drive(), _fat32_cluster_to_lba(target_dir_cluster), bpb->sectors_per_cluster, (u16*)buf);
        fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buf;
        int max_entries = (bpb->sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);

        if (path[component_len] == '\0') {
            // Final component — write the entry
            for (int i = 0; i < max_entries; i++) {
                if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                    for (int j = 0; j < 11; j++) entries[i].name[j] = fat_name[j];
                    entries[i].attr = attr;
                    entries[i].cluster_hi = (u16)(first_cluster >> 16);
                    entries[i].cluster_lo = (u16)(first_cluster & 0xFFFF);
                    entries[i].file_size = size;
                    ata_write_sectors(_fat32_get_current_drive(), _fat32_cluster_to_lba(target_dir_cluster), bpb->sectors_per_cluster, (u16*)buf);
                    break;
                }
            }
            kfree(buf);
            return;
        }

        // Intermediate component — navigate or create directory
        int found = 0;
        u32 next_cluster = 0;
        for (int i = 0; i < max_entries; i++) {
            if (entries[i].name[0] == 0x00) break;
            if (entries[i].name[0] == 0xE5) continue;
            int match = 1;
            for (int j = 0; j < 11; j++) if (entries[i].name[j] != fat_name[j]) { match = 0; break; }
            if (match && (entries[i].attr & FAT_ATTR_DIRECTORY)) {
                next_cluster = ((u32)entries[i].cluster_hi << 16) | entries[i].cluster_lo;
                if (next_cluster == 0) next_cluster = bpb->root_cluster;
                found = 1; break;
            }
        }

        if (found) {
            target_dir_cluster = next_cluster;
        } else {
            u32 new_cluster = _fat32_find_free_cluster();
            if (!new_cluster) { kfree(buf); return; }
            _fat32_set_fat_entry(new_cluster, 0x0FFFFFFF);
            for (int i = 0; i < max_entries; i++) {
                if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                    for (int j = 0; j < 11; j++) entries[i].name[j] = fat_name[j];
                    entries[i].attr = FAT_ATTR_DIRECTORY;
                    entries[i].cluster_hi = (u16)(new_cluster >> 16);
                    entries[i].cluster_lo = (u16)(new_cluster & 0xFFFF);
                    entries[i].file_size = 0;
                    ata_write_sectors(_fat32_get_current_drive(), _fat32_cluster_to_lba(target_dir_cluster), bpb->sectors_per_cluster, (u16*)buf);
                    break;
                }
            }
            // Init new dir with . and ..
            u8* dir_buf = (u8*)kmalloc(bpb->sectors_per_cluster * 512);
            if (dir_buf) {
                for (int i = 0; i < (int)(bpb->sectors_per_cluster * 512); i++) dir_buf[i] = 0;
                fat32_dir_entry_t* de = (fat32_dir_entry_t*)dir_buf;
                _fat32_name_to_83(".", de[0].name);  de[0].attr = FAT_ATTR_DIRECTORY;
                de[0].cluster_hi = (u16)(new_cluster >> 16); de[0].cluster_lo = (u16)(new_cluster & 0xFFFF);
                _fat32_name_to_83("..", de[1].name); de[1].attr = FAT_ATTR_DIRECTORY;
                de[1].cluster_hi = (u16)(target_dir_cluster >> 16); de[1].cluster_lo = (u16)(target_dir_cluster & 0xFFFF);
                ata_write_sectors(_fat32_get_current_drive(), _fat32_cluster_to_lba(new_cluster), bpb->sectors_per_cluster, (u16*)dir_buf);
                kfree(dir_buf);
            }
            target_dir_cluster = new_cluster;
        }
        path += component_len + 1;
        kfree(buf);
    }
}

int _fat32_find_full_path_file(const char* path, fat32_dir_entry_t* out_entry) {
    fat32_bpb_t* bpb = _fat32_get_bpb();
    u8 fat_name[11];
    for (int i = 0; i < 11; i++) fat_name[i] = ' ';
    int i = 0, j = 0;
    while (path[i] && j < 11) {
        char c = path[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        fat_name[j++] = c;
    }

    u8* buf = (u8*)kmalloc(bpb->sectors_per_cluster * 512);
    if (!buf) return 0;
    ata_read_sectors(_fat32_get_current_drive(), _fat32_cluster_to_lba(bpb->root_cluster), bpb->sectors_per_cluster, (u16*)buf);
    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buf;
    int max_entries = (bpb->sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);

    for (int k = 0; k < max_entries; k++) {
        if (entries[k].name[0] == 0x00) break;
        if (entries[k].name[0] == 0xE5) continue;
        int match = 1;
        for (int m = 0; m < 11; m++) if (entries[k].name[m] != fat_name[m]) { match = 0; break; }
        if (match) { if (out_entry) *out_entry = entries[k]; kfree(buf); return 1; }
    }
    kfree(buf);
    return 0;
}
