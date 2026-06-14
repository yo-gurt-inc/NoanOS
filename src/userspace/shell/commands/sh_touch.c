#include "shell/noan.h"
#include "shell/commands.h"

int sh_touch(int argc, char** argv) {
    if (argc < 2) { noan_print("Usage: touch <name>\n"); return 1; }
    noan_touch(argv[1]);
    return 0;
}
