//
//  HIDAnalyticsEvent.m
//  HIDAnalytics
//
//  Created by AB on 5/1/19.
//

#import <os/log.h>
#import "HIDAnalyticsEvent.h"
#import "HIDAnalyticsReporter.h"
#import "HIDAnalyticsEventField.h"
#import "HIDAnalyticsHistogramEventField.h"
#import "HIDAnalyticsEventPrivate.h"

static HIDAnalyticsReporter *__hidAnalyticsReporter = NULL;

@implementation HIDAnalyticsEvent
{
    NSMutableDictionary  *_fields;
    BOOL _isUpdated;
}

-(nullable instancetype) initWithAttributes:(NSString*) eventName description:(NSDictionary*) description
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    static dispatch_once_t once;
    
    dispatch_once(&once, ^{
        __hidAnalyticsReporter = [[HIDAnalyticsReporter alloc] init];
        [__hidAnalyticsReporter start];
    });
    
    self.name = eventName;
    self.desc = description;
    
    return self;
}

-(void) activate
{
    [__hidAnalyticsReporter registerEvent:self];
}

-(void) cancel
{
    [__hidAnalyticsReporter unregisterEvent:self];
}


-(NSString*) description
{
    NSMutableDictionary *desc = [[NSMutableDictionary alloc] init];
    desc[@"EventDescription"] = self.desc;
    desc[@"EventValue"] = self.value;
    
    NSData * jsonData = [NSJSONSerialization  dataWithJSONObject:desc options:0 error:nil];
    return [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
}


- (void)addField:(NSString*) fieldName
{
 
    if (!_fields) {
        _fields = [[NSMutableDictionary alloc] init];
    }
    
    HIDAnalyticsEventField *field = [[HIDAnalyticsEventField alloc] initWithName:fieldName];
    
    _fields[fieldName] = field;
}

- (void) setIntegerValue:(uint64_t) value forField:(NSString*) fieldName
{
    id<HIDAnalyticsEventFieldProtocol> field = [_fields objectForKey:fieldName];
    
    if (!field) {
        return;
    }
    
    [field setIntegerValue:value];
    _isUpdated = YES;
    
    if ([field isKindOfClass:[HIDAnalyticsEventField class]]) {
        [__hidAnalyticsReporter dispatchAnalyticsForEvent:self];
    }
}

- (void) setIntegerValue:(uint64_t __unused) value
{
    
}

-(id) value
{
    NSMutableArray *ret = nil;
    
    if (_isUpdated == NO) {
        return ret;
    }
    
    ret = [[NSMutableArray alloc] init];
    
    [_fields enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL __unused *stop) {
        
        id<HIDAnalyticsEventFieldProtocol> field = obj;
        NSMutableDictionary *fieldInfo = [[NSMutableDictionary alloc] init];
        fieldInfo[@"Name"] = key;
        
        if ([field isKindOfClass:[HIDAnalyticsHistogramEventField class]]) {
            fieldInfo[@"Type"] = @(kHIDAnalyticsEventTypeHistogram);
        } else {
            fieldInfo[@"Type"] = @(kHIDAnalyticsEventTypeBasic);
        }
        
        fieldInfo[@"Value"] = field.value;
        [ret addObject:fieldInfo];
        
    }];
    
    _isUpdated = NO;
    
    _isLogged |= (ret.count == 0);
    
    return ret.count == 0 ? NULL : ret;
    
}

-(void) setValue:(id) value
{
    [_fields enumerateKeysAndObjectsUsingBlock:^(id __unused key, id obj, BOOL __unused *stop) {
        
        id<HIDAnalyticsEventFieldProtocol> field = obj;
        
        field.value = value;
        
    }];
}

- (void)addHistogramFieldWithSegments:(NSString*) fieldName segments:(HIDAnalyticsHistogramSegmentConfig *)segments count:(NSInteger)count
{
    
    if (!_fields) {
        _fields = [[NSMutableDictionary alloc] init];
    }
    
    HIDAnalyticsHistogramEventField *field = [[HIDAnalyticsHistogramEventField alloc] initWithAttributes:fieldName segments:segments count:count];
    
    // don't expect this to be null
    _fields[fieldName] = field;
    
}


@end
