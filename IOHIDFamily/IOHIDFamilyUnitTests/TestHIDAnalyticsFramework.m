//
//  TestHIDAnalyticsFramework.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 11/28/18.
//

#import <XCTest/XCTest.h>
#import <HIDAnalytics/HIDAnalytics.h>
#import "IOHIDUnitTestUtility.h"

#define kHIDAnalyticsTestEvent "com.apple.TestHIDAnalytics"
#define kHIDAnalyticsTestFieldSingleSegment "com.apple.TestHIDAnalytics.FieldSingleSegment"
#define kHIDAnalyticsTestFieldMultipleSegment "com.apple.TestHIDAnalytics.FieldMultipleSegment"

@interface TestHIDAnalyticsFramework : XCTestCase

@end

@implementation TestHIDAnalyticsFramework
{
    HIDAnalyticsEvent *_haEvent;
}
- (void)setUp {
    
    _haEvent = [[HIDAnalyticsEvent alloc] initWithAttributes:@kHIDAnalyticsTestEvent  description:@{}];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST, _haEvent);
    
    HIDAnalyticsHistogramSegmentConfig testSingleSegmentConfig;
    
    testSingleSegmentConfig.bucket_count = 5;
    testSingleSegmentConfig.bucket_width = 2;
    testSingleSegmentConfig.bucket_base = 0;
    testSingleSegmentConfig.value_normalizer = 1;
    
    [_haEvent addHistogramFieldWithSegments:@kHIDAnalyticsTestFieldSingleSegment segments:&testSingleSegmentConfig count:1];
    
    HIDAnalyticsHistogramSegmentConfig testMultipleSegmentConfig[2];
    
    testMultipleSegmentConfig[0].bucket_count = 5;
    testMultipleSegmentConfig[0].bucket_width = 2;
    testMultipleSegmentConfig[0].bucket_base = 0;
    testMultipleSegmentConfig[0].value_normalizer = 1;
    
    testMultipleSegmentConfig[1].bucket_count = 10;
    testMultipleSegmentConfig[1].bucket_width = 3;
    testMultipleSegmentConfig[1].bucket_base = 10;
    testMultipleSegmentConfig[1].value_normalizer = 1;
    
    [_haEvent addHistogramFieldWithSegments:@kHIDAnalyticsTestFieldMultipleSegment segments:testMultipleSegmentConfig count:2];
    
    // we don't activate and cancel , since we don;t want to be in reporting cycle
    
}

- (void)tearDown {
   
}
-(NSArray*) getEventDataForField:(NSString*) fieldName
{
    NSString *desc = _haEvent.description;
    
    NSData *jsonData = [desc dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary *dict = [NSJSONSerialization JSONObjectWithData:jsonData options:NSJSONReadingAllowFragments error:nil];
    if (!dict) {
        return nil;
    }
   
    NSLog(@"%@",dict);
    
    NSArray *eventFields =  dict[@"EventValue"];
    __block NSArray *ret = nil;
    [eventFields enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx __unused, BOOL * _Nonnull stop) {
        
        if (![obj isKindOfClass:[NSDictionary class]]) return;
        
        NSDictionary *fieldInfo = (NSDictionary*)obj;
        
        if ([fieldInfo[@"Name"] isEqualToString:fieldName]) {
            ret = fieldInfo[@"Value"];
            *stop = YES;
            return;
        }
    }];
    
    return ret;
}
- (void)testHIDAnalyticsFramework {
   
    // Test Single Segment
    [_haEvent setIntegerValue:3 forField:@kHIDAnalyticsTestFieldSingleSegment];
    [_haEvent setIntegerValue:7 forField:@kHIDAnalyticsTestFieldSingleSegment];
    
    NSArray *updatedData = [self getEventDataForField:@kHIDAnalyticsTestFieldSingleSegment];
    
   
    // expect bucket index 1 and 3 to be 1 others 0
    HIDXCTAssertWithParameters(RETURN_FROM_TEST, updatedData && updatedData.count == 5);
    
    NSLog(@"%@",updatedData);
    
    [updatedData enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop __unused) {
        
        NSUInteger value = ((NSNumber*)obj).unsignedIntegerValue;
        
        if (idx == 1 || idx == 3) {
            HIDXCTAssertWithParameters(RETURN_FROM_TEST, value == 1);
        } else {
            HIDXCTAssertWithParameters(RETURN_FROM_TEST, value == 0);
        }
        
    }];
    
    
    // Test Multiple Segment
    
    [_haEvent setIntegerValue:3 forField:@kHIDAnalyticsTestFieldMultipleSegment];
    [_haEvent setIntegerValue:9 forField:@kHIDAnalyticsTestFieldMultipleSegment];
    [_haEvent setIntegerValue:14 forField:@kHIDAnalyticsTestFieldMultipleSegment];
    
    
    updatedData = [self getEventDataForField:@kHIDAnalyticsTestFieldMultipleSegment];
    
    // since base is from 10, so 3, 9 will go to 1st bucket of second segement i.e 5th bucket
    // and 14 should go to 1st bucket of segmenet 1
    // for segment 0 , 3 is in it's 1st bucket and 1 st bucket overall
    // 9 and 14 are in 4th bucket and 4th bucket overall
    // Note : start index of bucket is 0
    // so in end following buckets should have non zero value
    // 1: 1, 4: 2, 5: 2, 6: 1
    // others should be zero
    
    NSLog(@"%@",updatedData);
    
    
    [updatedData enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop __unused) {
        
        NSUInteger value = ((NSNumber*)obj).unsignedIntegerValue;
        
        if (idx == 1 || idx == 6) {
            HIDXCTAssertWithParameters(RETURN_FROM_TEST, value == 1);
        } else if (idx == 4 || idx == 5){
            HIDXCTAssertWithParameters(RETURN_FROM_TEST, value == 2);
        } else {
            HIDXCTAssertWithParameters(RETURN_FROM_TEST, value == 0);
        }
        
    }];
    
}

@end
