//
//  c99.m
//
// CONFIG C99 rdar://problem/6399225

#import <stdio.h>
#import <stdlib.h>

int main(char *argc, char *argv[]) {
    void (^blockA)(void) = ^ { ; };
    blockA();
    printf("%s: success\n", argv[0]);
    exit(0);
}
