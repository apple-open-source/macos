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

#import "SecKeybagSupport.h"
#import "SecDbKeychainItem.h"
#import "SecdTestKeychainUtilities.h"
#import "CKKS.h"
#import "SecItemPriv.h"
#import "SecItemServer.h"
#import "SecItemSchema.h"
#include "OSX/sec/Security/SecItemShim.h"
#import "server_security_helpers.h"
#import "spi.h"
#import "utilities/SecAKSWrappers.h"
#import <utilities/SecCFWrappers.h>
#import <utilities/SecFileLocations.h>
#import "utilities/der_plist.h"
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>
#if USE_KEYSTORE
#if __has_include(<libaks.h>)
#import <libaks.h>
#endif // aks.h
#endif
#import <sqlite3.h>
#import "mockaks.h"

#import <TargetConditionals.h>

#import <LocalAuthentication/LocalAuthentication.h>

#import "secdmock_db_version_10_5.h"
#import "secdmock_db_version_11_1.h"

#import "mockaksxcbase.h"

@interface secdmockaks : mockaksxcbase
@end

@interface NSData (secdmockaks)
- (NSString *)hexString;
@end

@implementation NSData (secdmockaks)
- (NSString *)hexString
{
    static const char tbl[] = "0123456789ABCDEF";
    NSUInteger len = self.length;
    char *buf = malloc(len * 2);
    [self enumerateByteRangesUsingBlock:^(const void *bytes, NSRange byteRange, BOOL *stop) {
        const uint8_t *p = (const uint8_t *)bytes;
        for (NSUInteger i = byteRange.location; i < NSMaxRange(byteRange); i++) {
            uint8_t byte = *p++;
            buf[i * 2 + 0] = tbl[(byte >> 4) & 0xf];
            buf[i * 2 + 1] = tbl[(byte >> 0) & 0xf];
        }
    }];
    return [[NSString alloc] initWithBytesNoCopy:buf length:len * 2 encoding:NSUTF8StringEncoding freeWhenDone:YES];
}
@end


@implementation secdmockaks


- (void)testAddDeleteItem
{
    NSDictionary* item = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                            (id)kSecAttrAccount : @"TestAccount",
                            (id)kSecAttrService : @"TestService",
                            (id)kSecUseDataProtectionKeychain : @(YES) };

    OSStatus result = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, 0, @"failed to add test item to keychain");

    NSMutableDictionary* dataQuery = item.mutableCopy;
    [dataQuery removeObjectForKey:(id)kSecValueData];
    dataQuery[(id)kSecReturnData] = @(YES);
    CFTypeRef foundItem = NULL;
    result = SecItemCopyMatching((__bridge CFDictionaryRef)dataQuery, &foundItem);
    XCTAssertEqual(result, 0, @"failed to find the data for the item we just added in the keychain");

    result = SecItemDelete((__bridge CFDictionaryRef)dataQuery);
    XCTAssertEqual(result, 0, @"failed to delete item");
}


- (void)createManyItems
{
    unsigned n;
    for (n = 0; n < 50; n++) {
        NSDictionary* item = @{
            (id)kSecClass : (id)kSecClassGenericPassword,
            (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
            (id)kSecAttrAccount : [NSString stringWithFormat:@"TestAccount-%u", n],
            (id)kSecAttrService : @"TestService",
            (id)kSecUseDataProtectionKeychain : @(YES)
        };
        OSStatus result = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
        XCTAssertEqual(result, errSecSuccess, @"failed to add test item to keychain: %u", n);
    }
}

- (void)createECKeyWithACL:(NSString *)label
{
    NSDictionary* keyParams = @{
        (__bridge id)kSecAttrKeyClass : (__bridge id)kSecAttrKeyClassPrivate,
        (__bridge id)kSecAttrKeyType : (__bridge id)kSecAttrKeyTypeEC,
        (__bridge id)kSecAttrKeySizeInBits: @(256),
    };
    SecAccessControlRef ref = SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenUnlocked, 0, NULL);
    XCTAssertNotEqual(ref, NULL, @"SAC");

    SecKeyRef key = SecKeyCreateRandomKey((__bridge CFDictionaryRef)keyParams, NULL);
    XCTAssertNotEqual(key, NULL, @"failed to create test key to keychain");

    NSDictionary* item = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassKey,
        (__bridge id)kSecValueRef: (__bridge id)key,
        (__bridge id)kSecAttrLabel: label,
        (__bridge id)kSecUseDataProtectionKeychain: @(YES),
        (__bridge id)kSecAttrAccessControl: (__bridge id)ref,
    };

    OSStatus result = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, 0, @"failed to add test key to keychain");
}

- (void)createManyACLKeyItems
{
    unsigned n;
    for (n = 0; n < 50; n++) {
        [self createECKeyWithACL: [NSString stringWithFormat:@"TestLabel-EC-%u", n]];
    }
}

- (void)findManyItems:(unsigned)searchLimit
{
    unsigned n;
    for (n = 0; n < searchLimit; n++) {
        NSDictionary* item = @{
                               (id)kSecClass : (id)kSecClassGenericPassword,
                               (id)kSecAttrAccount : [NSString stringWithFormat:@"TestAccount-%u", n],
                               (id)kSecAttrService : @"TestService",
                               (id)kSecUseDataProtectionKeychain : @(YES)
                               };
        OSStatus result = SecItemCopyMatching((__bridge CFDictionaryRef)item, NULL);
        XCTAssertEqual(result, errSecSuccess, @"failed to find test item to keychain: %u", n);
    }
}

- (void)findNoItems:(unsigned)searchLimit
{
    unsigned n;
    for (n = 0; n < searchLimit; n++) {
        NSDictionary* item = @{
                               (id)kSecClass : (id)kSecClassGenericPassword,
                               (id)kSecAttrAccount : [NSString stringWithFormat:@"TestAccount-%u", n],
                               (id)kSecAttrService : @"TestService",
                               (id)kSecUseDataProtectionKeychain : @(YES)
                               };
        OSStatus result = SecItemCopyMatching((__bridge CFDictionaryRef)item, NULL);
        XCTAssertEqual(result, errSecItemNotFound, @"not expecting to find an item (%d): %u", (int)result, n);
    }
}

- (void)deleteAllItems
{
    NSDictionary* item = @{
                           (id)kSecClass : (id)kSecClassGenericPassword,
                           (id)kSecAttrService : @"TestService",
                           (id)kSecUseDataProtectionKeychain : @(YES)
                           };
    OSStatus result = SecItemDelete((__bridge CFDictionaryRef)item);
    XCTAssertEqual(result, 0, @"failed to delete all test items");
}

- (void)testSecItemServerDeleteAll
{

    [self addAccessGroup:@"com.apple.bluetooth"];
    [self addAccessGroup:@"lockdown-identities"];
    [self addAccessGroup:@"apple"];

    // BT root key, should not be deleted
    NSMutableDictionary* bt = [@{
                                 (id)kSecClass : (id)kSecClassGenericPassword,
                                 (id)kSecAttrAccessGroup : @"com.apple.bluetooth",
                                 (id)kSecAttrService : @"BluetoothGlobal",
                                 (id)kSecAttrAccessible : (id)kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate,
                                 (id)kSecAttrSynchronizable : @(NO),
                                 (id)kSecValueData : [@"btkey" dataUsingEncoding:NSUTF8StringEncoding],
                                 } mutableCopy];

    // lockdown-identities, should not be deleted
    NSMutableDictionary* ld = [@{
                                 (id)kSecClass : (id)kSecClassKey,
                                 (id)kSecAttrAccessGroup : @"lockdown-identities",
                                 (id)kSecAttrLabel : @"com.apple.lockdown.identity.activation",
                                 (id)kSecAttrAccessible : (id)kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate,
                                 (id)kSecAttrSynchronizable : @(NO),
                                 (id)kSecValueData : [@"ldkey" dataUsingEncoding:NSUTF8StringEncoding],
                                 } mutableCopy];

    // general nonsyncable item, should be deleted
    NSMutableDictionary* s0 = [@{
                                 (id)kSecClass : (id)kSecClassGenericPassword,
                                 (id)kSecAttrService : @"NonsyncableService",
                                 (id)kSecAttrSynchronizable : @(NO),
                                 (id)kSecValueData : [@"s0pwd" dataUsingEncoding:NSUTF8StringEncoding],
                                 } mutableCopy];

    // general syncable item, should be deleted
    NSMutableDictionary* s1 = [@{
                                 (id)kSecClass : (id)kSecClassGenericPassword,
                                 (id)kSecAttrService : @"SyncableService",
                                 (id)kSecAttrSynchronizable : @(YES),
                                 (id)kSecValueData : [@"s0pwd" dataUsingEncoding:NSUTF8StringEncoding],
                                 } mutableCopy];

    // Insert all items
    OSStatus status;
    status = SecItemAdd((__bridge CFDictionaryRef)bt, NULL);
    XCTAssertEqual(status, errSecSuccess, "failed to add bt item to keychain");
    status = SecItemAdd((__bridge CFDictionaryRef)ld, NULL);
    XCTAssertEqual(status, errSecSuccess, "failed to add ld item to keychain");
    status = SecItemAdd((__bridge CFDictionaryRef)s0, NULL);
    XCTAssertEqual(status, errSecSuccess, "failed to add s0 item to keychain");
    status = SecItemAdd((__bridge CFDictionaryRef)s1, NULL);
    XCTAssertEqual(status, errSecSuccess, "failed to add s1 item to keychain");

    // Make sure they exist now
    bt[(id)kSecValueData] = nil;
    ld[(id)kSecValueData] = nil;
    s0[(id)kSecValueData] = nil;
    s1[(id)kSecValueData] = nil;
    status = SecItemCopyMatching((__bridge CFDictionaryRef)bt, NULL);
    XCTAssertEqual(status, errSecSuccess, "failed to find bt item in keychain");
    status = SecItemCopyMatching((__bridge CFDictionaryRef)ld, NULL);
    XCTAssertEqual(status, errSecSuccess, "failed to find ld item in keychain");
    status = SecItemCopyMatching((__bridge CFDictionaryRef)s0, NULL);
    XCTAssertEqual(status, errSecSuccess, "failed to find s0 item in keychain");
    status = SecItemCopyMatching((__bridge CFDictionaryRef)s1, NULL);
    XCTAssertEqual(status, errSecSuccess, "failed to find s1 item in keychain");

    // Nuke the keychain
    CFErrorRef error = NULL;
    _SecItemDeleteAll(&error);
    XCTAssertEqual(error, NULL, "_SecItemDeleteAll returned an error: %@", error);
    CFReleaseNull(error);

    // Does the function work properly with an error pre-set?
    error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, errSecItemNotFound, NULL);
    _SecItemDeleteAll(&error);
    XCTAssertEqual(CFErrorGetDomain(error), kCFErrorDomainOSStatus);
    XCTAssertEqual(CFErrorGetCode(error), errSecItemNotFound);
    CFReleaseNull(error);

    // Check the relevant items are missing
    status = SecItemCopyMatching((__bridge CFDictionaryRef)bt, NULL);
    XCTAssertEqual(status, errSecSuccess, "failed to find bt item in keychain");
    status = SecItemCopyMatching((__bridge CFDictionaryRef)ld, NULL);
    XCTAssertEqual(status, errSecSuccess, "failed to find ld item in keychain");
    status = SecItemCopyMatching((__bridge CFDictionaryRef)s0, NULL);
    XCTAssertEqual(status, errSecItemNotFound, "unexpectedly found s0 item in keychain");
    status = SecItemCopyMatching((__bridge CFDictionaryRef)s1, NULL);
    XCTAssertEqual(status, errSecItemNotFound, "unexpectedly found s1 item in keychain");
}

- (void)createManyKeys
{
    unsigned n;
    for (n = 0; n < 50; n++) {
        NSDictionary* keyParams = @{
            (id)kSecAttrKeyType : (id)kSecAttrKeyTypeRSA,
            (id)kSecAttrKeySizeInBits : @(1024)
        };
        SecKeyRef key = SecKeyCreateRandomKey((__bridge CFDictionaryRef)keyParams, NULL);
        NSDictionary* item = @{ (id)kSecClass : (id)kSecClassKey,
                                (id)kSecValueRef : (__bridge id)key,
                                (id)kSecAttrLabel : [NSString stringWithFormat:@"TestLabel-%u", n],
                                (id)kSecUseDataProtectionKeychain : @(YES) };
        
        OSStatus result = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
        XCTAssertEqual(result, 0, @"failed to add test key to keychain: %u", n);
    }
}


- (void)testBackupRestoreItem
{
    [self createManyItems];
    [self createManyKeys];
    [self createManyACLKeyItems];

    
    NSDictionary* item = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                            (id)kSecAttrAccount : @"TestAccount",
                            (id)kSecAttrService : @"TestService",
                            (id)kSecUseDataProtectionKeychain : @(YES) };

    OSStatus result = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, 0, @"failed to add test item to keychain");

    NSMutableDictionary* dataQuery = item.mutableCopy;
    [dataQuery removeObjectForKey:(id)kSecValueData];
    dataQuery[(id)kSecReturnData] = @(YES);
    CFTypeRef foundItem = NULL;

    /*
     * Create backup
     */

    CFDataRef keybag = CFDataCreate(kCFAllocatorDefault, NULL, 0);
    CFDataRef password = CFDataCreate(kCFAllocatorDefault, NULL, 0);

    CFDataRef backup = _SecKeychainCopyBackup(keybag, password);
    XCTAssert(backup, "expected to have a backup");

    result = SecItemDelete((__bridge CFDictionaryRef)dataQuery);
    XCTAssertEqual(result, 0, @"failed to delete item");

    result = SecItemCopyMatching((__bridge CFDictionaryRef)dataQuery, &foundItem);
    XCTAssertEqual(result, errSecItemNotFound,
                   @"failed to find the data for the item we just added in the keychain");
    CFReleaseNull(foundItem);

    /*
     * Restore backup and see that item is resurected
     */

    XCTAssertEqual(0, _SecKeychainRestoreBackup(backup, keybag, password));

    CFReleaseNull(backup);
    CFReleaseNull(password);
    CFReleaseNull(keybag);

    result = SecItemCopyMatching((__bridge CFDictionaryRef)dataQuery, &foundItem);
    XCTAssertEqual(result, 0, @"failed to find the data for the item we just added in the keychain");
    CFReleaseNull(foundItem);

    result = SecItemDelete((__bridge CFDictionaryRef)dataQuery);
    XCTAssertEqual(result, 0, @"failed to delete item");
}

- (void)testCreateSampleDatabase
{
    // The keychain code only does the right thing with generation count if TARGET_HAS_KEYSTORE
#if TARGET_HAS_KEYSTORE
    id mock = OCMClassMock([SecMockAKS class]);
    OCMStub([mock useGenerationCount]).andReturn(true);
#endif

    [self createManyItems];
    [self createManyKeys];
    [self createManyACLKeyItems];

    /*
     sleep(600);
     lsof -p $(pgrep xctest)
     sqlite3 database
     .output mydatabase.h
     .dump
     
     add header and footer
    */

    [self findManyItems:50];

#if TARGET_HAS_KEYSTORE
    [mock stopMocking];
#endif
}

- (void)testTestAKSGenerationCount
{
#if TARGET_HAS_KEYSTORE
    id mock = OCMClassMock([SecMockAKS class]);
    OCMStub([mock useGenerationCount]).andReturn(true);

    [self createManyItems];
    [self findManyItems:50];

    [mock stopMocking];
#endif
}


- (void)loadDatabase:(const char **)dumpstring
{
    const char *s;
    unsigned n = 0;

    // We need a fresh directory to plop down our SQLite data
    SetCustomHomeURLString((__bridge CFStringRef)[self createKeychainDirectoryWithSubPath:@"loadManualDB"]);
    NSString* path = CFBridgingRelease(__SecKeychainCopyPath());
    // On macOS the full path gets created, on iOS not (yet?)
    [self createKeychainDirectoryWithSubPath:@"loadManualDB/Library/Keychains"];

    sqlite3 *handle = NULL;
    
    XCTAssertEqual(SQLITE_OK, sqlite3_open([path UTF8String], &handle), "create keychain");
    
    while ((s = dumpstring[n++]) != NULL) {
        char * errmsg = NULL;
        XCTAssertEqual(SQLITE_OK, sqlite3_exec(handle, s, NULL, NULL, &errmsg),
                       "exec: %s: %s", s, errmsg);
        if (errmsg) {
            sqlite3_free(errmsg);
        }
    }
    XCTAssertEqual(SQLITE_OK, sqlite3_close(handle), "close sqlite");
}

- (void)checkIncremental
{
    /*
     * check that we made incremental vacuum mode
     */

    __block CFErrorRef localError = NULL;
    __block bool ok = true;
    __block int vacuumMode = -1;

    kc_with_dbt(true, NULL, ^bool (SecDbConnectionRef dbt) {
        ok &= SecDbPrepare(dbt, CFSTR("PRAGMA auto_vacuum"), &localError, ^(sqlite3_stmt *stmt) {
            ok = SecDbStep(dbt, stmt, NULL, ^(bool *stop) {
                vacuumMode = sqlite3_column_int(stmt, 0);
            });
        });
        return ok;
    });
    XCTAssertEqual(ok, true, "should work to fetch auto_vacuum value: %@", localError);
    XCTAssertEqual(vacuumMode, 2, "vacuum mode should be incremental (2)");

    CFReleaseNull(localError);

}

- (void)testUpgradeFromVersion10_5
{
    SecKeychainDbReset(^{
        NSLog(@"resetting database");
        [self loadDatabase:secdmock_db_version10_5];
    });

    NSLog(@"find items from old database");
    [self findManyItems:50];

    [self checkIncremental];
}

- (void)testUpgradeFromVersion11_1
{
    SecKeychainDbReset(^{
        NSLog(@"resetting database");
        [self loadDatabase:secdmock_db_version11_1];
    });

    NSLog(@"find items from old database");
    [self findManyItems:50];

    [self checkIncremental];
}

#if TARGET_OS_IOS

- (void)testPersonaBasic
{
    SecSecuritySetPersonaMusr(NULL);

    [self createManyItems];
    [self findManyItems:10];

    SecSecuritySetPersonaMusr(CFSTR("99C5D3CC-2C2D-47C4-9A1C-976EC047BF3C"));

    [self findNoItems:10];
    [self createManyItems];
    [self findManyItems:10];

    [self deleteAllItems];
    [self findNoItems:10];

    SecSecuritySetPersonaMusr(NULL);

    [self findManyItems:10];
    [self deleteAllItems];
    [self findNoItems:10];

    SecSecuritySetPersonaMusr(CFSTR("8E90C6BE-0AF6-40D6-8A4B-D46E4CF6A622"));

    [self createManyItems];

    SecSecuritySetPersonaMusr(CFSTR("38B615CA-02C2-4733-A71C-1ABA46D27B13"));

    [self findNoItems:10];
    [self createManyItems];

    SecSecuritySetPersonaMusr(NULL);

    XCTestExpectation *expection = [self expectationWithDescription:@"wait for deletion"];

    _SecKeychainDeleteMultiUser(@"8E90C6BE-0AF6-40D6-8A4B-D46E4CF6A622", ^(bool status, NSError *error) {
        [expection fulfill];
    });

    [self waitForExpectations:@[expection] timeout:10.0];

    /* check that delete worked ... */
    SecSecuritySetPersonaMusr(CFSTR("8E90C6BE-0AF6-40D6-8A4B-D46E4CF6A622"));
    [self findNoItems:10];

    /* ... but didn't delete another account */
    SecSecuritySetPersonaMusr(CFSTR("38B615CA-02C2-4733-A71C-1ABA46D27B13"));
    [self findManyItems:10];

    SecSecuritySetPersonaMusr(NULL);


    [self findNoItems:10];
}

#endif /* TARGET_OS_IOS */

#if USE_KEYSTORE

- (void)testAddKeyByReference
{
    NSDictionary* keyParams = @{ (id)kSecAttrKeyType : (id)kSecAttrKeyTypeRSA, (id)kSecAttrKeySizeInBits : @(1024) };
    SecKeyRef key = SecKeyCreateRandomKey((__bridge CFDictionaryRef)keyParams, NULL);
    NSDictionary* item = @{ (id)kSecClass : (id)kSecClassKey,
                            (id)kSecValueRef : (__bridge id)key,
                            (id)kSecAttrLabel : @"TestLabel",
                            (id)kSecUseDataProtectionKeychain : @(YES) };

    OSStatus result = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, 0, @"failed to add test item to keychain");

    NSMutableDictionary* refQuery = item.mutableCopy;
    [refQuery removeObjectForKey:(id)kSecValueData];
    refQuery[(id)kSecReturnRef] = @(YES);
    CFTypeRef foundItem = NULL;
    result = SecItemCopyMatching((__bridge CFDictionaryRef)refQuery, &foundItem);
    XCTAssertEqual(result, 0, @"failed to find the reference for the item we just added in the keychain");

    NSData* originalKeyData = (__bridge_transfer NSData*)SecKeyCopyExternalRepresentation(key, NULL);
    NSData* foundKeyData = (__bridge_transfer NSData*)SecKeyCopyExternalRepresentation((SecKeyRef)foundItem, NULL);
    XCTAssertEqualObjects(originalKeyData, foundKeyData, @"found key does not match the key we put in the keychain");

    result = SecItemDelete((__bridge CFDictionaryRef)refQuery);
    XCTAssertEqual(result, 0, @"failed to delete key");
}

- (bool)isLockedSoon:(keyclass_t)key_class
{
    if (key_class == key_class_d || key_class == key_class_dku)
        return false;
    if (self.lockCounter <= 0)
        return true;
    self.lockCounter--;
    return false;
}

/*
 * Lock in the middle of migration
 */
- (void)testUpgradeFromVersion10_5WhileLocked
{
    OSStatus result = 0;
    id mock = OCMClassMock([SecMockAKS class]);
    [[[[mock stub] andCall:@selector(isLockedSoon:) onObject:self] ignoringNonObjectArgs] isLocked:0];

    SecKeychainDbReset(^{
        NSLog(@"resetting database");
        [self loadDatabase:secdmock_db_version10_5];
    });

    self.lockCounter = 0;

    NSDictionary* item = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : @"TestAccount-11",
        (id)kSecAttrService : @"TestService",
        (id)kSecReturnData : @YES,
        (id)kSecUseDataProtectionKeychain : @YES
    };
    result = SecItemCopyMatching((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, errSecInteractionNotAllowed, @"SEP not locked?");

    XCTAssertEqual(self.lockCounter, 0, "Device didn't lock");

    NSLog(@"user unlock");
    [mock stopMocking];

    result = SecItemCopyMatching((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, 0, @"can't find item");


    NSLog(@"find items from old database");
    [self findManyItems:50];
}


- (void)testUpgradeFromVersion10_5HungSEP
{
    id mock = OCMClassMock([SecMockAKS class]);
    OSStatus result = 0;

    OCMStub([mock isSEPDown]).andReturn(true);

    SecKeychainDbReset(^{
        NSLog(@"resetting database");
        [self loadDatabase:secdmock_db_version10_5];
    });

    NSDictionary* item = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : @"TestAccount-0",
        (id)kSecAttrService : @"TestService",
        (id)kSecUseDataProtectionKeychain : @(YES)
    };
    result = SecItemCopyMatching((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, errSecNotAvailable, @"SEP not down?");

    kc_with_dbt(true, NULL, ^bool (SecDbConnectionRef dbt) {
        CFErrorRef error = NULL;
        int version = 0;
        SecKeychainDbGetVersion(dbt, &version, &error);
        XCTAssertEqual(error, NULL, "error getting version");
        XCTAssertEqual(version, 0x50a, "managed to upgrade when we shouldn't have");

        return true;
    });

    /* user got the SEP out of DFU */
    NSLog(@"SEP alive");
    [mock stopMocking];

    result = SecItemCopyMatching((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, 0, @"failed to find test item to keychain");

    kc_with_dbt(true, NULL, ^bool (SecDbConnectionRef dbt) {
        CFErrorRef error = NULL;
        int version = 0;
        SecKeychainDbGetVersion(dbt, &version, &error);
        XCTAssertEqual(error, NULL, "error getting version");
        const SecDbSchema *schema = current_schema();
        int schemaVersion = (((schema)->minorVersion) << 8) | ((schema)->majorVersion);
        XCTAssertEqual(version, schemaVersion, "didn't manage to upgrade");

        return true;
    });

    NSLog(@"find items from old database");
    [self findManyItems:50];
}

// We used to try and parse a CFTypeRef as a 32 bit int before trying 64, but that fails if keytype is weird.
- (void)testBindObjectIntParsing
{
    NSMutableDictionary* query = [@{ (id)kSecClass : (id)kSecClassKey,
                                     (id)kSecAttrCanEncrypt : @(YES),
                                     (id)kSecAttrCanDecrypt : @(YES),
                                     (id)kSecAttrCanDerive : @(NO),
                                     (id)kSecAttrCanSign : @(NO),
                                     (id)kSecAttrCanVerify : @(NO),
                                     (id)kSecAttrCanWrap : @(NO),
                                     (id)kSecAttrCanUnwrap : @(NO),
                                     (id)kSecAttrKeySizeInBits : @(256),
                                     (id)kSecAttrEffectiveKeySize : @(256),
                                     (id)kSecValueData : [@"a" dataUsingEncoding:NSUTF8StringEncoding],
                                     (id)kSecAttrKeyType : [NSNumber numberWithUnsignedInt:0x80000001L],    // durr
                                     } mutableCopy];

    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess, "Succeeded in adding key to keychain");

    query[(id)kSecValueData] = nil;
    NSDictionary* update = @{(id)kSecValueData : [@"b" dataUsingEncoding:NSUTF8StringEncoding]};
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)update), errSecSuccess, "Successfully updated key in keychain");

    CFTypeRef output = NULL;
    query[(id)kSecReturnAttributes] = @(YES);
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &output), errSecSuccess, "Found key in keychain");
    XCTAssertNotNil((__bridge id)output, "got output from SICM");
    XCTAssertEqual(CFGetTypeID(output), CFDictionaryGetTypeID(), "output is a dictionary");
    XCTAssertEqual(CFDictionaryGetValue(output, (id)kSecAttrKeyType), (__bridge CFNumberRef)[NSNumber numberWithUnsignedInt:0x80000001L], "keytype is unchanged");
}

- (NSData *)objectToDER:(NSDictionary *)dict
{
    CFPropertyListRef object = (__bridge CFPropertyListRef)dict;
    CFErrorRef error = NULL;

    size_t size = der_sizeof_plist(object, &error);
    if (!size) {
        return NULL;
    }
    NSMutableData *data = [NSMutableData dataWithLength:size];
    uint8_t *der = [data mutableBytes];
    uint8_t *der_end = der + size;
    uint8_t *der_start = der_encode_plist(object, &error, der, der_end);
    if (!der_start) {
        return NULL;
    }
    return data;
}

#if !TARGET_OS_WATCH
/* this should be enabled for watch too, but that causes a crash in the mock aks layer */

- (void)testUpgradeWithBadACLKey
{
    SecAccessControlRef ref = SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenUnlocked, 0, NULL);
    XCTAssertNotEqual(ref, NULL, @"SAC");

    NSDictionary *canaryItem = @{
        (__bridge NSString *)kSecClass : (__bridge NSString *)kSecClassInternetPassword,
        (__bridge NSString *)kSecAttrServer : @"server-here",
        (__bridge NSString *)kSecAttrAccount : @"foo",
        (__bridge NSString *)kSecAttrPort : @80,
        (__bridge NSString *)kSecAttrProtocol : @"http",
        (__bridge NSString *)kSecAttrAuthenticationType : @"dflt",
        (__bridge NSString *)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    };

    NSDictionary* canaryQuery = @{
        (id)kSecClass : (id)kSecClassInternetPassword,
        (__bridge NSString *)kSecAttrServer : @"server-here",
        (__bridge NSString *)kSecAttrAccount : @"foo",
        (__bridge NSString *)kSecAttrPort : @80,
        (__bridge NSString *)kSecAttrProtocol : @"http",
        (__bridge NSString *)kSecAttrAuthenticationType : @"dflt",
        (id)kSecUseDataProtectionKeychain : @(YES),
    };

    NSDictionary* baseQuery = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : @"TestAccount-0",
        (id)kSecAttrService : @"TestService",
        (id)kSecUseDataProtectionKeychain : @(YES),
    };

    NSMutableDictionary* query = [baseQuery  mutableCopy];
    [query addEntriesFromDictionary:@{
        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @YES,
    }];

    NSMutableDictionary* add = [baseQuery  mutableCopy];
    [add addEntriesFromDictionary:@{
        (__bridge id)kSecValueData: [@"foo" dataUsingEncoding:NSUTF8StringEncoding],
        (__bridge id)kSecAttrAccessControl: (__bridge id)ref,
    }];

    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)canaryItem, NULL), errSecSuccess, "should successfully add canaryItem");
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)canaryQuery, NULL), errSecSuccess, "should successfully get canaryItem");

    __block NSUInteger counter = 0;
    __block bool mutatedDatabase = true;
    while (mutatedDatabase) {
        mutatedDatabase = false;

        if (counter == 0) {
            /* corruption of version is not great if its > 7 */
            counter++;
        }

        kc_with_dbt(true, NULL, ^bool (SecDbConnectionRef dbt) {
            CFErrorRef localError = NULL;
            (void)SecDbExec(dbt, CFSTR("DELETE FROM genp"), &localError);
            CFReleaseNull(localError);
            return true;
        });
        SecKeychainDbReset(NULL);

        XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecItemNotFound, "should successfully get item");
        XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)add, NULL), errSecSuccess, "should successfully add item");
        XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess, "should successfully get item");

        XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)canaryQuery, NULL), errSecSuccess,
                       "should successfully get canaryItem pre %d", (int)counter);

        // lower database and destroy the item
        kc_with_dbt(true, NULL, ^bool (SecDbConnectionRef dbt) {
            CFErrorRef localError2 = NULL;
            __block bool ok = true;
            int version = 0;

            SecKeychainDbGetVersion(dbt, &version, &localError2);
            CFReleaseNull(localError2);

            // force a minor (if more then zero), otherwise pick major
            NSString *downgradeString = nil;
            if ((version & (0xff00)) != 0) {
                downgradeString = [NSString stringWithFormat:@"UPDATE tversion SET version='%d', minor='%d'",
                                                (version & 0xff), 0];
            } else {
                downgradeString = [NSString stringWithFormat:@"UPDATE tversion SET version='%d', minor='%d'",
                                                ((version & 0xff) - 1), 0];
            }

            ok = SecDbExec(dbt, (__bridge CFStringRef)downgradeString, &localError2);
            XCTAssertTrue(ok, "downgrade should be successful: %@", localError2);
            CFReleaseNull(localError2);

            __block NSData *data = nil;
            __block unsigned steps = 0;
            ok &= SecDbPrepare(dbt, CFSTR("SELECT data FROM genp"), &localError2, ^(sqlite3_stmt *stmt) {
                ok = SecDbStep(dbt, stmt, NULL, ^(bool *stop) {
                    steps++;

                    void *ptr = (uint8_t *)sqlite3_column_blob(stmt, 0);
                    size_t length = sqlite3_column_bytes(stmt, 0);
                    XCTAssertNotEqual(ptr, NULL, "should have ptr");
                    XCTAssertNotEqual(length, 0, "should have data");
                    data = [NSData dataWithBytes:ptr length:length];
                });
            });
            XCTAssertTrue(ok, "copy should be successful: %@", localError2);
            XCTAssertEqual(steps, 1, "steps should be 1, since we should only have one genp");
            XCTAssertNotNil(data, "should find the row");
            CFReleaseNull(localError2);


            NSMutableData *mutatedData = [data mutableCopy];

            // This used to loop over all (~850!) bytes of the data but that's too slow. We now do first 50, last 50 and 20 random bytes
            if (counter < 50) {
                mutatedDatabase = true;
                ((uint8_t *)[mutatedData mutableBytes])[counter] = 'X';
                ++counter;
            } else if (counter < 70) {
                mutatedDatabase = true;
                size_t idx = 50 + arc4random_uniform((uint32_t)(mutatedData.length - 100));
                ((uint8_t *)[mutatedData mutableBytes])[idx] = 'X';
                ++counter;
            } else if (counter < 120) {
                mutatedDatabase = true;
                size_t idx = mutatedData.length - (counter - 70 - 1);
                ((uint8_t *)[mutatedData mutableBytes])[idx] = 'X';
                ++counter;
            } else {
                counter = 0;
            }

            NSString *mutateString = [NSString stringWithFormat:@"UPDATE genp SET data=x'%@'",
                                      [mutatedData hexString]];

            ok &= SecDbPrepare(dbt, (__bridge CFStringRef)mutateString, &localError2, ^(sqlite3_stmt *stmt) {
                ok = SecDbStep(dbt, stmt, NULL, NULL);
            });
            XCTAssertTrue(ok, "corruption should be successful: %@", localError2);
            CFReleaseNull(localError2);

            return ok;
        });

        // force it to reload
        SecKeychainDbReset(NULL);

        //dont care about result, we might have done a good number on this item
        (void)SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL);
        XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)canaryQuery, NULL), errSecSuccess,
                       "should successfully get canaryItem final %d", (int)counter);
    }

    NSLog(@"count: %lu", (unsigned long)counter);

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)canaryQuery, NULL), errSecSuccess,
                   "should successfully get canaryItem final %d", (int)counter);
}
#endif /* !TARGET_OS_WATCH */

- (void)testInteractionFailuresFromReferenceKeys
{
    SecAccessControlRef ref = SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenUnlocked, 0, NULL);
    XCTAssertNotEqual(ref, NULL, @"SAC");

    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : @"TestAccount-0",
        (id)kSecAttrService : @"TestService",
        (id)kSecUseDataProtectionKeychain : @(YES),
        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @YES,
    };

    NSDictionary* add = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : @"TestAccount-0",
        (id)kSecAttrService : @"TestService",
        (id)kSecUseDataProtectionKeychain : @(YES),
        (id)kSecValueData: [@"foo" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrAccessControl: (__bridge id)ref,
    };


    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecItemNotFound, "should successfully get item");
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)add, NULL), errSecSuccess, "should successfully add item");
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess, "should successfully get item");

    [SecMockAKS failNextDecryptRefKey:[NSError errorWithDomain:@"foo" code:kAKSReturnNoPermission userInfo:nil]];
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecInteractionNotAllowed, "should successfully get item");

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess, "should successfully get item");
}

// This test fails before rdar://problem/60028419 because the keystore fails to check for errSecInteractionNotAllowed,
// tries to recreate the "broken" metadata key and if the keychain unlocks at the exact right moment succeeds and causes
// data loss.
- (void)testMetadataKeyRaceConditionFixed
{
    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : @"TestAccount-0",
        (id)kSecAttrService : @"TestService",
        (id)kSecValueData : [@"data" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecUseDataProtectionKeychain : @(YES),
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
    } mutableCopy];

    // Create item 1
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    // Drop metadata keys
    SecKeychainDbReset(NULL);
    [SecMockAKS setOperationsUntilUnlock:1];    // The first call is the metadata key unwrap to allow encrypting the item
    [SecMockAKS lockClassA];

    query[(id)kSecAttrAccount] = @"TestAcount-1";
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecInteractionNotAllowed);

    query[(id)kSecAttrAccount] = @"TestAccount-0";
    query[(id)kSecValueData] = nil;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    [SecMockAKS setOperationsUntilUnlock:-1];
}

// A test for if LAContext can reasonably be used
// Note that this test can currently _only_ run on iOS in the simulator; on the actual platforms it tries to access
// real AKS/biometrics, which fails in automation environments.
#if TARGET_OS_OSX || TARGET_OS_SIMULATOR
 - (void)testLAContext
{
    NSMutableDictionary* simplequery = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : @"TestAccount-0",
        (id)kSecAttrService : @"TestService",
        (id)kSecValueData : [@"data" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecUseDataProtectionKeychain : @(YES),
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
    } mutableCopy];
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)simplequery, NULL), errSecSuccess, "Succeeded in adding simple item to keychain");

    CFErrorRef cferror = NULL;
    SecAccessControlRef access = SecAccessControlCreateWithFlags(nil,
                                                                 kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly,
                                                                 kSecAccessControlUserPresence,
                                                                 &cferror);

    XCTAssertNil((__bridge NSError*)cferror, "Should be no error creating an access control");

    LAContext* context = [[LAContext alloc] init];

#if TARGET_OS_IPHONE || TARGET_OS_OSX
    // This field is only usable on iPhone/macOS. It isn't stricly necessary for this test, though.
    context.touchIDAuthenticationAllowableReuseDuration = 10;
#endif

    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : @"TestAccount-LAContext",
        (id)kSecAttrService : @"TestService",
        (id)kSecValueData : [@"data" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecUseDataProtectionKeychain : @(YES),
        (id)kSecAttrAccessControl : (__bridge id)access,
        (id)kSecUseAuthenticationContext : context,
    } mutableCopy];

    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess, "Succeeded in adding LA-protected item to keychain");

    NSMutableDictionary* findquery = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : @"TestAccount-LAContext",
        (id)kSecAttrService : @"TestService",
        (id)kSecUseDataProtectionKeychain : @(YES),
        (id)kSecUseAuthenticationContext : context,
        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @YES,
    } mutableCopy];

    CFTypeRef output = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findquery, &output), errSecSuccess, "Found key in keychain");

    XCTAssertNotNil((__bridge id)output, "Should have received something back from keychain");
    CFReleaseNull(output);
}
#endif // TARGET_OS_OSX || TARGET_OS_SIMULATOR

#endif /* USE_KEYSTORE */

@end
