//
//  TestHIDAnalyticsMemory.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 1/2/19.
//

#import <XCTest/XCTest.h>
#import <XCTest/XCTMemoryChecker.h>
#import <IOKit/hid/IOHIDAnalytics.h>
#import <CoreFoundation/CoreFoundation.h>
#import "IOHIDUnitTestUtility.h"

@interface TestHIDAnalyticsMemory : XCTestCase
{
     XCTMemoryChecker *_memoryChecker;
}
@end

@implementation TestHIDAnalyticsMemory

- (void)setUp {
    _memoryChecker = [[XCTMemoryChecker alloc]initWithDelegate:self];
}

- (void)tearDown {
   
}

- (void)testHIDAnalyticsMemory {
   
    NSInteger eventCount = 0;
   
    while(eventCount < 20) {
        
        [_memoryChecker assertObjectsOfTypes:@[@"HIDAnalyticsEvent", @"HIDAnalyticsEventField", @"HIDAnalyticsHistogramBucket", @"HIDAnalyticsHistogramSegment", @"HIDAnalyticsHistogramEventField"] invalidAfterScope:^{
    
            @autoreleasepool {
                
                CFTypeRef testHIDAnalyticsEvent = IOHIDAnalyticsEventCreate(CFSTR("testHIDAnalyticsEvent"), NULL);
                
                HIDXCTAssertWithParameters(RETURN_FROM_TEST, testHIDAnalyticsEvent, "Failed to create analytics event");
                
                IOHIDAnalyticsHistogramSegmentConfig testSegmentConfig;
                
                testSegmentConfig.bucket_count = 5;
                testSegmentConfig.bucket_width = 2;
                testSegmentConfig.bucket_base = 0;
                testSegmentConfig.value_normalizer = 1;
                
                IOHIDAnalyticsEventAddHistogramField(testHIDAnalyticsEvent, CFSTR("hidAnalyticsTestField"), &testSegmentConfig, 1);
                
                IOHIDAnalyticsEventActivate(testHIDAnalyticsEvent);
                
                IOHIDAnalyticsEventCancel(testHIDAnalyticsEvent);
                
                CFRelease(testHIDAnalyticsEvent);
                
            }
        }];
        
        eventCount++;
    
    }
    
}

@end
