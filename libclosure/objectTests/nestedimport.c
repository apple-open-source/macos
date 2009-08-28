//
//  nestedimport.m
//  testObjects
//
//  Created by Blaine Garst on 6/24/08.
//  Copyright 2008 Apple, Inc. All rights reserved.
//
// pure C nothing more needed
// CONFIG 


#include <stdio.h>
#include <stdlib.h>


int Global = 0;

void callVoidVoid(void (^closure)(void)) {
    closure();
}

int main(int argc, char *argv[]) {
    int i = 1;
    
    void (^vv)(void) = ^{
        if (argc > 0) {
            callVoidVoid(^{ Global = i; });
        }
    };
    
    i = 2;
    vv();
    if (Global != 1) {
        printf("%s: error, Global not set to captured value\n", argv[0]);
        exit(1);
    }
    printf("%s: success\n", argv[0]);
    return 0;
}