//
//  notcopied.m
//  testObjects
//
//  Created by Blaine Garst on 2/12/09.
//  Copyright 2009 Apple. All rights reserved.
//


#include <stdio.h>
#include <Block.h>
#include <Block_private.h>
#import <Foundation/Foundation.h>

// CONFIG RR rdar://6557292

// Test that a __block Block variable with a reference to a stack based Block is not copied
// when a Block referencing the __block Block varible is copied.
// No magic for __block variables.


int Retained = 0;

@interface TestObject : NSObject
@end
@implementation TestObject
- (id)retain {
    Retained = 1;
    [super retain];
}
@end


int main(int argc, char *argv[]) {
    TestObject *to = [[TestObject alloc] init];
    __block void (^someBlock)(void) = ^ { [to self]; };
    void (^someOtherBlock)(void) = ^ {
          someBlock();   // reference someBlock.  It shouldn't be copied under the new rules.
    };
    someOtherBlock = [someOtherBlock copy];
    if (Retained != 0) {
        printf("*** __block Block was copied when it shouldn't have\n", argv[0]);
        return 1;
    }
    printf("%s: success\n", argv[0]);
    return 0;
}