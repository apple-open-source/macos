//
//  simplemessage.m
//  bocktest
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// CONFIG GC RR

#import <Foundation/Foundation.h>


int main(char *argc, char *argv[]) {
    void (^blockA)(void) = ^ { 1 + 3; };
    // a block be able to be sent a message
    if (*(int *)(void *)blockA == 0x12345) [blockA copy];
    printf("%s: success\n", argv[0]);
    exit(0);
}
