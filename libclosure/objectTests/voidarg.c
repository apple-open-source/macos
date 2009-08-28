/*
 *  voidarg.c
 *  testObjects
 *
 *  Created by Blaine Garst on 2/17/09.
 *  Copyright 2009 Apple. All rights reserved.
 *
 */

// PURPOSE should complain about missing 'void' but both GCC and clang are supporting K&R instead
// CONFIG open error:

#include <stdio.h>

int Global;

void (^globalBlock)() = ^{ ++Global; };         // should be void (^gb)(void) = ...

int main(int argc, char *argv[]) {
    printf("%s: success", argv[0]);
    return 0;
}