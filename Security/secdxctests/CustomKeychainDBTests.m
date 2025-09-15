/*
 * Copyright (c) 2024 Apple Inc. All Rights Reserved.
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
#include <utilities/SecDb.h>
#include "keychain/securityd/SecItemSchema.h"
#include "keychain/securityd/SecItemServer.h"
#include "keychain/securityd/SecItemDb.h"
#include "keychain/securityd/SecItemDataSource.h"
#import <XCTest/XCTest.h>
#import "SecItemPriv.h"

@interface CustomKeychainDBTests : KeychainXCTest

@property CFStringRef keychainPath;
@property SecDbRef customDB;
@property SecurityClient securityClient;

@end

@implementation CustomKeychainDBTests

+ (void)setUp
{
    [super setUp];
}

- (NSString*)nameOfTest
{
    return [self.name componentsSeparatedByCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@" ]"]][1];
}

- (void)setUp
{
    [super setUp];
    // KeychainXCTest already sets up default keychain db with custom test-named directory
    
    // Now we will setup another keychain in /tmp/<TESTNAME>_CustomKeychain.UUID/...
    CFStringRef tmpPath = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("/tmp/%@_%s.%X/"), [self nameOfTest], "CustomKeychain", arc4random());
    CFStringRef keychainPath = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@Library/Keychains/keychain-2-debug.db"), tmpPath);
    
    CFErrorRef cfError =  NULL;
    SecDbRef customDB = SecServerKeychainDbCreate(keychainPath, &cfError);
    if (cfError != NULL)
    {
        XCTFail("Failed to create Custom Keychain DB");
    }
    NSArray *allowedAccessGroups = @[
        @"com.apple.security.securityd"
    ];
    SecurityClient client = {
        .accessGroups = (__bridge CFArrayRef)allowedAccessGroups,
    };
    _customDB = customDB;
    _keychainPath = keychainPath;
    _securityClient = client;
    
    CFRelease(tmpPath);
}

- (void)tearDown
{
    SecDbForceClose(_customDB);
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSError *error;
    if ([fileManager removeItemAtPath:(__bridge NSString * _Nonnull)(_keychainPath) error:&error]) {
        NSLog(@"Custom Keychain Directory removed successfully.");
    } else {
        NSLog(@"Failed to remove custom keychain directory: %@", error);
    }
    CFReleaseNull(_customDB);
    CFReleaseNull(_keychainPath);
    [super tearDown];
}

- (bool)addItemInCustomKeychain:(SecDbRef)db queryAttributes:(NSDictionary*)addQueryAttributes
{
    __block CFErrorRef cferror = NULL;
    bool response = kc_with_dbt(true, db, &cferror, ^bool(SecDbConnectionRef dbt) {
        return kc_transaction_type(dbt, kSecDbExclusiveTransactionType, &cferror, ^bool {
            SecDbItemRef item = SecDbItemCreateWithAttributes(NULL, kc_class_with_name(kSecClassGenericPassword), (__bridge CFDictionaryRef)addQueryAttributes, KEYBAG_DEVICE, &cferror);

            bool ret = SecDbItemInsert(item, dbt, false, false, &cferror);
            XCTAssertTrue(ret, "Should be able to add an item in custom keychain");
            
            return ret;
        });
    });
    
    XCTAssertNil((__bridge NSError *)cferror, "Should be no error performing add query on custom keychain");
    CFReleaseNull(cferror);
    return response;
}

- (NSUInteger)searchItemInCustomKeychain:(SecDbRef)db queryAttributes:(NSDictionary*)searchQueryAttributes
{
    __block CFErrorRef cferror = NULL;
    Query *q = query_create_with_limit( (__bridge CFDictionaryRef)searchQueryAttributes, NULL, kSecMatchUnlimited, NULL, &cferror);
    if (cferror!=NULL) {
        XCTFail("Should be no error creating query for custom keychain");
        return 0;
    }
    cferror = NULL;
    __block NSUInteger count = 0;
    bool ok = kc_with_dbt(true, db, &cferror, ^(SecDbConnectionRef dbt) {
        return SecDbItemQuery(q, NULL, dbt, &cferror, ^(SecDbItemRef item, bool *stop) {
            count += 1;
            NSLog(@"Custom Keychain queried item: %@", item);
            XCTAssertNotNil((__bridge NSDictionary*)item, "Should have queried for item in custom keychain");
        });
    });
    
    XCTAssertTrue(ok, "Should have successfully queried the custom keychain");
    XCTAssertNil((__bridge NSError *)cferror, "Should be no error performing search query on custom keychain");
    CFReleaseNull(cferror);
    return count;
}

// TODO: Currently testing with adding generic password..Might need to test others also
- (void)testManualGenpPasswordCustomDB
{
    
    NSDictionary* addQueryAttributesDefaultKeychain = @{
                 (id)kSecClass : (id)kSecClassGenericPassword,
                 (id)kSecValueData : [@"passwordDefault" dataUsingEncoding:NSUTF8StringEncoding],
                 (id)kSecAttrAccount : @"TestAccountDefault",
                 (id)kSecAttrService : @"TestServiceDefault",
                 (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
                 (id)kSecAttrAccessible  : @"ak",
                 (id)kSecUseDataProtectionKeychain : @(YES),
                 (id)kSecReturnRef : @(YES)
     };
    
    CFTypeRef result = NULL;
    // Add the item in the default keychain
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQueryAttributesDefaultKeychain, &result), errSecSuccess, @"Should have succeeded in adding item to default keychain");
    result = NULL;
    
    // Query for the added item in the default keychain
    NSMutableDictionary* searchQueryAttributesDefaultKeychain = [addQueryAttributesDefaultKeychain mutableCopy];
    [searchQueryAttributesDefaultKeychain removeObjectForKey:(id)kSecValueData];
    
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)searchQueryAttributesDefaultKeychain, &result), errSecSuccess, @"Should have succeeded in finding item in default keychain");
    
    // Add a item in custom keychain with modified kSecAttrAccount and kSecAttrService
     NSDictionary* addQueryAttributesCustomKeychain = @{
                 (id)kSecClass : (id)kSecClassGenericPassword,
                 (id)kSecValueData : [@"passwordCustom" dataUsingEncoding:NSUTF8StringEncoding],
                 (id)kSecAttrAccount : @"TestAccountCustom",
                 (id)kSecAttrService : @"TestServiceCustom",
                 (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
                 (id)kSecAttrAccessible  : @"ak",
                 (id)kSecUseDataProtectionKeychain : @(YES),
                 (id)kSecReturnRef : @(YES),
     };
    
    // Manually Add the item in custom keychain
    XCTAssertTrue([self addItemInCustomKeychain:_customDB queryAttributes:addQueryAttributesCustomKeychain], "Should have added item in Custom Keychain");
    
    // Manually Query for the added item in the custom keychain
    NSMutableDictionary* searchQueryAttributesCustomKeychain = [addQueryAttributesCustomKeychain mutableCopy];
    [searchQueryAttributesCustomKeychain removeObjectForKey:(id)kSecValueData];
    
    XCTAssertEqual([self searchItemInCustomKeychain:_customDB queryAttributes:searchQueryAttributesCustomKeychain], 1, "Should have found a item in custom keychain");
    
    // Since we separated the DB, the items shouldn't co-exist
    // Custom item shouldn't exist in default keychain or vice-versa
    result = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)searchQueryAttributesCustomKeychain, &result), errSecItemNotFound, @"Should have succeeded in not finding custom item in default keychain");
    
    // Default item shouldn't exist in custom keychain
    XCTAssertEqual([self searchItemInCustomKeychain:_customDB queryAttributes:searchQueryAttributesDefaultKeychain], 0, "Should have succeeded in not finding default item in custom keychain");
    
    CFReleaseNull(result);
}

- (void)testSecAPIGenpPasswordCustomDB
{
    NSDictionary* addQueryAttributesDefaultKeychain = @{
                (id)kSecClass : (id)kSecClassGenericPassword,
                (id)kSecValueData : [@"passwordDefault" dataUsingEncoding:NSUTF8StringEncoding],
                (id)kSecAttrAccount : @"TestAccountDefault",
                (id)kSecAttrService : @"TestServiceDefault",
                (id)kSecUseDataProtectionKeychain : @(YES),
                (id)kSecReturnRef : @(YES)
    };
    
    CFTypeRef result = NULL;
    // Add the item in the default keychain
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQueryAttributesDefaultKeychain, &result), errSecSuccess, @"Should have succeeded in adding item to default keychain");
        
    
    // Query for the added item in the default keychain
    NSMutableDictionary* searchQueryAttributesDefaultKeychain = [addQueryAttributesDefaultKeychain mutableCopy];
    [searchQueryAttributesDefaultKeychain removeObjectForKey:(id)kSecValueData];
    
    result = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)searchQueryAttributesDefaultKeychain, &result), errSecSuccess, @"Should have succeeded in finding item in default keychain");
    
    
    // Add a item in custom keychain with modified kSecAttrAccount and kSecAttrService
    NSDictionary* addQueryAttributesCustomKeychain = @{
                (id)kSecClass : (id)kSecClassGenericPassword,
                (id)kSecValueData : [@"passwordCustom" dataUsingEncoding:NSUTF8StringEncoding],
                (id)kSecAttrAccount : @"TestAccountCustom",
                (id)kSecAttrService : @"TestServiceCustom",
                (id)kSecUseDataProtectionKeychain : @(YES),
                (id)kSecReturnRef : @(YES),
    };
    CFErrorRef error = NULL;
   
    // Add the item in the custom keychain using ServerAPI
    XCTAssertTrue(SecServerItemAddWithCustomDb((__bridge CFDictionaryRef)addQueryAttributesCustomKeychain, _customDB, &_securityClient, &result, &error), @"Should have succeeded in adding item to custom keychain");
    XCTAssertNil((__bridge NSError*) error);
    
    // Query for the added item in the custom keychain using ServerAPI
    NSMutableDictionary* searchQueryAttributesCustomKeychain = [addQueryAttributesCustomKeychain mutableCopy];
    [searchQueryAttributesCustomKeychain removeObjectForKey:(id)kSecValueData];
    
    result = NULL;
    error = NULL;
    XCTAssertTrue(SecServerItemCopyMatchingWithCustomDb((__bridge CFDictionaryRef)searchQueryAttributesCustomKeychain, _customDB, &result, &_securityClient, &error));
    XCTAssertNil((__bridge NSError*) error);
    
    // Since we separated the DB, the items shouldn't co-exist
    // Custom item shouldn't exist in default keychain or vice-versa
    result = NULL;
    NSMutableDictionary* searchCustomQueryAttributesForDefaultKeychain = searchQueryAttributesCustomKeychain;
    
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)searchCustomQueryAttributesForDefaultKeychain, &result), errSecItemNotFound, @"Should have succeeded in not finding custom item in default keychain");
    
    // Default item shouldn't exist in custom keychain
    NSMutableDictionary* searchDefaultQueryAttributesForCustomKeychain = searchQueryAttributesDefaultKeychain;
    result = NULL;
    error = NULL;
    
    XCTAssertFalse(SecServerItemCopyMatchingWithCustomDb((__bridge CFDictionaryRef)searchDefaultQueryAttributesForCustomKeychain, _customDB, &result, &_securityClient, &error));
    
    XCTAssertEqual([(__bridge NSError*) error code], errSecItemNotFound);
    
    CFReleaseNull(result);
    CFReleaseNull(error);
    
}

- (void)testSecAPIGenpPasswordUpdateCustomDB
{
    NSDictionary* addQuery = @{
                (id)kSecClass : (id)kSecClassGenericPassword,
                (id)kSecValueData : [@"passwordCustom" dataUsingEncoding:NSUTF8StringEncoding],
                (id)kSecAttrAccount : @"TestAccountCustom",
                (id)kSecAttrService : @"TestServiceCustom",
                (id)kSecUseDataProtectionKeychain : @(YES),
                (id)kSecReturnRef : @(YES),
                (id)kSecReturnData: @(YES),
    };
    CFTypeRef result = NULL;
    CFErrorRef error = NULL;
    // Add the item in the custom keychain
    XCTAssertTrue(SecServerItemAddWithCustomDb((__bridge CFDictionaryRef)addQuery, _customDB, &_securityClient, &result, &error), @"Should have succeeded in adding item to custom keychain");
    XCTAssertNil((__bridge NSError*) error);
    
    NSDictionary* updateQuery = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : @"TestAccountCustom",
        (id)kSecAttrService : @"TestServiceCustom",
        (id)kSecUseDataProtectionKeychain : @(YES),
    };
    // Update item in the custom keychain
    XCTAssertTrue(SecServerItemUpdateWithCustomDb((__bridge CFDictionaryRef)updateQuery,
                                      _customDB,
                                      (__bridge CFDictionaryRef)@{
                                          (id)kSecAttrService : @"TestServiceCustomUpdated",
                                          (id)kSecValueData: [@"otherpassword" dataUsingEncoding:NSUTF8StringEncoding],
                                          }, &_securityClient, &error),  @"Should have succeeded in updating item to custom keychain");
    XCTAssertNil((__bridge NSError*) error);
  
    // Searching for old item should return item not found
    NSDictionary* searchOldDataQuery = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : @"TestAccountCustom",
        (id)kSecAttrService : @"TestServiceCustom",
        (id)kSecUseDataProtectionKeychain : @(YES),
    };
    
    result = NULL;
    error = NULL;
    XCTAssertFalse(SecServerItemCopyMatchingWithCustomDb((__bridge CFDictionaryRef)searchOldDataQuery, _customDB, &result, &_securityClient, &error));
    XCTAssertEqual([(__bridge NSError*) error code], errSecItemNotFound);
    
    NSDictionary* searchCurrentDataQuery = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : @"TestAccountCustom",
        (id)kSecAttrService : @"TestServiceCustomUpdated",
        (id)kSecUseDataProtectionKeychain : @(YES),
    };
    
    // Searching for the updated item should return found
    result = NULL;
    error = NULL;
    XCTAssertTrue(SecServerItemCopyMatchingWithCustomDb((__bridge CFDictionaryRef)searchCurrentDataQuery, _customDB, &result, &_securityClient, &error));
    XCTAssertNil((__bridge NSError*) error);
    
    CFReleaseNull(result);
    CFReleaseNull(error);
}

- (void)testSecAPIGenpPasswordDeleteCustomDB
{
    NSDictionary* addQuery = @{
                (id)kSecClass : (id)kSecClassGenericPassword,
                (id)kSecValueData : [@"passwordCustom" dataUsingEncoding:NSUTF8StringEncoding],
                (id)kSecAttrAccount : @"TestAccountCustom",
                (id)kSecAttrService : @"TestServiceCustom",
                (id)kSecUseDataProtectionKeychain : @(YES),
                (id)kSecReturnRef : @(YES),
                (id)kSecReturnData: @(YES),
    };
    
    CFTypeRef result = NULL;
    CFErrorRef error = NULL;
    // Add the item in the custom keychain
    XCTAssertTrue(SecServerItemAddWithCustomDb((__bridge CFDictionaryRef)addQuery, _customDB, &_securityClient, &result, &error), @"Should have succeeded in adding item to custom keychain");
    XCTAssertNil((__bridge NSError*) error);
    
    // Query for the added item in the custom keychain
    NSMutableDictionary* searchQuery = [addQuery mutableCopy];
    [searchQuery removeObjectForKey:(id)kSecValueData];
    
    result = NULL;
    error = NULL;
    XCTAssertTrue(SecServerItemCopyMatchingWithCustomDb((__bridge CFDictionaryRef)searchQuery, _customDB, &result, &_securityClient, &error));
    XCTAssertNil((__bridge NSError*) error);

    
    NSDictionary* deleteQuery = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : @"TestAccountCustom",
        (id)kSecAttrService : @"TestServiceCustom",
        (id)kSecUseDataProtectionKeychain : @(YES),
    };
        
    result = NULL;
    error = NULL;
    // Delete the added item
    XCTAssertTrue(SecServerItemDeleteWithCustomDb((__bridge CFDictionaryRef)deleteQuery, _customDB, &_securityClient, &error));
    XCTAssertNil((__bridge NSError*) error);
    
    // Searching for the item should return not found
    result = NULL;
    error = NULL;
    XCTAssertFalse(SecServerItemCopyMatchingWithCustomDb((__bridge CFDictionaryRef)searchQuery, _customDB, &result, &_securityClient, &error));
    XCTAssertEqual([(__bridge NSError*) error code], errSecItemNotFound);
    
    CFReleaseNull(result);
    CFReleaseNull(error);
}

@end
