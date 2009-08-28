//
//  simpleassign.m
//  bocktest
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// CONFIG GC RR


#import <Foundation/Foundation.h>


int main(char *argc, char *argv[]) {
    id aBlock;
    void (^blockA)(void) = ^ { printf("hello\n"); };
    // a block should be assignable to an id
    aBlock = blockA;
    blockA = aBlock;
    printf("%s: success\n", argv[0]);
    exit(0);
}
