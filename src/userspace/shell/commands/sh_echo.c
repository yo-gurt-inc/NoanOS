#include "shell/noan.h"
#include "shell/commands.h"

int sh_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '>') break;
        noan_print(argv[i]);
        if (i < argc - 1) noan_putchar(' ');
    }
    noan_putchar('\n');
    return 0;
}
