/* Test runner for automated testing (make test).
 *
 * Runs every executable under /bin one at a time and reports progress lines
 * to the screen (mirrored to the serial log by the kernel in TEST mode):
 *   [RUNNER-START]  -> before the sweep
 *   [RUN] <name>    -> before spawning a binary
 *   [DONE] <name>   -> after it terminated (or " ERROR" when it failed to load)
 *   [SKIP] <name>   -> binaries that must not be spawned
 *   [RUNNER-DONE]   -> sweep finished
 *
 * The kernel loads this binary at 0x800000 (like the shell) so that child
 * processes loading at 0xA00000 cannot overwrite it while it is blocked
 * waiting for them.
 *
 * Two kinds of binaries exist under /bin:
 *   - NOAN flat binaries (shell, cat, echo, ls, ...):   run via SYS_EXEC
 *   - static musl ELF binaries (hello, malloc, test, ...): SYS_EXEC_ELF
 * Both spawn a child task and block until it terminates.
 */

#include "core/types.h"
#include "cpu/syscall.h"

static void print(const char* s) {
    _syscall1(SYS_PRINT, (u32)s);
}

static int dir_entry(const char* path, u32 index, char* name_out) {
    return _syscall3(SYS_READDIR, (u32)path, index, (u32)name_out);
}

static int read_file(const char* path, char* buf, u32 len) {
    return _syscall3(SYS_READ_FILE, (u32)path, (u32)buf, len);
}

static char lower(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
    return c;
}

static int streq(const char* a, const char* b) {
    while (*a && *b) {
        if (lower(*a) != lower(*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

static void print_num(u32 v) {
    char b[12];
    int i = 11;
    b[i--] = '\0';
    if (v == 0) b[i--] = '0';
    while (v) { b[i--] = '0' + (v % 10); v /= 10; }
    print(&b[i + 1]);
}

/* Binaries that must not be spawned by the sweep: the shell and this runner
 * are interactive (they would block forever on the keyboard), and cpptest's
 * source is disabled so nothing maintains it. */
static const char* const SKIP_NAMES[] = { "shell", "runtests", "cpptest" };

static int is_skipped(const char* name) {
    for (u32 i = 0; i < sizeof(SKIP_NAMES) / sizeof(SKIP_NAMES[0]); i++) {
        if (streq(name, SKIP_NAMES[i])) return 1;
    }
    return 0;
}

void _start(int argc, char** argv) {
    (void)argc;
    (void)argv;

    print("[RUNNER-START]\n");

    int ran = 0;
    int failed = 0;
    int skipped = 0;
    char name[256];
    char path[256 + 5];
    char hdr[4];

    for (u32 i = 0; i < 4096; i++) {
        int r = dir_entry("/bin", i, name);
        if (r <= 0) break; /* end of directory (0) or error (-1) */

        if (is_skipped(name)) {
            print("[SKIP] ");
            print(name);
            print("\n");
            skipped++;
            continue;
        }

        /* Classify by magic: ELF vs NOAN */
        int k = 0;
        while (name[k] && k < 63) k++;
        path[0] = '/'; path[1] = 'b'; path[2] = 'i'; path[3] = 'n'; path[4] = '/';
        for (int j = 0; j < k; j++) path[5 + j] = name[j];
        path[5 + k] = '\0';

        int is_elf = 0;
        if (read_file(path, hdr, 4) >= 4) {
            is_elf = (hdr[0] == 0x7F && hdr[1] == 'E' && hdr[2] == 'L' && hdr[3] == 'F');
        }

        print("[RUN] ");
        print(name);
        print("\n");

        int ret;
        if (is_elf) {
            ret = _syscall1(SYS_EXEC_ELF, (u32)path);
        } else {
            ret = _syscall1(SYS_EXEC, (u32)name);
        }

        print("[DONE] ");
        print(name);
        if (ret < 0) {
            print(" ERROR\n");
            failed++;
        } else {
            print("\n");
            ran++;
        }
    }

    print("[RUNNER-DONE]\n");
    print("[SUMMARY] ran=");
    print_num(ran);
    print(" failed=");
    print_num(failed);
    print(" skipped=");
    print_num(skipped);
    print("\n");
    _syscall0(SYS_EXIT);
}
