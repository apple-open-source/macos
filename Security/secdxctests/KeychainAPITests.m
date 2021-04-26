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
#include "SecItemInternal.h"
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
#include <dispatch/dispatch.h>
#include <utilities/SecDb.h>
#include <sys/stat.h>
#include <utilities/SecFileLocations.h>
#include "der_plist.h"
#import "SecItemRateLimit_tests.h"
#include "ipc/server_security_helpers.h"
#include <Security/SecEntitlements.h>
#include "keychain/securityd/SecItemDb.h"

#if USE_KEYSTORE

@interface KeychainAPITests : KeychainXCTest
@end

@implementation KeychainAPITests

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
    // KeychainXCTest already sets up keychain with custom test-named directory
}

- (void)testReturnValuesInSecItemUpdate
{
    NSDictionary* addQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                                (id)kSecAttrAccount : @"TestAccount",
                                (id)kSecAttrService : @"TestService",
                                (id)kSecUseDataProtectionKeychain : @(YES),
                                (id)kSecReturnAttributes : @(YES)
                              };
    
    NSDictionary* updateQueryWithNoReturn = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                 (id)kSecAttrAccount : @"TestAccount",
                                 (id)kSecAttrService : @"TestService",
                                 (id)kSecUseDataProtectionKeychain : @(YES)
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

- (void)testBadTypeInParams
{
    NSMutableDictionary *attrs = @{
        (id)kSecClass: (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrLabel: @"testentry",
    }.mutableCopy;

    SecItemDelete((CFDictionaryRef)attrs);
    XCTAssertEqual(errSecSuccess, SecItemAdd((CFDictionaryRef)attrs, NULL));
    XCTAssertEqual(errSecSuccess, SecItemDelete((CFDictionaryRef)attrs));

    // We try to fool SecItem API with unexpected type of kSecAttrAccessControl attribute in query and it should not crash.
    attrs[(id)kSecAttrAccessControl] = @"string, no SecAccessControlRef!";
    XCTAssertEqual(errSecParam, SecItemAdd((CFDictionaryRef)attrs, NULL));
    XCTAssertEqual(errSecParam, SecItemDelete((CFDictionaryRef)attrs));
}

- (BOOL)passInternalAttributeToKeychainAPIsWithKey:(id)key value:(id)value {
     NSDictionary* badquery = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrService : @"AppClipTestService",
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
        key : value,
    };
    NSDictionary* badupdate = @{key : value};

    NSDictionary* okquery = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrService : @"AppClipTestService",
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    };
    NSDictionary* okupdate = @{(id)kSecAttrService : @"DifferentService"};

    if (SecItemAdd((__bridge CFDictionaryRef)badquery, NULL) != errSecParam) {
        XCTFail("SecItemAdd did not return errSecParam");
        return NO;
    }
    if (SecItemCopyMatching((__bridge CFDictionaryRef)badquery, NULL) != errSecParam) {
        XCTFail("SecItemCopyMatching did not return errSecParam");
        return NO;
    }
    if (SecItemUpdate((__bridge CFDictionaryRef)badquery, (__bridge CFDictionaryRef)okupdate) != errSecParam) {
        XCTFail("SecItemUpdate with bad query did not return errSecParam");
        return NO;
    }
    if (SecItemUpdate((__bridge CFDictionaryRef)okquery, (__bridge CFDictionaryRef)badupdate) != errSecParam) {
        XCTFail("SecItemUpdate with bad update did not return errSecParam");
        return NO;
    }
    if (SecItemDelete((__bridge CFDictionaryRef)badquery) != errSecParam) {
        XCTFail("SecItemDelete did not return errSecParam");
        return NO;
    }
    return YES;
}

// Expand this, rdar://problem/59297616
- (void)testNotAllowedToPassInternalAttributes {
    XCTAssert([self passInternalAttributeToKeychainAPIsWithKey:(__bridge NSString*)kSecAttrAppClipItem value:@YES], @"Expect errSecParam for 'clip' attribute");
}

#pragma mark - Corruption Tests

const uint8_t keychain_data[] = {
    0x62, 0x70, 0x6c, 0x69, 0x73, 0x74, 0x30, 0x30, 0xd2, 0x01, 0x02, 0x03,
    0x04, 0x5f, 0x10, 0x1b, 0x4e, 0x53, 0x57, 0x69, 0x6e, 0x64, 0x6f, 0x77,
    0x20, 0x46, 0x72, 0x61, 0x6d, 0x65, 0x20, 0x50, 0x72, 0x6f, 0x63, 0x65,
    0x73, 0x73, 0x50, 0x61, 0x6e, 0x65, 0x6c, 0x5f, 0x10, 0x1d, 0x4e, 0x53,
    0x57, 0x69, 0x6e, 0x64, 0x6f, 0x77, 0x20, 0x46, 0x72, 0x61, 0x6d, 0x65,
    0x20, 0x41, 0x62, 0x6f, 0x75, 0x74, 0x20, 0x54, 0x68, 0x69, 0x73, 0x20,
    0x4d, 0x61, 0x63, 0x5f, 0x10, 0x1c, 0x32, 0x38, 0x20, 0x33, 0x37, 0x33,
    0x20, 0x33, 0x34, 0x36, 0x20, 0x32, 0x39, 0x30, 0x20, 0x30, 0x20, 0x30,
    0x20, 0x31, 0x34, 0x34, 0x30, 0x20, 0x38, 0x37, 0x38, 0x20, 0x5f, 0x10,
    0x1d, 0x35, 0x36, 0x38, 0x20, 0x33, 0x39, 0x35, 0x20, 0x33, 0x30, 0x37,
    0x20, 0x33, 0x37, 0x39, 0x20, 0x30, 0x20, 0x30, 0x20, 0x31, 0x34, 0x34,
    0x30, 0x20, 0x38, 0x37, 0x38, 0x20, 0x08, 0x0d, 0x2b, 0x4b, 0x6a, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8a
};

dispatch_semaphore_t sema = NULL;

// The real corruption exit handler should xpc_transaction_exit_clean,
// let's be certain it does not. Also make sure exit handler gets called at all
static void SecDbTestCorruptionHandler(void)
{
    dispatch_semaphore_signal(sema);
}

- (void)testCorruptionHandler {
    __security_simulatecrash_enable(false);

    SecDbCorruptionExitHandler = SecDbTestCorruptionHandler;
    sema = dispatch_semaphore_create(0);

    // Test teardown will want to delete this keychain. Make sure it knows where to look...
    NSString* corruptedKeychainPath = [NSString stringWithFormat:@"%@-bad", [self nameOfTest]];

    if(self.keychainDirectoryPrefix) {
        XCTAssertTrue(secd_test_teardown_delete_temp_keychain([self.keychainDirectoryPrefix UTF8String]), "Should be able to delete the temp keychain");
    }

    self.keychainDirectoryPrefix = corruptedKeychainPath;

    secd_test_setup_temp_keychain([corruptedKeychainPath UTF8String], ^{
        CFStringRef keychain_path_cf = __SecKeychainCopyPath();

        CFStringPerformWithCString(keychain_path_cf, ^(const char *keychain_path) {
            int fd = open(keychain_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
            XCTAssert(fd > -1, "Could not open fd to write keychain: %{darwin.errno}d", errno);

            size_t written = write(fd, keychain_data, sizeof(keychain_data));
            XCTAssertEqual(written, sizeof(keychain_data), "Write garbage to disk, got %lu instead of %lu: %{darwin.errno}d", written, sizeof(keychain_data), errno);
            XCTAssertEqual(close(fd), 0, "Close keychain file failed: %{darwin.errno}d", errno);
        });

        CFReleaseNull(keychain_path_cf);
    });

    NSDictionary* query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccount : @"TestAccount",
                            (id)kSecAttrService : @"TestService",
                            (id)kSecUseDataProtectionKeychain : @(YES),
                            (id)kSecReturnAttributes : @(YES)
                            };

    CFTypeRef result = NULL;
    // Real keychain should xpc_transaction_exit_clean() after this, but we nerfed it
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &result), errSecNotAvailable, "Expected badness from corrupt keychain");
    XCTAssertEqual(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC)), 0, "Timed out waiting for corruption exit handler");

    sema = NULL;
    SecDbResetCorruptionExitHandler();
    CFReleaseNull(result);

    NSString* markerpath = [NSString stringWithFormat:@"%@-iscorrupt", CFBridgingRelease(__SecKeychainCopyPath())];
    struct stat info = {};
    XCTAssertEqual(stat([markerpath UTF8String], &info), 0, "Unable to stat corruption marker: %{darwin.errno}d", errno);
}

- (void)testRecoverFromCorruption {
    __security_simulatecrash_enable(false);

    // Setup does a reset, but that doesn't create the db yet so let's sneak in first
    __block struct stat before = {};
    WithPathInKeychainDirectory(CFSTR("keychain-2.db"), ^(const char *filename) {
        FILE* file = fopen(filename, "w");
        XCTAssert(file != NULL, "Didn't get a FILE pointer");
        fclose(file);
        XCTAssertEqual(stat(filename, &before), 0, "Unable to stat newly created file");
    });

    WithPathInKeychainDirectory(CFSTR("keychain-2.db-iscorrupt"), ^(const char *filename) {
        FILE* file = fopen(filename, "w");
        XCTAssert(file != NULL, "Didn't get a FILE pointer");
        fclose(file);
    });

    NSMutableDictionary* query = [@{(id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                                    (id)kSecAttrAccount : @"TestAccount",
                                    (id)kSecAttrService : @"TestService",
                                    (id)kSecUseDataProtectionKeychain : @(YES),
                                    (id)kSecReturnAttributes : @(YES)
                                    } mutableCopy];
    CFTypeRef result = NULL;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, &result), errSecSuccess, @"Should have added item to keychain");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemAdd");
    CFReleaseNull(result);

    query[(id)kSecValueData] = nil;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &result), errSecSuccess, @"Should have found item in keychain");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemCopyMatching");
    CFReleaseNull(result);

    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)query), errSecSuccess, @"Should have deleted item from keychain");

    WithPathInKeychainDirectory(CFSTR("keychain-2.db-iscorrupt"), ^(const char *filename) {
        struct stat markerinfo = {};
        XCTAssertNotEqual(stat(filename, &markerinfo), 0, "Expected not to find corruption marker after killing keychain");
    });

    __block struct stat after = {};
    WithPathInKeychainDirectory(CFSTR("keychain-2.db"), ^(const char *filename) {
        FILE* file = fopen(filename, "w");
        XCTAssert(file != NULL, "Didn't get a FILE pointer");
        fclose(file);
        XCTAssertEqual(stat(filename, &after), 0, "Unable to stat newly created file");
    });

    if (before.st_birthtimespec.tv_sec == after.st_birthtimespec.tv_sec) {
        XCTAssertLessThan(before.st_birthtimespec.tv_nsec, after.st_birthtimespec.tv_nsec, "db was not deleted and recreated");
    } else {
        XCTAssertLessThan(before.st_birthtimespec.tv_sec, after.st_birthtimespec.tv_sec, "db was not deleted and recreated");
    }
}

- (void)testInetBinaryFields {
    NSData* note = [@"OBVIOUS_NOTES_DATA" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* history = [@"OBVIOUS_HISTORY_DATA" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* client0 = [@"OBVIOUS_CLIENT0_DATA" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* client1 = [@"OBVIOUS_CLIENT1_DATA" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* client2 = [@"OBVIOUS_CLIENT2_DATA" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* client3 = [@"OBVIOUS_CLIENT3_DATA" dataUsingEncoding:NSUTF8StringEncoding];

    NSData* originalPassword = [@"asdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassInternetPassword,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrDescription : @"desc",
        (id)kSecAttrServer : @"server",
        (id)kSecAttrAccount : @"test-account",
        (id)kSecValueData : originalPassword,
        (id)kSecDataInetExtraNotes : note,
        (id)kSecDataInetExtraHistory : history,
        (id)kSecDataInetExtraClientDefined0 : client0,
        (id)kSecDataInetExtraClientDefined1 : client1,
        (id)kSecDataInetExtraClientDefined2 : client2,
        (id)kSecDataInetExtraClientDefined3 : client3,

        (id)kSecReturnAttributes : @YES,
    } mutableCopy];

    CFTypeRef cfresult = nil;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, &cfresult), errSecSuccess, "Should be able to add an item using new binary fields");
    NSDictionary* result = (NSDictionary*)CFBridgingRelease(cfresult);
    XCTAssertNotNil(result, "Should have some sort of result");

    XCTAssertNil(result[(id)kSecDataInetExtraNotes], "Notes field should not be returned as an attribute from add");
    XCTAssertNil(result[(id)kSecDataInetExtraHistory], "Notes field should not be returned as an attribute from add");

    NSDictionary* queryFind = @{
        (id)kSecClass : (id)kSecClassInternetPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrAccount : @"test-account",
    };

    NSMutableDictionary* queryFindOneWithJustAttributes = [[NSMutableDictionary alloc] initWithDictionary:queryFind];
    queryFindOneWithJustAttributes[(id)kSecReturnAttributes] = @YES;

    NSMutableDictionary* queryFindAllWithJustAttributes = [[NSMutableDictionary alloc] initWithDictionary:queryFindOneWithJustAttributes];
    queryFindAllWithJustAttributes[(id)kSecMatchLimit] = (id)kSecMatchLimitAll;

    NSDictionary* queryFindOneWithAttributesAndData = @{
        (id)kSecClass : (id)kSecClassInternetPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecReturnAttributes : @YES,
        (id)kSecReturnData: @YES,
        (id)kSecAttrAccount : @"test-account",
    };

    NSDictionary* queryFindAllWithAttributesAndData = @{
        (id)kSecClass : (id)kSecClassInternetPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecReturnAttributes : @YES,
        (id)kSecReturnData: @YES,
        (id)kSecMatchLimit : (id)kSecMatchLimitAll,
        (id)kSecAttrAccount : @"test-account",
    };

    /* Copy with a single record limite, but with attributes only */
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)queryFindOneWithJustAttributes, &cfresult), errSecSuccess, "Should be able to find an item");

    result = (NSDictionary*)CFBridgingRelease(cfresult);
    XCTAssertNotNil(result, "Should have some sort of result");

    XCTAssertNil(result[(id)kSecDataInetExtraNotes], "Notes field should not be returned as an attribute from copymatching when finding a single item");
    XCTAssertNil(result[(id)kSecDataInetExtraHistory], "Notes field should not be returned as an attribute from copymatching when finding a single item");
    XCTAssertNil(result[(id)kSecDataInetExtraClientDefined0], "ClientDefined0 field should not be returned as an attribute from copymatching when finding a single item");
    XCTAssertNil(result[(id)kSecDataInetExtraClientDefined1], "ClientDefined1 field should not be returned as an attribute from copymatching when finding a single item");
    XCTAssertNil(result[(id)kSecDataInetExtraClientDefined2], "ClientDefined2 field should not be returned as an attribute from copymatching when finding a single item");
    XCTAssertNil(result[(id)kSecDataInetExtraClientDefined3], "ClientDefined3 field should not be returned as an attribute from copymatching when finding a single item");

    /* Copy with no limit, but with attributes only */
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)queryFindAllWithJustAttributes, &cfresult), errSecSuccess, "Should be able to find an item");
    NSArray* arrayResult = (NSArray*)CFBridgingRelease(cfresult);
    XCTAssertNotNil(arrayResult, "Should have some sort of result");
    XCTAssertTrue([arrayResult isKindOfClass:[NSArray class]], "Should have received an array back from copymatching");
    XCTAssertEqual(arrayResult.count, 1, "Array should have one element");

    result = arrayResult[0];
    XCTAssertNil(result[(id)kSecDataInetExtraNotes], "Notes field should not be returned as an attribute from copymatching when finding all items");
    XCTAssertNil(result[(id)kSecDataInetExtraHistory], "Notes field should not be returned as an attribute from copymatching when finding all items");
    XCTAssertNil(result[(id)kSecDataInetExtraClientDefined0], "ClientDefined0 field should not be returned as an attribute from copymatching when finding all items");
    XCTAssertNil(result[(id)kSecDataInetExtraClientDefined1], "ClientDefined1 field should not be returned as an attribute from copymatching when finding all items");
    XCTAssertNil(result[(id)kSecDataInetExtraClientDefined2], "ClientDefined2 field should not be returned as an attribute from copymatching when finding all items");
    XCTAssertNil(result[(id)kSecDataInetExtraClientDefined3], "ClientDefined3 field should not be returned as an attribute from copymatching when finding all items");

    /* Copy with single-record limit, but with attributes and data */
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)queryFindOneWithAttributesAndData, &cfresult), errSecSuccess, "Should be able to find an item");
    result = (NSDictionary*)CFBridgingRelease(cfresult);
    XCTAssertNotNil(result, "Should have some sort of result");

    XCTAssertEqualObjects(note, result[(id)kSecDataInetExtraNotes], "Notes field should be returned as data");
    XCTAssertEqualObjects(history, result[(id)kSecDataInetExtraHistory], "History field should be returned as data");
    XCTAssertEqualObjects(client0, result[(id)kSecDataInetExtraClientDefined0], "Client Defined 0 field should be returned as data");
    XCTAssertEqualObjects(client1, result[(id)kSecDataInetExtraClientDefined1], "Client Defined 1 field should be returned as data");
    XCTAssertEqualObjects(client2, result[(id)kSecDataInetExtraClientDefined2], "Client Defined 2 field should be returned as data");
    XCTAssertEqualObjects(client3, result[(id)kSecDataInetExtraClientDefined3], "Client Defined 3 field should be returned as data");

    /* Copy with no limit, but with attributes and data */
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)queryFindAllWithAttributesAndData, &cfresult), errSecSuccess, "Should be able to find an item");
    arrayResult = (NSArray*)CFBridgingRelease(cfresult);
    XCTAssertNotNil(arrayResult, "Should have some sort of result");
    XCTAssertTrue([arrayResult isKindOfClass:[NSArray class]], "Should have received an array back from copymatching");
    XCTAssertEqual(arrayResult.count, 1, "Array should have one element");
    result = arrayResult[0];
    XCTAssertEqualObjects(note, result[(id)kSecDataInetExtraNotes], "Notes field should be returned as data");
    XCTAssertEqualObjects(history, result[(id)kSecDataInetExtraHistory], "History field should be returned as data");
    XCTAssertEqualObjects(client0, result[(id)kSecDataInetExtraClientDefined0], "Client Defined 0 field should be returned as data");
    XCTAssertEqualObjects(client1, result[(id)kSecDataInetExtraClientDefined1], "Client Defined 1 field should be returned as data");
    XCTAssertEqualObjects(client2, result[(id)kSecDataInetExtraClientDefined2], "Client Defined 2 field should be returned as data");
    XCTAssertEqualObjects(client3, result[(id)kSecDataInetExtraClientDefined3], "Client Defined 3 field should be returned as data");

    /* Copy just looking for the password */
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecClass : (id)kSecClassInternetPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecReturnData: @YES,
        (id)kSecAttrAccount : @"test-account",
    }, &cfresult), errSecSuccess, "Should be able to find an item");

    NSData* password = (NSData*)CFBridgingRelease(cfresult);
    XCTAssertNotNil(password, "Should have some sort of password");
    XCTAssertTrue([password isKindOfClass:[NSData class]], "Password is a data");
    XCTAssertEqualObjects(originalPassword, password, "Should still be able to fetch the original password");

    NSData* newHistoryContents = [@"gone" dataUsingEncoding:NSUTF8StringEncoding];

    NSDictionary* updateQuery = @{
        (id)kSecDataInetExtraHistory : newHistoryContents,
    };

    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)queryFind, (__bridge CFDictionaryRef)updateQuery), errSecSuccess, "Should be able to update a history field");

    // And find it again
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)queryFindOneWithAttributesAndData, &cfresult), errSecSuccess, "Should be able to find an item");
    result = (NSDictionary*)CFBridgingRelease(cfresult);
    XCTAssertNotNil(result, "Should have some sort of result");

    XCTAssertEqualObjects(note, result[(id)kSecDataInetExtraNotes], "Notes field should be returned as data");
    XCTAssertEqualObjects(newHistoryContents, result[(id)kSecDataInetExtraHistory], "History field should be updated");
    XCTAssertEqualObjects(client0, result[(id)kSecDataInetExtraClientDefined0], "Client Defined 0 field should be returned as data");
    XCTAssertEqualObjects(client1, result[(id)kSecDataInetExtraClientDefined1], "Client Defined 1 field should be returned as data");
    XCTAssertEqualObjects(client2, result[(id)kSecDataInetExtraClientDefined2], "Client Defined 2 field should be returned as data");
    XCTAssertEqualObjects(client3, result[(id)kSecDataInetExtraClientDefined3], "Client Defined 3 field should be returned as data");
}

// When this test starts failing, hopefully rdar://problem/60332379 got fixed
- (void)testBadDateCausesDERDecodeValidationError {
    // Wonky time calculation hastily stolen from SecGregorianDateGetAbsoluteTime and tweaked
    // As of right now this causes CFCalendarDecomposeAbsoluteTime with Zulu calendar to give a seemingly incorrect date which then causes DER date validation issues
    CFAbsoluteTime absTime = (CFAbsoluteTime)(((-(1902 * 365) + -38) * 24 + 0) * 60 + -1) * 60 + 1;
    absTime -= 0.0004;  // Just to make sure the nanoseconds keep getting encoded/decoded properly
    CFDateRef date = CFDateCreate(NULL, absTime);

    CFErrorRef error = NULL;
    size_t plistSize = der_sizeof_plist(date, &error);
    XCTAssert(error == NULL);
    XCTAssertGreaterThan(plistSize, 0);

    // Encode without repair does not validate dates because that changes behavior I do not want to fiddle with
    uint8_t* der = calloc(1, plistSize);
    uint8_t* der_end = der + plistSize;
    uint8_t* result = der_encode_plist(date, &error, der, der_end);
    XCTAssert(error == NULL);
    XCTAssertEqual(der, result);

    // ...but decoding does and will complain
    CFPropertyListRef decoded = NULL;
    XCTAssert(der_decode_plist(NULL, &decoded, &error, der, der_end) == NULL);
    XCTAssert(error != NULL);
    XCTAssertEqual(CFErrorGetDomain(error), kCFErrorDomainOSStatus);
    NSString* description = CFBridgingRelease(CFErrorCopyDescription(error));
    XCTAssert([description containsString:@"Invalid date"]);

    CFReleaseNull(error);
    free(der);
}

// When this test starts failing, hopefully rdar://problem/60332379 got fixed
- (void)testBadDateWithDEREncodingRepairProducesDefaultValue {
    // Wonky time calculation hastily stolen from SecGregorianDateGetAbsoluteTime and tweaked
    // As of right now this causes CFCalendarDecomposeAbsoluteTime with Zulu calendar to give a seemingly incorrect date which then causes DER date validation issues
    CFAbsoluteTime absTime = (CFAbsoluteTime)(((-(1902 * 365) + -38) * 24 + 0) * 60 + -1) * 60 + 1;
    absTime -= 0.0004;  // Just to make sure the nanoseconds keep getting encoded/decoded properly
    CFDateRef date = CFDateCreate(NULL, absTime);

    CFErrorRef error = NULL;
    size_t plistSize = der_sizeof_plist(date, &error);
    XCTAssert(error == NULL);
    XCTAssertGreaterThan(plistSize, 0);

    uint8_t* der = calloc(1, plistSize);
    uint8_t* der_end = der + plistSize;
    uint8_t* encoderesult = der_encode_plist_repair(date, &error, true, der, der_end);
    XCTAssert(error == NULL);
    XCTAssertEqual(der, encoderesult);

    CFPropertyListRef decoded = NULL;
    const uint8_t* decoderesult = der_decode_plist(NULL, &decoded, &error, der, der_end);
    XCTAssertEqual(der_end, decoderesult);
    XCTAssertEqual(CFGetTypeID(decoded), CFDateGetTypeID());
    XCTAssertEqualWithAccuracy(CFDateGetAbsoluteTime(decoded), 0, 60 * 60 * 24);
}

- (void)testContainersWithBadDateWithDEREncodingRepairProducesDefaultValue {
    // Wonky time calculation hastily stolen from SecGregorianDateGetAbsoluteTime and tweaked
    // As of right now this causes CFCalendarDecomposeAbsoluteTime with Zulu calendar to give a seemingly incorrect date which then causes DER date validation issues
    CFAbsoluteTime absTime = (CFAbsoluteTime)(((-(1902 * 365) + -38) * 24 + 0) * 60 + -1) * 60 + 1;
    absTime -= 0.0004;
    CFDateRef date = CFDateCreate(NULL, absTime);

    NSDictionary* dict = @{
        @"dateset": [NSSet setWithObject:(__bridge id)date],
        @"datearray": @[(__bridge id)date],
    };

    CFErrorRef error = NULL;
    size_t plistSize = der_sizeof_plist((__bridge CFTypeRef)dict, &error);
    XCTAssertNil((__bridge NSError*)error, "Should be no error checking the size of the plist");
    XCTAssertGreaterThan(plistSize, 0);

    uint8_t* der = calloc(1, plistSize);
    uint8_t* der_end = der + plistSize;
    uint8_t* encoderesult = der_encode_plist_repair((__bridge CFTypeRef)dict, &error, true, der, der_end);
    XCTAssertNil((__bridge NSError*)error, "Should be no error encoding the plist");
    XCTAssertEqual(der, encoderesult);

    CFPropertyListRef decoded = NULL;
    const uint8_t* decoderesult = der_decode_plist(NULL, &decoded, &error, der, der_end);
    XCTAssertNil((__bridge NSError*)error, "Should be no error decoding the plist");
    XCTAssertEqual(der_end, decoderesult);

    XCTAssertNotNil((__bridge NSDictionary*)decoded, "Should have decoded some dictionary");
    if(decoded == nil) {
        return;
    }

    XCTAssertEqual(CFGetTypeID(decoded), CFDictionaryGetTypeID());
    CFDictionaryRef decodedCFDictionary = decoded;

    {
        CFSetRef decodedCFSet = CFDictionaryGetValue(decodedCFDictionary, CFSTR("dateset"));
        XCTAssertNotNil((__bridge NSSet*)decodedCFSet, "Should have some CFSet");

        if(decodedCFSet != NULL) {
            XCTAssertEqual(CFGetTypeID(decodedCFSet), CFSetGetTypeID());
            XCTAssertEqual(CFSetGetCount(decodedCFSet), 1, "Should have one item in set");

            __block bool dateprocessed = false;
            CFSetForEach(decodedCFSet, ^(const void *value) {
                XCTAssertEqual(CFGetTypeID(value), CFDateGetTypeID());
                XCTAssertEqualWithAccuracy(CFDateGetAbsoluteTime(value), 0, 60 * 60 * 24);
                dateprocessed = true;
            });

            XCTAssertTrue(dateprocessed, "Should have processed at least one date in the set");
        }
    }

    {
        CFArrayRef decodedCFArray =  CFDictionaryGetValue(decodedCFDictionary, CFSTR("datearray"));
        XCTAssertNotNil((__bridge NSArray*)decodedCFArray, "Should have some CFArray");

        if(decodedCFArray != NULL) {
            XCTAssertEqual(CFGetTypeID(decodedCFArray), CFArrayGetTypeID());
            XCTAssertEqual(CFArrayGetCount(decodedCFArray), 1, "Should have one item in array");

            __block bool dateprocessed = false;
            CFArrayForEach(decodedCFArray, ^(const void *value) {
                XCTAssertEqual(CFGetTypeID(value), CFDateGetTypeID());
                XCTAssertEqualWithAccuracy(CFDateGetAbsoluteTime(value), 0, 60 * 60 * 24);
                dateprocessed = true;
            });

            XCTAssertTrue(dateprocessed, "Should have processed at least one date in the array");
        }
    }

    CFReleaseNull(decoded);
}

- (void)testSecItemCopyMatchingWithBadDateInItem {
    // Wonky time calculation hastily stolen from SecGregorianDateGetAbsoluteTime and tweaked
    // As of right now this causes CFCalendarDecomposeAbsoluteTime with Zulu calendar to give a seemingly incorrect date which then causes DER date validation issues
    CFAbsoluteTime absTime = (CFAbsoluteTime)(((-(1902 * 365) + -38) * 24 + 0) * 60 + -1) * 60 + 1;
    absTime -= 0.0004;
    CFDateRef date = CFDateCreate(NULL, absTime);

    NSDictionary* addQuery = @{
        //(id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrCreationDate : (__bridge id)date,
        (id)kSecAttrModificationDate : (__bridge id)date,

        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrAccount : @"TestAccount",
        (id)kSecAttrService : @"TestService",
        (id)kSecAttrAccessGroup : @"com.apple.security.securityd",

        (id)kSecAttrAccessible  : @"ak",
        (id)kSecAttrTombstone : [NSNumber numberWithInt: 0],
        (id)kSecAttrMultiUser : [[NSData alloc] init],
    };

    __block CFErrorRef cferror = NULL;
    kc_with_dbt(true, &cferror, ^bool(SecDbConnectionRef dbt) {
        return kc_transaction_type(dbt, kSecDbExclusiveTransactionType, &cferror, ^bool {
            SecDbItemRef item = SecDbItemCreateWithAttributes(NULL, kc_class_with_name(kSecClassGenericPassword), (__bridge CFDictionaryRef)addQuery, KEYBAG_DEVICE, &cferror);

            bool ret = SecDbItemInsert(item, dbt, false, &cferror);

            XCTAssertTrue(ret, "Should be able to add an item");
            CFReleaseNull(item);

            return ret;
        });
    });

    NSDictionary* findQuery = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrAccount : @"TestAccount",
        (id)kSecAttrService : @"TestService",
        (id)kSecAttrAccessGroup : @"com.apple.security.securityd",

        (id)kSecReturnAttributes : @YES,
        (id)kSecReturnData: @YES,
        (id)kSecMatchLimit: (id)kSecMatchLimitAll,
    };

    CFTypeRef result = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findQuery, &result), errSecItemNotFound, @"Should not be able to find our misdated item");

    // This is a bit of a mystery: how do these items have a bad date that's not in the item?

    CFReleaseNull(result);
    CFReleaseNull(date);
}

- (void)testSecItemCopyMatchingWithBadDateInSQLColumn {
    NSDictionary* addQuery = @{
        (id)kSecClass : (id)kSecClassGenericPassword,

        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrAccount : @"TestAccount",
        (id)kSecAttrService : @"TestService",
        (id)kSecAttrAccessGroup : @"com.apple.security.securityd",

        (id)kSecAttrAccessible  : @"ak",
        (id)kSecAttrService : @"",

        (id)kSecUseDataProtectionKeychain: @YES,
    };

    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)addQuery, NULL);
    XCTAssertEqual(status, errSecSuccess, "Should be able to add an item to the keychain");

    // Modify the cdat/mdat columns in the keychain db
    __block CFErrorRef cferror = NULL;
    kc_with_dbt(true, &cferror, ^bool(SecDbConnectionRef dbt) {
        return kc_transaction_type(dbt, kSecDbExclusiveTransactionType, &cferror, ^bool {
            CFErrorRef updateError = NULL;

            // Magic number extracted from testSecItemCopyMatchingWithBadDateInItem
            SecDbExec(dbt,
                      (__bridge CFStringRef)@"UPDATE genp SET cdat = -59984755259.0004, mdat = -59984755259.0004;",
                      &updateError);

            XCTAssertNil((__bridge NSError*)updateError, "Should be no error updating the table");
            CFReleaseNull(updateError);

            return true;
        });
    });

    // Can we find the item?
    NSDictionary* findQuery = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrAccount : @"TestAccount",
        (id)kSecAttrService : @"TestService",
        (id)kSecAttrAccessGroup : @"com.apple.security.securityd",

        (id)kSecReturnAttributes : @YES,
        (id)kSecReturnData: @YES,
        (id)kSecMatchLimit: (id)kSecMatchLimitAll,
    };

    CFTypeRef result = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findQuery, &result), errSecSuccess, @"Should be able to find our misdated item");

    CFReleaseNull(result);
}

- (void)testDurableWriteAPI
{
    NSDictionary* addQuery = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrAccount : @"TestAccount",
        (id)kSecAttrService : @"TestService",
        (id)kSecUseDataProtectionKeychain : @(YES),
        (id)kSecReturnAttributes : @(YES),
    };

    NSDictionary* updateQuery = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : @"TestAccount",
        (id)kSecAttrService : @"TestService",
        (id)kSecUseDataProtectionKeychain : @(YES),
    };

    CFTypeRef result = NULL;

    // Add the item
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery, &result), errSecSuccess, @"Should have succeeded in adding test item to keychain");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemAdd");
    CFReleaseNull(result);

    // Using the API without the entitlement should fail
    CFErrorRef cferror = NULL;
    XCTAssertEqual(SecItemPersistKeychainWritesAtHighPerformanceCost(&cferror), errSecMissingEntitlement, @"Should not be able to persist keychain writes without the entitlement");
    XCTAssertNotNil((__bridge NSError*)cferror, "Should be an error persisting keychain writes without the entitlement");
    CFReleaseNull(cferror);

    // But with the entitlement, you're good
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivatePerformanceImpactingAPI, kCFBooleanTrue);

    XCTAssertEqual(SecItemPersistKeychainWritesAtHighPerformanceCost(&cferror), errSecSuccess, @"Should be able to persist keychain writes");
    XCTAssertNil((__bridge NSError*)cferror, "Should be no error persisting keychain writes");

    // And we can update the item
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)updateQuery,
                                 (__bridge CFDictionaryRef)@{
                                     (id)kSecValueData: [@"otherpassword" dataUsingEncoding:NSUTF8StringEncoding],
                                                           }),
                   errSecSuccess, "should be able to update item with clean update query");

    XCTAssertEqual(SecItemPersistKeychainWritesAtHighPerformanceCost(&cferror), errSecSuccess, @"Should be able to persist keychain writes after an update");
    XCTAssertNil((__bridge NSError*)cferror, "Should be no error persisting keychain writes");

    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)updateQuery), errSecSuccess, "Should be able to delete item");
    XCTAssertEqual(SecItemPersistKeychainWritesAtHighPerformanceCost(&cferror), errSecSuccess, @"Should be able to persist keychain writes after a delete");
    XCTAssertNil((__bridge NSError*)cferror, "Should be no error persisting keychain writes");
}


#pragma mark - SecItemRateLimit

// This is not super accurate in BATS, so put some margin around what you need
- (void)sleepAlternativeForXCTest:(double)interval
{
    dispatch_semaphore_t localsema = dispatch_semaphore_create(0);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * interval), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
        dispatch_semaphore_signal(localsema);
    });
    dispatch_semaphore_wait(localsema, DISPATCH_TIME_FOREVER);
}

- (void)testSecItemRateLimitTimePasses {
    SecItemRateLimit* rl = [SecItemRateLimit getStaticRateLimit];
    [rl forceEnabled: true];

    for (int idx = 0; idx < rl.roCapacity; ++idx) {
        XCTAssertTrue(isReadOnlyAPIRateWithinLimits());
    }

    for (int idx = 0; idx < rl.rwCapacity; ++idx) {
        XCTAssertTrue(isModifyingAPIRateWithinLimits());
    }

    [self sleepAlternativeForXCTest: 2];
    XCTAssertTrue(isReadOnlyAPIRateWithinLimits());
    XCTAssertTrue(isModifyingAPIRateWithinLimits());

    [SecItemRateLimit resetStaticRateLimit];
}

- (void)testSecItemRateLimitResetAfterExceed {
    SecItemRateLimit* rl = [SecItemRateLimit getStaticRateLimit];
	[rl forceEnabled: true];

    for (int idx = 0; idx < rl.roCapacity; ++idx) {
        XCTAssertTrue(isReadOnlyAPIRateWithinLimits());
    }
    XCTAssertFalse(isReadOnlyAPIRateWithinLimits());
	XCTAssertTrue(isReadOnlyAPIRateWithinLimits());

    for (int idx = 0; idx < rl.rwCapacity; ++idx) {
        XCTAssertTrue(isModifyingAPIRateWithinLimits());
    }
    XCTAssertFalse(isModifyingAPIRateWithinLimits());
    XCTAssertTrue(isModifyingAPIRateWithinLimits());

    [SecItemRateLimit resetStaticRateLimit];
}

- (void)testSecItemRateLimitMultiplier {
    SecItemRateLimit* rl = [SecItemRateLimit getStaticRateLimit];
    [rl forceEnabled: true];

    int ro_iterations_before = 0;
    for (; ro_iterations_before < rl.roCapacity; ++ro_iterations_before) {
        XCTAssertTrue(isReadOnlyAPIRateWithinLimits());
    }
    XCTAssertFalse(isReadOnlyAPIRateWithinLimits());

    int rw_iterations_before = 0;
    for (; rw_iterations_before < rl.rwCapacity; ++rw_iterations_before) {
        XCTAssertTrue(isModifyingAPIRateWithinLimits());
    }
    XCTAssertFalse(isModifyingAPIRateWithinLimits());


    int ro_iterations_after = 0;
    for (; ro_iterations_after < rl.roCapacity; ++ro_iterations_after) {
        XCTAssertTrue(isReadOnlyAPIRateWithinLimits());
    }
    XCTAssertFalse(isReadOnlyAPIRateWithinLimits());

    int rw_iterations_after = 0;
    for (; rw_iterations_after < rl.rwCapacity; ++rw_iterations_after) {
        XCTAssertTrue(isModifyingAPIRateWithinLimits());
    }
    XCTAssertFalse(isModifyingAPIRateWithinLimits());

    XCTAssertEqualWithAccuracy(rl.limitMultiplier * ro_iterations_before, ro_iterations_after, 1);
    XCTAssertEqualWithAccuracy(rl.limitMultiplier * rw_iterations_before, rw_iterations_after, 1);
    [SecItemRateLimit resetStaticRateLimit];
}

// We stipulate that this test is run on an internal release.
// If this were a platform binary limits would be enforced, but it should not be so they should not.
- (void)testSecItemRateLimitInternalPlatformBinariesOnly {
    SecItemRateLimit* rl = [SecItemRateLimit getStaticRateLimit];

    for (int idx = 0; idx < 3 * MAX(rl.roCapacity, rl.rwCapacity); ++idx) {
        XCTAssertTrue(isReadOnlyAPIRateWithinLimits());
        XCTAssertTrue(isModifyingAPIRateWithinLimits());
    }

    [SecItemRateLimit resetStaticRateLimit];
}

@end

#endif
