#include "cpu/syscall.h"
#include "cpu/gdt.h"
#include "io/kprint.h"
#include "io/keyboard.h"
#include "io/serial.h"
#include "core/malloc.h"
#include "cpu/task.h"
#include "storage/fat32.h"
#include "storage/elf.h"
#include "io/terminal.h"
#include "system/power.h"
#include "core/panic.h"
#include "system/rtc.h"
#include "storage/ata.h"
#include "system/timer.h"
#include "storage/noan.h"
#include "core/initrd.h"

extern void syscall_stub(void);

/* -------------------------------------------------------------------------
 * Linux i386 syscall numbers (int 0x80 ABI)
 * musl statically-linked binaries use these numbers.
 * ------------------------------------------------------------------------- */
#define LINUX_SYS_EXIT      1
#define LINUX_SYS_FORK      2
#define LINUX_SYS_READ      3
#define LINUX_SYS_WRITE     4
#define LINUX_SYS_OPEN      5
#define LINUX_SYS_CLOSE     6
#define LINUX_SYS_WAITPID   7
#define LINUX_SYS_UNLINK   10
#define LINUX_SYS_EXECVE   11
#define LINUX_SYS_CHDIR    12
#define LINUX_SYS_LSEEK    19
#define LINUX_SYS_GETPID   20
#define LINUX_SYS_ACCESS   33
#define LINUX_SYS_RENAME   38
#define LINUX_SYS_BRK      45
#define LINUX_SYS_IOCTL    54
#define LINUX_SYS_DUP2     63
#define LINUX_SYS_MMAP     90
#define LINUX_SYS_MUNMAP   91
#define LINUX_SYS_CLONE   120
#define LINUX_SYS_UNAME   122
#define LINUX_SYS_LLSEEK  140
#define LINUX_SYS_GETCWD  183
#define LINUX_SYS_FSTAT64 197
#define LINUX_SYS_STAT64  195
#define LINUX_SYS_MMAP2   192
#define LINUX_SYS_GETUID  199
#define LINUX_SYS_GETGID  200
#define LINUX_SYS_GETEUID 201
#define LINUX_SYS_GETEGID 202
#define LINUX_SYS_GETDENTS64 220
#define LINUX_SYS_FCNTL64 221
#define LINUX_SYS_SET_TLS 243

/* Minimalist utsname for uname() */
typedef struct {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
} linux_utsname_t;

static void strcpy_n(char* dst, const char* src, int n) {
    int i = 0;
    while (i < n - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

/* Write `len` bytes from `buf` to terminal */
static void terminal_write(const char* buf, u32 len) {
    for (u32 i = 0; i < len; i++)
        terminal_putchar(buf[i]);
}

/* Open a FAT32 file and allocate an fd slot in the current process.
   Returns fd >= 3 on success, -1 on error. */
static int proc_open(const char* path) {
    process_t* proc = get_current_process();
    if (!proc) return -1;

    fat32_dir_entry_t entry;
    extern int _fat32_find_entry(const char*, fat32_dir_entry_t*);
    if (!_fat32_find_entry(path, &entry)) return -1;

    for (int fd = 3; fd < MAX_FDS; fd++) {
        if (proc->fds[fd].kind == FD_FREE) {
            proc->fds[fd].kind       = FD_FILE;
            proc->fds[fd].fat_cluster = ((u32)entry.cluster_hi << 16) | entry.cluster_lo;
            proc->fds[fd].offset     = 0;
            proc->fds[fd].size       = entry.file_size;
            return fd;
        }
    }
    return -1; /* EMFILE */
}

/* Read from an fd into buf, up to count bytes. Returns bytes read. */
static int proc_read_fd(int fd, char* buf, u32 count) {
    process_t* proc = get_current_process();
    if (!proc || fd < 0 || fd >= MAX_FDS) return -1;

    fd_entry_t* f = &proc->fds[fd];

    if (f->kind == FD_STDIN) {
        int c = keyboard_getchar();
        if (c == 0) return 0;
        buf[0] = (char)c;
        return 1;
    }

    if (f->kind == FD_FILE) {
        u32 remaining = f->size - f->offset;
        if (count > remaining) count = remaining;
        if (count == 0) return 0;

        extern u32 _fat32_cluster_to_lba(u32);
        extern u32 _fat32_get_fat_entry(u32);
        extern ata_drive_t* _fat32_get_current_drive();
        extern fat32_bpb_t* _fat32_get_bpb();

        fat32_bpb_t* bpb = _fat32_get_bpb();
        ata_drive_t* drive = _fat32_get_current_drive();
        u32 cluster_size = bpb->sectors_per_cluster * 512;
        u32 cluster = f->fat_cluster;
        u32 skip = f->offset;

        while (skip >= cluster_size && cluster < 0x0FFFFFF8) {
            skip -= cluster_size;
            cluster = _fat32_get_fat_entry(cluster);
        }

        u32 total = 0;
        u8 tmp[512];
        while (total < count && cluster < 0x0FFFFFF8) {
            u32 lba = _fat32_cluster_to_lba(cluster);
            for (u32 s = 0; s < bpb->sectors_per_cluster && total < count; s++) {
                ata_read_sectors(drive, lba + s, 1, (u16*)tmp);
                u32 start = (skip > 0 && s == 0) ? skip : 0;
                for (u32 i = start; i < 512 && total < count; i++)
                    buf[total++] = tmp[i];
                skip = 0;
            }
            cluster = _fat32_get_fat_entry(cluster);
        }
        f->offset += total;
        return (int)total;
    }

    return -1;
}

/* -------------------------------------------------------------------------
 * Linux syscall dispatcher — called for any num >= 1 that matches Linux ABI
 * Returns 1 if handled, 0 if not a Linux syscall we know.
 * ------------------------------------------------------------------------- */
static int linux_syscall(struct registers* regs, u32 esp) {
    u32 num  = regs->eax;
    u32 arg1 = regs->ebx;
    u32 arg2 = regs->ecx;
    u32 arg3 = regs->edx;

    serial_puts("[syscall "); serial_dec(num); serial_puts("]\n");

    switch (num) {
        case LINUX_SYS_EXIT: {
            process_t* cur = get_current_process();
            if (cur) {
                cur->state = TASK_TERMINATED;
                if (cur->parent_id > 0) {
                    process_t* p = get_process_list();
                    process_t* start = p;
                    do {
                        if (p->id == cur->parent_id && p->state == TASK_WAITING)
                            p->state = TASK_READY;
                        p = p->next;
                    } while (p != start);
                }
            }
            regs->eax = 0;
            return 1;
        }

        case 252: /* exit_group — same as exit for single-threaded */
            {
                process_t* cur = get_current_process();
                if (cur) {
                    cur->state = TASK_TERMINATED;
                    if (cur->parent_id > 0) {
                        process_t* p = get_process_list();
                        process_t* start = p;
                        do {
                            if (p->id == cur->parent_id && p->state == TASK_WAITING) {
                                serial_puts("[waking parent pid="); serial_dec(p->id); serial_puts("]\n");
                                p->state = TASK_READY;
                            }
                            p = p->next;
                        } while (p != start);
                    }
                }
                regs->eax = 0;
                return 1;
            }

        case LINUX_SYS_WRITE: {
            /* write(fd, buf, count) */
            u32 fd    = arg1;
            const char* buf = (const char*)arg2;
            u32 count = arg3;
            serial_puts("[write fd="); serial_dec(fd);
            serial_puts(" count="); serial_dec(count);
            serial_puts(" buf="); serial_hex(arg2); serial_puts("]\n");
            
            /* Validate buffer pointer */
            if (!buf && count > 0) {
                regs->eax = (u32)-14; /* EFAULT */
                return 1;
            }
            
            if (fd == 1 || fd == 2) {
                terminal_write(buf, count);
                regs->eax = count;
            } else {
                regs->eax = (u32)-9; /* EBADF */
            }
            return 1;
        }

        case 146: { /* writev(fd, iov, iovcnt) */
            u32 fd = arg1;
            if (fd != 1 && fd != 2) { regs->eax = (u32)-9; return 1; }
            u32* iov   = (u32*)arg2;
            u32 iovcnt = arg3;
            u32 total  = 0;
            for (u32 i = 0; i < iovcnt; i++) {
                const char* base = (const char*)iov[i*2];
                u32 len          = iov[i*2+1];
                if (base && len) { terminal_write(base, len); total += len; }
            }
            regs->eax = total;
            return 1;
        }

        case LINUX_SYS_READ: {
            int r = proc_read_fd((int)arg1, (char*)arg2, arg3);
            regs->eax = (u32)r;
            return 1;
        }

        case LINUX_SYS_OPEN: {
            int fd = proc_open((const char*)arg1);
            regs->eax = (fd >= 0) ? (u32)fd : (u32)-2; /* ENOENT */
            return 1;
        }

        case LINUX_SYS_CLOSE: {
            process_t* proc = get_current_process();
            int fd = (int)arg1;
            if (proc && fd >= 3 && fd < MAX_FDS)
                proc->fds[fd].kind = FD_FREE;
            regs->eax = 0;
            return 1;
        }

        case LINUX_SYS_DUP2: {
            int oldfd = (int)arg1;
            int newfd = (int)arg2;
            process_t* proc = get_current_process();
            if (!proc || oldfd < 0 || oldfd >= MAX_FDS || newfd < 0 || newfd >= MAX_FDS) {
                regs->eax = (u32)-9; return 1;
            }
            if (oldfd == newfd) { regs->eax = newfd; return 1; }
            
            // Close newfd if open
            if (proc->fds[newfd].kind != FD_FREE) proc->fds[newfd].kind = FD_FREE;
            
            // Copy oldfd to newfd
            proc->fds[newfd] = proc->fds[oldfd];
            regs->eax = newfd;
            return 1;
        }

        case LINUX_SYS_UNLINK: {
            const char* path = (const char*)arg1;
            fat32_rm(path, 0); // flags=0 for file
            regs->eax = 0; // assume success
            return 1;
        }

        case LINUX_SYS_RENAME: {
            // For now just return success - full implementation needs FAT32 rename
            regs->eax = 0;
            return 1;
        }

        case LINUX_SYS_FORK: {
            /* fork not fully supported yet - return ENOSYS */
            regs->eax = (u32)-38; /* ENOSYS */
            return 1;
        }

        case LINUX_SYS_WAITPID: {
            int pid = (int)arg1;
            int* status = (int*)arg2;
            int options = (int)arg3;
            
            process_t* cur = get_current_process();
            if (!cur) { regs->eax = (u32)-1; return 1; }

wait_retry:
            // Find terminated child
            process_t* p = get_process_list();
            process_t* start = p;
            process_t* found = NULL;
            
            do {
                if (p->parent_id == cur->id && p->state == TASK_TERMINATED) {
                    if (pid == -1 || p->id == pid) {
                        found = p;
                        break;
                    }
                }
                p = p->next;
            } while (p != start);

            if (found) {
                if (status) *status = 0; // exit code 0
                regs->eax = (u32)found->id;
                return 1;
            } else {
                // No terminated child yet
                if (options & 1) { // WNOHANG
                    regs->eax = 0;
                    return 1;
                } else {
                    // Block and switch to another process
                    cur->state = TASK_WAITING;
                    cur->esp = esp;
                    u32 new_esp = task_switch(esp);
                    // When we wake up, check again
                    if (cur->state == TASK_READY) {
                        cur->state = TASK_RUNNING;
                        goto wait_retry;
                    }
                    return new_esp;
                }
            }
        }

        case LINUX_SYS_EXECVE: {
            const char* path = (const char*)arg1;
            char** argv = (char**)arg2;
            char** envp = (char**)arg3;
            
            int ret = task_exec(path, argv, envp);
            if (ret < 0) {
                regs->eax = (u32)-2; /* ENOENT */
                return 1;
            }
            // Success - return new ESP to start executing the new program
            process_t* proc = get_current_process();
            return proc->esp;
        }

        case LINUX_SYS_CHDIR: {
            const char* path = (const char*)arg1;
            process_t* proc = get_current_process();
            if (!proc) { regs->eax = (u32)-1; return 1; }
            
            // Simple implementation - just copy the path
            int i = 0;
            while (path[i] && i < 255) {
                proc->cwd[i] = path[i];
                i++;
            }
            proc->cwd[i] = '\0';
            regs->eax = 0;
            return 1;
        }

        case LINUX_SYS_GETCWD: {
            char* buf = (char*)arg1;
            u32 size = arg2;
            process_t* proc = get_current_process();
            if (!proc || !buf) { regs->eax = (u32)-14; return 1; }
            
            u32 len = 0;
            while (proc->cwd[len]) len++;
            if (len + 1 > size) { regs->eax = (u32)-34; return 1; } // ERANGE
            
            for (u32 i = 0; i <= len; i++) buf[i] = proc->cwd[i];
            regs->eax = (u32)buf;
            return 1;
        }

        case LINUX_SYS_LSEEK: {
            process_t* proc = get_current_process();
            int fd = (int)arg1;
            u32 offset = arg2;
            int whence = (int)arg3;
            serial_puts("[lseek fd="); serial_dec(fd); serial_puts("]\n");
            if (!proc || fd < 0 || fd >= MAX_FDS) { 
                serial_puts("[lseek: bad fd]\n");
                regs->eax = (u32)-9; return 1; 
            }
            fd_entry_t* f = &proc->fds[fd];
            serial_puts("[lseek kind="); serial_dec(f->kind); serial_puts("]\n");
            if (f->kind != FD_FILE) { 
                serial_puts("[lseek: not a file]\n");
                regs->eax = (u32)-9; return 1; 
            }
            if (whence == 0) f->offset = offset;
            else if (whence == 1) f->offset += offset;
            else if (whence == 2) f->offset = f->size + offset;
            if (f->offset > f->size) f->offset = f->size;
            regs->eax = f->offset;
            return 1;
        }

        case LINUX_SYS_LLSEEK: {
            process_t* proc = get_current_process();
            int fd = (int)arg1;
            u32 offset = arg3;
            int whence = regs->esi;
            u32* result = (u32*)regs->edi;
            if (!proc || fd < 0 || fd >= MAX_FDS) { regs->eax = (u32)-9; return 1; }
            fd_entry_t* f = &proc->fds[fd];
            if (f->kind != FD_FILE) { regs->eax = (u32)-9; return 1; }
            if (whence == 0) f->offset = offset;
            else if (whence == 1) f->offset += offset;
            else if (whence == 2) f->offset = f->size + offset;
            if (f->offset > f->size) f->offset = f->size;
            if (result) { result[0] = f->offset; result[1] = 0; }
            regs->eax = 0;
            return 1;
        }

        case LINUX_SYS_GETPID: {
            process_t* proc = get_current_process();
            regs->eax = proc ? (u32)proc->id : 1;
            return 1;
        }

        case LINUX_SYS_ACCESS: {
            const char* path = (const char*)arg1;
            // int mode = (int)arg2; // F_OK=0, R_OK=4, W_OK=2, X_OK=1
            fat32_dir_entry_t entry;
            if (_fat32_find_entry(path, &entry)) {
                regs->eax = 0; // file exists
            } else {
                regs->eax = (u32)-2; // ENOENT
            }
            return 1;
        }

        case LINUX_SYS_BRK: {
            /* brk(addr): if addr==0 return current brk.
               If addr > current brk, extend (identity mapped so just update).
               Returns new brk on success. */
            process_t* proc = get_current_process();
            if (!proc) { regs->eax = (u32)-12; return 1; }
            u32 new_brk = arg1;
            if (new_brk == 0) {
                regs->eax = proc->brk_end;
            } else if (new_brk >= proc->brk_end) {
                /* Zero the new pages (they're identity-mapped and writable) */
                u8* start = (u8*)proc->brk_end;
                u8* end   = (u8*)new_brk;
                while (start < end) *start++ = 0;
                proc->brk_end = new_brk;
                regs->eax = proc->brk_end;
            } else {
                /* Shrink: just update */
                proc->brk_end = new_brk;
                regs->eax = proc->brk_end;
            }
            return 1;
        }

        case LINUX_SYS_MMAP:
        case LINUX_SYS_MMAP2: {
            /* Minimal mmap: anonymous fixed allocation via brk extension.
               musl uses mmap for large allocations. We only handle MAP_ANONYMOUS. */
            process_t* proc = get_current_process();
            /* arg4=flags would be in esi; we assume anonymous */
            u32 len = arg2;
            if (!proc || len == 0) { regs->eax = (u32)-12; return 1; }
            /* Round up to 4KB */
            len = (len + 0xFFF) & ~0xFFFu;
            u32 addr = proc->brk_end;
            /* Zero the region */
            u8* p = (u8*)addr;
            for (u32 i = 0; i < len; i++) p[i] = 0;
            proc->brk_end = addr + len;
            regs->eax = addr;
            return 1;
        }

        case LINUX_SYS_MUNMAP:
            regs->eax = 0; /* pretend success */
            return 1;

        case LINUX_SYS_UNAME: {
            linux_utsname_t* u = (linux_utsname_t*)arg1;
            if (u) {
                strcpy_n(u->sysname,    "NoanOS",   65);
                strcpy_n(u->nodename,   "noan",     65);
                strcpy_n(u->release,    "1.0.0",    65);
                strcpy_n(u->version,    "#1",       65);
                strcpy_n(u->machine,    "i686",     65);
                strcpy_n(u->domainname, "(none)",   65);
            }
            regs->eax = 0;
            return 1;
        }

        case LINUX_SYS_FSTAT64:
            /* Return a zeroed struct — good enough for musl startup checks */
            if (arg2) {
                u8* s = (u8*)arg2;
                for (int i = 0; i < 96; i++) s[i] = 0;
            }
            regs->eax = 0;
            return 1;

        case LINUX_SYS_STAT64:
            /* stat64(path, struct stat64*) - return zeroed struct */
            if (arg2) {
                u8* s = (u8*)arg2;
                for (int i = 0; i < 96; i++) s[i] = 0;
            }
            regs->eax = 0;
            return 1;

        case LINUX_SYS_IOCTL:
            regs->eax = 0;
            return 1;

        case LINUX_SYS_GETUID:
        case LINUX_SYS_GETGID:
        case LINUX_SYS_GETEUID:
        case LINUX_SYS_GETEGID:
            regs->eax = 0; /* root */
            return 1;

        case LINUX_SYS_SET_TLS: {
            /* set_thread_area(struct user_desc*) — install %gs TLS descriptor.
             * user_desc layout: entry_number(u32), base_addr(u32), limit(u32), flags...
             * We fix entry_number=6 (selector 0x33) and set GDT[6].base = base_addr. */
            u32* udesc = (u32*)arg1;
            if (udesc) {
                u32 base = udesc[1]; /* base_addr is second field */
                udesc[0] = 6;       /* tell musl we used entry 6 */
                gdt_set_tls(base);
            }
            regs->eax = 0;
            return 1;
        }

        case LINUX_SYS_GETDENTS64: {
            int fd = (int)arg1;
            u8* dirp = (u8*)arg2;
            u32 count = arg3;
            
            process_t* proc = get_current_process();
            if (!proc || fd < 0 || fd >= MAX_FDS) { regs->eax = (u32)-9; return 1; }
            
            fd_entry_t* f = &proc->fds[fd];
            if (f->kind != FD_FILE) { regs->eax = (u32)-9; return 1; }
            
            // Read FAT32 directory entries and convert to linux_dirent64
            ata_drive_t* drive = _fat32_get_current_drive();
            fat32_bpb_t* bpb = _fat32_get_bpb();
            if (!drive || !bpb) { regs->eax = (u32)-9; return 1; }
            
            u32 cluster = f->fat_cluster;
            u32 entry_idx = f->offset; // track which entry we're at
            u32 bytes_written = 0;
            
            u8* buf = (u8*)kmalloc(bpb->sectors_per_cluster * 512);
            if (!buf) { regs->eax = (u32)-12; return 1; }
            
            while (cluster >= 2 && cluster < 0x0FFFFFF8 && bytes_written < count) {
                ata_read_sectors(drive, _fat32_cluster_to_lba(cluster), bpb->sectors_per_cluster, (u16*)buf);
                fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buf;
                int max_entries = (bpb->sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);
                
                for (int i = 0; i < max_entries && bytes_written < count; i++) {
                    if (entries[i].name[0] == 0x00) goto done;
                    if (entries[i].name[0] == 0xE5) continue;
                    if (entries[i].attr == FAT_ATTR_LFN) continue;
                    if (entry_idx > 0) { entry_idx--; continue; }
                    
                    // linux_dirent64: u64 d_ino, i64 d_off, u16 d_reclen, u8 d_type, char d_name[]
                    char name[256];
                    int nlen = 0;
                    for (int j = 0; j < 8 && entries[i].name[j] != ' '; j++) name[nlen++] = entries[i].name[j];
                    if (entries[i].name[8] != ' ') {
                        name[nlen++] = '.';
                        for (int j = 8; j < 11 && entries[i].name[j] != ' '; j++) name[nlen++] = entries[i].name[j];
                    }
                    name[nlen] = 0;
                    
                    u16 reclen = 19 + nlen + 1; // align to 8 bytes
                    reclen = (reclen + 7) & ~7;
                    if (bytes_written + reclen > count) goto done;
                    
                    u64 ino = (u64)i + 1;
                    s64 off = (s64)(f->offset + 1);
                    u8 type = (entries[i].attr & FAT_ATTR_DIRECTORY) ? 4 : 8; // DT_DIR=4, DT_REG=8
                    
                    *((u64*)(dirp + bytes_written)) = ino;
                    *((s64*)(dirp + bytes_written + 8)) = off;
                    *((u16*)(dirp + bytes_written + 16)) = reclen;
                    *((u8*)(dirp + bytes_written + 18)) = type;
                    for (int k = 0; k <= nlen; k++) dirp[bytes_written + 19 + k] = name[k];
                    
                    bytes_written += reclen;
                    f->offset++;
                }
                cluster = _fat32_get_fat_entry(cluster);
            }
done:
            kfree(buf);
            regs->eax = bytes_written;
            return 1;
        }

        case LINUX_SYS_FCNTL64:
            regs->eax = 0;
            return 1;

        case LINUX_SYS_CLONE:
            /* fork/thread — return ENOSYS for now */
            regs->eax = (u32)-38;
            return 1;

        case 258: /* set_tid_address */
            regs->eax = get_current_process() ? (u32)get_current_process()->id : 1;
            return 1;

        case 67:  /* sigaction */
        case 126: /* sigprocmask */
        case 119: /* sigreturn */
        case 175: /* rt_sigaction */
        case 174: /* rt_sigprocmask */
            regs->eax = 0;
            return 1;

        default:
            {
                process_t* cur2 = get_current_process();
                if (cur2 && cur2->is_elf) {
                    serial_puts("[elf unknown syscall "); serial_dec(num); serial_puts("]\n");
                    regs->eax = (u32)-38; /* ENOSYS */
                    return 1;
                }
            }
            return 0; /* not a Linux syscall we handle */
    }
}

u32 syscall_handler(u32 esp) {
    struct registers* regs = (struct registers*)esp;
    u32 num = regs->eax;
    u32 arg1 = regs->ebx;
    u32 arg2 = regs->ecx;
    u32 arg3 = regs->edx;

    int ret = 0;

    /* Try Linux ABI only for ELF processes */
    process_t* cur = get_current_process();
    if (cur && cur->is_elf) {
        u32 result = linux_syscall(regs, esp);
        if (result) {
            /* result could be:
             *  1 = handled, no context switch needed
             *  or a new ESP value from task_switch
             * If result > 1, it's a new ESP, so use it.
             * Otherwise use the original esp. */
            if (num == LINUX_SYS_EXIT || num == 252)
                return task_switch(esp);
            if (result > 1)
                return result;
            return esp;
        }
    }

    switch (num) {
        case SYS_EXIT:
            process_t* current = get_current_process();
            if (current) {
                current->state = TASK_TERMINATED;
                if (current->parent_id > 0) {
                    process_t* p = get_process_list();
                    process_t* start = p;
                    do {
                        if (p->id == current->parent_id) {
                            if (p->state == TASK_WAITING) p->state = TASK_READY;
                            break;
                        }
                        p = p->next;
                    } while (p != start);
                }
            }
            return task_switch(esp);

        case SYS_EXEC: {
            u32 entry = noan_load((const char*)arg1);
            if (entry) {
                keyboard_set_enabled(1);
                noan_execute(entry, (const char*)arg1);
                keyboard_flush();
                ret = 0;
            } else {
                ret = -1;
            }
            break;
        }

        case SYS_EXEC_ELF: {
            process_t* proc = elf_load_file((const char*)arg1);
            if (proc) {
                /* Build Linux _start stack with auxv so musl __init_tls works:
                 *   argc=0, argv=NULL, envp=NULL, then auxv pairs, then AT_NULL
                 * auxv entries musl needs: AT_PAGESZ(6), AT_PHDR(3), AT_PHNUM(5), AT_PHENT(4)
                 * We pass safe zeros for PHDR/PHNUM/PHENT (no TLS segment in our binaries).
                 */
                u32* usp = (u32*)proc->ustack;
                /* auxv: AT_NULL terminator */
                *(--usp) = 0;          /* value */
                *(--usp) = 0;          /* AT_NULL type */
                /* AT_PAGESZ = 6 */
                *(--usp) = 4096;
                *(--usp) = 6;
                /* AT_PHENT = 4 */
                *(--usp) = 32;         /* sizeof(elf_ph_t) */
                *(--usp) = 4;
                /* AT_PHNUM = 5 */
                *(--usp) = 0;
                *(--usp) = 5;
                /* AT_PHDR = 3 */
                *(--usp) = 0;
                *(--usp) = 3;
                /* envp NULL */
                *(--usp) = 0;
                /* argv NULL */
                *(--usp) = 0;
                /* argc */
                *(--usp) = 0;

                u32* ksp = (u32*)proc->esp;
                ksp[14] = (u32)usp;

                process_t* parent = get_current_process();
                if (parent) {
                    proc->parent_id = parent->id;
                    parent->esp = esp;
                    parent->state = TASK_WAITING;
                }
                ret = proc->id;
            } else {
                ret = -1;
            }
            break;
        }

        case SYS_PRINT:
            kprint((const char*)arg1);
            break;
        case SYS_READ:
            ret = keyboard_getchar();
            if (ret == 0) {
                process_t* current = get_current_process();
                if (current) current->state = TASK_SLEEPING;
                return task_switch(esp);
            }
            break;
        case SYS_READ_NB:
            ret = keyboard_getchar();
            break;
        case SYS_FLUSH_KB:
            keyboard_flush();
            break;
        case SYS_KB_ENABLE:
            keyboard_set_enabled((int)arg1);
            break;
        case SYS_MALLOC:
            ret = (u32)kmalloc(arg1);
            break;
        case SYS_FREE:
            kfree((void*)arg1);
            break;
        case SYS_LS:
            fat32_ls();
            break;
        case SYS_CD:
            fat32_cd((const char*)arg1);
            break;
        case SYS_CAT:
            fat32_cat((const char*)arg1);
            break;
        case SYS_PUTCHAR:
            terminal_putchar((char)arg1);
            break;
        case SYS_CLEAR:
            kprint_init();
            break;
        case SYS_YIELD:
            return task_switch(esp);
        case SYS_REBOOT:
            reboot();
            break;
        case SYS_SHUTDOWN:
            shutdown();
            break;
        case SYS_MKDIR:
            fat32_mkdir((const char*)arg1);
            break;
        case SYS_RM:
            fat32_rm((const char*)arg1, (int)arg2);
            break;
        case SYS_TOUCH:
            fat32_touch((const char*)arg1);
            break;
        case SYS_ECHO_FILE:
            fat32_echo((const char*)arg1, (const char*)arg2, (int)arg3);
            break;
        case SYS_CP:
            fat32_copy((const char*)arg1, (const char*)arg2);
            break;
        case SYS_MV:
            fat32_move((const char*)arg1, (const char*)arg2);
            break;
        case SYS_READ_FILE:
            ret = fat32_read((const char*)arg1, (char*)arg2, arg3);
            break;
        case SYS_PANIC:
            panic((const char*)arg1);
            break;
        case SYS_GET_TIME:
            rtc_get_time((u8*)arg1, (u8*)arg2, (u8*)arg3);
            break;
        case SYS_GET_DATE:
            rtc_get_date((u8*)arg1, (u8*)arg2, (u16*)arg3);
            break;
        case SYS_MEM_INFO:
            get_malloc_info((malloc_info_t*)arg1);
            break;
        case SYS_LIST_DISKS:
            ata_list_drives();
            break;
        case SYS_STAT:
            fat32_stat((const char*)arg1);
            break;
        case SYS_GET_TICKS:
            ret = (int)timer_get_ticks();
            break;
        default:
            kprint("Unknown syscall: ");
            kprint_dec(num);
            kprint("\n");
            break;
    }

    regs->eax = (u32)ret;

    process_t* current = get_current_process();
    if (current && current->state == TASK_WAITING)
        return task_switch(esp);

    return esp;
}

void syscall_init(void) {
    kprint("[Setting up syscall gate 0x80]\n");
    idt_set_gate(0x80, (u32)syscall_stub, 0x08, 0xEE);
    kprint("[Syscall gate installed]\n");
}
