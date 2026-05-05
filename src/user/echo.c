#include "libnoan.h"

void _start(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        print(argv[i]);
        if (i < argc - 1) putchar(' ');
    }
    putchar('\n');
    putchar('h');
    exit();
}
