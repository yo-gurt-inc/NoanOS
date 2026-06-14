#include "shell/noan.h"
#include "shell/commands.h"

int sh_mv(int argc, char** argv) {
    if (argc < 3) { noan_print("Usage: mv <src> <dest>\n"); return 1; }
    noan_mv(argv[1], argv[2]);
    return 0;
}
