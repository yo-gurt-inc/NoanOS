#include "shell/noan.h"
#include "shell/commands.h"

int sh_cp(int argc, char** argv) {
    if (argc < 3) { noan_print("Usage: cp <src> <dest>\n"); return 1; }
    noan_cp(argv[1], argv[2]);
    return 0;
}
