/*
* Copyright (c) 2019 Apple Inc. All Rights Reserved.
*
* @APPLE_LICENSE_HEADER_START@
*
* This file contains Original Code and/or Modifications of Original Code
* as defined in and that are subject to the Apple Public Source License
* Version 2.0 (the 'License'). You may not use this file except in
* compliance with the License. Please obtain a copy of the License at
* http://www.opensource.apple.com/apsl/ and read it before using this
* file.
*
* The Original Code and all software distributed under the License are
* distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
* EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
* INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
* Please see the License for the specific language governing rights and
* limitations under the License.
*
* @APPLE_LICENSE_HEADER_END@
*/


#import <XCTest/XCTest.h>

@interface SecItemTests : XCTestCase

@end

@implementation SecItemTests

- (void)setUp {
    [self deleteAll];
}

- (void)tearDown {
    [self deleteAll];
}



- (void)testAddGenericPassword {
    NSDictionary *attrs;

    attrs = [self addGenericPassword:@"delete-me" service:@"delete-me"];
    XCTAssertNotNil(attrs, "should create genp");
    XCTAssertNil(attrs[@"OSStatus"], "should have no error");
}

- (void)testAddTwoGenericPassword {
    NSDictionary *attrs;

    attrs = [self addGenericPassword:@"delete-me" service:@"delete-me"];
    XCTAssertNotNil(attrs, "should create genp");
    XCTAssertNil(attrs[@"OSStatus"], "should have no error");

    attrs = [self addGenericPassword:@"delete-me2s" service:@"delete-me"];
    XCTAssertNotNil(attrs, "should create genp");
    XCTAssertNil(attrs[@"OSStatus"], "should have no error");
}

- (void)testAddCollidingGenericPassword {
    NSDictionary *attrs;

    attrs = [self addGenericPassword:@"delete-me" service:@"delete-me"];
    XCTAssertNotNil(attrs, "should create genp");

    attrs = [self addGenericPassword:@"delete-me" service:@"delete-me"];
    XCTAssertNotNil(attrs, "should create genp");
    XCTAssertEqual(attrs[@"OSStatus"], @(errSecDuplicateItem), "should have duplicate item");
}


- (void)testMoveGenericPassword {
    NSDictionary *attrs;

    attrs = [self addGenericPassword:@"delete-me" service:@"delete-me"];
    XCTAssertNotNil(attrs, "should create genp");

    attrs = [self addGenericPassword:@"delete-me" service:@"delete-me"];
    XCTAssertNotNil(attrs, "should create genp");
    XCTAssertEqual(attrs[@"OSStatus"], @(errSecDuplicateItem), "should have duplicate item");

    XCTAssertEqual([self moveGenericPassword:@"delete-me" service:@"delete-me"
                                  newAccount:@"delete-me2" newService:@"delete-me2"],
                   noErr);

    XCTAssertEqual([self moveGenericPassword:@"delete-me" service:@"delete-me"
                                  newAccount:@"delete-me2" newService:@"delete-me2"],
                   errSecItemNotFound);

}

//-MARK: helper functions

- (void)deleteAll {
    NSDictionary *allGenericPassword = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @(YES),
    };
    (void)SecItemDelete((__bridge CFDictionaryRef)allGenericPassword);
}


- (NSDictionary *)addGenericPassword:(NSString *)account service:(NSString *)service
{
    NSDictionary* addQuery = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrAccount : account,
        (id)kSecAttrService : service,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecUseDataProtectionKeychain : @(YES),
        (id)kSecReturnAttributes : @(YES),
    };
    CFTypeRef result = NULL;

    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)addQuery, &result);
    if (status != 0) {
        return @{ @"OSStatus": @(status) };
    }
    if (result == NULL) {
        return NULL;
    }
    if (CFGetTypeID(result) != CFDictionaryGetTypeID()) {
        CFRelease(result);
        return NULL;
    }

    return (__bridge NSDictionary *)result;
}


- (OSStatus)moveGenericPassword:(NSString *)account service:(NSString *)service
                     newAccount:(NSString *)newAccount newService:(NSString *)newService
{
    NSDictionary* updateQuery = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrAccount : account,
        (id)kSecAttrService : service,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecUseDataProtectionKeychain : @(YES),
        (id)kSecReturnAttributes : @(YES),
    };
    NSDictionary *newAttributes = @{
        (id)kSecAttrAccount : newAccount,
        (id)kSecAttrService : newService,
    };

    OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)updateQuery, (__bridge CFDictionaryRef)newAttributes);
    return status;
}


@end
