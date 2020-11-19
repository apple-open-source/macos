//
//  TestHIDPreferences.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 10/10/19.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import <HID/HID.h>
#import <HIDPreferences/HIDPreferences.h>
#import <AssertMacros.h>
#import <IOKit/hid/IOHIDPreferences.h>

#define kHIDPreferencesTestKey "HIDPreferencesTest"

@interface TestHIDPreferences : XCTestCase {
    HIDEventSystemClient *_client;
}

@end

@implementation TestHIDPreferences


-(void) setupEventSystemClient {
    
    require(!_client, exit);
    
    _client = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
     HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, _client != NULL);
    
exit:
    return;
    
}
-(void) setUp {
    [super setUp];
    
    [self setupEventSystemClient];
    
    
}

-(void) tearDown {
    
    [super tearDown];
}

-(void) validateHostedSystem {
    
    BOOL ret = false;
    id value = nil;
    
    // Number
    ret = [_client setProperty:@(2) forKey:@(kHIDPreferencesTestKey)];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, ret != NO);
    
    value = [_client propertyForKey:@(kHIDPreferencesTestKey)];
    NSLog(@"%@",value);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, value && [value isKindOfClass:[NSNumber class]] && ((NSNumber*)value).integerValue == 2);
    
    
    // String
    ret = [_client setProperty:@"Hello-World" forKey:@(kHIDPreferencesTestKey)];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, ret != NO);
    
    value = [_client propertyForKey:@(kHIDPreferencesTestKey)];
    NSLog(@"%@",value);
   
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG,value && [value isKindOfClass:[NSString class]] && [((NSString*)value) isEqualToString:@"Hello-World"]);
    
    // Dictionary
    ret = [_client setProperty:@{@"A":@(2)} forKey:@(kHIDPreferencesTestKey)];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, ret != NO);
    
    value = [_client propertyForKey:@(kHIDPreferencesTestKey)];
    NSLog(@"%@",value);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG,value && [value isKindOfClass:[NSDictionary class]] && ((NSNumber*)((NSDictionary*)value)[@"A"]).integerValue == 2);
    
    // Array
    ret = [_client setProperty:@[@"A"] forKey:@(kHIDPreferencesTestKey)];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, ret != NO);
    
    value = [_client propertyForKey:@(kHIDPreferencesTestKey)];
    NSLog(@"%@",value);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, value && [value isKindOfClass:[NSArray class]] && [((NSArray*)value) containsObject:@"A"]);
    
}

-(void) validateAPI {
    
    NSString *key = @"HIDTest";
    NSString *value = @"HIDTestValue";
    NSString *domain = @"com.apple.HIDTest";
    NSDictionary *valuesToSet = @{@"KeyA" : @"ValueA", @"KeyB" : @"ValueB"};
    NSDictionary *iohidValuesToSet = @{@"KeyA" : @"ValueAIOHID", @"KeyB" : @"ValueBIOHID"};
    NSArray      *valueSetKeys = @[@"KeyA", @"KeyB"];
    NSDictionary *valueSetReturned = nil;
    NSString     *valueForDomain = nil;
    
    id retValue = nil;
    
    HIDPreferencesSet((__bridge CFStringRef)key, (__bridge CFTypeRef)value, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, (__bridge CFStringRef)domain);
    
    retValue = (__bridge_transfer id)HIDPreferencesCopy((__bridge CFStringRef)key, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, (__bridge CFStringRef)domain);
    
    NSLog(@"retValue = %@",retValue);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG,value && [value isKindOfClass:[NSString class]] && [((NSString*)value) isEqualToString:retValue]);
    
    value = @"HIDTestValueAny";
    
    HIDPreferencesSet((__bridge CFStringRef)key, (__bridge CFTypeRef)value, kCFPreferencesAnyUser, kCFPreferencesAnyHost, (__bridge CFStringRef)domain);
    
    retValue = (__bridge_transfer id)HIDPreferencesCopy((__bridge CFStringRef)key, kCFPreferencesAnyUser, kCFPreferencesAnyHost, (__bridge CFStringRef)domain);
    
    NSLog(@"retValue = %@",retValue);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG,value && [value isKindOfClass:[NSString class]] && [((NSString*)value) isEqualToString:retValue]);
    
    HIDPreferencesSetMultiple((__bridge CFDictionaryRef)valuesToSet, NULL, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, (__bridge CFStringRef)domain);
    
    valueSetReturned = (__bridge NSDictionary*)HIDPreferencesCopyMultiple((__bridge CFArrayRef)valueSetKeys, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, (__bridge CFStringRef)domain);
    
    NSLog(@"retValMultiple = %@",valueSetReturned);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG,valueSetReturned && [valueSetReturned isEqual:valuesToSet]);
    
    value = @"Domain specific value";
    key = @"HIDTestDomain";
    
    HIDPreferencesSetDomain((__bridge CFStringRef)key, (__bridge CFTypeRef)value, (__bridge CFStringRef)domain);
    
    valueForDomain = (__bridge NSString*)HIDPreferencesCopyDomain((__bridge CFStringRef)key, (__bridge CFStringRef)domain);
    
    NSLog(@"value for domain = %@",valueForDomain);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG,valueForDomain && [valueForDomain isEqual:value]);
    
    key = @"HIDTestNULLValue";
    
    HIDPreferencesSet((__bridge CFStringRef)key, NULL, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, (__bridge CFStringRef)domain);
    
    retValue = (__bridge id)HIDPreferencesCopy((__bridge CFStringRef)key, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, (__bridge CFStringRef)domain);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, retValue == nil);
    
    retValue = (__bridge id)HIDPreferencesCopyMultiple(NULL, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, (__bridge CFStringRef)domain);
    
    NSLog(@"Null keys copy multiple %@",retValue);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, retValue);
    
    key = @"HIDTestDomainNULLValue";
    
    HIDPreferencesSetDomain((__bridge CFStringRef)key, NULL, (__bridge CFStringRef)domain);
    
    retValue = (__bridge id)HIDPreferencesCopyDomain((__bridge CFStringRef)key, (__bridge CFStringRef)domain);
    
    NSLog(@"ret value %@",retValue);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG, retValue == nil);
    
    //API stability test, can't test any value here
    HIDPreferencesSetMultiple(NULL, NULL, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, (__bridge CFStringRef)domain);
    
    //Test IOHID version of API
    
    key = @"HIDTestDomainIOHIDvalue";
    
    value = @"IOHIDTestValueAny";
    
    IOHIDPreferencesSet((__bridge CFStringRef)key, (__bridge CFTypeRef)value, kCFPreferencesAnyUser, kCFPreferencesAnyHost, (__bridge CFStringRef)domain);
    
    retValue = (__bridge_transfer id)IOHIDPreferencesCopy((__bridge CFStringRef)key, kCFPreferencesAnyUser, kCFPreferencesAnyHost, (__bridge CFStringRef)domain);
    
    NSLog(@"retValue (IOHID) = %@",retValue);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG,value && [value isKindOfClass:[NSString class]] && [((NSString*)value) isEqualToString:retValue]);
    
    IOHIDPreferencesSetMultiple((__bridge CFDictionaryRef)iohidValuesToSet, NULL, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, (__bridge CFStringRef)domain);
    
    valueSetReturned = (__bridge NSDictionary*)IOHIDPreferencesCopyMultiple((__bridge CFArrayRef)valueSetKeys, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost, (__bridge CFStringRef)domain);
    
    NSLog(@"retValMultiple (IOHID) = %@",valueSetReturned);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG,valueSetReturned && [valueSetReturned isEqual:iohidValuesToSet]);
    
    
    value = @"Domain specific value IOHID";
    key = @"HIDTestDomainIOHID";
    
    IOHIDPreferencesSetDomain((__bridge CFStringRef)key, (__bridge CFTypeRef)value, (__bridge CFStringRef)domain);
    
    valueForDomain = (__bridge NSString*)IOHIDPreferencesCopyDomain((__bridge CFStringRef)key, (__bridge CFStringRef)domain);
    
    NSLog(@"value for domain (IOHID) = %@",valueForDomain);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG,valueForDomain && [valueForDomain isEqual:value]);
    
}

-(void) testHIDPreferences {
    
    [self validateHostedSystem];
    [self validateAPI];
    
}

@end
