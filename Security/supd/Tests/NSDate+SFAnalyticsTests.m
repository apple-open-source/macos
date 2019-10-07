//
//  NSDate+SFAnalyticsTests.m
//  KeychainAnalyticsTests
//

#import <XCTest/XCTest.h>

#import "../Analytics/NSDate+SFAnalytics.h"

@interface NSDate_SFAnalyticsTests : XCTestCase

@end

@implementation NSDate_SFAnalyticsTests

- (void)testCurrentTimeSeconds
{
    NSTimeInterval expectedTime = [[NSDate date] timeIntervalSince1970];
    NSTimeInterval actualTimeWithWiggle = [[NSDate date] timeIntervalSince1970];
    XCTAssertEqualWithAccuracy(actualTimeWithWiggle, expectedTime, 1, @"Expected to get roughly the same amount of seconds");
}

- (void)testCurrentTimeSecondsWithRounding
{
    NSTimeInterval factor = 3; // 3 seconds

    // Round into the same bucket
    NSTimeInterval now = [[NSDate date] timeIntervalSince1970];
    NSTimeInterval expectedTime = now + factor;
    NSTimeInterval actualTimeWithWiggle = [[NSDate date] timeIntervalSince1970WithBucket:SFAnalyticsTimestampBucketSecond];
    XCTAssertEqualWithAccuracy(actualTimeWithWiggle, expectedTime, factor, @"Expected to get roughly the same rounded time within the rounding factor");

    // Round into the next bucket
    now = [[NSDate date] timeIntervalSince1970];
    expectedTime = now + factor;
    sleep(factor);
    actualTimeWithWiggle = [[NSDate date] timeIntervalSince1970WithBucket:SFAnalyticsTimestampBucketSecond];
    XCTAssertEqualWithAccuracy(actualTimeWithWiggle, expectedTime, factor + 1, @"Expected to get roughly the same rounded time within the rounding factor");
}

@end
