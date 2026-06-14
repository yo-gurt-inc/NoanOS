#include "shell/noan.h"
#include "shell/commands.h"

int sh_panic(int argc, char** argv) {
    noan_panic(argc < 2 ? "User requested kernel panic via shell." : argv[1]);
    return 0;
}
