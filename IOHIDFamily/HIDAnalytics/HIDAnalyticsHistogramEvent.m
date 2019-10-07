//
//  HIDAnalyticsHistogramEvent.m
//  HIDAnalytics
//
//  Created by AB on 5/1/19.
//

#import "HIDAnalyticsHistogramEvent.h"
#import "HIDAnalyticsHistogramEventField.h"
#import "HIDAnalyticsEventPrivate.h"


@implementation HIDAnalyticsHistogramEvent
{
    HIDAnalyticsHistogramEventField *_field;
    BOOL  _isUpdated;
}

- (nullable instancetype) initWithAttributes:(NSString*) eventName description:(NSDictionary*) description
{
    
    self = [super initWithAttributes:eventName description:description];
    if (!self) {
        return nil;
    }
    
    return self;
}

- (void) setIntegerValue:(uint64_t) value
{
    [_field setIntegerValue:value];
    _isUpdated = YES;
}

- (void) addHistogramFieldWithSegments:(NSString*)fieldName segments:(HIDAnalyticsHistogramSegmentConfig *)segments count:(NSInteger)count
{
    _field = [[HIDAnalyticsHistogramEventField alloc] initWithAttributes:fieldName segments:segments count:count];
}

- (void) addField:(NSString* __unused) fieldName
{
    //unwanted
}

- (void) setIntegerValue:(uint64_t __unused) value forField:(NSString* __unused) fieldName
{
    //unwanted
}

-(id) value
{
    // add static info about event
    NSMutableArray *ret = nil;
    
    if (_isUpdated == NO) {
        return ret;
    }
    
    ret = [[NSMutableArray alloc] init];
    
    NSMutableDictionary *fieldInfo = [[NSMutableDictionary alloc] init];
    fieldInfo[@"Name"] = _field.fieldName;
    fieldInfo[@"Type"] = @(kHIDAnalyticsEventTypeHistogram);
    fieldInfo[@"Value"] = _field.value;
    [ret addObject:fieldInfo];
    
    _isUpdated = NO;
    
    return ret.count == 0 ? NULL : ret;
    
}

-(void) setValue:(id) value
{
    _field.value = value;
}

@end
