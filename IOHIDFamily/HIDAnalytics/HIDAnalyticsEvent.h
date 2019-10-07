//
//  HIDAnalyticsEvent.h
//  IOHIDFamily
//
//  Created by AB on 5/1/19.
//


#import <Foundation/Foundation.h>
#import <HIDAnalytics/HIDAnalyticsCAPI.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, HIDAnalyticsEventType) {
    kHIDAnalyticsEventTypeUnknown=0,
    kHIDAnalyticsEventTypeHistogram,
    kHIDAnalyticsEventTypeBasic,
};

@interface HIDAnalyticsEvent : NSObject

@property (retain) NSString* name;
@property (retain) NSDictionary *desc;

- (instancetype)init NS_UNAVAILABLE;

- (nullable instancetype) initWithAttributes:(NSString*) eventName description:(NSDictionary*) description;

- (void) activate;

- (void) cancel;

- (void) addHistogramFieldWithSegments:(NSString*)fieldName segments:(HIDAnalyticsHistogramSegmentConfig *)segments count:(NSInteger)count;

- (void) addField:(NSString*) fieldName;

- (void) setIntegerValue:(uint64_t) value forField:(NSString*) fieldName;

- (void) setIntegerValue:(uint64_t) value;

@end

NS_ASSUME_NONNULL_END

