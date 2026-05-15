#include "cpu/syscall.h"
#include "shell/commands.h"

// Note: In this simple OS, we'll let the shell track current_path
// but we could also have a syscall if the kernel tracked it.
// For now, sh_pwd will be handled specifically in shell.c or via a global.
int sh_pwd(int argc, char** argv) {
    (void)argc; (void)argv;
    // This will be special-cased or we'll pass the path in
    return 0;
}
