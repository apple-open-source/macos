
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "IOSerialTestLib.h"

#define MAX_PATH 256

// This tests stresses the read path, by having a thread read from the tty and
// another thread closing.

int main(int argc, const char * argv[])
{
    if (argc < 3) {
        printf("Usage:\n"
               "test_tty_read_close [writepath] [readpath] \n");
        return 0;
    }
    
    struct stat file_stat;
    const char* writepath;
    const char* readpath;
    
    if ((strnlen(argv[1], MAX_PATH) == MAX_PATH) ||
        (strnlen(argv[2], MAX_PATH) == MAX_PATH)) {
        printf("[FAIL] test_tty_read_close: path length is too long\n");
        return -1;
    }
    
    writepath = argv[1];
    readpath = argv[2];
    
    // check presence of device node
    if (-1 == stat(writepath, &file_stat)) {
        printf("[FAIL] test_tty_read_close: file does not exist\n");
        return -1;
    }
    if (-1 == stat(readpath, &file_stat)) {
        printf("[FAIL] test_tty_read_close: file does not exist\n");
        return -1;
    }
    
    if (-1 == testReadClose(writepath, readpath)) {
        goto fail;
    }
    
    printf("[PASS] test_tty_read_close\n");
    return 0;
    
fail:
    printf("[FAIL] test_tty_read_close\n");
    return -1;
}
