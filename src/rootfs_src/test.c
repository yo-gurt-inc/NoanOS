#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_PTRS 100
#define STRESS_ITERS 1000

int main(void)
{
    printf("=== NoanOS libc stress test ===\n");

    /* printf tests */
    printf("String: %s\n", "works");
    printf("Integer: %d\n", 12345);
    printf("Negative: %d\n", -42);
    printf("Character: %c\n", 'A');

    /* malloc/free stress */
    printf("Running malloc/free stress...\n");

    for (int i = 0; i < STRESS_ITERS; i++)
    {
        void *p = malloc(64);

        if (!p)
        {
            printf("FAIL: malloc failed at iteration %d\n", i);
            return 1;
        }

        free(p);
    }

    printf("PASS: malloc/free stress\n");

    /* multiple allocations */
    printf("Running multi-allocation test...\n");

    void *ptrs[NUM_PTRS];

    for (int i = 0; i < NUM_PTRS; i++)
    {
        ptrs[i] = malloc(i * 100 + 1);

        if (!ptrs[i])
        {
            printf("FAIL: allocation %d\n", i);
            return 1;
        }
    }

    for (int i = 0; i < NUM_PTRS; i++)
        free(ptrs[i]);

    printf("PASS: multi-allocation\n");

    /* memory corruption test */
    printf("Running memory integrity test...\n");

    char *buf = malloc(1024);

    if (!buf)
    {
        printf("FAIL: integrity allocation\n");
        return 1;
    }

    memset(buf, 0xAA, 1024);

    for (int i = 0; i < 1024; i++)
    {
        if ((unsigned char)buf[i] != 0xAA)
        {
            printf("FAIL: corruption at byte %d\n", i);
            free(buf);
            return 1;
        }
    }

    free(buf);

    printf("PASS: memory integrity\n");

    printf("=== ALL TESTS PASSED ===\n");

    return 0;
}
