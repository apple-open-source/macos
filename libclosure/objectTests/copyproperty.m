//
//  copyproperty.m
//  bocktest
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// CONFIG GC RR

#import <Foundation/Foundation.h>
#include <stdio.h>

@interface TestObject : NSObject {
    long (^getInt)(void);
}
@property(copy) long (^getInt)(void);
@end

@implementation TestObject
@synthesize getInt;
@end

@interface CountingObject : NSObject
@end

int Retained = 0;

@implementation CountingObject
- (id) retain {
    Retained = 1;
    return [super retain];
}
@end

int main(char *argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    TestObject *to = [[TestObject alloc] init];
    TestObject *co = [[CountingObject alloc] init];
    long (^localBlock)(void) = ^{ return 10L + (long)[co self]; };
    to.getInt = localBlock;
    if (localBlock == to.getInt) {
        printf("%s: ****block property not copied!!\n", argv[0]);
        return 1;
    }
    if (![NSGarbageCollector defaultCollector] && Retained == 0) {
        printf("%s: ****didn't copy block import\n", argv[0]);
        return 1;
    }
    printf("%s: success\n", argv[0]);
    return 0;
}
