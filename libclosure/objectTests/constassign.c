//
//  constassign.c
//  bocktest
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// shouldn't be able to assign to a const pointer
// CONFIG error: assignment of read-only

#import <stdio.h>

void foo(void) { printf("I'm in foo\n"); }
void bar(void) { printf("I'm in bar\n"); }

int main(char *argc, char *argv[]) {
    void (*const fptr)(void) = foo;
    void (^const  blockA)(void) = ^ { printf("hello\n"); };
    blockA = ^ { printf("world\n"); } ;
    fptr = bar;
    printf("%s: success\n", argv[0]);
    return 0;
}
