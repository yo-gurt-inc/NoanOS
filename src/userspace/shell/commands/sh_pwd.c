#include "shell/noan.h"
#include "shell/commands.h"

extern char current_path[256];

int sh_pwd(int argc, char** argv) {
    (void)argc; (void)argv;
    noan_print(current_path);
    noan_print("\n");
    return 0;
}
