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
#import "OTConstants.h"

#import <utilities/SecCFWrappers.h>
#include <utilities/SecDb.h>
#include <utilities/SecFileLocations.h>

#import <SecurityFoundation/SFEncryptionOperation.h>

#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>
#include <dispatch/dispatch.h>
#include <sys/stat.h>

#include "keychain/securityd/SecItemSchema.h"
#include "keychain/securityd/SecItemServer.h"
#include "keychain/securityd/SecItemDb.h"
#include "keychain/securityd/SecItemDataSource.h"
#import "SecItemRateLimit_tests.h"
#include "der_plist.h"
#include "ipc/server_security_helpers.h"

#include <Security/SecIdentityPriv.h>
#include <Security/SecBasePriv.h>
#include <Security/SecEntitlements.h>

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

    CFStringRef keychainPath = __SecKeychainCopyPath();
    CFStringRef corruptFilename = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@-iscorrupt"), keychainPath);

    CFStringPerformWithCString(corruptFilename, ^(const char *filename) {
        struct stat markerinfo = {};
        XCTAssertNotEqual(stat(filename, &markerinfo), 0, "Expected not to find corruption marker after killing keychain");
    });

    __block struct stat after = {};
    CFStringPerformWithCString(keychainPath, ^(const char *filename) {
        FILE* file = fopen(filename, "w");
        XCTAssert(file != NULL, "Didn't get a FILE pointer");
        fclose(file);
        XCTAssertEqual(stat(filename, &after), 0, "Unable to stat newly created file");
    });

    CFReleaseNull(corruptFilename);
    CFReleaseNull(keychainPath);

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

            bool ret = SecDbItemInsert(item, dbt, false, false, &cferror);

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

- (void)testSecItemUUIDPersistentRef {
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);
    CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
    CFUUIDBytes uuidBytes = CFUUIDGetUUIDBytes(uuid);
    CFDataRef uuidData = CFDataCreate(NULL, (void*) &uuidBytes, sizeof(uuidBytes));

    CFDataRef pref = _SecItemCreateUUIDBasedPersistentRef(kSecClassInternetPassword, uuidData, NULL);
    XCTAssertNotNil((__bridge NSData*)pref, "persistent ref data should not be nil");
    
    CFStringRef return_class = NULL;
    CFDataRef return_uuid = NULL;
    sqlite_int64 return_rowid;
    
    XCTAssertTrue(_SecItemParsePersistentRef(pref, &return_class, &return_rowid, &return_uuid, NULL));

    XCTAssertNotNil((__bridge NSData*)return_uuid, "uuid should not be nil");
    XCTAssertNotNil((__bridge NSString*)return_class, "class should not be nil");

    NSData *ns_uuid = (__bridge NSData*)return_uuid;
    NSString *ns_class = (__bridge NSString*)return_class;
    
    XCTAssertTrue([ns_uuid isEqualToData:(__bridge NSData*)uuidData], "uuids should be equal)");
    XCTAssertTrue([ns_class isEqualToString:(id)kSecClassInternetPassword], "classes should be equal)");

    CFReleaseNull(return_uuid);
    CFReleaseNull(uuid);
    CFReleaseNull(pref);

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
}

- (void)testSecItemAddAndCopyMatchingWithUUIDPersistentRefs
{
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    NSDictionary* addQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                (id)kSecValueData : [@"uuid" dataUsingEncoding:NSUTF8StringEncoding],
                                (id)kSecAttrAccount : @"TestUUIDPersistentRefAccount",
                                (id)kSecAttrService : @"TestUUIDPersistentRefService",
                                (id)kSecUseDataProtectionKeychain : @(YES),
                                (id)kSecReturnAttributes : @(YES)
                              };

    CFTypeRef result = NULL;

    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery, &result), errSecSuccess, @"Should have succeeded in adding test item to keychain");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemAdd");
    XCTAssertTrue(isDictionary(result), "result should be a dictionary");
    CFReleaseNull(result);

    NSMutableDictionary *addQueryMutable = [addQuery mutableCopy];
    addQueryMutable[(id)kSecValueData] = nil;
    addQueryMutable[(id)kSecReturnPersistentRef] = @(YES);

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)addQueryMutable, &result), errSecSuccess, @"Should have found item in keychain");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemCopyMatching");

    XCTAssertTrue(isDictionary(result), "result should be a dictionary");

    CFDataRef uuid = CFDictionaryGetValue(result, v10itempersistentref.name);
    XCTAssertNil((__bridge id)uuid, "uuid should be nil");

    CFDataRef classAndUUIDPersistentRef = CFDictionaryGetValue(result, kSecValuePersistentRef);
    XCTAssertNotNil((__bridge id)classAndUUIDPersistentRef, "classAndUUIDPersistentRef should not be nil");

    CFStringRef return_class = NULL;
    sqlite_int64 return_rowid = 0;
    CFDataRef return_uuid = NULL;

    _SecItemParsePersistentRef(classAndUUIDPersistentRef, &return_class, &return_rowid, &return_uuid, NULL);
    XCTAssertNotNil((__bridge id)return_uuid, "return_uuid should not be nil");
    XCTAssertNotNil((__bridge id)return_class, "return_class should not be nil");

    XCTAssertEqualObjects((__bridge id)return_class, (__bridge id)kSecClassGenericPassword, "classes should be equal");

    NSDictionary* findPersistentRefQuery = @{ (id)kSecValuePersistentRef : (__bridge NSData*)classAndUUIDPersistentRef,
                                              (id)kSecReturnAttributes : @YES,
                                              (id)kSecMatchLimit : (id)kSecMatchLimitOne
    };

    CFTypeRef foundRef = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findPersistentRefQuery, &foundRef), errSecSuccess, @"Should have found persistent reference in keychain");

    XCTAssertNotNil((__bridge id)foundRef, "foundRef should not be nil");
    XCTAssertTrue(isDictionary(foundRef), "foundRef should be a dictionary");

    uuid = CFDictionaryGetValue(foundRef, v10itempersistentref.name);
    XCTAssertNil((__bridge id)uuid, "uuid should be nil");

    CFDataRef reFetchedPersistentRef = CFDictionaryGetValue(result, kSecValuePersistentRef);
    CFStringRef second_return_class = NULL;
    sqlite_int64 second_return_rowid = 0;
    CFDataRef second_return_uuid = NULL;
    _SecItemParsePersistentRef(reFetchedPersistentRef, &second_return_class, &second_return_rowid, &second_return_uuid, NULL);

    XCTAssertEqualObjects((__bridge id)return_class, (__bridge id)second_return_class, "classes should be equal");
    XCTAssertTrue(return_rowid == second_return_rowid, "rowids should be equal");
    XCTAssertEqualObjects((__bridge id)return_uuid, (__bridge id)second_return_uuid, "uuids should be equal");

    CFReleaseNull(return_uuid);
    CFReleaseNull(second_return_uuid);
    CFReleaseNull(result);

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
}

- (void)testSecItemCopyMatchingDoesNotReturnPersistRefs
{
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    NSDictionary* addQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                (id)kSecValueData : [@"uuid" dataUsingEncoding:NSUTF8StringEncoding],
                                (id)kSecAttrAccount : @"TestUUIDPersistentRefAccount",
                                (id)kSecAttrService : @"TestUUIDPersistentRefService",
                                (id)kSecUseDataProtectionKeychain : @(YES),
                                (id)kSecReturnAttributes : @(YES),
                                (id)kSecReturnPersistentRef : @(YES)
                              };

    CFTypeRef result = NULL;

    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery, &result), errSecSuccess, @"Should have succeeded in adding test item to keychain");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemAdd");
    XCTAssertTrue(isDictionary(result), "result should be a dictionary");
    CFDataRef persistRef = CFRetainSafe(CFDictionaryGetValue(result, kSecValuePersistentRef));
    XCTAssertNotNil((__bridge id)persistRef, @"persistRef should not be nil");
    CFReleaseNull(result);

    NSDictionary* findPersistentRefQuery = @{ (id)kSecValuePersistentRef : (__bridge NSData*)persistRef,
                                              (id)kSecReturnAttributes : @YES,
                                              (id)kSecMatchLimit : (id)kSecMatchLimitOne
    };
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findPersistentRefQuery, &result), errSecSuccess, @"Should have found item in keychain");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemCopyMatching");
    XCTAssertTrue(isDictionary(result), "result should be a dictionary");

    CFDataRef uuid = CFDictionaryGetValue(result, v10itempersistentref.name);
    XCTAssertNil((__bridge id)uuid, "uuid should be nil");
    CFReleaseNull(persistRef);
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
}

- (void)testKeychainItemUpgradePhase3
{
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);

    // perform a bunch of SecItemAdds without the feature flag enabled so that the keychain has items with a NULL persist ref
    NSDictionary* addQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                (id)kSecValueData : [@"uuid" dataUsingEncoding:NSUTF8StringEncoding],
                                (id)kSecAttrAccount : @"testKeychainItemUpgradePhase3Account1",
                                (id)kSecAttrService : @"TestUUIDPersistentRefService",
                                (id)kSecUseDataProtectionKeychain : @(YES),
                                (id)kSecReturnAttributes : @(YES),
                                (id)kSecReturnPersistentRef : @(YES)
    };

    CFTypeRef result = NULL;

    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery, &result), errSecSuccess, @"Should have succeeded in adding test item to keychain");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemAdd");
    XCTAssertTrue(isDictionary(result), "result should be a dictionary");
    CFDataRef oldStylePersistRef1 = CFRetainSafe(CFDictionaryGetValue(result, kSecValuePersistentRef));
    XCTAssertNotNil((__bridge id)oldStylePersistRef1, @"oldStylePersistRef should not be nil");
    XCTAssertTrue(CFDataGetLength(oldStylePersistRef1) == 12, "oldStylePersistRef should be 12 bytes long");
    CFReleaseNull(result);

    // perform a bunch of SecItemAdds without the feature flag enabled so that the keychain has items with a NULL persist ref
    addQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                (id)kSecValueData : [@"uuid" dataUsingEncoding:NSUTF8StringEncoding],
                                (id)kSecAttrAccount : @"testKeychainItemUpgradePhase3Account3",
                                (id)kSecAttrService : @"TestUUIDPersistentRefService",
                                (id)kSecUseDataProtectionKeychain : @(YES),
                                (id)kSecReturnAttributes : @(YES),
                                (id)kSecReturnPersistentRef : @(YES)
    };

    result = NULL;

    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery, &result), errSecSuccess, @"Should have succeeded in adding test item to keychain");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemAdd");
    XCTAssertTrue(isDictionary(result), "result should be a dictionary");
    CFDataRef oldStylePersistRef3 = CFRetainSafe(CFDictionaryGetValue(result, kSecValuePersistentRef));
    XCTAssertNotNil((__bridge id)oldStylePersistRef3, @"oldStylePersistRef should not be nil");
    XCTAssertTrue(CFDataGetLength(oldStylePersistRef3) == 12, "oldStylePersistRef should be 12 bytes long");
    CFReleaseNull(result);


    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    addQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                  (id)kSecValueData : [@"uuid" dataUsingEncoding:NSUTF8StringEncoding],
                  (id)kSecAttrAccount : @"testKeychainItemUpgradePhase3Account2",
                  (id)kSecAttrService : @"TestUUIDPersistentRefService",
                  (id)kSecUseDataProtectionKeychain : @(YES),
                  (id)kSecReturnAttributes : @(YES),
                  (id)kSecReturnPersistentRef : @(YES)
    };

    result = NULL;

    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery, &result), errSecSuccess, @"Should have succeeded in adding test item to keychain");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemAdd");
    XCTAssertTrue(isDictionary(result), "result should be a dictionary");
    CFDataRef uuidStylePersistRef = CFRetainSafe(CFDictionaryGetValue(result, kSecValuePersistentRef));
    XCTAssertNotNil((__bridge id)uuidStylePersistRef, @"uuidStylePersistRef should not be nil");
    XCTAssertTrue(CFDataGetLength(uuidStylePersistRef) == 20, "uuidStylePersistRef should be 20 bytes long");
    CFReleaseNull(result);

    // now force a phase 3 item upgrade
    __block CFErrorRef kcError = NULL;
    kc_with_dbt(true, &kcError, ^(SecDbConnectionRef dbt) {
        return kc_transaction(dbt, &kcError, ^{
            CFErrorRef localError = NULL;
            CFErrorRef phase3Error = NULL;
            bool inProgress = false;
            bool ok = UpgradeItemPhase3(dbt, &inProgress, &phase3Error);
            if (!ok) {
                SecErrorPropagate(phase3Error, &localError);
            }

            XCTAssertFalse(inProgress, "inProgress bool should be false");
            XCTAssertNil((__bridge id)localError, "error should be nil");
            XCTAssertTrue(ok, "UpgradeItemPhase3 should return true");

            return (bool)true;
        });
    });


    NSDictionary *query = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                             (id)kSecUseDataProtectionKeychain : @(YES),
                             (id)kSecReturnAttributes : @(YES),
                             (id)kSecReturnPersistentRef : @(YES),
                             (id)kSecMatchLimit : (id)kSecMatchLimitAll,
    };

    result = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &result), errSecSuccess, @"Should have succeeded in adding test item to keychain");

    if (isArray(result)) {
        CFDictionaryRef item1 = CFArrayGetValueAtIndex(result, 0);
        CFDictionaryRef item2 = CFArrayGetValueAtIndex(result, 1);
        CFDictionaryRef item3 = CFArrayGetValueAtIndex(result, 2);
        
        NSString* item1Account = (__bridge id)CFDictionaryGetValue(item1, kSecAttrAccount);
        NSString* item2Account = (__bridge id)CFDictionaryGetValue(item2, kSecAttrAccount);
        NSString* item3Account = (__bridge id)CFDictionaryGetValue(item3, kSecAttrAccount);

        NSData* item1PersistRef = (__bridge id)CFDictionaryGetValue(item1, kSecValuePersistentRef);
        NSData* item2PersistRef = (__bridge id)CFDictionaryGetValue(item2, kSecValuePersistentRef);
        NSData* item3PersistRef = (__bridge id)CFDictionaryGetValue(item3, kSecValuePersistentRef);

        if ([item1Account isEqualToString:@"testKeychainItemUpgradePhase3Account1"]) {
            XCTAssertFalse([item1PersistRef isEqualToData:(__bridge NSData*)oldStylePersistRef1], "item update phase 3 should have updated the persistref");
        } else if ([item2Account isEqualToString:@"testKeychainItemUpgradePhase3Account1"]) {
            XCTAssertFalse([item2PersistRef isEqualToData:(__bridge NSData*)oldStylePersistRef1], "item update phase 3 should have updated the persistref");
        } else if ([item3Account isEqualToString:@"testKeychainItemUpgradePhase3Account1"]) {
            XCTAssertFalse([item3PersistRef isEqualToData:(__bridge NSData*)oldStylePersistRef1], "item update phase 3 should have updated the persistref");
        }
        if ([item1Account isEqualToString:@"testKeychainItemUpgradePhase3Account3"]) {
            XCTAssertFalse([item1PersistRef isEqualToData:(__bridge NSData*)oldStylePersistRef3], "item update phase 3 should have updated the persistref");
        } else if ([item2Account isEqualToString:@"testKeychainItemUpgradePhase3Account3"]) {
            XCTAssertFalse([item2PersistRef isEqualToData:(__bridge NSData*)oldStylePersistRef3], "item update phase 3 should have updated the persistref");
        } else if ([item3Account isEqualToString:@"testKeychainItemUpgradePhase3Account3"]) {
            XCTAssertFalse([item3PersistRef isEqualToData:(__bridge NSData*)oldStylePersistRef3], "item update phase 3 should have updated the persistref");
        }

        if ([item1Account isEqualToString:@"testKeychainItemUpgradePhase3Account2"]) {
            XCTAssertTrue([item1PersistRef isEqualToData:(__bridge NSData*)uuidStylePersistRef], "item update phase 3 should NOT have updated the persistref");
        } else if ([item2Account isEqualToString:@"testKeychainItemUpgradePhase3Account2"]) {
            XCTAssertTrue([item2PersistRef isEqualToData:(__bridge NSData*)uuidStylePersistRef], "item update phase 3 should NOT have updated the persistref");
        } else if ([item3Account isEqualToString:@"testKeychainItemUpgradePhase3Account2"]) {
            XCTAssertTrue([item3PersistRef isEqualToData:(__bridge NSData*)uuidStylePersistRef], "item update phase 3 should NOT have updated the persistref");
        }
    } else {
        XCTFail("Expected SecItemCopyMatching to return an Array of 3 items, instead returned: %@", result);
    }
    
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
    CFReleaseNull(oldStylePersistRef1);
    CFReleaseNull(oldStylePersistRef3);
    CFReleaseNull(uuidStylePersistRef);
    
    clearLastRowIDHandledForTests();
}

- (void)testBatchingOfRowIDs
{
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);

    for (int i = 0; i < 250; i++) {
        // perform a bunch of SecItemAdds without the feature flag enabled so that the keychain has items with a NULL persist ref
        NSDictionary* addQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecValueData : [@"uuid" dataUsingEncoding:NSUTF8StringEncoding],
                                    (id)kSecAttrAccount : [NSString stringWithFormat:@"testKeychainItemUpgradePhase3Account%d", i],
                                    (id)kSecAttrService : [NSString stringWithFormat:@"TestUUIDPersistentRefService%d", i],
                                    (id)kSecUseDataProtectionKeychain : @(YES),
                                    (id)kSecReturnAttributes : @(YES),
                                    (id)kSecReturnPersistentRef : @(YES)
        };

        CFTypeRef result = NULL;

        XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery, &result), errSecSuccess, @"Should have succeeded in adding test item to keychain");
        XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemAdd");
        XCTAssertTrue(isDictionary(result), "result should be a dictionary");
        CFDataRef oldStylePersistRef1 = CFDictionaryGetValue(result, kSecValuePersistentRef);
        XCTAssertNotNil((__bridge id)oldStylePersistRef1, @"oldStylePersistRef should not be nil");
        XCTAssertTrue(CFDataGetLength(oldStylePersistRef1) == 12, "oldStylePersistRef should be 12 bytes long");
        CFReleaseNull(result);
    }

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    // now force a phase 3 item upgrade, inProgress bit should be true
    __block CFErrorRef kcError = NULL;
    kc_with_dbt(true, &kcError, ^(SecDbConnectionRef dbt) {
        return kc_transaction(dbt, &kcError, ^{
            CFErrorRef localError = NULL;
            CFErrorRef phase3Error = NULL;
            bool inProgress = false;
            bool ok = UpgradeItemPhase3(dbt, &inProgress, &phase3Error);
            if (!ok) {
                SecErrorPropagate(phase3Error, &localError);
            }

            XCTAssertTrue(inProgress, "inProgress bool should be true");
            XCTAssertNil((__bridge id)localError, "error should be nil");
            XCTAssertTrue(ok, "UpgradeItemPhase3 should return true");

            return (bool)true;
        });
    });

    CFReleaseNull(kcError);

    // now force a phase 3 item upgrade /again/ and the inProgress bit should be true
    kc_with_dbt(true, &kcError, ^(SecDbConnectionRef dbt) {
        return kc_transaction(dbt, &kcError, ^{
            CFErrorRef localError = NULL;
            CFErrorRef phase3Error = NULL;
            bool inProgress = false;
            bool ok = UpgradeItemPhase3(dbt, &inProgress, &phase3Error);
            if (!ok) {
                SecErrorPropagate(phase3Error, &localError);
            }

            XCTAssertTrue(inProgress, "inProgress bool should be true");
            XCTAssertNil((__bridge id)localError, "error should be nil");
            XCTAssertTrue(ok, "UpgradeItemPhase3 should return true");

            return (bool)true;
        });
    });

    CFReleaseNull(kcError);

    // now force a phase 3 item upgrade /again/ and the inProgress bit should be false
    kc_with_dbt(true, &kcError, ^(SecDbConnectionRef dbt) {
        return kc_transaction(dbt, &kcError, ^{
            CFErrorRef localError = NULL;
            CFErrorRef phase3Error = NULL;
            bool inProgress = false;
            bool ok = UpgradeItemPhase3(dbt, &inProgress, &phase3Error);
            if (!ok) {
                SecErrorPropagate(phase3Error, &localError);
            }

            XCTAssertFalse(inProgress, "inProgress bool should be false");
            XCTAssertNil((__bridge id)localError, "error should be nil");
            XCTAssertTrue(ok, "UpgradeItemPhase3 should return true");

            return (bool)true;
        });
    });
    
    clearLastRowIDHandledForTests();
}

- (void)testContinuedProcessingKeychainItemsIfLowerRowIDsAreCorrupt
{
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
    
    /* add a password */
    const char *v_data = "test";
    NSData *pwdata = [[NSData alloc]initWithBytes:v_data length:strlen(v_data)];
    
    for (int i = 0; i < 100; i++) {
        // perform a bunch of SecItemAdds without the feature flag enabled so that the keychain has items with a NULL persist ref
        NSDictionary* addQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccount : [NSString stringWithFormat:@"smith%d", i],
                                    (id)kSecAttrService : @"corrupt.spamcop.net",
                                    (id)kSecUseDataProtectionKeychain : @(YES),
                                    (id)kSecReturnAttributes : @(YES),
                                    (id)kSecReturnPersistentRef : @(YES),
                                    (id)kSecValueData : pwdata
        };

        CFTypeRef result = NULL;

        XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery, &result), errSecSuccess, @"Should have succeeded in adding test item to keychain");
    }

    SecKeychainDbForceClose();
    SecKeychainDbReset(^{
        /* corrupt all the password */
        NSString *keychain_path = CFBridgingRelease(__SecKeychainCopyPath());
        char corrupt_item_sql[80];
        sqlite3 *db;

        int opened = sqlite3_open([keychain_path UTF8String], &db);
        XCTAssertEqual(opened, SQLITE_OK, @"should be SQLITE_OK");
        
        for(int i=1; i<=100;i++) {
            snprintf(corrupt_item_sql, sizeof(corrupt_item_sql), "UPDATE genp SET data=X'12345678' WHERE rowid=%d", i);
          
            int statement_executed = sqlite3_exec(db, corrupt_item_sql, NULL, NULL, NULL);
            XCTAssertEqual(statement_executed, SQLITE_OK, @"corrupting keychain item");
        }

        int closed = sqlite3_close_v2(db);
        XCTAssertEqual(closed, SQLITE_OK, @"Should be able to close db");
    });
    
    // should have 100 corrupted items in the keychain now
    //now add 250 more 
    
    for (int i = 0; i < 250; i++) {
        // perform a bunch of SecItemAdds without the feature flag enabled so that the keychain has items with a NULL persist ref
        NSDictionary* addQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecValueData : [@"uuid" dataUsingEncoding:NSUTF8StringEncoding],
                                    (id)kSecAttrAccount : [NSString stringWithFormat:@"testKeychainItemUpgradePhase3Account%d", i],
                                    (id)kSecAttrService : [NSString stringWithFormat:@"TestUUIDPersistentRefService%d", i],
                                    (id)kSecUseDataProtectionKeychain : @(YES),
                                    (id)kSecReturnAttributes : @(YES),
                                    (id)kSecReturnPersistentRef : @(YES)
        };

        CFTypeRef result = NULL;

        XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery, &result), errSecSuccess, @"Should have succeeded in adding test item to keychain");
        XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemAdd");
        XCTAssertTrue(isDictionary(result), "result should be a dictionary");
        CFDataRef oldStylePersistRef1 = CFDictionaryGetValue(result, kSecValuePersistentRef);
        XCTAssertNotNil((__bridge id)oldStylePersistRef1, @"oldStylePersistRef should not be nil");
        XCTAssertTrue(CFDataGetLength(oldStylePersistRef1) == 12, "oldStylePersistRef should be 12 bytes long");
        CFReleaseNull(result);
    }

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    // now force a phase 3 item upgrade, inProgress bit should be true
    __block CFErrorRef kcError = NULL;
    kc_with_dbt(true, &kcError, ^(SecDbConnectionRef dbt) {
        return kc_transaction(dbt, &kcError, ^{
            CFErrorRef localError = NULL;
            CFErrorRef phase3Error = NULL;
            bool inProgress = false;
            bool ok = UpgradeItemPhase3(dbt, &inProgress, &phase3Error);
            if (!ok) {
                SecErrorPropagate(phase3Error, &localError);
            }

            XCTAssertTrue(inProgress, "inProgress bool should be true");
            XCTAssertNil((__bridge id)localError, "error should be nil");
            XCTAssertTrue(ok, "UpgradeItemPhase3 should return true");

            return (bool)true;
        });
    });

    CFReleaseNull(kcError);

    // now force a phase 3 item upgrade /again/ and the inProgress bit should be true
    kc_with_dbt(true, &kcError, ^(SecDbConnectionRef dbt) {
        return kc_transaction(dbt, &kcError, ^{
            CFErrorRef localError = NULL;
            CFErrorRef phase3Error = NULL;
            bool inProgress = false;
            bool ok = UpgradeItemPhase3(dbt, &inProgress, &phase3Error);
            if (!ok) {
                SecErrorPropagate(phase3Error, &localError);
            }

            XCTAssertTrue(inProgress, "inProgress bool should be true");
            XCTAssertNil((__bridge id)localError, "error should be nil");
            XCTAssertTrue(ok, "UpgradeItemPhase3 should return true");

            return (bool)true;
        });
    });

    CFReleaseNull(kcError);

    // now force a phase 3 item upgrade /again/ and the inProgress bit should be true
    kc_with_dbt(true, &kcError, ^(SecDbConnectionRef dbt) {
        return kc_transaction(dbt, &kcError, ^{
            CFErrorRef localError = NULL;
            CFErrorRef phase3Error = NULL;
            bool inProgress = false;
            bool ok = UpgradeItemPhase3(dbt, &inProgress, &phase3Error);
            if (!ok) {
                SecErrorPropagate(phase3Error, &localError);
            }

            XCTAssertTrue(inProgress, "inProgress bool should be true");
            XCTAssertNil((__bridge id)localError, "error should be nil");
            XCTAssertTrue(ok, "UpgradeItemPhase3 should return true");

            return (bool)true;
        });
    });
    
    
    // now force a phase 3 item upgrade /again again/ and the inProgress bit should remain false even with 100 corrupted items
    kc_with_dbt(true, &kcError, ^(SecDbConnectionRef dbt) {
        return kc_transaction(dbt, &kcError, ^{
            CFErrorRef localError = NULL;
            CFErrorRef phase3Error = NULL;
            bool inProgress = false;
            bool ok = UpgradeItemPhase3(dbt, &inProgress, &phase3Error);
            if (!ok) {
                SecErrorPropagate(phase3Error, &localError);
            }

            XCTAssertFalse(inProgress, "inProgress bool should be false");
            XCTAssertNil((__bridge id)localError, "error should be nil");
            XCTAssertTrue(ok, "UpgradeItemPhase3 should return true");

            return (bool)true;
        });
    });
    
    NSMutableDictionary* copyMatchingCorruptQuery = [NSMutableDictionary dictionary];
    copyMatchingCorruptQuery[(id)kSecClass] = (id)kSecClassGenericPassword;
    copyMatchingCorruptQuery[(id)kSecAttrService] = @"corrupt.spamcop.net";
    copyMatchingCorruptQuery[(id)kSecReturnAttributes] = @YES;
    copyMatchingCorruptQuery[(id)kSecReturnPersistentRef] = @YES;
    copyMatchingCorruptQuery[(id)kSecMatchLimit] = (id)kSecMatchLimitAll;

    CFTypeRef item = NULL;
    // this call will remove all corrupted items and should return errSecItemNotFound
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)copyMatchingCorruptQuery, &item), errSecItemNotFound);
    XCTAssertNil((__bridge id)item, "item should be nil");
    
    clearLastRowIDHandledForTests();
}

- (void)testDSItemMergeWithPersistentRefs
{
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKS, @YES);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementKeychainAccessGroups, @YES);

    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecAttrAccount : @"testaccount",
        (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
        (id)kSecAttrSyncViewHint : @"Passwords",
        (id)kSecAttrPCSPlaintextPublicKey : [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecReturnPersistentRef : @YES

    } mutableCopy];

    CFTypeRef prefGENP = NULL;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, &prefGENP), errSecSuccess);
    XCTAssertNotNil((__bridge id)prefGENP, "persistent ref should not be nil");
    XCTAssertTrue(CFDataGetLength(prefGENP) == 20, "persistent ref length should be 20");

    query = nil;
    query = [NSMutableDictionary dictionary];
    query[(id)kSecValuePersistentRef] = (__bridge id)(prefGENP);
    query[(id)kSecReturnAttributes] = @YES;

    CFTypeRef item = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &item), errSecSuccess);
    XCTAssertNotNil((__bridge id)item, "item should not be nil");
    XCTAssertTrue(isDictionary(item), "item should be a dictionary");

    query = [@{
        (id)kSecClass : (id)kSecClassInternetPassword,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecAttrAccount : @"testaccount",
        (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
        (id)kSecAttrSyncViewHint : @"Passwords",
        (id)kSecAttrPCSPlaintextPublicKey : [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecReturnPersistentRef : @YES

    } mutableCopy];

    CFTypeRef prefINET = NULL;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, &prefINET), errSecSuccess);
    XCTAssertNotNil((__bridge id)prefINET, "persistent ref should not be nil");
    XCTAssertTrue(CFDataGetLength(prefINET) == 20, "persistent ref length should be 20");

    query = nil;
    query = [NSMutableDictionary dictionary];
    query[(id)kSecValuePersistentRef] = (__bridge id)(prefINET);
    query[(id)kSecReturnAttributes] = @YES;

    item = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &item), errSecSuccess);
    XCTAssertNotNil((__bridge id)item, "item should not be nil");
    XCTAssertTrue(isDictionary(item), "item should be a dictionary");

    query = [@{
        (id)kSecClass : (id)kSecClassCertificate,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
        (id)kSecAttrSyncViewHint : @"Passwords",
        (id)kSecAttrPCSPlaintextPublicKey : [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecReturnPersistentRef : @YES

    } mutableCopy];

    CFTypeRef prefCERT = NULL;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, &prefCERT), errSecSuccess);
    XCTAssertNotNil((__bridge id)prefCERT, "persistent ref should not be nil");
    XCTAssertTrue(CFDataGetLength(prefCERT) == 20, "persistent ref length should be 20");

    query = nil;
    query = [NSMutableDictionary dictionary];
    query[(id)kSecValuePersistentRef] = (__bridge id)(prefCERT);
    query[(id)kSecReturnAttributes] = @YES;

    item = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &item), errSecSuccess);
    XCTAssertNotNil((__bridge id)item, "item should not be nil");
    XCTAssertTrue(isDictionary(item), "item should be a dictionary");

    query = [@{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
        (id)kSecAttrSyncViewHint : @"Passwords",
        (id)kSecAttrPCSPlaintextPublicKey : [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecReturnPersistentRef : @YES

    } mutableCopy];

    CFTypeRef prefKEY = NULL;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, &prefKEY), errSecSuccess);
    XCTAssertNotNil((__bridge id)prefKEY, "persistent ref should not be nil");
    XCTAssertTrue(CFDataGetLength(prefKEY) == 20, "persistent ref length should be 20");

    query = nil;
    query = [NSMutableDictionary dictionary];
    query[(id)kSecValuePersistentRef] = (__bridge id)(prefKEY);
    query[(id)kSecReturnAttributes] = @YES;

    item = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &item), errSecSuccess);
    XCTAssertNotNil((__bridge id)item, "item should not be nil");
    XCTAssertTrue(isDictionary(item), "item should be a dictionary");

    __block CFErrorRef kcError = NULL;
    kc_with_dbt(true, &kcError, ^(SecDbConnectionRef dbt) {
        return kc_transaction(dbt, &kcError, ^{
            CFErrorRef localError = NULL;
            SOSObjectRef mergedItem = NULL;
            //item doesn't exist case
            const SecDbClass* classP = kc_class_with_name(kSecClassGenericPassword);
            NSMutableDictionary* dict = [@{
                (id)kSecClass : (id)kSecClassGenericPassword,
                (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                (id)kSecAttrAccount : @"testaccount2",
                (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                (id)kSecAttrSyncViewHint : @"Passwords",
                (id)kSecAttrPCSPlaintextPublicKey : [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
            } mutableCopy];

            SecDbItemRef peerObject = SecDbItemCreateWithAttributes(kCFAllocatorDefault, classP, (__bridge CFDictionaryRef)dict, KEYBAG_DEVICE, &localError);
            XCTAssertNotNil((__bridge id)peerObject, "peerObject should not be nil");

            SOSMergeResult result = dsMergeObject((SOSTransactionRef)dbt, (SOSObjectRef)peerObject, &mergedItem, &localError);
            XCTAssertTrue(result == kSOSMergePeersObject, "result should equal a local merge");

            //item that does match an item in the keychain, the incoming item wins but keeps the same persist ref uuid
            dict = [@{
                (id)kSecClass : (id)kSecClassGenericPassword,
                (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                (id)kSecAttrAccount : @"testaccount",
                (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                (id)kSecAttrSyncViewHint : @"Passwords",
                (id)kSecAttrPCSPlaintextPublicKey : [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
            } mutableCopy];

            peerObject = SecDbItemCreateWithAttributes(kCFAllocatorDefault, classP, (__bridge CFDictionaryRef)dict, KEYBAG_DEVICE, &localError);
            XCTAssertNotNil((__bridge id)peerObject, "peerObject should not be nil");

            result = dsMergeObject((SOSTransactionRef)dbt, (SOSObjectRef)peerObject, &mergedItem, &localError);
            XCTAssertTrue(result == kSOSMergePeersObject, "result should equal a local merge");
            CFDataRef mergedUUIDData = SecDbItemGetPersistentRef((SecDbItemRef)mergedItem, &localError);
            XCTAssertNotNil((__bridge id)mergedUUIDData, "mergedUUIDData should not be nil");
            CFDataRef originalUUIDData = CFDataCreateCopyFromRange(kCFAllocatorDefault, prefGENP, CFRangeMake(4, 16));
            XCTAssertNotNil((__bridge id)originalUUIDData, "originalUUIDData should not be nil");
            XCTAssertEqualObjects((__bridge id)mergedUUIDData, (__bridge id)originalUUIDData, "persist ref UUIDs should be equal");
            return (bool)true;
        });
    });

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
}

- (void)testSecItemFetchDigestsWithUUIDPrefs
{
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecAttrAccount : @"testaccount",
        (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
        (id)kSecAttrSyncViewHint : @"Passwords",
        (id)kSecAttrPCSPlaintextPublicKey : [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecReturnPersistentRef : @YES

    } mutableCopy];

    CFTypeRef pref1 = NULL;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, &pref1), errSecSuccess);
    XCTAssertNotNil((__bridge id)pref1, "persistent ref should not be nil");
    XCTAssertTrue(CFDataGetLength(pref1) == 20, "persistent ref length should be 20");

    query = nil;
    query = [NSMutableDictionary dictionary];
    query[(id)kSecValuePersistentRef] = (__bridge id)(pref1);
    query[(id)kSecReturnAttributes] = @YES;

    CFTypeRef item = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &item), errSecSuccess);
    XCTAssertNotNil((__bridge id)item, "item should not be nil");
    XCTAssertTrue(isDictionary(item), "item should be a dictionary");

    query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecAttrAccount : @"testaccount2",
        (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
        (id)kSecAttrSyncViewHint : @"Passwords",
        (id)kSecAttrPCSPlaintextPublicKey : [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecReturnPersistentRef : @YES

    } mutableCopy];

    CFTypeRef pref2 = NULL;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, &pref2), errSecSuccess);
    XCTAssertNotNil((__bridge id)pref2, "persistent ref should not be nil");
    XCTAssertTrue(CFDataGetLength(pref2) == 20, "persistent ref length should be 20");


    _SecItemFetchDigests((id)kSecClassGenericPassword, @"com.apple.security.ckks", ^(NSArray *items, NSError *error) {
        XCTAssertNotNil(items, "items should not be nil");
        XCTAssertEqual([items count], 2, "there should be 2 items");
        NSDictionary* item1 = items[0];
        NSDictionary* item2 = items[1];
        XCTAssertNotNil(item1, "item1 should not be nil");
        XCTAssertNotNil(item2, "item2 should not be nil");

        NSData* data1 = item1[(id)kSecValueData];
        NSData* data2 = item2[(id)kSecValueData];
        XCTAssertNotNil(data1, "data1 should not be nil");
        XCTAssertNotNil(data2, "data2 should not be nil");

        NSData* item1Pref = item1[(id)kSecValuePersistentRef];
        NSData* item2Pref = item2[(id)kSecValuePersistentRef];
        XCTAssertNotNil(item1, "item1 should not be nil");
        XCTAssertNotNil(item2, "item2 should not be nil");

        XCTAssertEqualObjects(item1Pref, (__bridge id)pref1, "persistent refs should be equal");
        XCTAssertEqualObjects(item2Pref, (__bridge id)pref2, "persistent refs should be equal");

        XCTAssertNil(error, "error should be nil");
    });

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
}

static NSString *certDataBase64 = @"\
MIIEQjCCAyqgAwIBAgIJAJdFadWqNIfiMA0GCSqGSIb3DQEBBQUAMHMxCzAJBgNVBAYTAkNaMQ8wDQYD\
VQQHEwZQcmFndWUxFTATBgNVBAoTDENvc21vcywgSW5jLjEXMBUGA1UEAxMOc3VuLmNvc21vcy5nb2Qx\
IzAhBgkqhkiG9w0BCQEWFHRoaW5nQHN1bi5jb3Ntb3MuZ29kMB4XDTE2MDIyNjE0NTQ0OVoXDTE4MTEy\
MjE0NTQ0OVowczELMAkGA1UEBhMCQ1oxDzANBgNVBAcTBlByYWd1ZTEVMBMGA1UEChMMQ29zbW9zLCBJ\
bmMuMRcwFQYDVQQDEw5zdW4uY29zbW9zLmdvZDEjMCEGCSqGSIb3DQEJARYUdGhpbmdAc3VuLmNvc21v\
cy5nb2QwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC5u9gnYEDzQIVu7yC40VcXTZ01D9CJ\
oD/mH62tebEHEdfVPLWKeq+uAHnJ6fTIJQvksaISOxwiOosFjtI30mbe6LZ/oK22wYX+OUwKhAYjZQPy\
RYfuaJe/52F0zmfUSJ+KTbUZrXbVVFma4xPfpg4bptvtGkFJWnufvEEHimOGmO5O69lXA0Hit1yLU0/A\
MQrIMmZT8gb8LMZGPZearT90KhCbTHAxjcBfswZYeL8q3xuEVHXC7EMs6mq8IgZL7mzSBmrCfmBAIO0V\
jW2kvmy0NFxkjIeHUShtYb11oYYyfHuz+1vr1y6FIoLmDejKVnwfcuNb545m26o+z/m9Lv9bAgMBAAGj\
gdgwgdUwHQYDVR0OBBYEFGDdpPELS92xT+Hkh/7lcc+4G56VMIGlBgNVHSMEgZ0wgZqAFGDdpPELS92x\
T+Hkh/7lcc+4G56VoXekdTBzMQswCQYDVQQGEwJDWjEPMA0GA1UEBxMGUHJhZ3VlMRUwEwYDVQQKEwxD\
b3Ntb3MsIEluYy4xFzAVBgNVBAMTDnN1bi5jb3Ntb3MuZ29kMSMwIQYJKoZIhvcNAQkBFhR0aGluZ0Bz\
dW4uY29zbW9zLmdvZIIJAJdFadWqNIfiMAwGA1UdEwQFMAMBAf8wDQYJKoZIhvcNAQEFBQADggEBAFYi\
Zu/dfAMOrD51bYxP88Wu6iDGBe9nMG/0lkKgnX5JQKCxfxFMk875rfa+pljdUMOaPxegOXq1DrYmQB9O\
/pHI+t7ozuWHRj2zKkVgMWAygNWDPcoqBEus53BdAgA644aPN2JvnE4NEPCllOMKftPoIWbd/5ZjCx3a\
bCuxBdXq5YSmiEnOdGfKeXjeeEiIDgARb4tLgH5rkOpB1uH/ZCWn1hkiajBhrGhhPhpA0zbkZg2Ug+8g\
XPlx1yQB1VOJkj2Z8dUEXCaRRijInCJ2eU+pgJvwLV7mxmSED7DEJ+b+opxJKYrsdKBU6RmYpPrDa+KC\
/Yfu88P9hKKj0LmBiREA\
";

static NSString *keyDataBase64 = @"\
MIIEogIBAAKCAQEAubvYJ2BA80CFbu8guNFXF02dNQ/QiaA/5h+trXmxBxHX1Ty1inqvrgB5yen0yCUL\
5LGiEjscIjqLBY7SN9Jm3ui2f6CttsGF/jlMCoQGI2UD8kWH7miXv+dhdM5n1Eifik21Ga121VRZmuMT\
36YOG6bb7RpBSVp7n7xBB4pjhpjuTuvZVwNB4rdci1NPwDEKyDJmU/IG/CzGRj2Xmq0/dCoQm0xwMY3A\
X7MGWHi/Kt8bhFR1wuxDLOpqvCIGS+5s0gZqwn5gQCDtFY1tpL5stDRcZIyHh1EobWG9daGGMnx7s/tb\
69cuhSKC5g3oylZ8H3LjW+eOZtuqPs/5vS7/WwIDAQABAoIBAGcwmQAPdyZus3OVwa1NCUD2KyB+39KG\
yNmWwgx+br9Jx4s+RnJghVh8BS4MIKZOBtSRaEUOuCvAMNrupZbD+8leq34vDDRcQpCizr+M6Egj6FRj\
Ewl+7Mh+yeN2hbMoghL552MTv9D4Iyxteu4nuPDd/JQ3oQwbDFIL6mlBFtiBDUr9ndemmcJ0WKuzor6a\
3rgsygLs8SPyMefwIKjh5rJZls+iv3AyVEoBdCbHBz0HKgLVE9ZNmY/gWqda2dzAcJxxMdafeNVwHovv\
BtyyRGnA7Yikx2XT4WLgKfuUsYLnDWs4GdAa738uxPBfiddQNeRjN7jRT1GZIWCk0P29rMECgYEA8jWi\
g1Dph+4VlESPOffTEt1aCYQQWtHs13Qex95HrXX/L49fs6cOE7pvBh7nVzaKwBnPRh5+3bCPsPmRVb7h\
k/GreOriCjTZtyt2XGp8eIfstfirofB7c1lNBjT61BhgjJ8Moii5c2ksNIOOZnKtD53n47mf7hiarYkw\
xFEgU6ECgYEAxE8Js3gIPOBjsSw47XHuvsjP880nZZx/oiQ4IeJb/0rkoDMVJjU69WQu1HTNNAnMg4/u\
RXo31h+gDZOlE9t9vSXHdrn3at67KAVmoTbRknGxZ+8tYpRJpPj1hyufynBGcKwevv3eHJHnE5eDqbHx\
ynZFkXemzT9aMy3R4CCFMXsCgYAYyZpnG/m6WohE0zthMFaeoJ6dSLGvyboWVqDrzXjCbMf/4wllRlxv\
cm34T2NXjpJmlH2c7HQJVg9uiivwfYdyb5If3tHhP4VkdIM5dABnCWoVOWy/NvA7XtE+KF/fItuGqKRP\
WCGaiRHoEeqZ23SQm5VmvdF7OXNi/R5LiQ3o4QKBgAGX8qg2TTrRR33ksgGbbyi1UJrWC3/TqWWTjbEY\
uU51OS3jvEQ3ImdjjM3EtPW7LqHSxUhjGZjvYMk7bZefrIGgkOHx2IRRkotcn9ynKURbD+mcE249beuc\
6cFTJVTrXGcFvqomPWtV895A2JzECQZvt1ja88uuu/i2YoHDQdGJAoGAL2TEgiMXiunb6PzYMMKKa+mx\
mFnagF0Ek3UJ9ByXKoLz3HFEl7cADIkqyenXFsAER/ifMyCoZp/PDBd6ZkpqLTdH0jQ2Yo4SllLykoiZ\
fBWMfjRu4iw9E0MbPB3blmtzfv53BtWKy0LUOlN4juvpqryA7TgaUlZkfMT+T1TC7xU=\
";


static SecIdentityRef
CreateTestIdentity(void)
{
    NSData *certData = [[NSData alloc] initWithBase64EncodedString:certDataBase64 options:0];
    SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)certData);

    XCTAssertNotNil((__bridge id)cert, "cert should not be nil");

    NSData *keyData = [[NSData alloc] initWithBase64EncodedString:keyDataBase64 options:0];
    NSDictionary *keyAttrs = @{
                               (id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA,
                               (id)kSecAttrKeySizeInBits: @2048,
                               (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate
                               };
    SecKeyRef privateKey = SecKeyCreateWithData((CFDataRef)keyData, (CFDictionaryRef)keyAttrs, NULL);
    XCTAssertNotNil((__bridge id)privateKey, "privateKey should not be nil");

    // Create identity from certificate and private key.
    SecIdentityRef identity = SecIdentityCreate(kCFAllocatorDefault, cert, privateKey);
    CFReleaseNull(privateKey);
    CFReleaseNull(cert);

    return identity;
}

static void
CheckIdentityItem(NSString *accessGroup, OSStatus expectedStatus)
{
    NSDictionary *check = @{
                             (id)kSecClass : (id)kSecClassIdentity,
                             (id)kSecAttrAccessGroup : accessGroup,
                             (id)kSecAttrLabel : @"item-delete-me",
                             (id)kSecUseDataProtectionKeychain: @YES,
                             };
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)check, NULL), expectedStatus, "result of SecItemCopyMatching should equal to expected status");
}

-(void)testSecIdentityRefWithNewUUIDPersistentRefs
{
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    OSStatus status;
    CFTypeRef ref = NULL;

    NSDictionary *clean1 = @{
        (id)kSecClass : (id)kSecClassIdentity,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
    };
    NSDictionary *clean2 = @{
        (id)kSecClass : (id)kSecClassIdentity,
        (id)kSecAttrAccessGroup : @"com.apple.lakitu",
    };

    SecIdentityRef identity = CreateTestIdentity();
    XCTAssertNotNil((__bridge id)identity, "identity should not be nil");

    /*
     * Add item
     */

    NSDictionary *add = @{
        (id)kSecValueRef : (__bridge id)identity,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrLabel : @"item-delete-me",
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecReturnPersistentRef: (id)kCFBooleanTrue,
    };

    status = SecItemAdd((__bridge CFDictionaryRef)add, &ref);
    XCTAssertNotNil((__bridge id)ref, "ref should not be nil");
    XCTAssertEqual(status, errSecSuccess, "status should equal errSecSuccess");
    NSDictionary *query2 = @{
        (id)kSecValuePersistentRef : (__bridge id)ref,
        (id)kSecUseDataProtectionKeychain : (id)kCFBooleanTrue,
    };

    CFTypeRef item = NULL;
    status = SecItemCopyMatching((__bridge CFDictionaryRef)query2, &item);
    XCTAssertEqual(status, errSecSuccess, "status should equal errSecSuccess");

    CheckIdentityItem(@"com.apple.security.ckks", errSecSuccess);
    CheckIdentityItem(@"com.apple.lakitu", errSecItemNotFound);


    /*
     * Update access group
     */
    NSDictionary *query = @{
        (id)kSecClass : (id)kSecClassIdentity,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrLabel : @"item-delete-me",
        (id)kSecUseDataProtectionKeychain : (id)kCFBooleanTrue,
    };
    NSDictionary *modified = @{
        (id)kSecAttrAccessGroup : @"com.apple.lakitu",
    };

    status = SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)modified);
    XCTAssertEqual(status, errSecSuccess, "status should equal errSecSuccess");

    CheckIdentityItem(@"com.apple.security.ckks", errSecItemNotFound);
    CheckIdentityItem(@"com.apple.lakitu", errSecSuccess);

    /*
     * Check persistent ref
     */
    CFDataRef data = NULL;

    NSDictionary *prefQuery = @{
        (id)kSecClass : (id)kSecClassIdentity,
        (id)kSecAttrAccessGroup : @"com.apple.lakitu",
        (id)kSecAttrLabel : @"item-delete-me",
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecReturnPersistentRef : (id)kCFBooleanTrue,
    };
    status = SecItemCopyMatching((__bridge CFDictionaryRef)prefQuery, (CFTypeRef *)&data);
    XCTAssertEqual(status, errSecSuccess, "status should equal errSecSuccess");

    /*
     * Update access group for identity
     */
    query2 = @{
        (id)kSecValuePersistentRef : (__bridge id)data,
        (id)kSecUseDataProtectionKeychain : (id)kCFBooleanTrue,
    };

    item = NULL;
    status = SecItemCopyMatching((__bridge CFDictionaryRef)query2, &item);
    XCTAssertEqual(status, errSecSuccess, "status should equal errSecSuccess");

    NSDictionary *modified2 = @{
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
    };

    status = SecItemUpdate((__bridge CFDictionaryRef)query2, (__bridge CFDictionaryRef)modified2);
    XCTAssertEqual(status, errSecInternal, "status should equal errSecInternal");

    (void)SecItemDelete((__bridge CFDictionaryRef)clean1);
    (void)SecItemDelete((__bridge CFDictionaryRef)clean2);

    CFReleaseNull(identity);

    CheckIdentityItem(@"com.apple.security.ckks", errSecItemNotFound);
    CheckIdentityItem(@"com.apple.lakitu", errSecItemNotFound);

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
}

-(void)testCopyAccessGroupForPersistentRef
{
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);
    CFErrorRef localError = NULL;

    SecurityClient client = {
        .task = NULL,
        .accessGroups = (__bridge CFArrayRef)@[
            @"com.apple.security.ckks"
        ],
        .canAccessNetworkExtensionAccessGroups = true,
    };
    
    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecAttrAccount : @"testaccount",
        (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
        (id)kSecAttrSyncViewHint : @"Passwords",
        (id)kSecAttrPCSPlaintextPublicKey : [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecReturnPersistentRef : @YES

    } mutableCopy];

    CFTypeRef pref1 = NULL;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, &pref1), errSecSuccess);
    XCTAssertNotNil((__bridge id)pref1, "persistent ref should not be nil");
    XCTAssertTrue(CFDataGetLength(pref1) == 20, "persistent ref length should be 20");

    query = nil;
    query = [NSMutableDictionary dictionary];
    query[(id)kSecValuePersistentRef] = (__bridge id)(pref1);
    query[(id)kSecReturnAttributes] = @YES;
    query[(id)kSecReturnPersistentRef] = @YES;

    CFTypeRef item = NULL;
    XCTAssertTrue(_SecItemCopyMatching((__bridge CFDictionaryRef)query, &client, &item, &localError));
    XCTAssertNotNil((__bridge id)item, "item should not be nil");
    XCTAssertTrue(isDictionary(item), "item should be a dictionary");
    CFDataRef foundPref = CFDictionaryGetValue(item, kSecValuePersistentRef);
    XCTAssertEqualObjects((__bridge NSData*)pref1, (__bridge NSData*)foundPref, @"persistent refs should be equal");
}

@end

#endif
