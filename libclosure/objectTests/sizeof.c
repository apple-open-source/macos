/*
 *  sizeof.c
 *  testObjects
 *
 *  Created by Blaine Garst on 2/17/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <stdio.h>

// CONFIG error:

int main(int argc, char *argv[]) {

    void (^aBlock)(void) = ^{ printf("hellow world\n"); };

    printf("the size of a block is %ld\n", sizeof(*aBlock));
    printf("%s: success\n", argv[0]);
    return 0;
}