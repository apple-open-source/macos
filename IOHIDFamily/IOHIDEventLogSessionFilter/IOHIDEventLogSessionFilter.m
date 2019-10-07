//
//  IOHIDEventLogSessionFilter.m
//  IOHIDEventLogSessionFilter
//
//  Created by AB on 10/9/18.
//

#import <Foundation/Foundation.h>
#import "IOHIDEventLogSessionFilter.h"
#import "IOHIDDebug.h"
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/hid/IOHIDLibPrivate.h>
#import <IOKit/hid/IOHIDKeys.h>
#import <IOKit/hid/IOHIDEvent.h>
#import <os/log.h>
#import <AssertMacros.h>
#import <IOKit/hid/IOHIDUsageTables.h>
#import <IOKit/hid/IOHIDEventSystemKeys.h>


static inline void logHIDEvent(uint8_t level, NSData* eventData, uint8_t category) {
    os_log_with_type(_IOHIDLogCategory(category), level, "%{IOHIDFamily:event}.*P", (int)eventData.length, [eventData bytes]);
}

typedef struct {
    uint8_t enabled:1;
    uint8_t level:4;
    uint8_t reserverd:3;
} PreferenceStatus;

@implementation IOHIDEventLogSessionFilter
{
    PreferenceStatus      _eventLogPreferenceStatus[kIOHIDEventTypeCount];
    NSDictionary          *_transportLogPreferenceStatus;
    BOOL                  _logForTransport;
}

- (nullable instancetype)initWithSession:(HIDSession *)session
{
    self = [super init];
    if (!self) {
        return self;
    }
    
    HIDLogDebug("IOHIDEventLogSessionFilter::initWithSession: %@", session);
    
    _transportLogPreferenceStatus = [self createTransportLogPreferenceTracker];
    
    id transportPreferencesValue = (__bridge_transfer id)CFPreferencesCopyAppValue(CFSTR(kIOHIDLoggingPreferenceValueForTransportType), kIOHIDFamilyPreferenceApplicationID);
    
    id eventPreferencesValue = (__bridge_transfer id)CFPreferencesCopyAppValue(CFSTR(kIOHIDLoggingPreferenceValueForEventType), kIOHIDFamilyPreferenceApplicationID);
    
    [self updateEventTypePreference:eventPreferencesValue];
    [self updateTransportTypePreference:transportPreferencesValue];
    
    return self;
}
-(NSDictionary*) createTransportLogPreferenceTracker
{
    NSMutableDictionary *transportLogPreferenceStatus = [[NSMutableDictionary alloc] init];
    
    NSArray *knownTransportTypes = @[@"usb", @"bluetooth", @"ipa", @"airplay", @"i2c", @"spu", @"spi"];
    
    for (NSString *transportType in knownTransportTypes) {
        PreferenceStatus status;
        bzero(&status, sizeof(PreferenceStatus));
        transportLogPreferenceStatus[transportType] = [[NSMutableData alloc] initWithBytes:&status length:sizeof(PreferenceStatus)];
    }
    return transportLogPreferenceStatus;
}
- (void)dealloc
{
    HIDLogDebug("IOHIDEventLogSessionFilter dealloc");
}

- (NSString *)description
{
    return @"IOHIDEventLogSessionFilter";
}

- (nullable id)propertyForKey:(NSString *)key
{
    id result = nil;
    
    if ([key isEqualToString:@(kIOHIDSessionFilterDebugKey)]) {
        NSMutableDictionary * debug = [NSMutableDictionary new];
        debug[@"Class"] = @"IOHIDEventLogSessionFilter";
        
        result = debug;
    }
    
    return result;
}
-(void) updateEventTypePreference:(id) preferencesValue
{
    NSArray *eventPreferences = nil;
    
    require(preferencesValue, exit);
    require([preferencesValue isKindOfClass:[NSArray class]], exit);
    
    eventPreferences = (NSArray*)preferencesValue;
    
    for (id preference in eventPreferences) {
        
        if (![preference isKindOfClass:[NSDictionary class]]) {
            continue;
        }
        
        NSDictionary *eventPreference = (NSDictionary*)preference;
        bool         enabled = false;
        uint8_t      level   = OS_LOG_TYPE_INFO;
        uint8_t      eventType = kIOHIDEventTypeCount;
        
        if ([eventPreference objectForKey:@"enabled"]) {
            enabled = [eventPreference[@"enabled"] isKindOfClass:[NSNumber class]] ? ((NSNumber*)eventPreference[@"enabled"]).boolValue : NO;
        }
        
        if ([eventPreference objectForKey:@"level"]) {
            level = [eventPreference[@"level"] isKindOfClass:[NSNumber class]]  ? ((NSNumber*)eventPreference[@"level"]).unsignedCharValue : OS_LOG_TYPE_INFO;
        }
        
        if ([eventPreference objectForKey:@"type"]) {
            eventType = [eventPreference[@"type"] isKindOfClass:[NSNumber class]] ? ((NSNumber*)eventPreference[@"type"]).unsignedCharValue : kIOHIDEventTypeCount;
        }
        
        if (eventType >= kIOHIDEventTypeCount) {
            continue;
        }
        
        _eventLogPreferenceStatus[eventType].enabled = enabled;
        _eventLogPreferenceStatus[eventType].level = level;
    }
    
exit:
    return;
    
}
-(void) updateTransportTypePreference:(id) preferencesValue
{
    NSArray *transportPreferences = nil;
    PreferenceStatus *status = NULL;
    
    require(preferencesValue, exit);
    require([preferencesValue isKindOfClass:[NSArray class]], exit);
    
    transportPreferences = (NSArray*)preferencesValue;
    
    _logForTransport = NO;
    
    for (id preference in transportPreferences) {
        
        if (![preference isKindOfClass:[NSDictionary class]]) {
            continue;
        }
        
        NSDictionary *transportPreference = (NSDictionary*)preference;
        NSString*    transport = nil;
        BOOL         enabled = NO;
        uint8_t      level = OS_LOG_TYPE_INFO;
        
        if ([transportPreference objectForKey:@"type"]) {
            transport = [transportPreference[@"type"] isKindOfClass:[NSString class]] ? (NSString*)transportPreference[@"type"] : nil;
        }
        
        if (!transport || ![_transportLogPreferenceStatus objectForKey:transport]) {
            continue;
        }
        
        if ([transportPreference objectForKey:@"enabled"]) {
            enabled = [transportPreference[@"enabled"] isKindOfClass:[NSNumber class]] ? ((NSNumber*)transportPreference[@"enabled"]).boolValue : NO;
        }
        
        if ([transportPreference objectForKey:@"level"]) {
            level = [transportPreference[@"level"] isKindOfClass:[NSNumber class]] ? ((NSNumber*)transportPreference[@"level"]).unsignedCharValue : OS_LOG_TYPE_INFO;
        }
        
        status = (PreferenceStatus*)[(NSData*)_transportLogPreferenceStatus[transport] bytes];
        
        if (!status) {
            continue;
        }
        
        status->enabled = enabled;
        status->level = level;
        
        _logForTransport |= enabled;
        
    }
exit:
    return;
}
- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key
{
    BOOL result = true;
    
    if ([key isEqualToString:@kIOHIDReloadLoggingPreferences]) {
        
        HIDLog("IOHIDEventLogSessionFilter Reload Logging Preferences");
        
        id transportPreferencesValue = (__bridge_transfer id)CFPreferencesCopyAppValue(CFSTR(kIOHIDLoggingPreferenceValueForTransportType), kIOHIDFamilyPreferenceApplicationID);
        
        id eventPreferencesValue = (__bridge_transfer id)CFPreferencesCopyAppValue(CFSTR(kIOHIDLoggingPreferenceValueForEventType), kIOHIDFamilyPreferenceApplicationID);
        
        [self updateEventTypePreference:eventPreferencesValue];
        [self updateTransportTypePreference:transportPreferencesValue];
        
    } else if ([key isEqualToString:@kIOHIDLoggingPreferenceValueForEventType]) {
        
        HIDLog("IOHIDEventLogSessionFilter set property kIOHIDLoggingPreferenceValueForEventType");
        
        [self updateEventTypePreference:value];
        
    } else if ([key isEqualToString:@kIOHIDLoggingPreferenceValueForTransportType]) {
        
        HIDLog("IOHIDEventLogSessionFilter set property kIOHIDLoggingPreferenceValueForTransportType");
        
        [self updateTransportTypePreference:value];
        
    } else {
        result = false;
    }
    
    return result;
}
-(BOOL) voidEventUsageIfTypeKeyboardKeypad:(HIDEvent*) event
{
    BOOL ret = NO;
    NSInteger usagePage = [event integerValueForField:kIOHIDEventFieldKeyboardUsagePage];
    if (usagePage == kHIDPage_KeyboardOrKeypad) {
        [event setIntegerValue:0 forField:kIOHIDEventFieldKeyboardUsage];
        ret = YES;
    }
    return ret;
}
-(NSData*) createModifiedkeyboardEventData:(HIDEvent*) event
{
    
    NSData   *ret = nil;
    HIDEvent *eventCopy = nil;
    
    // walk through event to determine if usage page is keyboard/trackpad
    // we need to null usages for those keyboard event for privacy concerns
    
    eventCopy = [event copy];
    
    require(eventCopy, exit);
    
    // check parent event
    if (eventCopy.type == kIOHIDEventTypeKeyboard) {
        [self voidEventUsageIfTypeKeyboardKeypad:eventCopy];
    }
    
    // check children
    for (HIDEvent *childEvent in eventCopy.children) {
        
        if (childEvent.type == kIOHIDEventTypeKeyboard) {
            [self voidEventUsageIfTypeKeyboardKeypad:childEvent];
        }
    }
    
    ret = [eventCopy serialize:HIDEventSerializationTypeFast error:nil];
    
exit:
    return ret;
    
}
-(BOOL) logForEventType:(HIDEvent*) event
{
    NSData   *eventData = nil;
    BOOL ret = NO;
    
    require(_eventLogPreferenceStatus[event.type].enabled, exit);
    
    if ([event conformsToEventType:kIOHIDEventTypeKeyboard]) {
        
        eventData = [self createModifiedkeyboardEventData:event];
        
    } else {
        eventData = [event serialize:HIDEventSerializationTypeFast error:nil];
    }
    
    require(eventData, exit);
    
    logHIDEvent(_eventLogPreferenceStatus[event.type].level, eventData, kIOHIDLogCategoryService);
    
    ret = YES;
    
exit:
    return ret;
}
-(BOOL) logForTransportType:(HIDEvent*) event service:(HIDEventService*) service
{
    NSString  *tmp = nil;
    NSString  *transport = nil;
    NSData    *eventData = nil;
    BOOL      ret = NO;
    uint8_t   logCategory = kIOHIDLogCategoryService;
    PreferenceStatus *status = NULL;
    
    require(service, exit);
    
    // check if all transport preferences are disabled then maybe we can skip
    // get transport key for service, getting transport key may be little tricky
    // in terms of time sometimes
    
    require(_logForTransport, exit);
    
    tmp = [service propertyForKey:@kIOHIDTransportKey];
    
    require(tmp, exit);
    
    transport = [tmp lowercaseString];
    
    require([_transportLogPreferenceStatus objectForKey:transport], exit);
    
    status = (PreferenceStatus*)[(NSData*)_transportLogPreferenceStatus[transport] bytes];
    
    require(status, exit);
    
    require(status->enabled, exit);
    
    eventData = [event serialize:HIDEventSerializationTypeFast error:nil];
    
    require(eventData, exit);
    
    if ([transport isEqualToString:@"airplay"] || [transport isEqualToString:@"iap"]) {
        logCategory = kIOHIDServiceLogCategoryCarplay;
    }
    
    logHIDEvent(status->level , eventData, logCategory);
    
    ret = YES;
    
exit:
    return ret;
}
- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                        forService:(HIDEventService *)service
{
    
    require(event, exit);
    
    if (![self logForEventType:event]) {
        [self logForTransportType:event service:service];
    }
exit:
    return event;
}

- (void)activate
{
    HIDLogInfo("IOHIDEventLogSessionFilter activate");
}

- (void)serviceNotification:(HIDEventService * __unused)service added:(BOOL __unused)added
{
    //
}
@end

