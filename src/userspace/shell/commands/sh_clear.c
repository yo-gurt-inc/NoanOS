#include "shell/noan.h"
#include "shell/commands.h"

int sh_clear(int argc, char** argv) { (void)argc; (void)argv; noan_clear(); return 0; }
