#include "shell/noan.h"
#include "shell/commands.h"

int sh_shutdown(int argc, char** argv) { (void)argc; (void)argv; noan_shutdown(); return 0; }
