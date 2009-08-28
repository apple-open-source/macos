//
//  copy__block.m
//  testObjects
//
//  Created by Blaine Garst on 4/8/09.
//  Copyright 2009 Apple, Inc. All rights reserved.
//

// CONFIG open -C99 GC rdar://6767989

/*
  We need to test that copying __block variables is done with GC write-barriers.
  Until rdar://6767851 this was done with single assignment write-barriers which
  got very unhappy if the bits they moved happened to be a GC "garbage" block.
  So we need to be able to reproduce this unhappy case reliably to test that we
  can see the old and new behaviors as the code gets fixed underneath us in stages. 
 */
 
#import <Foundation/Foundation.h>
#import <Block.h>

void fillStackWithGarbage() {
    id pointers[64];
    for (int i = 0; i < 64; ++i) {
        pointers[i] = [NSObject new];
    }
}

void *(^getBlockVariableValue)(void);
void (^setBlockVariableValue)(void *);

@interface TestObject : NSObject
@end
@implementation TestObject
- (void)finalize {
    setBlockVariableValue(self);        // does die on a write barrier !!! XXX
    printf("set value okay\n");
    Block_copy(setBlockVariableValue);  // copying should move gc_ptr to the heap
    [super finalize];
}
@end


void doTest() {
    // In the rdar above the __block was a bool set to false which only reset the low byte
    // Here we use all the uninitialized bytes
    __block void *gc_ptr;  // leave uninitialized on purpose
    setBlockVariableValue = ^(void *arg) { gc_ptr = arg; }; // XXX needs write-barrier
    // Now lets generate some garbage and set gc_ptr to such
    for (int i = 0; i < 1000; ++i)
        [TestObject new];
    [[NSGarbageCollector defaultCollector] collectIfNeeded];
    [[NSGarbageCollector defaultCollector] collectExhaustively];
    getBlockVariableValue = Block_copy(^{ return gc_ptr; });
    // Now test to make sure that we copied the uninitialized bits to the heap.
    if (getBlockVariableValue() != gc_ptr) {
        printf("didn't copy uninitialized value to heap\n");
        exit(1);
    }
}
    

int main(int argc, char *argv[]) {
    doTest();
    printf("%s: success\n", argv[0]);
    return 0;
}