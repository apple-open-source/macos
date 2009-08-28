//
//  layout.m
//  bocktest
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#include <Foundation/Foundation.h>
#include <objc/runtime.h>

// CONFIG GC

@interface TestObject : NSObject {
    int (^getInt)(void);
    int object;
}
@property(assign, readonly) int (^getInt)(void);
@end

@implementation TestObject
@synthesize getInt;
@end


int main(char *argc, char *argv[]) {
    TestObject *to = [[TestObject alloc] init];
    //to = [NSCalendarDate new];
    const char *layout = class_getIvarLayout(*(Class *)to);
    if (!layout) {
        printf("%s: **** no layout for class TestObject!!!\n", argv[0]);
        exit(1);
    }
    //printf("layout is:\n");
    int cursor = 0;
    // we're looking for slot 1
    int seeking = 1;
    while (*layout) {
        int skip = (*layout) >> 4;
        int process = (*layout) & 0xf;
        //printf("(%x) skip %d, process %d\n", (*layout), skip, process);
        cursor += skip;
        if ((cursor <= seeking) && ((cursor + process) > seeking)) {
            printf("%s: Success!\n", argv[0]);
            return 0;
        }
        cursor += process;
        ++layout;
    }
    printf("%s: ***failure, didn't scan slot %d\n", argv[0], seeking);
    return 1;
}
