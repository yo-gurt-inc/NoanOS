#include "shell/noan.h"
#include "shell/commands.h"

int sh_rm(int argc, char** argv) {
    if (argc < 2) { noan_print("Usage: rm <name>\n"); return 1; }
    noan_rm(argv[1], 0);
    return 0;
}
