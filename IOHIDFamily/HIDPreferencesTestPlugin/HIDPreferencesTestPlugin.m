//
//  HIDPreferencesTestPlugin.m
//  HIDPreferencesTestPlugin
//
//  Created by AB on 10/8/19.
//

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import "HIDPreferencesTestPlugin.h"
#import <os/log.h>
#import <IOKit/hid/IOHIDEventSystemKeys.h>
#import <HIDPreferences/HIDPreferencesCAPI.h>
#import "IOHIDDebug.h"

static NSString *kHIDPreferencesTestPluginDomain = @"com.apple.HIDPreferencesTestPlugin";


@implementation HIDPreferencesTestPlugin

-(nullable instancetype) initWithSession:(HIDSession *)session {
    
    self = [super init];
    if (!self) {
        return self;
    }
    
    _session = session;

    return self;
}

- (nullable id)propertyForKey:(NSString *)key {
    
   HIDLogDebug("HIDPreferencesTestPlugin::propertyForKey: %@", key);
    
    id result = nil;
    
    if (key && [key isEqualToString:@(kIOHIDSessionFilterDebugKey)]) {
        NSMutableDictionary * debug = [NSMutableDictionary new];
        debug[@"Class"] = @"HIDPreferencesTestPlugin";
        
        result = debug;
    } else if (key && [key isEqualToString:@"HIDPreferencesTest"]) {
        
      result =  (__bridge_transfer id)HIDPreferencesCopy((__bridge CFStringRef)key, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, (__bridge CFStringRef)kHIDPreferencesTestPluginDomain);
        HIDLogDebug("HIDPreferencesTestPlugin::propertyForKey: %@ Value : %@", key, result);
        if (!result) {
            result = @"unknown";
        }
    }
    
    return result;
}

- (BOOL)setProperty:(nullable id)value forKey:(NSString *)key {
    
    HIDLogDebug("HIDPreferencesTestPlugin::setProperty: key : %@, value : %@", key, value);
    
    bool ret = NO;
    
    if (key && [key isEqualToString:@"HIDPreferencesTest"]) {
        
        HIDPreferencesSet((__bridge CFStringRef)key, (__bridge CFTypeRef)value, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, (__bridge CFStringRef)kHIDPreferencesTestPluginDomain);
        
        HIDPreferencesSynchronize(kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, (__bridge CFStringRef)kHIDPreferencesTestPluginDomain);
        
        ret = YES;
    }
    
    return ret;
}

-(void) activate {
}

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event forService:(HIDEventService * __unused)service {
    return event;
}


@end
