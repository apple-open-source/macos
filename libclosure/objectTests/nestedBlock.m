//
//  nestedBlock.m
//  testObjects
//
//  Created by Blaine Garst on 6/24/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

// CONFIG RR

#include <stdio.h>
#include <Block.h>
#import <Foundation/Foundation.h>

int Retained = 0;

@interface TestObject : NSObject
@end
@implementation TestObject
- (id)retain {
    Retained = 1;
    [super retain];
}
@end

void callVoidVoid(void (^closure)(void)) {
    closure();
}

int main(int argc, char *argv[]) {
    TestObject *to = [[TestObject alloc] init];
    int i = argc;
    
    // use a copy & see that it updates i
    callVoidVoid(Block_copy(^{
        if (i > 0) {
            callVoidVoid(^{ [to self]; });
        }
    }));
    
    if (Retained == 0) {
        printf("*** %s didn't update Retained\n", argv[0]);
        return 1;
    }
    printf("%s: success\n", argv[0]);
    return 0;
}