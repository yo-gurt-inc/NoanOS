#include "shell/noan.h"
#include "shell/commands.h"

extern char current_path[256];

static int sh_strlen(const char* s) { int n=0; while(s[n]) n++; return n; }
static void sh_strcpy(char* d, const char* s) { while((*d++=*s++)); }
static void sh_strcat(char* d, const char* s) { while(*d) d++; while((*d++=*s++)); }

int sh_cd(int argc, char** argv) {
    if (argc < 2) { noan_print("Usage: cd <path>\n"); return 1; }
    const char* dir = argv[1];

    noan_cd(dir);

    /* Update the userspace path string */
    if (dir[0] == '/' && dir[1] == '\0') {
        sh_strcpy(current_path, "/");
    } else if (dir[0] == '.' && dir[1] == '.' && dir[2] == '\0') {
        /* Strip last component */
        int len = sh_strlen(current_path);
        if (len > 1) {
            len--;
            while (len > 0 && current_path[len] != '/') len--;
            current_path[len == 0 ? 1 : len] = '\0';
        }
    } else {
        if (current_path[sh_strlen(current_path)-1] != '/')
            sh_strcat(current_path, "/");
        sh_strcat(current_path, dir);
    }
    return 0;
}
