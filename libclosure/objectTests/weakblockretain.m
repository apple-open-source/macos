//
//  weakblockretain.m
//  testObjects
//
//  Created by Blaine Garst on 11/3/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// CONFIG RR rdar://5847976
//
// Test that weak block variables don't retain/release their contents



#import <Foundation/Foundation.h>
#import <Block.h>

int RetainCalled;
int ReleaseCalled;

@interface TestObject : NSObject
@end

@implementation TestObject

- (id)retain {
    RetainCalled = 1;
}
- (void)release {
    ReleaseCalled = 1;
}

void  testLocalScope(void) {
    __block TestObject *__weak to = [[TestObject alloc] init];
    // when we leave the scope a byref release call is made
    // this recovers the __block storage but leaves the contents alone
    // XXX make 10^6 of these to make sure we collect 'em
}

@end

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    testLocalScope();
    if (RetainCalled || ReleaseCalled) {
        printf("testLocalScope had some problems\n");
        return 1;
    }
        
    __block TestObject *__weak to = [[TestObject alloc] init];
    void (^block)(void) = ^ { printf("is it still real? %p\n", to); };
    void (^blockCopy)(void) = Block_copy(block);
    if (RetainCalled) {
        printf("Block_copy retain had some problems\n");
        return 1;
    }
    if (ReleaseCalled) {
        printf("Block_copy release had some problems\n");
        return 1;
    }
    
    printf("%s: Success\n", argv[0]);
    return 0;
}