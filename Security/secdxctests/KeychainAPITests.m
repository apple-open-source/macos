/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#import "KeychainXCTest.h"
#import "SecDbKeychainItem.h"
#import "SecdTestKeychainUtilities.h"
#import "CKKS.h"
#import "SecDbKeychainItemV7.h"
#import "SecItemPriv.h"
#import "SecItemServer.h"
#import "spi.h"
#import "SecDbKeychainSerializedItemV7.h"
#import "SecDbKeychainSerializedMetadata.h"
#import "SecDbKeychainSerializedSecretData.h"
#import "SecDbKeychainSerializedAKSWrappedKey.h"
#import <utilities/SecCFWrappers.h>
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

void* testlist = NULL;

#if USE_KEYSTORE

@interface KeychainAPITests : KeychainXCTest
@end

@implementation KeychainAPITests

+ (void)setUp
{
    [super setUp];
    
    SecCKKSDisable();
    securityd_init(NULL);
}

- (void)setUp
{
    [super setUp];
    
    NSArray* partsOfName = [self.name componentsSeparatedByCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@" ]"]];
    secd_test_setup_temp_keychain([partsOfName[1] UTF8String], NULL);
}

- (void)testReturnValuesInSecItemUpdate
{
    NSDictionary* addQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                                (id)kSecAttrAccount : @"TestAccount",
                                (id)kSecAttrService : @"TestService",
                                (id)kSecAttrNoLegacy : @(YES),
                                (id)kSecReturnAttributes : @(YES)
                              };
    
    NSDictionary* updateQueryWithNoReturn = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                 (id)kSecAttrAccount : @"TestAccount",
                                 (id)kSecAttrService : @"TestService",
                                 (id)kSecAttrNoLegacy : @(YES)
                                 };
    
    CFTypeRef result = NULL;
    
    // Add the item
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery, &result), errSecSuccess, @"Should have succeeded in adding test item to keychain");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemAdd");
    CFReleaseNull(result);
    
    // And we can update the item
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)updateQueryWithNoReturn, (__bridge CFDictionaryRef)@{(id)kSecValueData: [@"otherpassword" dataUsingEncoding:NSUTF8StringEncoding]}), errSecSuccess, "failed to update item with clean update query");
    
    // great, a normal update works
    // now let's do updates with various queries which include return parameters to ensure they succeed on macOS and throw errors on iOS.
    // this is a status-quo compromise between changing iOS match macOS (which has lamé no-op characteristics) and changing macOS to match iOS, which risks breaking existing clients
    
#if TARGET_OS_OSX
    NSMutableDictionary* updateQueryWithReturnAttributes = updateQueryWithNoReturn.mutableCopy;
    updateQueryWithReturnAttributes[(id)kSecReturnAttributes] = @(YES);
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)updateQueryWithReturnAttributes, (__bridge CFDictionaryRef)@{(id)kSecValueData: [@"return-attributes" dataUsingEncoding:NSUTF8StringEncoding]}), errSecSuccess, "failed to update item with return attributes query");
    
    NSMutableDictionary* updateQueryWithReturnData = updateQueryWithNoReturn.mutableCopy;
    updateQueryWithReturnAttributes[(id)kSecReturnData] = @(YES);
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)updateQueryWithReturnData, (__bridge CFDictionaryRef)@{(id)kSecValueData: [@"return-data" dataUsingEncoding:NSUTF8StringEncoding]}), errSecSuccess, "failed to update item with return data query");
    
    NSMutableDictionary* updateQueryWithReturnRef = updateQueryWithNoReturn.mutableCopy;
    updateQueryWithReturnAttributes[(id)kSecReturnRef] = @(YES);
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)updateQueryWithReturnRef, (__bridge CFDictionaryRef)@{(id)kSecValueData: [@"return-ref" dataUsingEncoding:NSUTF8StringEncoding]}), errSecSuccess, "failed to update item with return ref query");
    
    NSMutableDictionary* updateQueryWithReturnPersistentRef = updateQueryWithNoReturn.mutableCopy;
    updateQueryWithReturnAttributes[(id)kSecReturnPersistentRef] = @(YES);
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)updateQueryWithReturnPersistentRef, (__bridge CFDictionaryRef)@{(id)kSecValueData: [@"return-persistent-ref" dataUsingEncoding:NSUTF8StringEncoding]}), errSecSuccess, "failed to update item with return persistent ref query");
#else
    NSMutableDictionary* updateQueryWithReturnAttributes = updateQueryWithNoReturn.mutableCopy;
    updateQueryWithReturnAttributes[(id)kSecReturnAttributes] = @(YES);
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)updateQueryWithReturnAttributes, (__bridge CFDictionaryRef)@{(id)kSecValueData: [@"return-attributes" dataUsingEncoding:NSUTF8StringEncoding]}), errSecParam, "failed to generate error updating item with return attributes query");
    
    NSMutableDictionary* updateQueryWithReturnData = updateQueryWithNoReturn.mutableCopy;
    updateQueryWithReturnData[(id)kSecReturnData] = @(YES);
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)updateQueryWithReturnData, (__bridge CFDictionaryRef)@{(id)kSecValueData: [@"return-data" dataUsingEncoding:NSUTF8StringEncoding]}), errSecParam, "failed to generate error updating item with return data query");
    
    NSMutableDictionary* updateQueryWithReturnRef = updateQueryWithNoReturn.mutableCopy;
    updateQueryWithReturnRef[(id)kSecReturnRef] = @(YES);
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)updateQueryWithReturnRef, (__bridge CFDictionaryRef)@{(id)kSecValueData: [@"return-ref" dataUsingEncoding:NSUTF8StringEncoding]}), errSecParam, "failed to generate error updating item with return ref query");
    
    NSMutableDictionary* updateQueryWithReturnPersistentRef = updateQueryWithNoReturn.mutableCopy;
    updateQueryWithReturnPersistentRef[(id)kSecReturnPersistentRef] = @(YES);
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)updateQueryWithReturnPersistentRef, (__bridge CFDictionaryRef)@{(id)kSecValueData: [@"return-persistent-ref" dataUsingEncoding:NSUTF8StringEncoding]}), errSecParam, "failed to generate error updating item with return persistent ref query");
#endif
}

@end

#endif
