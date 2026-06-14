#include "shell/noan.h"
#include "shell/commands.h"

int sh_reboot(int argc, char** argv) { (void)argc; (void)argv; noan_reboot(); return 0; }
