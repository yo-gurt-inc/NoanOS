#include "shell/noan.h"
#include "shell/commands.h"

extern shell_command_t commands[];
extern int num_commands;

int sh_help(int argc, char** argv) {
    (void)argc; (void)argv;
    noan_print("Available commands:\n");
    for (int i = 0; i < num_commands; i++) {
        noan_print("  ");
        noan_print(commands[i].name);
        noan_print(" - ");
        noan_print(commands[i].help);
        noan_putchar('\n');
    }
    return 0;
}
