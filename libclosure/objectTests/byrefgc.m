//
//  byrefgc.m
//  testObjects
//
//  Created by Blaine Garst on 5/16/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
//  CONFIG GC -C99


#import <Foundation/Foundation.h>
#include <stdio.h>
#include <Block.h>

int DidFinalize = 0;
int GotHi = 0;

int VersionCounter = 0;

@interface TestObject : NSObject {
    int version;
}
- (void) hi;
@end

@implementation TestObject


- init {
    version = VersionCounter++;
    return self;
}

- (void)finalize {
    DidFinalize++;
    [super finalize];
}
- (void) hi {
    GotHi++;
}

@end


void (^get_block(void))(void) {
    __block TestObject * to = [[TestObject alloc] init];
    return [^{ [to hi]; to = [[TestObject alloc] init]; } copy];
}

int main(int argc, char *argv[]) {
    
    void (^voidvoid)(void) = get_block();
    voidvoid();
    voidvoid();
    voidvoid();
    voidvoid();
    voidvoid();
    voidvoid();
    voidvoid = nil;
    for (int i = 0; i < 8000; ++i) {
        [NSObject new];
    }
    [[NSGarbageCollector defaultCollector] collectIfNeeded];
    if ((DidFinalize + 2) < VersionCounter) {
        printf("*** %s didn't recover all objects %d/%d\n", argv[0], DidFinalize, VersionCounter);
        return 1;
    }
    printf("%s: success\n", argv[0]);
    return 0;
}