/*
 *  shorthandexpression.c
 *  testObjects
 *
 *  Created by Blaine Garst on 9/16/08.
 *  Copyright 2008 Apple. All rights reserved.
 *
 *  CONFIG error:
 */


void foo() {
    void (^b)(void) = ^(void)printf("hello world\n");
}

int main(int argc, char *argv[]) {
    printf("%s: this shouldn't compile\n", argv[0]);
    return 1;
}