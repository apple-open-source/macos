
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "IOSerialTestLib.h"

#define MAX_PATH 256

int main(int argc, const char * argv[])
{
    if (argc < 3) {
        printf("Usage:\n"
               "test_tty_read_write [readpath] [writepath]\n"
               "test_tty_read_write [readpath] [writepath] [message]\n");
        return 0;
    }
    
    struct stat file_stat;
    const char* readpath;
    const char* writepath;
    const char* message = NULL;
    
    if (strnlen(argv[1], MAX_PATH) == MAX_PATH) {
        printf("[FAIL] test_tty_read_write: path length is too long\n");
        return -1;
    }
    if (strnlen(argv[2], MAX_PATH) == MAX_PATH) {
        printf("[FAIL] test_tty_read_write: path length is too long\n");
        return -1;
    }
    
    if (argc == 4) {
        if (strnlen(argv[3], MAX_PATH) == MAX_PATH) {
            printf("[FAIL] test_tty_read_write: message is too long\n");
        }
        else {
            message = argv[3];
        }
    }
    
    readpath = argv[1];
    writepath = argv[2];
    
    // check presence of device node
    if (-1 == stat(readpath, &file_stat)) {
        printf("[FAIL] test_tty_read_write: file does not exist\n");
        return -1;
    }
    if (-1 == stat(writepath, &file_stat)) {
        printf("[FAIL] test_tty_read_write: file does not exist\n");
        return -1;
    }
    
    if (-1 == testReadWrite(readpath, writepath, message)) {
        goto fail;
    }
    
    printf("[PASS] test_tty_read_write\n");
    return 0;
    
fail:
    printf("[FAIL] test_tty_read_write\n");
    return -1;
}

