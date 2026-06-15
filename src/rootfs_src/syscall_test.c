#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

int main() {
    printf("=== SYSCALL TEST ===\n");
    
    // Test getpid()
    printf("getpid: %d\n", getpid());
    
    // Test write()
    write(1, "write: OK\n", 10);
    
    // Test open/read/close/lseek
    int fd = open("hello", O_RDONLY);
    printf("open returned: %d\n", fd);
    if (fd >= 0) {
        char buf[16];
        int n = read(fd, buf, 10);
        printf("open/read: fd=%d bytes=%d\n", fd, n);
        if (n > 0) {
            printf("first bytes: %02x %02x %02x\n", buf[0], buf[1], buf[2]);
        }
        
        off_t pos = lseek(fd, 5, SEEK_SET);
        printf("lseek SEEK_SET: %ld\n", (long)pos);
        
        pos = lseek(fd, 2, SEEK_CUR);
        printf("lseek SEEK_CUR: %ld\n", (long)pos);
        
        pos = lseek(fd, 0, SEEK_END);
        printf("lseek SEEK_END: %ld\n", (long)pos);
        
        close(fd);
        printf("close: OK\n");
    } else {
        printf("open: FAIL\n");
    }
    
    // Test stat()
    struct stat st;
    if (stat("hello", &st) == 0) {
        printf("stat: OK\n");
    } else {
        printf("stat: FAIL\n");
    }
    
    // Test fork (expect ENOSYS)
    pid_t p = fork();
    if (p == -1) {
        printf("fork: ENOSYS (expected)\n");
    } else {
        printf("fork: unexpected success\n");
    }
    
    // Test execve (expect ENOSYS)
    char *argv[] = {NULL};
    if (execve("hello", argv, NULL) == -1) {
        printf("execve: ENOSYS (expected)\n");
    }
    
    printf("=== TEST DONE ===\n");
    return 0;
}
