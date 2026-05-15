#include "cpu/syscall.h"
#include "core/types.h"

int sh_panic(int argc, char** argv) {
    if (argc < 2) {
        _syscall1(SYS_PANIC, (u32)"User requested kernel panic via shell.");
    } else {
        // Combine arguments into one string if needed, or just take the first one
        _syscall1(SYS_PANIC, (u32)argv[1]);
    }
    return 0; // Never reached
}
