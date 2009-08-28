#include <stdio.h>

// CONFIG

int AGlobal;

int main(int argc, char *argv[]) {
    void (^f)(void) = ^ { AGlobal++; };
    
    printf("%s: success\n", argv[0]);
    return 0;

}