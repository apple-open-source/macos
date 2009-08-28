//
//  refcounting.m
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// CONFIG GC RR -C99

#import <Foundation/Foundation.h>
#import <Block.h>
#import <Block_private.h>


int main(char *argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int i = 10;
    void (^blockA)(void) = ^ { printf("i is %d\n", i); };

    // make sure we can retain/release it
    for (int i = 0; i < 1000; ++i) {
        [blockA retain];
    }
    for (int i = 0; i < 1000; ++i) {
        [blockA release];
    }
    // smae for a copy
    void (^blockAcopy)(void) = [blockA copy];
    for (int i = 0; i < 1000; ++i) {
        [blockAcopy retain];
    }
    for (int i = 0; i < 1000; ++i) {
        [blockAcopy release];
    }
    [blockAcopy release];
    // now for the other guy
    blockAcopy = Block_copy(blockA);
        
    for (int i = 0; i < 1000; ++i) {
        void (^blockAcopycopy)(void) = Block_copy(blockAcopy);
        if (blockAcopycopy != blockAcopy) {
            printf("copy %p of copy %p wasn't the same!!\n", (void *)blockAcopycopy, (void *)blockAcopy);
            exit(1);
        }
    }
    for (int i = 0; i < 1000; ++i) {
        Block_release(blockAcopy);
    }
    Block_release(blockAcopy);
    printf("%s: success\n", argv[0]);
    return 0;
}
