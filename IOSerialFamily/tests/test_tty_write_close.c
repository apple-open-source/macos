
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "IOSerialTestLib.h"

#define MAX_PATH 256

// This tests stresses the write path, by having a thread write to the tty and
// another thread closing.

int main(int argc, const char * argv[])
{
    if (argc < 2) {
        printf("Usage:\n"
               "test_tty_write_close [path]\n");
        return 0;
    }
    
    struct stat file_stat;
    const char* path;
    
    if (strnlen(argv[1], MAX_PATH) == MAX_PATH) {
        printf("[FAIL] test_tty_write_close: path length is too long\n");
        return -1;
    }
    
    path = argv[1];
    
    // check presence of device node
    if (-1 == stat(path, &file_stat)) {
        printf("[FAIL] test_tty_write_close: file does not exist\n");
        return -1;
    }
    
    if (-1 == testWriteClose(path)) {
        goto fail;
    }
    
    printf("[PASS] test_tty_write_close\n");
    return 0;
    
fail:
    printf("[FAIL] test_tty_write_close\n");
    return -1;
}
