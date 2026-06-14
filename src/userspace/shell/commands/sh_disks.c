#include "shell/noan.h"
#include "shell/commands.h"

int sh_disks(int argc, char** argv) { (void)argc; (void)argv; noan_list_disks(); return 0; }
