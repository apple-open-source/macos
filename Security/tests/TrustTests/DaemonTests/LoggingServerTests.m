//
//  LoggingServerTests.m
//  Security
//
//  Created by Bailey Basile on 6/11/19.
//

#include <AssertMacros.h>
#import <XCTest/XCTest.h>

#import "trust/trustd/SecTrustLoggingServer.h"

@interface LoggingServerTests : XCTestCase
@end

@implementation LoggingServerTests

- (void)testIntegerTruncation {
    XCTAssertEqualObjects(TATruncateToSignificantFigures(5, 1), @(5));
    XCTAssertEqualObjects(TATruncateToSignificantFigures(5, 2), @(5));
    XCTAssertEqualObjects(TATruncateToSignificantFigures(42, 1), @(40));
    XCTAssertEqualObjects(TATruncateToSignificantFigures(42, 2), @(42));
    XCTAssertEqualObjects(TATruncateToSignificantFigures(-335, 1), @(-300));
    XCTAssertEqualObjects(TATruncateToSignificantFigures(-335, 2), @(-330));
    XCTAssertEqualObjects(TATruncateToSignificantFigures(-335, 3), @(-335));
    XCTAssertEqualObjects(TATruncateToSignificantFigures(12345678901LL, 2), @(12000000000LL));
    XCTAssertEqualObjects(TATruncateToSignificantFigures(12345678901LL, 7), @(12345670000LL));
    XCTAssertEqualObjects(TATruncateToSignificantFigures(-12345678901LL, 3), @(-12300000000LL));
}

@end
