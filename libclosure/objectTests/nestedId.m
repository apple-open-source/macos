//
//  nestedBlock.m
//  testObjects
//
//  Created by Blaine Garst on 6/24/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//



#include <stdio.h>
#include <Block.h>
#import <Foundation/Foundation.h>

int Retained = 0;

// CONFIG RR

void (^savedBlock)(void);
void (^savedBlock2)(void);

void saveit(void (^block)(void)) {
    savedBlock = Block_copy(block);
}
void callit() {
    savedBlock();
}
void releaseit() {
    Block_release(savedBlock);
    savedBlock = nil;
}
void saveit2(void (^block)(void)) {
    savedBlock2 = Block_copy(block);
}
void callit2() {
    savedBlock2();
}
void releaseit2() {
    Block_release(savedBlock2);
    savedBlock2 = nil;
}

@interface TestObject : NSObject
@end

@implementation TestObject
- (id)retain {
    ++Retained;
    [super retain];
    return self;
}
- (void)release {
    --Retained;
    [super retain];
}

        
@end
id global;

void test(id param) {
    saveit(^{
        saveit2(
            ^{ 
                global = param;
            });
    });
}


int main(int argc, char *argv[]) {
    TestObject *to = [[TestObject alloc] init];
    
    test(to);
    if (Retained == 0) {
        printf("*** %s didn't update Retained\n", argv[0]);
        return 1;
    }
    callit();
    callit2();
    printf("%s: success\n", argv[0]);
    return 0;
}