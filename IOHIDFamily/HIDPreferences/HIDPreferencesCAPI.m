//
//  HIDPreferencesCAPI.m
//  HIDPreferences
//
//  Created by AB on 10/7/19.
//

#import "HIDPreferences.h"
#import "HIDPreferencesCAPI.h"
#import "HIDPreferencesPrivate.h"
#import <AssertMacros.h>
#import <TargetConditionals.h>
#import "IOHIDDebug.h"

static HIDPreferences* __hidPreferences = nil;

#pragma mark -
static inline HIDPreferences *getHIDPreferencesInterface(void) {
    
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        if (!__hidPreferences) {
             __hidPreferences =  [[HIDPreferences alloc] init];
        }
    });
    return __hidPreferences;
}

#pragma mark -
#pragma mark Set

void HIDPreferencesSet(CFStringRef key, CFTypeRef value , CFStringRef user, CFStringRef host, CFStringRef domain) {
#if TARGET_OS_OSX

    HIDPreferences *hidPreferences = getHIDPreferencesInterface();
    require(hidPreferences, exit);
    
    @autoreleasepool {
        [hidPreferences set:(__bridge NSString*)key value:(__bridge id)value user:(__bridge NSString*)user host:(__bridge NSString*)host domain:(__bridge NSString*)domain];
    }
exit:
#else
    CFPreferencesSetValue(key, value, domain, user , host);
#endif
    return;
}

#pragma mark -
#pragma mark Copy

CFTypeRef __nullable HIDPreferencesCopy(CFStringRef key, CFStringRef user, CFStringRef host, CFStringRef domain) {
#if TARGET_OS_OSX
    __block id ret = nil;

    dispatch_semaphore_t semaphore = nil;
    HIDPreferences *hidPreferences = nil;
    
    semaphore = dispatch_semaphore_create(0);
    require(semaphore, exit);
    
    hidPreferences = getHIDPreferencesInterface();
    require(hidPreferences, exit);
    
    @autoreleasepool {
        [hidPreferences copy:(__bridge NSString*)key user:(__bridge NSString*)user host:(__bridge NSString*)host domain:(__bridge NSString*)domain reply:^(id  _Nullable data) {
            ret = data;
            dispatch_semaphore_signal(semaphore);
        }];
        if (dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, kHIDXPCTimeoutInSec * NSEC_PER_SEC)) != 0) {
            HIDLogError("HIDPreferences XPC connection timeout");
        }
    }
exit:
    return ret ? (__bridge_retained CFTypeRef)ret : NULL;
#else
    return  CFPreferencesCopyValue(key, domain, user, host);
#endif
}

#pragma mark -
#pragma mark Synchronize

void HIDPreferencesSynchronize(CFStringRef user, CFStringRef host, CFStringRef domain) {
#if TARGET_OS_OSX
    HIDPreferences *hidPreferences = getHIDPreferencesInterface();
    require(hidPreferences, exit);
        
    @autoreleasepool {
        [hidPreferences synchronize:(__bridge NSString*)user host:(__bridge NSString*)host domain:(__bridge NSString*)domain];
    }
exit:
#else
    CFPreferencesSynchronize(domain, user, host);
#endif
        return;
}

#pragma mark -
#pragma mark copy multiple

CFDictionaryRef __nullable HIDPreferencesCopyMultiple(CFArrayRef __nullable keys, CFStringRef user, CFStringRef host, CFStringRef domain) {
    
#if TARGET_OS_OSX
    dispatch_semaphore_t semaphore = nil;
    HIDPreferences *hidPreferences = nil;
    __block id ret = nil;
    
    semaphore =  dispatch_semaphore_create(0);
    require(semaphore, exit);
    
    hidPreferences = getHIDPreferencesInterface();
    require(hidPreferences, exit);
    
    @autoreleasepool {
        [hidPreferences copyMultiple:(__bridge NSArray*)keys user:(__bridge NSString*)user host:(__bridge NSString*)host domain:(__bridge NSString*)domain reply:^(id  _Nullable data) {
            
            ret = data;
            dispatch_semaphore_signal(semaphore);
            
        }];
        if (dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, kHIDXPCTimeoutInSec * NSEC_PER_SEC)) != 0) {
            HIDLogError("HIDPreferences XPC connection timeout");
        }
    }
exit:
    return ret ? (__bridge_retained CFTypeRef)ret : NULL;
#else
    return CFPreferencesCopyMultiple(keys, domain, user, host);
#endif
}


#pragma mark -
#pragma mark set multiple

void HIDPreferencesSetMultiple(CFDictionaryRef __nullable keysToSet , CFArrayRef __nullable keysToRemove, CFStringRef user, CFStringRef host, CFStringRef domain) {
    
#if TARGET_OS_OSX
    
    HIDPreferences *hidPreferences = nil;
    
    hidPreferences = getHIDPreferencesInterface();
    require(hidPreferences, exit);
    
    @autoreleasepool {
        [hidPreferences setMultiple:(__bridge NSDictionary*)keysToSet keysToRemove:(__bridge NSArray*)keysToRemove user:(__bridge NSString*)user host:(__bridge NSString*)host domain:(__bridge NSString*)domain];
    }
exit:
#else
    CFPreferencesSetMultiple(keysToSet, keysToRemove, domain, user, host);
#endif
    return;
    
}

#pragma mark -
#pragma mark set domain
void HIDPreferencesSetDomain(CFStringRef key,  CFTypeRef __nullable value, CFStringRef domain) {
#if TARGET_OS_OSX
    HIDPreferences *hidPreferences = nil;
    
    hidPreferences = getHIDPreferencesInterface();
    require(hidPreferences, exit);
    
    @autoreleasepool {
        [hidPreferences setDomain:(__bridge NSString*)key value:(__bridge id)value domain:(__bridge NSString*)domain];
    }
exit:
#else
    CFPreferencesSetAppValue(key, value, domain);
#endif
    return;
}


#pragma mark -
#pragma mark copy domain

CFTypeRef __nullable HIDPreferencesCopyDomain(CFStringRef key, CFStringRef domain) {
#if TARGET_OS_OSX
    HIDPreferences *hidPreferences = nil;
    dispatch_semaphore_t semaphore = nil;
    __block id ret = nil;
       
    semaphore =  dispatch_semaphore_create(0);
    require(semaphore, exit);
    
    hidPreferences = getHIDPreferencesInterface();
    require(hidPreferences, exit);

    @autoreleasepool {
        [hidPreferences copyDomain:(__bridge NSString*)key domain:(__bridge NSString*)domain reply:^(id  _Nullable data) {
            ret = data;
            dispatch_semaphore_signal(semaphore);
        }];
        if (dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, kHIDXPCTimeoutInSec * NSEC_PER_SEC)) != 0) {
            HIDLogError("HIDPreferences XPC connection timeout");
        }
    }
exit:
    return ret ? (__bridge_retained CFTypeRef)ret : NULL;
#else
    return CFPreferencesCopyAppValue(key, domain);
#endif
    
}


