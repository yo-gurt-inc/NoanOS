#ifndef NOAN_H
#define NOAN_H

#include "core/types.h"
#include "cpu/syscall.h"

/* Output */
static inline void noan_print(const char* s)   { _syscall1(SYS_PRINT,   (u32)s); }
static inline void noan_putchar(char c)         { _syscall1(SYS_PUTCHAR, (u32)c); }
static inline void noan_clear(void)             { _syscall0(SYS_CLEAR); }

/* Input */
static inline int  noan_getchar(void)           { return _syscall0(SYS_READ); }
static inline int  noan_getchar_nb(void)        { return _syscall0(SYS_READ_NB); }
static inline void noan_flush_kb(void)          { _syscall0(SYS_FLUSH_KB); }

/* Process */
static inline void noan_exit(void)              { _syscall0(SYS_EXIT); }
static inline void noan_yield(void)             { _syscall0(SYS_YIELD); }
static inline int  noan_exec(const char* path)  { return _syscall1(SYS_EXEC,     (u32)path); }
static inline int  noan_exec_elf(const char* path) { return _syscall1(SYS_EXEC_ELF, (u32)path); }
static inline void noan_panic(const char* msg)  { _syscall1(SYS_PANIC, (u32)msg); }

/* Memory */
static inline void* noan_malloc(u32 size)       { return (void*)_syscall1(SYS_MALLOC, size); }
static inline void  noan_free(void* ptr)        { _syscall1(SYS_FREE, (u32)ptr); }

/* Filesystem */
static inline void noan_ls(void)                             { _syscall0(SYS_LS); }
static inline void noan_cd(const char* path)                 { _syscall1(SYS_CD,    (u32)path); }
static inline void noan_cat(const char* path)                { _syscall1(SYS_CAT,   (u32)path); }
static inline void noan_mkdir(const char* path)              { _syscall1(SYS_MKDIR, (u32)path); }
static inline void noan_touch(const char* path)              { _syscall1(SYS_TOUCH, (u32)path); }
static inline void noan_stat(const char* path)               { _syscall1(SYS_STAT,  (u32)path); }
static inline void noan_rm(const char* path, int flags)      { _syscall2(SYS_RM,    (u32)path, (u32)flags); }
static inline void noan_cp(const char* src, const char* dst) { _syscall2(SYS_CP,    (u32)src,  (u32)dst); }
static inline void noan_mv(const char* src, const char* dst) { _syscall2(SYS_MV,    (u32)src,  (u32)dst); }
static inline void noan_write_file(const char* path, const char* data, int flags) {
    _syscall3(SYS_ECHO_FILE, (u32)path, (u32)data, (u32)flags);
}
static inline int  noan_read_file(const char* path, char* buf, u32 len) {
    return _syscall3(SYS_READ_FILE, (u32)path, (u32)buf, len);
}

/* System */
static inline void noan_reboot(void)            { _syscall0(SYS_REBOOT); }
static inline void noan_shutdown(void)          { _syscall0(SYS_SHUTDOWN); }
static inline void noan_list_disks(void)        { _syscall0(SYS_LIST_DISKS); }
static inline u32  noan_get_ticks(void)         { return (u32)_syscall0(SYS_GET_TICKS); }
static inline void noan_get_time(u8* h, u8* m, u8* s)           { _syscall3(SYS_GET_TIME, (u32)h, (u32)m, (u32)s); }
static inline void noan_get_date(u8* d, u8* mo, u16* yr)        { _syscall3(SYS_GET_DATE, (u32)d, (u32)mo, (u32)yr); }
static inline void noan_mem_info(void* info)    { _syscall1(SYS_MEM_INFO, (u32)info); }

#endif
