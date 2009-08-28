//
//  recursive-assign-int.m
//  testObjects
//
//  Created by Blaine Garst on 12/4/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

// CONFIG open rdar://6416474

// The compiler is prefetching x->forwarding before evaluting code that recomputes forwarding and so the value goes to a place that is never seen again.

#include <stdio.h>
#include <stdlib.h>
#include <Block.h>

typedef void (^blockOfVoidReturningVoid)(void);

blockOfVoidReturningVoid globalBlock;

int nTHCopy(blockOfVoidReturningVoid  block) {
    globalBlock = Block_copy(block);
    return 1;
}

int main(int argc, char* argv[]) {
    
    __block int x = 0;
    
    x = nTHCopy(^{
        printf("%d should reflect what nTHCopy provided\n", x);
        if (x == 0) {
            printf("but it wasn't updated properly!\n");
        }
    });
    
    globalBlock();
    if (x == 0) {
        printf("x here should be 1, but instead is: %d\n", x);
        return 1;
    }
    printf("%s: Success\n", argv[0]);
    return 0;
}

