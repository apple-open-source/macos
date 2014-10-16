
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "IOSerialTestLib.h"

#define MAX_PATH 256

int main(int argc, const char * argv[])
{
    if (argc < 2) {
        printf("Usage:\n"
               "test_tty_open [path]\n");
        return 0;
    }
    
    struct stat file_stat;
    const char* path;
    
    if (strnlen(argv[1], MAX_PATH) == MAX_PATH) {
        printf("[FAIL] test_tty_open: path length is too long\n");
        return -1;
    }
    
    path = argv[1];
    
    // check presence of device node
    if (-1 == stat(path, &file_stat)) {
        printf("[FAIL] test_tty_open: file does not exist\n");
        return -1;
    }
    
    if (-1 == testOpenClose(path)) {
        goto fail;
    }
    
    printf("[PASS] test_tty_open\n");
    return 0;
    
fail:
    printf("[FAIL] test_tty_open\n");
    return -1;
}