//
//  FakeXCTest
//
//  Copyright (c) 2014 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#include "FakeXCTestCase.h"

extern int (*XFakeXCTestCallback)(const char *fmt, va_list);

@interface XCTest : NSObject

+ (void)setLimit:(const char *)testLimit;
+ (int) runTests;

@end

void FakeXCFailureHandler(XCTestCase *, BOOL, const char *, NSUInteger, NSString *, NSString *, ...);

#define FakeXCFailure(test, format...)                  \
    FakeXCFailureHandler(test, false, __FILE__, __LINE__, @"failure", @"" format)

#define XCTAssert(expression, format...)                \
    @try {                                              \
        BOOL expressionValue = !!(expression);          \
        if (!expressionValue) {                         \
            FakeXCFailure(self, format);                \
        }                                               \
    }                                                   \
    @catch (...) {                                      \
        FakeXCFailure(self, format);                    \
    }

#define XCTAssertTrue(expression, format...)            \
    XCTAssert(expression, format)


#define XCTAssertNotEqual(e1, e2, format...)            \
    @try {                                              \
        __typeof(e1) ee1 = e1;                          \
        __typeof(e2) ee2 = e2;                          \
        if (ee1 == ee2) {                               \
            FakeXCFailure(self, format);                \
        }                                               \
    }                                                   \
    @catch (...) {                                      \
        FakeXCFailure(self, format);                    \
    }

#define XCTAssertEqual(e1, e2, format...)               \
    @try {                                              \
        __typeof(e1) ee1 = e1;                          \
        __typeof(e2) ee2 = e2;                          \
        if (ee1 != ee2) {                               \
            FakeXCFailure(self, format);                \
        }                                               \
    }                                                   \
    @catch (...) {                                      \
        FakeXCFailure(self, format);                    \
    }

