#include "shell/noan.h"
#include "shell/commands.h"

int sh_ls(int argc, char** argv) {
    (void)argc; (void)argv;
    noan_ls();
    return 0;
}
