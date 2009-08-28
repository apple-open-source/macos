//
//  weakblock.m
//  testObjects
//
//  Created by Blaine Garst on 10/30/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// CONFIG RR rdar://5847976
//
// Super basic test - does compiler a) compile and b) call out on assignments

#import <Foundation/Foundation.h>

// provide our own version for testing

int GotCalled = 0;

void _Block_object_assign(void *destAddr, const void *object, const int flags) {
    printf("_Block_object_assign(dest %p, value %p, flags %x)\n", destAddr, object, flags);
    ++GotCalled;
}

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
    __block TestObject *__weak  testObject = to;    // iniitialization does NOT require support function
}

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    GotCalled = 0;
    testRR();
    if (GotCalled == 1) {
        printf("called out to support function on initialization\n");
        return 1;
    }
    printf("%s: Success\n", argv[0]);
    return 0;
}