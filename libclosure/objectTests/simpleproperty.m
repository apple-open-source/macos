//
//  simpleproperty.m
//  bocktest
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// CONFIG GC RR

#include <stdio.h>

@interface TestObject {

}
@property(assign, readonly) int (^getInt)(void);
@end



int main(char *argc, char *argv[]) {
    printf("%s: success\n", argv[0]);
    return 0;
}
