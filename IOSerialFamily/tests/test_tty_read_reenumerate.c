
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "IOSerialTestLib.h"

#define MAX_PATH 256

// This tests stresses the read path, by having a process write to the tty and
// another process calling reenumerate (to simulate a hardware unplug).
// If a race condition exists, this would likely cause a panic.

int main(int argc, const char * argv[])
{
    if (argc < 4) {
        printf("Usage: test_tty_read_reenumerate [writepath] [readpath] [locationid]\n\n"
               "writepath: path of the device used for writing, e.g.: /dev/cu.usbserial.XX\n"
               "readpath: path of the device used for reading, e.g.: /dev/cu.usbserial.XX\n"
               "locationid: location id of the device used for reading, e.g.: 0xfa130000\n");
        return 0;
    }
    
    struct stat file_stat;
    const char* writepath;
    const char* readpath;
    const char* locationid;
    
    if ((strnlen(argv[1], MAX_PATH) == MAX_PATH) ||
        (strnlen(argv[2], MAX_PATH) == MAX_PATH)) {
        printf("[FAIL] test_tty_read_reenumerate: path length is too long\n");
        return -1;
    }
    
    writepath = argv[1];
    readpath = argv[2];
    locationid = argv[3]; // format: "0xfa130000"
    
    // check presence of device node
    if (-1 == stat(writepath, &file_stat)) {
        printf("[FAIL] test_tty_read_reenumerate: %s does not exist\n", writepath);
        return -1;
    }
    if (-1 == stat(readpath, &file_stat)) {
        printf("[FAIL] test_tty_read_reenumerate: %s does not exist\n", readpath);
        return -1;
    }

    if (-1 == testReadReenumerate(writepath, readpath, locationid)) {
        goto fail;
    }
    
    printf("[PASS] test_tty_read_reenumerate\n");
    return 0;
    
fail:
    printf("[FAIL] test_tty_read_reenumerate\n");
    return -1;
}
