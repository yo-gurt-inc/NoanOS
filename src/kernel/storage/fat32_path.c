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

/* =========================================================================
 * VFAT long filename (LFN) support
 *
 * Directory layout for a long-named file:
 *   [LFN seq 0x40|N, chars 0-12] [LFN seq N-1, chars 13-25] ... [seq 1] [8.3 entry]
 * Each LFN entry stores 13 UCS-2 chars across bytes 1-10/14-25/28-31, attr
 * 0x0F at byte 11 and the 8.3 alias checksum at byte 13. Names are written
 * exactly as given (case preserved); lookups stay case-insensitive.
 * ========================================================================= */

#define LFN_CHARS_PER_ENTRY 13

static u8 lfn_checksum(const u8* short_name) {
    u8 sum = 0;
    for (int i = 0; i < 11; i++) {
        sum = (u8)(((sum & 1) ? 0x80 : 0) + (sum >> 1) + short_name[i]);
    }
    return sum;
}

/* Read UCS-2 char slot j (0..12) from an LFN entry given as raw bytes. */
static u16 lfn_char(const u8* e, int j) {
    static const int off[13] = { 1,3,5,7,9, 14,16,18,20,22,24, 28,30 };
    return (u16)(e[off[j]] | (e[off[j] + 1] << 8));
}

/* Write UCS-2 char slot j of an LFN entry; val 0x0000 terminates, 0xFFFF
 * is filler after the terminator. */
static void lfn_set_char(u8* e, int j, u16 val) {
    static const int off[13] = { 1,3,5,7,9, 14,16,18,20,22,24, 28,30 };
    e[off[j]]     = (u8)(val & 0xFF);
    e[off[j] + 1] = (u8)(val >> 8);
}

/* Reconstruct the readable 8.3 form of an alias: base + optional .ext. */
static void alias_readable(const u8* fat_name, char* out, int out_size) {
    int n = 0;
    for (int j = 0; j < 8 && fat_name[j] != ' '; j++) {
        if (n < out_size - 1) out[n++] = (char)fat_name[j];
    }
    if (fat_name[8] != ' ') {
        if (n < out_size - 1) out[n++] = '.';
        for (int j = 8; j < 11 && fat_name[j] != ' '; j++) {
            if (n < out_size - 1) out[n++] = (char)fat_name[j];
        }
    }
    out[n] = '\0';
}

static char lfn_lower(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
    return c;
}

int _fat32_entry_full_name(const fat32_dir_entry_t* dir_entries, int idx,
                           char* out, int out_size) {
    if (out_size <= 0) return 0;

    /* Walk the LFN run directly above the short entry: slot idx-t must be an
     * LFN entry with sequence t, the top one (t == n) carrying 0x40. */
    const u8* short_raw = (const u8*)&dir_entries[idx];
    u8 want_sum = lfn_checksum(short_raw);
    int n = 0;
    while (idx - (n + 1) >= 0) {
        const u8* e = (const u8*)&dir_entries[idx - (n + 1)];
        if (e[11] != FAT_ATTR_LFN) break;
        int seq = e[0] & 0x1F;
        int last = (e[0] & 0x40) != 0;
        if (seq != n + 1) break;
        if (e[13] != want_sum) break;
        if (last) { n++; break; }
        n++;
    }
    if (n == 0) {
        /* Plain 8.3 entry */
        alias_readable(short_raw, out, out_size);
        return 1;
    }

    /* Collect chunks: slot idx-t holds chars [(n-t)*13 ... ]; emit from the
     * top chunk (t == n) downwards. */
    int total = 0;
    int max_chars = n * LFN_CHARS_PER_ENTRY;
    for (int t = n; t >= 1 && total < max_chars; t--) {
        const u8* e = (const u8*)&dir_entries[idx - t];
        for (int j = 0; j < LFN_CHARS_PER_ENTRY && total < max_chars; j++) {
            u16 c = lfn_char(e, j);
            if (c == 0x0000 || c == 0xFFFF) { t = -1; break; } /* terminator */
            if (c < 0x80 && total < out_size - 1) out[total++] = (char)c;
        }
    }
    if (total == 0) {
        /* Empty or broken run: fall back to the alias */
        alias_readable(short_raw, out, out_size);
        return 1;
    }
    out[total] = '\0';
    return 1;
}

int _fat32_entry_matches(const fat32_dir_entry_t* dir_entries, int idx,
                         const char* component) {
    /* If the entry has a valid LFN run above it, compare the full name
     * case-insensitively. */
    const u8* short_raw = (const u8*)&dir_entries[idx];
    u8 want_sum = lfn_checksum(short_raw);
    int n = 0;
    while (idx - (n + 1) >= 0) {
        const u8* e = (const u8*)&dir_entries[idx - (n + 1)];
        if (e[11] != FAT_ATTR_LFN) break;
        int seq = e[0] & 0x1F;
        if (seq != n + 1 || e[13] != want_sum) break;
        n++;
        if (e[0] & 0x40) break;
    }

    if (n > 0) {
        char full[256];
        _fat32_entry_full_name(dir_entries, idx, full, sizeof(full));
        const char* a = full;
        const char* b = component;
        while (*a && *b) {
            if (lfn_lower(*a) != lfn_lower(*b)) return 0;
            a++; b++;
        }
        return *a == *b;
    }

    /* No LFN run: byte-exact comparison against the 8.3-converted name
     * (matches historical behavior, including the '.'/'..' aliases). */
    u8 fat_name[11];
    _fat32_name_to_83(component, fat_name);
    for (int j = 0; j < 11; j++) {
        if (short_raw[j] != fat_name[j]) return 0;
    }
    return 1;
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
            if (entries[i].attr == FAT_ATTR_LFN) continue;
            if (_fat32_entry_matches(entries, i, component)) {
                found_entry = entries[i]; found = 1; break;
            }
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

/* Write one directory entry (8.3 `fat_name`, LFN run added when `component`
 * is not exactly representable) into a free run of `entries`. Returns 1 on
 * success; the caller persists the cluster. Falls back to a bare 8.3 entry
 * if no contiguous free run for the LFN chain exists. */
static int dir_add_entry(fat32_dir_entry_t* entries, int max_entries,
                         const char* component, const u8* fat_name,
                         u8 attr, u32 first_cluster, u32 size) {
    int len = 0;
    while (component[len]) len++;

    /* Does the name need an LFN run? Add one whenever the lossless alias is
     * not byte-identical to the requested name (covers >8.3 names and mixed
     * case, so listings show the name exactly as created). */
    int k = 0;
    {
        char readable[80];
        alias_readable(fat_name, readable, sizeof(readable));
        const char* a = readable;
        const char* b = component;
        int same = 1;
        while (*a && *b) {
            if (*a != *b) { same = 0; break; }
            a++; b++;
        }
        if (*a != *b) same = 0;
        if (!same && len > 0) {
            k = (len + LFN_CHARS_PER_ENTRY - 1) / LFN_CHARS_PER_ENTRY;
        }
    }

    /* Find a run of k+1 consecutive free (0x00/0xE5) slots. */
    for (int i = 0; i + k < max_entries; i++) {
        int free_run = 1;
        for (int t = 0; t <= k; t++) {
            u8 first = entries[i + t].name[0];
            if (first != 0x00 && first != 0xE5) { free_run = 0; break; }
        }
        if (!free_run) continue;

        if (k > 0) {
            u8 sum = lfn_checksum(fat_name);
            /* chunk p (0-based, stored first = top) holds chars [13p..13p+12]
             * and carries sequence number k-p (0x40 marker on the top). */
            for (int p = 0; p < k; p++) {
                u8* e = (u8*)&entries[i + p];
                for (int b = 0; b < 32; b++) e[b] = 0;
                e[0]  = (u8)(((p == 0) ? 0x40 : 0) | (k - p));
                e[11] = FAT_ATTR_LFN;
                e[13] = sum;
                for (int j = 0; j < LFN_CHARS_PER_ENTRY; j++) {
                    int ci = p * LFN_CHARS_PER_ENTRY + j;
                    u16 v = 0xFFFF;
                    if (ci < len) v = (u16)(u8)component[ci];
                    else if (ci == len) v = 0x0000; /* terminator after the last char */
                    lfn_set_char(e, j, v);
                }
            }
        }

        int si = i + k; /* slot of the 8.3 entry, right after its LFN run */
        for (int j = 0; j < 11; j++) entries[si].name[j] = fat_name[j];
        entries[si].attr = attr;
        entries[si].cluster_hi = (u16)(first_cluster >> 16);
        entries[si].cluster_lo = (u16)(first_cluster & 0xFFFF);
        entries[si].file_size = size;
        return 1;
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
            if (dir_add_entry(entries, max_entries, component, fat_name,
                              attr, first_cluster, size)) {
                ata_write_sectors(_fat32_get_current_drive(), _fat32_cluster_to_lba(target_dir_cluster), bpb->sectors_per_cluster, (u16*)buf);
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
            if (entries[i].attr == FAT_ATTR_LFN) continue;
            if (!(entries[i].attr & FAT_ATTR_DIRECTORY)) continue;
            if (_fat32_entry_matches(entries, i, component)) {
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
            if (dir_add_entry(entries, max_entries, component, fat_name,
                              FAT_ATTR_DIRECTORY, new_cluster, 0)) {
                ata_write_sectors(_fat32_get_current_drive(), _fat32_cluster_to_lba(target_dir_cluster), bpb->sectors_per_cluster, (u16*)buf);
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
