//
//  devices.m
//  IOHIDFamily
//
//  Created by YG on 4/14/16.
//
//

#import <Foundation/Foundation.h>
#include <stdio.h>
#include <strings.h>
#include <getopt.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDEventSystemClient.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>
#include "AssertMacros.h"
#include "hdutil.h"
#include "utility.h"



NSDictionary * createServiceInfo (IOHIDServiceClientRef service);
void printService (IOHIDServiceClientRef service);


NSNumber * copyServiceNumberPropertyForKey (IOHIDServiceClientRef service, NSString *key, NSNumber *def) {
    NSNumber * value = (NSNumber *) CFBridgingRelease(IOHIDServiceClientCopyProperty (service,  (__bridge CFStringRef)(key)));
    if (!value) {
       value = def;
    }
    return value;
}

NSString * copyServiceStringPropertyForKey (IOHIDServiceClientRef service, NSString *key, NSString *def) {
    NSString * value = (NSString *) CFBridgingRelease(IOHIDServiceClientCopyProperty (service,  (__bridge CFStringRef)(key)));
    if (!value) {
       value = def;
    }
    return value;
}

NSDictionary * createServiceInfoDictionary (IOHIDServiceClientRef service) {
    
    NSMutableDictionary *serviceInfo = [[NSMutableDictionary  alloc] init];
    if (serviceInfo == NULL) {
        return serviceInfo;
    }
    NSNumber *defNum = @0;
    NSString *defStr = @"Unknown";
    
    [serviceInfo setValue:(NSNumber *) CFBridgingRelease(IOHIDServiceClientGetRegistryID (service)) forKey: @"RegistryID"];
    [serviceInfo setValue:copyServiceNumberPropertyForKey(service, @kIOHIDVendorIDKey, defNum) forKey:@kIOHIDVendorIDKey];
    [serviceInfo setValue:copyServiceNumberPropertyForKey(service, @kIOHIDProductIDKey, defNum) forKey:@kIOHIDProductIDKey];
    [serviceInfo setValue:copyServiceNumberPropertyForKey(service, @kIOHIDTransportKey, defNum) forKey:@kIOHIDTransportKey];
    [serviceInfo setValue:copyServiceNumberPropertyForKey(service, @kIOHIDPrimaryUsageKey, defNum) forKey:@kIOHIDPrimaryUsageKey];
    [serviceInfo setValue:copyServiceNumberPropertyForKey(service, @kIOHIDPrimaryUsagePageKey, defNum) forKey:@kIOHIDPrimaryUsagePageKey];
    [serviceInfo setValue:copyServiceNumberPropertyForKey(service, @kIOHIDLocationIDKey, defNum) forKey:@kIOHIDLocationIDKey];
    [serviceInfo setValue:copyServiceStringPropertyForKey(service, @kIOHIDTransportKey, defStr) forKey:@kIOHIDTransportKey];
    [serviceInfo setValue:copyServiceStringPropertyForKey(service, @kIOHIDProductKey, defStr) forKey:@kIOHIDProductKey];
    [serviceInfo setValue:copyServiceStringPropertyForKey(service, @kIOHIDManufacturerKey, defStr) forKey:@kIOHIDManufacturerKey];
 
    return serviceInfo;
}

NSString * createFilterString (const char * str) {
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
    [filterString writeToFile: @"/dev/stdout" atomically: NO];
    return filterString;
}

NSString * createPropertiesString (const char * str) {
    return createFilterString (str);
}

NSDictionary * createFilterDictionary (NSString *str) {

    NSError      *e = NULL;
    NSDictionary *dict = [NSJSONSerialization JSONObjectWithData:[str dataUsingEncoding:NSUTF8StringEncoding] options:NSJSONReadingMutableContainers  error:&e];
    if (e) {
        NSLog(@"NSJSONSerialization %@", e);
    }
    return dict;
}

id createPropertiesDicitonary (NSString *str) {
    NSError      *e = NULL;
    id value = [NSJSONSerialization JSONObjectWithData:[str dataUsingEncoding:NSUTF8StringEncoding] options:NSJSONReadingMutableContainers  error:&e];
    if (e) {
        NSLog(@"NSJSONSerialization %@", e);
    }
    return value;
}

