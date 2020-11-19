//
//  HIDAnalyticsCAPI.m
//  HIDAnalytics
//
//  Created by AB on 11/27/18.
//

#include "HIDAnalyticsCAPI.h"
#include "HIDAnalyticsEvent.h"
#include "HIDAnalyticsHistogramEvent.h"
#include <AssertMacros.h>


HIDAnalyticsEventRef HIDAnalyticsEventCreate(CFStringRef eventName, CFDictionaryRef description)
{
    
    HIDAnalyticsEvent *event = nil;
    NSDictionary * _description  = nil;
    
    NSString * _eventName = [NSString stringWithString:(__bridge NSString*)eventName];
    __Require_Quiet(_eventName, exit);
    
    _description = description ? (__bridge_transfer NSDictionary *)CFPropertyListCreateDeepCopy(kCFAllocatorDefault, description, kCFPropertyListMutableContainersAndLeaves) : nil;
    
    event = [[HIDAnalyticsEvent alloc] initWithAttributes:_eventName description:_description];
    __Require_Quiet(event, exit);
    
exit:
    return (__bridge_retained HIDAnalyticsEventRef)event;
}

void HIDAnalyticsEventAddHistogramField(HIDAnalyticsEventRef event, CFStringRef fieldName, HIDAnalyticsHistogramSegmentConfig* segments, CFIndex count)
{
    NSString *_fieldName = [NSString stringWithString:(__bridge NSString*)fieldName];
    __Require_Quiet(_fieldName, exit);
    
    [(__bridge HIDAnalyticsEvent*)event addHistogramFieldWithSegments:_fieldName segments:segments count:(NSInteger)count];
exit:
    return;
}

void HIDAnalyticsEventAddField(HIDAnalyticsEventRef  event, CFStringRef  fieldName)
{
    NSString *_fieldName = [NSString stringWithString:(__bridge NSString*)fieldName];
    __Require_Quiet(_fieldName, exit);
    
    [(__bridge HIDAnalyticsEvent*)event addField:_fieldName];
exit:
    return;
}

void HIDAnalyticsEventActivate(HIDAnalyticsEventRef event)
{
    [(__bridge HIDAnalyticsEvent*)event activate];
}

void HIDAnalyticsEventCancel(HIDAnalyticsEventRef event)
{
    [(__bridge HIDAnalyticsEvent*)event cancel];
}

void HIDAnalyticsEventSetIntegerValueForField(CFTypeRef event, CFStringRef fieldName, uint64_t value)
{
    // We just search for field name in fields list, so NULL won't be problem here.
    // We can't afford to create copy here, since this is critical path
    [(__bridge HIDAnalyticsEvent*)event setIntegerValue:value forField:(__bridge NSString*)fieldName];
}

HIDAnalyticsHistogramEventRef __nullable HIDAnalyticsHistogramEventCreate(CFStringRef eventName, CFDictionaryRef _Nullable description, CFStringRef fieldName,HIDAnalyticsHistogramSegmentConfig*  segments, CFIndex count)
{
    
    HIDAnalyticsHistogramEvent *event = nil;
    NSDictionary * _description = nil;
    
    
    NSString * _eventName = [NSString stringWithString:(__bridge NSString*)eventName];
    __Require_Quiet(_eventName, exit);
    
    _description = description ? (__bridge_transfer NSDictionary *)CFPropertyListCreateDeepCopy(kCFAllocatorDefault, description, kCFPropertyListMutableContainersAndLeaves) : nil;
    
    event = [[HIDAnalyticsHistogramEvent alloc] initWithAttributes:_eventName description:_description];
    
    __Require_Quiet(event, exit);
    
    [event addHistogramFieldWithSegments:(__bridge NSString*)fieldName segments:segments count:(NSInteger)count];
exit:
    return (__bridge_retained HIDAnalyticsHistogramEventRef)event;
}

void HIDAnalyticsHistogramEventSetIntegerValue(HIDAnalyticsHistogramEventRef event, uint64_t value)
{
    [(__bridge HIDAnalyticsHistogramEvent*)event setIntegerValue:value];
}
