//
//  counting.m
//  testObjects
//
//  Created by Blaine Garst on 9/23/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Block.h>
#import <stdio.h>
#include <libkern/OSAtomic.h>

// CONFIG RR -C99  rdar://6557292

int allocated = 0;
int recovered = 0;

@interface TestObject : NSObject
@end
@implementation TestObject
- init {
    //printf("allocated...\n");
    OSAtomicIncrement32(&allocated);
    return self;
}
- (void)dealloc {
    //printf("deallocated...\n");
    OSAtomicIncrement32(&recovered);
    [super dealloc];
}
- (void)finalize {
    //printf("finalized...\n");
    OSAtomicIncrement32(&recovered);
    [super finalize];
}

#if 0
- (id)retain {
    printf("retaining...\n");
    return [super retain];
}

- (void)release {
    printf("releasing...\n");
    [super release];
}
#endif
@end

void recoverMemory(const char *caller) {
    NSGarbageCollector *collector = [NSGarbageCollector defaultCollector];
    if (collector) {
        [collector collectIfNeeded];
        [collector collectExhaustively];
    }
    if (recovered != allocated) {
        printf("after %s recovered %d vs allocated %d\n", caller, recovered, allocated);
        exit(1);
    }
}

// test that basic refcounting works
void testsingle() {
    TestObject *to = [TestObject new];
    void (^b)(void) = [^{ printf("hi %p\n", to); } copy];
    [b release];
    [to release];
    recoverMemory("testSingle");
}

void testlatch() {
    TestObject *to = [TestObject new];
    void (^b)(void) = [^{ printf("hi %p\n", to); } copy];
    for (int i = 0; i < 0xfffff; ++i) {
        Block_copy(b);
    }
    for (int i = 0; i < 10; ++i) {
        Block_release(b);
    }
    [b release];
    [to release];
    // cheat
    OSAtomicIncrement32(&recovered);
    recoverMemory("testlatch");
}

void testmultiple() {
    TestObject *to = [TestObject new];
    void (^b)(void) = [^{ printf("hi %p\n", to); } copy];
#if 2
    for (int i = 0; i < 10; ++i) {
        Block_copy(b);
    }
    for (int i = 0; i < 10; ++i) {
        Block_release(b);
    }
#endif
    [b release];
    [to release];
    recoverMemory("testmultiple");
}

int main(int argc, char *argv[]) {
    testsingle();
    testlatch();
    testmultiple();
    printf("%s: success\n", argv[0]);
    return 0;
}