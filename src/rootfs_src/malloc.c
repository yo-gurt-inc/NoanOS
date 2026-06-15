#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    char *p = malloc(128);

    if (!p)
    {
        puts("malloc failed");
        return 1;
    }

    puts("malloc works");

    free(p);

    puts("free works");
    return 0;
}
