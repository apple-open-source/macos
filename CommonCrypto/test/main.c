#include <stdio.h>


#include "testenv.h"

int main (int argc, char * const *argv) {

    printf("WARNING: If running those tests on a device with a passcode, DONT FORGET TO UNLOCK!!!\n");
    tests_begin(argc, argv);
    return 0;
}
