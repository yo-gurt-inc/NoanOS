#include "shell/noan.h"
#include "shell/commands.h"

int sh_stat(int argc, char** argv) {
    if (argc < 2) { noan_print("Usage: stat <name>\n"); return 1; }
    noan_stat(argv[1]);
    return 0;
}
