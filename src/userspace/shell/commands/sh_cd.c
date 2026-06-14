#include "shell/noan.h"
#include "shell/commands.h"

int sh_cd(int argc, char** argv) {
    if (argc < 2) { noan_print("Usage: cd <path>\n"); return 1; }
    noan_cd(argv[1]);
    return 0;
}
