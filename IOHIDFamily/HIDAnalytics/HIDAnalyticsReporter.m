//
//  HIDAnalyticsReporter.m
//  HIDAnalytics
//
//  Created by AB on 11/26/18.
//

#import "HIDAnalyticsReporter.h"
#import "HIDAnalyticsEvent.h"
#import "HIDAnalyticsEventPrivate.h"


#import <dlfcn.h>
#import <CoreAnalytics/CoreAnalytics.h>
#import <os/log.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>

#define HID_ANALYTICS_LOG_INTERVAL 172000 //sec

@implementation HIDAnalyticsReporter
{
    NSMutableSet<HIDAnalyticsEvent*> *_events;
    dispatch_queue_t                 _queue;
    dispatch_source_t                _timer;
}

-(nullable instancetype) init
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    __weak HIDAnalyticsReporter *weakSelf = self;
    
    _events = [[NSMutableSet alloc] init];
    _queue = dispatch_queue_create("com.apple.hidanalytics", dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_UTILITY, 0));
    
    if (!_queue) {
        return nil;
    }
    
    _timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
    
    if (!_timer) {
        return nil;
    }
    
    dispatch_source_set_event_handler(_timer, ^{
        
        __strong HIDAnalyticsReporter *strongSelf = weakSelf;
        
        if (!strongSelf) {
            return;
        }
        
         for (HIDAnalyticsEvent *event in strongSelf->_events) {
    
             // Need to copy value as original value will be reset
             // Note : Call to AnalyticsSendEvent doesn't ensure immediate xpc.
             // reference may be saved and used when ready to send
             
             os_log(OS_LOG_DEFAULT, "HIDAnalytics Timer Send event %@",event.name);
             
             [strongSelf logAnalyticsEvent:event];
             event.value = @(0);
         }
        
    });
    
    dispatch_source_set_timer(_timer, DISPATCH_TIME_FOREVER, 0, 0);
    dispatch_activate(_timer);
    
    return self;
}

-(void) dealloc
{
    [_events removeAllObjects];
    
    [self stop];
    
    dispatch_source_cancel(_timer);
    
}

-(NSArray*) createBucketData:(NSString*) fieldName fieldvalue:(id) fieldvalue fieldDescription:(NSString*) fieldDescription
{
    
    if (!fieldvalue) return nil;
    
    if (![fieldvalue isKindOfClass:[NSArray class]]) {
        return nil;
    }
    
    NSArray *bucketData = (NSArray*)fieldvalue;
    
    NSMutableArray *ret = [[NSMutableArray alloc] init];
    
    [bucketData enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull __unused stop) {
        
        if (![obj isKindOfClass:[NSNumber class]]) {
            return;
        }
        
        //dimension field
        NSString *analyicsFieldKeyID = [NSString stringWithFormat:@"%@BucketID",fieldName];
        NSNumber *analyticsFieldKeyData = @(idx);
        NSString *analyticsFieldValueID = [NSString stringWithFormat:@"%@BucketCount",fieldName];
        NSNumber *analyticsFieldValueData = [(NSNumber*)obj copy];
        //dimension field
        NSString *descriptionID = [NSString stringWithFormat:@"%@Description",fieldName];
        NSString *descriptionData = fieldDescription;
        
        NSMutableDictionary *fieldInfo = [[NSMutableDictionary alloc] init];
        fieldInfo[analyicsFieldKeyID] = analyticsFieldKeyData;
        fieldInfo[analyticsFieldValueID] = analyticsFieldValueData;
        
        if (descriptionData) {
            fieldInfo[descriptionID] = descriptionData;
        }
        
        [ret addObject:fieldInfo];
        
        /*
         Example :
         suppose we have to log following data
         {
             0:3,
             1: 2,
             2:0,
             3: 0
         }
         
         Above is data for 4 buckets with bucketID and bucketCount value
         
         to log given data in analytics we have to create
         1. Events fields as
            -> <FieldName>BucketID
            -> <FieldName>BucketCount
         
         and log each dictionary as following:
         {
         <>BucketID : 0
         <>BucketCount : 3
         } ,
         {
         <>BucketID : 1
         <>BucketCount : 2
         } ,
         {
         <>BucketID : 2
         <>BucketCount : 0
         } ,
         {
         <>BucketID : 3
         <>BucketCount : 0
         } ,
         
         Note : Given keys for dictinary represents eventFields
         where <>BucketID is bounded integer field (*use bounded integer for dimensions, *dimesion in CA term is something we can use to filter data,)
         <BucketCount> is normal integer field
         
         TODO : currently CA doesn't support list data structure, adopt new APIs once supported
         */
        
        
    }];
    
    return ret;
    
}

-(void) logAnalyticsEvent:(NSString*) eventName eventDescription:(NSString*)eventDescription eventValue:(NSArray*) eventValue
{
    
    if (!eventValue) return;
    
    [eventValue enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx __unused , BOOL * _Nonnull stop __unused) {
        
        if (![obj isKindOfClass:[NSDictionary class]]) {
            return;
        }
        
        NSDictionary *fieldInfo = (NSDictionary*)obj;
        NSString *fieldName = fieldInfo[@"Name"];
        NSNumber *fieldType = fieldInfo[@"Type"];
        id fieldValue = fieldInfo[@"Value"];
        
        if (fieldValue == nil || fieldName == nil || fieldType == nil) {
            return;
        }
        
        if ([fieldType unsignedIntegerValue] == kHIDAnalyticsEventTypeHistogram) {
            
            NSArray* processedInfo = [self createBucketData:fieldName fieldvalue:fieldValue fieldDescription:eventDescription];
            
            if (!processedInfo) return;
            
            [processedInfo enumerateObjectsUsingBlock:^(id  _Nonnull objP, NSUInteger idxP __unused, BOOL * _Nonnull stopP __unused) {
                
                NSDictionary *infoToLog = (NSDictionary*)objP;
                AnalyticsSendEventLazy(eventName, ^NSDictionary<NSString *,NSObject *> *{
                    return infoToLog;
                });
                
            }];
            
            
        } else {
            
            NSDictionary *infoToLog = @{@"FieldName" : fieldName, @"FieldValue" : fieldValue};
            
            AnalyticsSendEventLazy(eventName, ^NSDictionary<NSString *,NSObject *> *{
                return infoToLog;
            });
        }
        
    }];
    
}

-(void) logAnalyticsEvent:(HIDAnalyticsEvent*) event
{
   
    NSString *eventName = event.name;
    NSString *eventDescription = event.desc ? [NSString stringWithFormat:@"%@",event.desc] : nil;
    NSArray *eventValue = event.value;
    
    [self logAnalyticsEvent:eventName eventDescription:eventDescription eventValue:eventValue];
    
}

-(void) registerEvent:(HIDAnalyticsEvent*) event
{
    dispatch_sync(_queue, ^{
        
        [_events addObject:event];
        
    });
   
    return;
}

-(void) unregisterEvent:(HIDAnalyticsEvent*) event
{
   
    dispatch_sync(_queue, ^{
        
        // don't send analaytics always here since it can be lot of churn with service going away on dark sleep cycles
        // send analytics only when we have never send any analytics for given event. This doesn't buy us much in
        // terms of data , but we use analytics to track different kind of hid sevices, so let's log if service terminate
        // before timer had chance to log it.
        if (!event.isLogged) {
            os_log(OS_LOG_DEFAULT, "HIDAnalytics Unregister Send event %@",event.name);
            [self logAnalyticsEvent:event];
        }
        
        [_events removeObject:event];
        
    });
}

-(void) start
{
    dispatch_source_set_timer(_timer, dispatch_time(DISPATCH_TIME_NOW, 0), NSEC_PER_SEC * HID_ANALYTICS_LOG_INTERVAL, 0);
}

-(void) stop
{
    dispatch_source_set_timer(_timer, DISPATCH_TIME_FOREVER, 0, 0);
    
}

-(void) dispatchAnalyticsForEvent:(HIDAnalyticsEvent*) event
{
    __block NSArray *eventValue = nil;
    __block NSString *eventName = nil;
    __block NSString *eventDescription = nil;
    

    dispatch_sync(_queue, ^{
        
        if (![_events containsObject:event]) {
            os_log_error(OS_LOG_DEFAULT, "HIDAnalytics Event not registered");
            return;
        }
        
        eventValue = [[NSArray alloc] initWithArray:(NSArray*)event.value];
        eventName = event.name;
        eventDescription = event.desc ? [NSString stringWithFormat:@"%@",event.desc] : nil;
    });
    
    
    if (!eventValue) {
        return;
    }
    
    os_log(OS_LOG_DEFAULT, "HIDAnalytics Set Value Send event %@",event.name);
    
    __weak HIDAnalyticsReporter* weakSelf = self;
    
    dispatch_async(_queue, ^{
        
        __strong HIDAnalyticsReporter* strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        
        [strongSelf logAnalyticsEvent:eventName eventDescription:eventDescription eventValue:eventValue];
        
    });
}
@end

