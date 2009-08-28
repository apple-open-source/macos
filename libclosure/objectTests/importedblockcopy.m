//
//  importedblockcopy.m
//  testObjects
//
//  Created by Blaine Garst on 10/16/08.
//  Copyright 2008 Apple. All rights reserved.
//

// CONFIG GC RR rdar://6297435 -C99
// really just GC but might as well test RR too

#import <Foundation/Foundation.h>
#import "Block.h"

int Allocated = 0;
int Reclaimed = 0;

@interface TestObject : NSObject
@end

@implementation TestObject
- (void) dealloc {
    ++Reclaimed;
    [super dealloc];
}

- (void)finalize {
    ++Reclaimed;
    [super finalize];
}

- init {
    self = [super init];
    ++Allocated;
    return self;
}

@end

void theTest() {
    // establish a block with an object reference
    TestObject *to = [[TestObject alloc] init];
    void (^inner)(void) = ^ {
        [to self];  // something that will hold onto "to"
    };
    // establish another block that imports the first one...
    void (^outer)(void) = ^ {
        inner();
        inner();
    };
    // now when we copy outer the compiler will _Block_copy_assign inner
    void (^outerCopy)(void) = Block_copy(outer);
    // but when released, at least under GC, it won't let go of inner (nor its import: "to")
    Block_release(outerCopy);
    [to release];
}


int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSGarbageCollector *collector = [NSGarbageCollector defaultCollector];
    
    for (int i = 0; i < 200; ++i)
        theTest();
    [pool drain];
    [collector collectExhaustively];
    if ((Reclaimed+10) <= Allocated) {
        printf("whoops, reclaimed only %d of %d allocated\n", Reclaimed, Allocated);
        return 1;
    }
    printf("%s: Success!\n", argv[0]);
    return 0;
}