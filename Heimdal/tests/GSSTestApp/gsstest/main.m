//
//  gsstest
//
//  Copyright (c) 2014 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "FakeXCTest.h"

int main(int argc, const char * argv[]) {
    int res;
    @autoreleasepool {
        res = [XCTest runTests];
    }
    return !!res;
}
