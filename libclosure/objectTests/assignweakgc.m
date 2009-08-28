#import <Foundation/Foundation.h>

// CONFIG GC

int GlobalInt = 0;
int GlobalInt2 = 0;

id objc_assign_weak(id value, id *location) {
    GlobalInt = 1;
    *location = value;
    return value;
}

id objc_read_weak(id *location) {
    GlobalInt2 = 1;
    return *location;
}

//void (^GlobalVoidVoid)(void);

void (^__weak Henry)(void);

int main(char *argc, char *argv[]) {
    // an object should not be retained within a stack Block
    __block int i = 0;
    void (^local)(void);
    Henry = ^ {  ++i; };
    if (GlobalInt == 1) {
        printf("%s: success\n", argv[0]);
        exit(0);
    }
    else {
        printf("%s: problem with weak write-barrier assignment of global block\n", argv[0]);
    }
    exit(1);
}
