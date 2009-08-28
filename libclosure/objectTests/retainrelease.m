//
//  retainrelease.m
//  bocktest
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// CONFIG GC -C99

#import <Foundation/Foundation.h>

@interface TestObject : NSObject {
}
@end

int GlobalInt = 0;

@implementation TestObject
- (id) retain {
    ++GlobalInt;
    return self;
}


@end

int main(int argc, char *argv[]) {
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   // an object should not be retained within a stack Block
   TestObject *to = [[TestObject alloc] init];
   TestObject *to2 = [[TestObject alloc] init];
   void (^blockA)(void) = ^ { [to self]; printf("using argc %d\n", argc); [to2 self]; };
   if (GlobalInt == 0) {
        printf("%s: success\n", argv[0]);
        exit(0);
   }
   printf("%s: object retained inside stack closure\n", argv[0]);
   exit(1);
}
   