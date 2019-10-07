//
//  HIDAnalyticsCAPI.m
//  HIDAnalytics
//
//  Created by AB on 11/27/18.
//

#include "HIDAnalyticsCAPI.h"
#include "HIDAnalyticsEvent.h"
#include "HIDAnalyticsHistogramEvent.h"

HIDAnalyticsEventRef HIDAnalyticsEventCreate(CFStringRef eventName, CFDictionaryRef description)
{
    
    HIDAnalyticsEvent *event = nil;
    
    event = [[HIDAnalyticsEvent alloc] initWithAttributes:(__bridge NSString*)eventName description:(__bridge NSDictionary*)description];
    
    if (!event) {
        return NULL;
    }
    
    return (__bridge_retained HIDAnalyticsEventRef)event;
}

void HIDAnalyticsEventAddHistogramField(HIDAnalyticsEventRef event, CFStringRef fieldName, HIDAnalyticsHistogramSegmentConfig* segments, CFIndex count)
{
    [(__bridge HIDAnalyticsEvent*)event addHistogramFieldWithSegments:(__bridge NSString*)fieldName segments:segments count:(NSInteger)count];
}

void HIDAnalyticsEventAddField(HIDAnalyticsEventRef  event, CFStringRef  fieldName)
{
    [(__bridge HIDAnalyticsEvent*)event addField:(__bridge NSString*)fieldName];
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
    [(__bridge HIDAnalyticsEvent*)event setIntegerValue:value forField:(__bridge NSString*)fieldName];
}

HIDAnalyticsHistogramEventRef __nullable HIDAnalyticsHistogramEventCreate(CFStringRef eventName, CFDictionaryRef _Nullable description, CFStringRef fieldName,HIDAnalyticsHistogramSegmentConfig*  segments, CFIndex count)
{
    
    HIDAnalyticsHistogramEvent *event = nil;
    
    event = [[HIDAnalyticsHistogramEvent alloc] initWithAttributes:(__bridge NSString*)eventName description:(__bridge NSDictionary*)description];
    
    if (!event) {
        return NULL;
    }
    
    [event addHistogramFieldWithSegments:(__bridge NSString*)fieldName segments:segments count:(NSInteger)count];
    
    return (__bridge_retained HIDAnalyticsHistogramEventRef)event;
}

void HIDAnalyticsHistogramEventSetIntegerValue(HIDAnalyticsHistogramEventRef event, uint64_t value)
{
    [(__bridge HIDAnalyticsHistogramEvent*)event setIntegerValue:value];
}
