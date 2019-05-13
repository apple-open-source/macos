//
//  devices.m
//  IOHIDFamily
//
//  Created by YG on 4/14/16.
//
//
#include "hdutil.h"
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <strings.h>
#include <getopt.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDEventSystemClient.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>
#include "AssertMacros.h"
#include "utility.h"
#include <IOKit/hid/IOHIDUsageTables.h>

static NSArray *matchingKeys(void) {
    return @[ @kIOHIDVendorIDKey, @kIOHIDProductIDKey, @kIOHIDTransportKey,
              @kIOHIDPrimaryUsageKey, @kIOHIDPrimaryUsagePageKey, @kIOHIDLocationIDKey,
              @kIOHIDTransportKey, @kIOHIDProductKey, @kIOHIDManufacturerKey, @kIOClassKey ];
}

NSDictionary *createServiceInfoDictionary(IOHIDServiceClientRef service) {
    NSMutableDictionary *serviceInfo = [[NSMutableDictionary  alloc] init];
    
    if (serviceInfo == NULL) {
        return serviceInfo;
    }
    
    [serviceInfo setValue:(NSNumber *)CFBridgingRelease(IOHIDServiceClientGetRegistryID(service)) forKey: @kIORegistryEntryIDKey];
    
    for (NSString *key in matchingKeys()) {
        [serviceInfo setValue:CFBridgingRelease(IOHIDServiceClientCopyProperty(service, (__bridge CFStringRef)key)) forKey:key];
    }
 
    return serviceInfo;
}

NSDictionary *createDeviceInfoDictionary(IOHIDDeviceRef device) {
    NSMutableDictionary *deviceInfo = [[NSMutableDictionary  alloc] init];
    uint64_t regID;
    
    if (deviceInfo == NULL) {
        return deviceInfo;
    }
    
    IORegistryEntryGetRegistryEntryID(IOHIDDeviceGetService(device), &regID);
    [deviceInfo setValue:@(regID) forKey: @kIORegistryEntryIDKey];
    
    for (NSString *key in matchingKeys()) {
        [deviceInfo setValue:IOHIDDeviceGetProperty(device, (__bridge CFStringRef)key) forKey:key];
    }
    
    return deviceInfo;
}

static NSString *createMatchingString(const char * str) {
    NSError *e = NULL;
    NSMutableString *filterString = [NSMutableString stringWithUTF8String:str];
    NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:@"0[xX][0-9a-f]+" options:NSRegularExpressionCaseInsensitive error:&e];
    NSUInteger start = 0;
    NSTextCheckingResult * result = NULL;
    do {
        result = [regex firstMatchInString:filterString options:0 range: NSMakeRange(start, filterString.length - start)];
        if (result) {
            NSScanner *scan = [NSScanner scannerWithString:[filterString substringWithRange:result.range]];
            unsigned long long value = 0;
            [scan scanHexLongLong:&value];
            NSString *valueString = [NSString stringWithFormat:@"%llu",value];
            [filterString replaceCharactersInRange:result.range withString: valueString];
            start = result.range.location + valueString.length;
        }
    } while (result);
    
    return filterString;
}

NSString * createPropertiesString (const char * str) {
    return createMatchingString (str);
}

id createPropertiesDicitonary (NSString *str) {
    NSError      *e = NULL;
    id value = [NSJSONSerialization JSONObjectWithData:[str dataUsingEncoding:NSUTF8StringEncoding] options:NSJSONReadingMutableContainers  error:&e];
    if (e) {
        NSLog(@"NSJSONSerialization %@", e);
    }
    return value;
}

static NSDictionary *matchingDictionaryForDeviceString(NSString *matching)
{
    NSDictionary *matchingDictionary = nil;
    
    if ([[NSString stringWithUTF8String:"keyboard"] containsString:matching]) {
        matchingDictionary = @{ @(kIOHIDPrimaryUsagePageKey) : @(kHIDPage_GenericDesktop),
                                @(kIOHIDPrimaryUsageKey) : @(kHIDUsage_GD_Keyboard) };
    } else if ([[NSString stringWithUTF8String:"mouse"] containsString:matching]) {
        matchingDictionary = @{ @(kIOHIDPrimaryUsagePageKey) : @(kHIDPage_GenericDesktop),
                                @(kIOHIDPrimaryUsageKey) : @(kHIDUsage_GD_Mouse) };
    } else if ([[NSString stringWithUTF8String:"digitizer"] containsString:matching]) {
        matchingDictionary = @{ @(kIOHIDPrimaryUsagePageKey) : @(kHIDPage_Digitizer) };
    }
    
    return matchingDictionary;
}

bool setClientMatching(IOHIDEventSystemClientRef client, const char *str) {
    bool        result          = false;
    NSError     *error          = nil;
    NSString    *matchString    = nil;
    id          matchingObj     = nil;
    
    matchString = createMatchingString(str);
    
    if (!matchString || matchString.length == 0) {
        return result;
    }
    
    if ([[matchString substringToIndex:1] isEqual:@"["] ||
        [[matchString substringToIndex:1] isEqual:@"{"]) {
        matchingObj = [NSJSONSerialization JSONObjectWithData:[matchString dataUsingEncoding:NSUTF8StringEncoding] options:NSJSONReadingMutableContainers error:&error];
    } else {
        matchingObj = matchingDictionaryForDeviceString(matchString);
    }
   
    if (!client || error || !matchingObj) {
        if (error) {
            NSLog(@"Serialization error: %@", error);
        }
        goto exit;
    }
    
    if ([matchingObj isKindOfClass:[NSDictionary class]]) {
        IOHIDEventSystemClientSetMatching(client, (__bridge CFDictionaryRef)matchingObj);
        result = true;
    } else if ([matchingObj isKindOfClass:[NSArray class]]) {
        IOHIDEventSystemClientSetMatchingMultiple(client, (__bridge CFArrayRef)matchingObj);
        result = true;
    }
    
exit:
    return result;
}

bool setManagerMatching(IOHIDManagerRef manager, const char *str) {
    bool        result          = false;
    NSError     *error          = nil;
    NSString    *matchString    = nil;
    id          matchingObj     = nil;
    
    matchString = createMatchingString(str);
    
    if ([[matchString substringToIndex:1] isEqual:@"["] ||
        [[matchString substringToIndex:1] isEqual:@"{"]) {
        matchingObj = [NSJSONSerialization JSONObjectWithData:[matchString dataUsingEncoding:NSUTF8StringEncoding] options:NSJSONReadingMutableContainers error:&error];
    } else {
        matchingObj = matchingDictionaryForDeviceString(matchString);
    }
    
    if (!manager || error || !matchingObj) {
        if (error) {
            NSLog(@"Serialization error: %@", error);
        }
        goto exit;
    }
    
    if ([matchingObj isKindOfClass:[NSDictionary class]]) {
        IOHIDManagerSetDeviceMatching(manager, (__bridge CFDictionaryRef)matchingObj);
        result = true;
    } else if ([matchingObj isKindOfClass:[NSArray class]]) {
        IOHIDManagerSetDeviceMatchingMultiple(manager, (__bridge CFArrayRef)matchingObj);
        result = true;
    }
    
exit:
    return result;
}



NSString * formatPropertyValue(id value, NSUInteger integerBase)
{
    NSString * str;
    NSMutableString * fmt = [[NSMutableString alloc] initWithString:@"%"];
    if (value == nil && integerBase != 0) {
        value = @(0);
    }
    if ([value isKindOfClass:[NSNumber class]]) {
        switch (integerBase) {
            case 16:
                [fmt appendString:@"lx"];
                [fmt insertString:@"0x" atIndex:0];
                break;
            case 10:
            default:
                [fmt appendString:@"ld"];
                break;
        }
        str = [NSString stringWithFormat:fmt, ((NSNumber *) value).longLongValue];
    } else {
        [fmt appendString:@"@"];
        str = [NSString stringWithFormat:fmt, value];
    }
    return str;
}
