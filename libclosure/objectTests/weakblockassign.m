
//
//  weakblockassign.m
//  testObjects
//
//  Created by Blaine Garst on 10/30/08.
//  Copyright 2008 Apple. All rights reserved.
//
// CONFIG GC rdar://5847976
//
// Super basic test - does compiler a) compile and b) call out on assignments

#import <Foundation/Foundation.h>

// provide our own version for testing

int GotCalled = 0;

#if 0
void _Block_object_assign(void *destAddr, const void *object, const int flags) {
    if (flags != 9) {
        printf("flags value %d should be 9\n", flags);
        exit(1);
    }
    printf("_Block_object_assign(dest %p, value %p, flags %x)\n", destAddr, object, flags);
    ++GotCalled;
}
#else
void objc_assign_weak(id value, id *location) {
    ++GotCalled;
    *location = value;
}
#endif

int recovered = 0;

@interface TestObject : NSObject {
}
@end

@implementation TestObject
- (id)retain {
    printf("Whoops, retain called!\n");
    exit(1);
}
- (void)finalize {
    ++recovered;
    [super finalize];
}
- (void)dealloc {
    ++recovered;
    [super dealloc];
}
@end


void testRR() {
    // create test object
    TestObject *to = [[TestObject alloc] init];
    __block TestObject *__weak  testObject = to;    // initialization does NOT require support function
    
    // there could be a Block that references "testObject" and that block could have been copied to the
    // heap and the Block_byref forwarding pointer aims at the heap object.
    // Assigning to it should trigger, under GC, the objc_assign_weak call
    testObject = [NSObject new];    // won't last long :-)
}

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    GotCalled = 0;
    testRR();
    if (GotCalled == 0) {
        printf("didn't call out to support function on assignment!!\n");
        return 1;
    }
    printf("%s: Success\n", argv[0]);
    return 0;
}