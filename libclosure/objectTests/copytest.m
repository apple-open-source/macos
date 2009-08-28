//
//  copytest.m
//  bocktest
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// CONFIG GC RR

#import <Foundation/Foundation.h>

int GlobalInt = 0;
void setGlobalInt(int value) { GlobalInt = value; }

#ifdef __cplusplus
extern "C"
#endif

const char * _Block_dump(void *);

int main(char *argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    id object = [NSObject new];
    int y = 0;
    //gc_block_init();
    // must use x+y to avoid optimization of using a global block
    void (^callSetGlobalInt)(int x) = ^(int x) { setGlobalInt(x + y); };
    // a block be able to be sent a message
    void (^callSetGlobalIntCopy)(int) = [callSetGlobalInt copy];
    if (callSetGlobalIntCopy == callSetGlobalInt) {
        printf("copy looks like: %s\n", _Block_dump(callSetGlobalIntCopy));
        printf("%s: failure, copy is identical\n", argv[0]);
        exit(1);
    }
    callSetGlobalIntCopy(10);
    if (GlobalInt != 10) {
        printf("%s: failure, copy did not set global int\n", argv[0]);
        exit(1);
    }
    printf("%s: success\n", argv[0]);
    exit(0);
}
