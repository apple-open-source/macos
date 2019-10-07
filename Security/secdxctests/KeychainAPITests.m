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
#include <dispatch/dispatch.h>
#include <utilities/SecDb.h>
#include <sys/stat.h>
#include <utilities/SecFileLocations.h>

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
    SecDbCorruptionExitHandler = SecDbTestCorruptionHandler;
    sema = dispatch_semaphore_create(0);

    secd_test_setup_temp_keychain([[NSString stringWithFormat:@"%@-bad", [self nameOfTest]] UTF8String], ^{
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

@end

#endif
