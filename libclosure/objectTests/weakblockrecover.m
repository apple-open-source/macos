//
//  weakblockrecover.m
//  testObjects
//
//  Created by Blaine Garst on 11/3/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// CONFIG GC -C99 rdar://5847976



#import <Foundation/Foundation.h>
#import <Block.h>

int Allocated = 0;
int Recovered = 0;

@interface TestObject : NSObject
@end

@implementation TestObject

- init {
    ++Allocated;
    return self;
}
- (void)dealloc {
    ++Recovered;
    [super dealloc];
}
- (void)finalize {
    ++Recovered;
    [super finalize];
}


@end

void testRecovery() {
    NSMutableArray *listOfBlocks = [NSMutableArray new];
    for (int i = 0; i < 1000; ++i) {
        __block TestObject *__weak to = [[TestObject alloc] init];
        void (^block)(void) = ^ { printf("is it still real? %p\n", to); };
        [listOfBlocks addObject:[block copy]];
        [to release];
    }
    // let's see if we can recover any under GC circumstances
    NSGarbageCollector *collector = [NSGarbageCollector defaultCollector];
    [collector collectIfNeeded];
    [collector collectExhaustively];
    [listOfBlocks self]; // by using it here we keep listOfBlocks alive across the GC
}

int main(int argc, char *argv[]) {
    testRecovery();
    if ((Recovered + 10) < Allocated) {
        printf("Only %d weakly referenced test objects recovered, vs %d allocated\n", Recovered, Allocated);
        return 1;
    }
    printf("%s: Success\n", argv[0]);
    return 0;
}