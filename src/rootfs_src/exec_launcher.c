/*
 * exec_launcher — standalone execve test for NoanOS.
 *
 * Expected output when run from the shell:
 *   launcher: about to exec /bin/hello
 *   Hello World          <-- from hello binary
 *
 * "launcher: execve FAILED" must never appear.
 */
#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("launcher: about to exec /bin/hello\n");
    fflush(stdout);

    char* argv[] = { "hello", NULL };
    execve("/bin/hello", argv, NULL);

    /* Only reached if execve failed */
    printf("launcher: execve FAILED\n");
    return 1;
}
