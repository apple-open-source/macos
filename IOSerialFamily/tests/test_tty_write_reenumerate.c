
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "IOSerialTestLib.h"

#define MAX_PATH 256

// This tests stresses the write path, by having a process write to the tty and
// another process calling reenumerate (to simulate a hardware unplug).
// If a race condition exists, this would likely cause a panic.

int main(int argc, const char * argv[])
{
    if (argc < 3) {
        printf("Usage:\n"
               "test_tty_write_reenumerate [path] [deviceid]\n");
        return 0;
    }
    
    struct stat file_stat;
    const char* path;
    const char* deviceid;
    
    if (strnlen(argv[1], MAX_PATH) == MAX_PATH) {
        printf("[FAIL] test_tty_write_reenumerate: path length is too long\n");
        return -1;
    }
    
    path = argv[1];
    deviceid = argv[2]; // format: "0x403,0x6001"
    
    // check presence of device node
    if (-1 == stat(path, &file_stat)) {
        printf("[FAIL] test_tty_write_reenumerate: file does not exist\n");
        return -1;
    }
    
    if (-1 == testWriteReenumerate(path, deviceid)) {
        goto fail;
    }
    
    printf("[PASS] test_tty_write_reenumerate\n");
    return 0;
    
fail:
    printf("[FAIL] test_tty_write_reenumerate\n");
    return -1;
}
