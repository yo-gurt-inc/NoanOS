#include "shell/noan.h"
#include "shell/commands.h"

int sh_mkdir(int argc, char** argv) {
    if (argc < 2) { noan_print("Usage: mkdir <name>\n"); return 1; }
    noan_mkdir(argv[1]);
    return 0;
}
