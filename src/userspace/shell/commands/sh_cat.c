#include "shell/noan.h"
#include "shell/commands.h"

int sh_cat(int argc, char** argv) {
    if (argc < 2) { noan_print("Usage: cat <name>\n"); return 1; }
    noan_cat(argv[1]);
    return 0;
}
