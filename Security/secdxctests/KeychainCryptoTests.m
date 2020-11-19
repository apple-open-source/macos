/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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
#import "SecDbKeychainMetadataKeyStore.h"
#import "SecItemPriv.h"
#import "SecItemServer.h"
#import "spi.h"
#import "SecDbKeychainSerializedItemV7.h"
#import "SecDbKeychainSerializedMetadata.h"
#import "SecDbKeychainSerializedMetadataKey.h"
#import "SecDbKeychainSerializedSecretData.h"
#import "SecDbKeychainSerializedAKSWrappedKey.h"
#import <utilities/SecCFWrappers.h>
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFCryptoServicesErrors.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>
#import <notify.h>

@interface SecDbKeychainItemV7 ()

+ (SFAESKeySpecifier*)keySpecifier;

@end

#if USE_KEYSTORE
#include "OSX/utilities/SecAKSWrappers.h"

@interface KeychainCryptoTests : KeychainXCTest
@end

@implementation KeychainCryptoTests

static keyclass_t parse_keyclass(CFTypeRef value) {
    if (!value || CFGetTypeID(value) != CFStringGetTypeID()) {
        return 0;
    }
    
    if (CFEqual(value, kSecAttrAccessibleWhenUnlocked)) {
        return key_class_ak;
    }
    else if (CFEqual(value, kSecAttrAccessibleAfterFirstUnlock)) {
        return key_class_ck;
    }
    else if (CFEqual(value, kSecAttrAccessibleAlwaysPrivate)) {
        return key_class_dk;
    }
    else if (CFEqual(value, kSecAttrAccessibleWhenUnlockedThisDeviceOnly)) {
        return key_class_aku;
    }
    else if (CFEqual(value, kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly)) {
        return key_class_cku;
    }
    else if (CFEqual(value, kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate)) {
        return key_class_dku;
    }
    else if (CFEqual(value, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly)) {
        return key_class_akpu;
    }
    else {
        return 0;
    }
}

- (NSDictionary* _Nullable)addTestItemExpecting:(OSStatus)code account:(NSString*)account accessible:(NSString*)accessible
{
    NSDictionary* addQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                                (id)kSecAttrAccount : account,
                                (id)kSecAttrService : [NSString stringWithFormat:@"%@-Service", account],
                                (id)kSecAttrAccessible : (id)accessible,
                                (id)kSecUseDataProtectionKeychain : @(YES),
                                (id)kSecReturnAttributes : @(YES),
                                };
    CFTypeRef result = NULL;

    if(code == errSecSuccess) {
        XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery, &result), code, @"Should have succeeded in adding test item to keychain");
        XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemAdd");
    } else {
        XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery, &result), code, @"Should have failed to adding test item to keychain with code %d", (int)code);
        XCTAssertNil((__bridge id)result, @"Should not have received a dictionary back from SecItemAdd");
    }

    return CFBridgingRelease(result);
}

- (NSDictionary* _Nullable)findTestItemExpecting:(OSStatus)code account:(NSString*)account
{
    NSDictionary* findQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                 (id)kSecAttrAccount : account,
                                 (id)kSecAttrService : [NSString stringWithFormat:@"%@-Service", account],
                                 (id)kSecUseDataProtectionKeychain : @(YES),
                                 (id)kSecReturnAttributes : @(YES),
                                 };
    CFTypeRef result = NULL;

    if(code == errSecSuccess) {
        XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findQuery, &result), code, @"Should have succeeded in finding test tiem");
        XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemCopyMatching");
    } else {
        XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findQuery, &result), code, @"Should have failed to find items in keychain with code %d", (int)code);
        XCTAssertNotNil((__bridge id)result, @"Should not have received a dictionary back from SecItemCopyMatching");
    }

    return CFBridgingRelease(result);
}


- (void)testBasicEncryptDecrypt
{
    CFDataRef enc = NULL;
    CFErrorRef error = NULL;
    SecAccessControlRef ac = NULL;

    NSDictionary* secretData = @{(id)kSecValueData : @"secret here"};
    CFDictionaryRef emptyDict = (__bridge CFDictionaryRef)@{};

    ac = SecAccessControlCreate(NULL, &error);
    XCTAssertNotNil((__bridge id)ac, @"failed to create access control with error: %@", (__bridge id)error);
    XCTAssertNil((__bridge id)error, @"encountered error attempting to create access control: %@", (__bridge id)error);
    XCTAssertTrue(SecAccessControlSetProtection(ac, kSecAttrAccessibleWhenUnlocked, &error), @"failed to set access control protection with error: %@", error);
    XCTAssertNil((__bridge id)error, @"encountered error attempting to set access control protection: %@", (__bridge id)error);

    XCTAssertTrue(ks_encrypt_data(KEYBAG_DEVICE, ac, NULL, (__bridge CFDictionaryRef)secretData, emptyDict, emptyDict, &enc, true, &error), @"failed to encrypt data with error: %@", error);
    XCTAssertTrue(enc != NULL, @"failed to get encrypted data from encryption function");
    XCTAssertNil((__bridge id)error, @"encountered error attempting to encrypt data: %@", (__bridge id)error);
    CFReleaseNull(ac);

    CFMutableDictionaryRef attributes = NULL;
    uint32_t version = 0;
    NSData* dummyACM = [NSData dataWithBytes:"dummy" length:5];
    const SecDbClass* class = kc_class_with_name(kSecClassGenericPassword);
    NSArray* dummyArray = [NSArray array];

    keyclass_t keyclass = 0;
    XCTAssertTrue(ks_decrypt_data(KEYBAG_DEVICE, kAKSKeyOpDecrypt, &ac, (__bridge CFDataRef _Nonnull)dummyACM, enc, class, (__bridge CFArrayRef)dummyArray, &attributes, &version, true, &keyclass, &error), @"failed to decrypt data with error: %@", error);
    XCTAssertNil((__bridge id)error, @"encountered error attempting to decrypt data: %@", (__bridge id)error);
    XCTAssertEqual(keyclass, key_class_ak, @"failed to get back the keyclass from decryption");

    CFTypeRef aclProtection = ac ? SecAccessControlGetProtection(ac) : NULL;
    XCTAssertNotNil((__bridge id)aclProtection, @"failed to get ACL from keychain item decryption");
    if (aclProtection) {
        XCTAssertTrue(CFEqual(aclProtection, kSecAttrAccessibleWhenUnlocked), @"the acl we got back from decryption does not match what we put in");
    }
    CFReleaseNull(ac);

    CFReleaseNull(error);
    CFReleaseNull(enc);
}

- (void)testGetMetadataThenData
{
    NSDictionary* item = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                            (id)kSecAttrAccount : @"TestAccount",
                            (id)kSecAttrService : @"TestService",
                            (id)kSecUseDataProtectionKeychain : @(YES) };

    OSStatus result = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, 0, @"failed to add test item to keychain");

    NSMutableDictionary* metadataQuery = item.mutableCopy;
    [metadataQuery removeObjectForKey:(id)kSecValueData];
    metadataQuery[(id)kSecReturnAttributes] = @(YES);
    CFTypeRef foundItem = NULL;
    result = SecItemCopyMatching((__bridge CFDictionaryRef)metadataQuery, &foundItem);
    XCTAssertEqual(result, 0, @"failed to find the metadata for the item we just added in the keychain");

    NSMutableDictionary* dataQuery = [(__bridge NSDictionary*)foundItem mutableCopy];
    dataQuery[(id)kSecReturnData] = @(YES);
    dataQuery[(id)kSecClass] = (id)kSecClassGenericPassword;
    dataQuery[(id)kSecUseDataProtectionKeychain] = @(YES);
    result = SecItemCopyMatching((__bridge CFDictionaryRef)dataQuery, &foundItem);
    XCTAssertEqual(result, 0, @"failed to find the data for the item we just added to the keychain");

    NSData* foundData = (__bridge NSData*)foundItem;
    if ([foundData isKindOfClass:[NSData class]]) {
        NSString* foundPassword = [[NSString alloc] initWithData:(__bridge NSData*)foundItem encoding:NSUTF8StringEncoding];
        XCTAssertEqualObjects(foundPassword, @"password", @"found password (%@) does not match the expected password", foundPassword);
    }
    else {
        XCTAssertTrue(false, @"found data is not the expected class: %@", foundData);
    }
}

- (void)testGetReference
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
}

- (void)testMetadataQueriesDoNotGetSecret
{
    NSDictionary* item = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                            (id)kSecAttrAccount : @"TestAccount",
                            (id)kSecAttrService : @"TestService",
                            (id)kSecUseDataProtectionKeychain : @(YES) };

    OSStatus result = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, 0, @"failed to add test item to keychain");

    NSMutableDictionary* metadataQuery = item.mutableCopy;
    [metadataQuery removeObjectForKey:(id)kSecValueData];
    metadataQuery[(id)kSecReturnAttributes] = @(YES);
    CFTypeRef foundItem = NULL;
    result = SecItemCopyMatching((__bridge CFDictionaryRef)metadataQuery, &foundItem);
    XCTAssertEqual(result, 0, @"failed to find the metadata for the item we just added in the keychain");

    NSData* data = [(__bridge NSDictionary*)foundItem valueForKey:(id)kSecValueData];
    XCTAssertNil(data, @"unexpectedly found data in a metadata query");
}

- (void)testDeleteItem
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

- (SecDbKeychainSerializedItemV7*)serializedItemWithPassword:(NSString*)password metadataAttributes:(NSDictionary*)metadata
{
    NSError* error;
    SecDbKeychainItemV7* item = [[SecDbKeychainItemV7 alloc] initWithSecretAttributes:@{(id)kSecValueData : password} metadataAttributes:metadata tamperCheck:[[NSUUID UUID] UUIDString] keyclass:9];
    [item encryptMetadataWithKeybag:0 error:&error];
    XCTAssertNil(error, "error encrypting metadata with keybag 0");
    [item encryptSecretDataWithKeybag:0 accessControl:SecAccessControlCreate(NULL, NULL) acmContext:nil error:&error];
    XCTAssertNil(error, "error encrypting secret data with keybag 0");
    SecDbKeychainSerializedItemV7* serializedItem = [[SecDbKeychainSerializedItemV7 alloc] init];
    serializedItem.encryptedMetadata = item.encryptedMetadataBlob;
    serializedItem.encryptedSecretData = item.encryptedSecretDataBlob;
    serializedItem.keyclass = 9;
    return serializedItem;
}

- (void)testTamperChecksThwartTampering
{
    SecDbKeychainSerializedItemV7* serializedItem1 = [self serializedItemWithPassword:@"first password" metadataAttributes:nil];
    SecDbKeychainSerializedItemV7* serializedItem2 = [self serializedItemWithPassword:@"second password" metadataAttributes:nil];
    
    serializedItem1.encryptedSecretData = serializedItem2.encryptedSecretData;
    NSData* tamperedSerializedItemBlob = serializedItem1.data;
    
    NSError* error = nil;
    SecDbKeychainItemV7* item = [[SecDbKeychainItemV7 alloc] initWithData:tamperedSerializedItemBlob decryptionKeybag:0 error:&error];
    XCTAssertNil(item, @"unexpectedly deserialized an item blob which has been tampered");
    XCTAssertNotNil(error, @"failed to get an error when deserializing tampered item blob");
}

- (void)testCacheExpiration
{

    NSDictionary* item = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                            (id)kSecAttrAccount : @"TestAccount",
                            (id)kSecAttrService : @"TestService",
                            (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
                            (id)kSecUseDataProtectionKeychain : @YES };

    OSStatus result = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, 0, @"failed to add test item to keychain");

    NSMutableDictionary* dataQuery = item.mutableCopy;
    [dataQuery removeObjectForKey:(id)kSecValueData];
    dataQuery[(id)kSecReturnData] = @(YES);

    CFTypeRef foundItem = NULL;

    result = SecItemCopyMatching((__bridge CFDictionaryRef)dataQuery, &foundItem);
    XCTAssertEqual(result, 0, @"failed to find the data for the item we just added in the keychain");
    CFReleaseNull(foundItem);

    self.lockState = LockStateLockedAndDisallowAKS;
    
    result = SecItemCopyMatching((__bridge CFDictionaryRef)dataQuery, &foundItem);
    XCTAssertEqual(result, errSecInteractionNotAllowed, @"get the lock error");
    XCTAssertEqual(foundItem, NULL, @"got item anyway: %@", foundItem);

    self.lockState = LockStateUnlocked;

    result = SecItemCopyMatching((__bridge CFDictionaryRef)dataQuery, &foundItem);
    XCTAssertEqual(result, 0, @"failed to find the data for the item we just added in the keychain");
    CFReleaseNull(foundItem);

    result = SecItemDelete((__bridge CFDictionaryRef)dataQuery);
    XCTAssertEqual(result, 0, @"failed to delete item");
}

- (void)trashMetadataClassAKey
{
    __block CFErrorRef cferror = NULL;
    kc_with_dbt(true, &cferror, ^bool(SecDbConnectionRef dbt) {
        CFStringRef sql = CFSTR("UPDATE metadatakeys SET data = ? WHERE keyclass = '6'");
        NSData* garbage = [@"super bad key" dataUsingEncoding:NSUTF8StringEncoding];
        SecDbPrepare(dbt, sql, &cferror, ^(sqlite3_stmt *stmt) {
            SecDbBindObject(stmt, 1, (__bridge CFDataRef)garbage, &cferror);
            SecDbStep(dbt, stmt, &cferror, NULL);
            XCTAssertEqual(cferror, NULL, "Should be no error trashing class A metadatakey");
            CFReleaseNull(cferror);
        });
        XCTAssertEqual(cferror, NULL, "Should be no error completing SecDbPrepare for trashing class A metadatakey");
        return true;
    });
    CFReleaseNull(cferror);

    [[SecDbKeychainMetadataKeyStore sharedStore] dropClassAKeys];
}

- (void)deleteMetadataClassAKey
{
    CFErrorRef cferror = NULL;

    kc_with_dbt(true, &cferror, ^bool(SecDbConnectionRef dbt) {
        CFErrorRef errref = NULL;
        SecDbExec(dbt, CFSTR("DELETE FROM metadatakeys WHERE keyclass = '6'"), &errref);
        XCTAssertEqual(errref, NULL, "Should be no error deleting class A metadatakey");
        CFReleaseNull(errref);
        return true;
    });
    CFReleaseNull(cferror);

    [[SecDbKeychainMetadataKeyStore sharedStore] dropClassAKeys];
}

- (void)checkDatabaseExistenceOfMetadataKey:(keyclass_t)keyclass shouldExist:(bool)shouldExist value:(NSData*)expectedData
{
    CFErrorRef cferror = NULL;
    __block NSData* wrappedKey;
    kc_with_dbt(true, &cferror, ^bool(SecDbConnectionRef dbt) {
        __block CFErrorRef errref = NULL;

        NSString* sql = [NSString stringWithFormat:@"SELECT data, actualKeyclass FROM metadatakeys WHERE keyclass = %d", keyclass];
        __block bool ok = true;
        ok &= SecDbPrepare(dbt, (__bridge CFStringRef)sql, &errref, ^(sqlite3_stmt *stmt) {
            ok &= SecDbStep(dbt, stmt, &errref, ^(bool *stop) {
                wrappedKey = [[NSData alloc] initWithBytes:sqlite3_column_blob(stmt, 0) length:sqlite3_column_bytes(stmt, 0)];
            });
        });

        XCTAssertTrue(ok, "Should have completed all operations correctly");
        XCTAssertEqual(errref, NULL, "Should be no error trying to find class A metadatakey");

        if(shouldExist) {
            XCTAssertNotNil(wrappedKey, "Metadata class key should exist");
            if (expectedData) {
                XCTAssertEqualObjects(wrappedKey, expectedData);
            }
        } else {
            XCTAssertNil(wrappedKey, "Metadata class key should not exist");
        }
        CFReleaseNull(errref);
        return true;
    });
    CFReleaseNull(cferror);
}

- (void)testKeychainCorruptionCopyMatching
{
    NSDictionary* item = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                            (id)kSecAttrAccount : @"TestAccount",
                            (id)kSecAttrService : @"TestService",
                            (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
                            (id)kSecUseDataProtectionKeychain : @YES };

    OSStatus result = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, 0, @"failed to add test item to keychain");
    [self checkDatabaseExistenceOfMetadataKey:key_class_ak shouldExist:true value:nil];

    NSMutableDictionary* dataQuery = item.mutableCopy;
    [dataQuery removeObjectForKey:(id)kSecValueData];
    dataQuery[(id)kSecReturnData] = @(YES);

    CFTypeRef foundItem = NULL;

    result = SecItemCopyMatching((__bridge CFDictionaryRef)dataQuery, &foundItem);
    XCTAssertEqual(result, 0, @"failed to find the data for the item we just added in the keychain");
    CFReleaseNull(foundItem);

    [self trashMetadataClassAKey];
    [self checkDatabaseExistenceOfMetadataKey:key_class_ak shouldExist:true value:[@"super bad key" dataUsingEncoding:NSUTF8StringEncoding]];

    /* when metadata corrupted, we should not find the item */
    result = SecItemCopyMatching((__bridge CFDictionaryRef)dataQuery, &foundItem);
    XCTAssertEqual(result, errSecItemNotFound, @"failed to find the data for the item we just added in the keychain");
    CFReleaseNull(foundItem);

    // Just calling SecItemCopyMatching shouldn't have created a new metadata key
    [self checkDatabaseExistenceOfMetadataKey:key_class_ak shouldExist:true value:[@"super bad key" dataUsingEncoding:NSUTF8StringEncoding]];

    /* CopyMatching will delete corrupt pre-emptively */
    result = SecItemDelete((__bridge CFDictionaryRef)dataQuery);
    XCTAssertEqual(result, -25300, @"corrupt item was not deleted for us");
}

- (void)testKeychainDeletionCopyMatching
{
    NSDictionary* item = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                            (id)kSecAttrAccount : @"TestAccount",
                            (id)kSecAttrService : @"TestService",
                            (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
                            (id)kSecUseDataProtectionKeychain : @YES };

    OSStatus result = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, 0, @"failed to add test item to keychain");
    [self checkDatabaseExistenceOfMetadataKey:key_class_ak shouldExist:true value:nil];

    NSMutableDictionary* dataQuery = item.mutableCopy;
    [dataQuery removeObjectForKey:(id)kSecValueData];
    dataQuery[(id)kSecReturnData] = @(YES);

    CFTypeRef foundItem = NULL;

    result = SecItemCopyMatching((__bridge CFDictionaryRef)dataQuery, &foundItem);
    XCTAssertEqual(result, 0, @"failed to find the data for the item we just added in the keychain");
    CFReleaseNull(foundItem);

    [self deleteMetadataClassAKey];
    [self checkDatabaseExistenceOfMetadataKey:key_class_ak shouldExist:false value:nil];

    /* when metadata corrupted, we should not find the item */
    result = SecItemCopyMatching((__bridge CFDictionaryRef)dataQuery, &foundItem);
    XCTAssertEqual(result, errSecItemNotFound, @"failed to find the data for the item we just added in the keychain");
    CFReleaseNull(foundItem);

    // Just calling SecItemCopyMatching shouldn't have created a new metadata key
    [self checkDatabaseExistenceOfMetadataKey:key_class_ak shouldExist:false value:nil];

    /* CopyMatching will delete corrupt pre-emptively */
    result = SecItemDelete((__bridge CFDictionaryRef)dataQuery);
    XCTAssertEqual(result, -25300, @"corrupt item was not deleted for us");
}

- (void)testKeychainCorruptionAddOverCorruptedEntry
{
    __security_simulatecrash_enable(false);

    CFTypeRef foundItem = NULL;
    NSDictionary* item = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                            (id)kSecAttrAccount : @"TestAccount",
                            (id)kSecAttrService : @"TestService",
                            (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
                            (id)kSecUseDataProtectionKeychain : @YES };

    OSStatus result = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, 0, @"failed to add test item to keychain");

    NSMutableDictionary* dataQuery = item.mutableCopy;
    [dataQuery removeObjectForKey:(id)kSecValueData];
    dataQuery[(id)kSecReturnData] = @(YES);

    result = SecItemCopyMatching((__bridge CFDictionaryRef)dataQuery, &foundItem);
    XCTAssertEqual(result, 0, @"failed to find the data for the item we just added in the keychain");
    CFReleaseNull(foundItem);

    [self trashMetadataClassAKey];

    result = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, 0, @"failed to add test item to keychain");

    result = SecItemDelete((__bridge CFDictionaryRef)dataQuery);
    XCTAssertEqual(result, 0, @"failed to delete item");
}

- (void)testKeychainCorruptionUpdateCorruptedEntry
{
    __security_simulatecrash_enable(false);

    CFTypeRef foundItem = NULL;
    NSDictionary* item = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                            (id)kSecAttrAccount : @"TestAccount",
                            (id)kSecAttrService : @"TestService",
                            (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
                            (id)kSecUseDataProtectionKeychain : @YES };

    OSStatus result = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, 0, @"failed to add test item to keychain");

    NSMutableDictionary* dataQuery = item.mutableCopy;
    [dataQuery removeObjectForKey:(id)kSecValueData];
    dataQuery[(id)kSecReturnData] = @(YES);

    result = SecItemCopyMatching((__bridge CFDictionaryRef)dataQuery, &foundItem);
    XCTAssertEqual(result, 0, @"failed to find the data for the item we just added in the keychain");
    CFReleaseNull(foundItem);

    [self trashMetadataClassAKey];

    NSMutableDictionary* updateQuery = item.mutableCopy;
    updateQuery[(id)kSecValueData] = NULL;
    NSDictionary *updateData = @{
        (id)kSecValueData : [@"foo" dataUsingEncoding:NSUTF8StringEncoding],
    };

    result = SecItemUpdate((__bridge CFDictionaryRef)updateQuery,
                           (__bridge CFDictionaryRef)updateData );
    XCTAssertEqual(result, errSecItemNotFound, @"failed to add test item to keychain");

    result = SecItemDelete((__bridge CFDictionaryRef)dataQuery);
    XCTAssertEqual(result, 0, @"failed to delete item");
}

- (void)testNoCrashWhenMetadataDecryptionFails
{
    CFDataRef enc = NULL;
    CFErrorRef error = NULL;
    SecAccessControlRef ac = NULL;

    self.allowDecryption = NO;

    NSDictionary* secretData = @{(id)kSecValueData : @"secret here"};
    CFDictionaryRef emptyDict = (__bridge CFDictionaryRef)@{};

    ac = SecAccessControlCreate(NULL, &error);
    XCTAssertNotNil((__bridge id)ac, @"failed to create access control with error: %@", (__bridge id)error);
    XCTAssertNil((__bridge id)error, @"encountered error attempting to create access control: %@", (__bridge id)error);
    XCTAssertTrue(SecAccessControlSetProtection(ac, kSecAttrAccessibleWhenUnlocked, &error), @"failed to set access control protection with error: %@", error);
    XCTAssertNil((__bridge id)error, @"encountered error attempting to set access control protection: %@", (__bridge id)error);

    XCTAssertTrue(ks_encrypt_data(KEYBAG_DEVICE, ac, NULL, (__bridge CFDictionaryRef)secretData, emptyDict, emptyDict, &enc, true, &error), @"failed to encrypt data with error: %@", error);
    XCTAssertTrue(enc != NULL, @"failed to get encrypted data from encryption function");
    XCTAssertNil((__bridge id)error, @"encountered error attempting to encrypt data: %@", (__bridge id)error);
    CFReleaseNull(ac);

    CFMutableDictionaryRef attributes = NULL;
    uint32_t version = 0;
    NSData* dummyACM = [NSData dataWithBytes:"dummy" length:5];
    const SecDbClass* class = kc_class_with_name(kSecClassGenericPassword);
    NSArray* dummyArray = [NSArray array];

    keyclass_t keyclass = 0;
    XCTAssertNoThrow(ks_decrypt_data(KEYBAG_DEVICE, kAKSKeyOpDecrypt, &ac, (__bridge CFDataRef _Nonnull)dummyACM, enc, class, (__bridge CFArrayRef)dummyArray, &attributes, &version, true, &keyclass, &error), @"unexpected exception when decryption fails");
    XCTAssertEqual(keyclass, key_class_ak, @"failed to get back the keyclass when decryption failed");

    self.allowDecryption = YES;
}

#if 0
// these tests fail until we address <rdar://problem/37523001> Fix keychain lock state check to be both secure and fast for EDU mode
- (void)testOperationsDontUseCachedKeysWhileLockedWithAKSAvailable // simulating the backup situation
{
    self.lockState = LockStateLockedAndAllowAKS;

    NSDictionary* item = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                            (id)kSecAttrAccount : @"TestAccount",
                            (id)kSecAttrService : @"TestService",
                            (id)kSecUseDataProtectionKeychain : @(YES) };

    OSStatus result = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, 0, @"failed to add test item to keychain");

    NSMutableDictionary* metadataQuery = item.mutableCopy;
    [metadataQuery removeObjectForKey:(id)kSecValueData];
    metadataQuery[(id)kSecReturnAttributes] = @(YES);
    CFTypeRef foundItem = NULL;
    result = SecItemCopyMatching((__bridge CFDictionaryRef)metadataQuery, &foundItem);
    XCTAssertEqual(result, 0, @"failed to find the metadata for the item we just added in the keychain");

    XCTAssertTrue(self.didAKSDecrypt, @"we did not go through AKS to decrypt the metadata key while locked - bad!");

    NSMutableDictionary* dataQuery = item.mutableCopy;
    dataQuery[(id)kSecReturnData] = @(YES);
    result = SecItemCopyMatching((__bridge CFDictionaryRef)dataQuery, &foundItem);
    XCTAssertEqual(result, 0, @"failed to find the data for the item we just added to the keychain");

    NSData* foundData = (__bridge NSData*)foundItem;
    if ([foundData isKindOfClass:[NSData class]]) {
        NSString* foundPassword = [[NSString alloc] initWithData:(__bridge NSData*)foundItem encoding:NSUTF8StringEncoding];
        XCTAssertEqualObjects(foundPassword, @"password", @"found password (%@) does not match the expected password", foundPassword);
    }
    else {
        XCTAssertTrue(false, @"found data is not the expected class: %@", foundData);
    }
}

- (void)testNoResultsWhenLocked
{
    NSDictionary* item = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                            (id)kSecAttrAccount : @"TestAccount",
                            (id)kSecAttrService : @"TestService",
                            (id)kSecUseDataProtectionKeychain : @(YES) };

    OSStatus result = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
    XCTAssertEqual(result, 0, @"failed to add test item to keychain");

    self.lockState = LockStateLockedAndDisallowAKS;

    NSMutableDictionary* metadataQuery = item.mutableCopy;
    [metadataQuery removeObjectForKey:(id)kSecValueData];
    metadataQuery[(id)kSecReturnAttributes] = @(YES);
    CFTypeRef foundItem = NULL;
    result = SecItemCopyMatching((__bridge CFDictionaryRef)metadataQuery, &foundItem);
    XCTAssertEqual(foundItem, NULL, @"somehow still got results when AKS was locked");
}
#endif

+ (NSData*)fakeDecrypt:(SFAuthenticatedCiphertext*)ciphertext withKey:(SFSymmetricKey*)key error:(NSError**)error
{
    return nil;
}

- (void)testRecoverFromBadMetadataKey
{
    __security_simulatecrash_enable(false);

    // Disable caching, so we can change AKS encrypt/decrypt
    id mockSecDbKeychainMetadataKeyStore = OCMClassMock([SecDbKeychainMetadataKeyStore class]);
    OCMStub([mockSecDbKeychainMetadataKeyStore cachingEnabled]).andReturn(false);

    NSDictionary* addQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                                (id)kSecAttrAccount : @"TestAccount",
                                (id)kSecAttrService : @"TestService",
                                (id)kSecUseDataProtectionKeychain : @(YES),
                                (id)kSecReturnAttributes : @(YES),
                             };

    NSDictionary* findQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                 (id)kSecAttrAccount : @"TestAccount",
                                 (id)kSecAttrService : @"TestService",
                                 (id)kSecUseDataProtectionKeychain : @(YES),
                                 (id)kSecReturnAttributes : @(YES),
                                };
    
#if TARGET_OS_OSX
    NSDictionary* updateQuery = findQuery;
#else
    // iOS won't tolerate kSecReturnAttributes in SecItemUpdate
    NSDictionary* updateQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                   (id)kSecAttrAccount : @"TestAccount",
                                   (id)kSecAttrService : @"TestService",
                                   (id)kSecUseDataProtectionKeychain : @(YES),
                                 };
#endif

    NSDictionary* addQuery2 = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                 (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                                 (id)kSecAttrAccount : @"TestAccount-second",
                                 (id)kSecAttrService : @"TestService-second",
                                 (id)kSecUseDataProtectionKeychain : @(YES),
                                 (id)kSecReturnAttributes : @(YES),
                                 };

    NSDictionary* findQuery2 = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                  (id)kSecAttrAccount : @"TestAccount-second",
                                  (id)kSecAttrService : @"TestService-second",
                                  (id)kSecUseDataProtectionKeychain : @(YES),
                                  (id)kSecReturnAttributes : @(YES),
                                 };

    CFTypeRef result = NULL;

    // Add the item
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery, &result), errSecSuccess, @"Should have succeeded in adding test item to keychain");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemAdd");
    CFReleaseNull(result);

    // Add a second item, for fun and profit
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery2, &result),
                   errSecSuccess,
                   @"Should have succeeded in adding test2 item to keychain");

    // And we can find te item
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findQuery, &result), errSecSuccess, @"Should be able to find item");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemCopyMatching");
    CFReleaseNull(result);

    // And we can update the item
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)updateQuery,
                                 (__bridge CFDictionaryRef)@{(id)kSecValueData: [@"otherpassword" dataUsingEncoding:NSUTF8StringEncoding]}),
                   errSecSuccess,
                   "Should be able to update an item");

    // And find it again
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findQuery, &result), errSecSuccess, @"Should be able to find item");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemCopyMatching");
    CFReleaseNull(result);

    // And we can find the second item
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findQuery2, &result),
                   errSecSuccess, @"Should be able to find second item");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemCopyMatching for item 2");
    CFReleaseNull(result);

    ///////////////////////////////////////////////////////////////////////////////////
    // Now, the metadata keys go corrupt (fake that by changing the underlying AKS key)
    [self setNewFakeAKSKey:[NSData dataWithBytes:"NotthesameAKSkeyas0123456789etc" length:32]];

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findQuery, &result), errSecItemNotFound,
                   "should have received errSecItemNotFound when metadata keys are invalid");
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findQuery, &result), errSecItemNotFound,
                   "Multiple finds of the same item should receive errSecItemNotFound when metadata keys are invalid");
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findQuery, &result), errSecItemNotFound,
                   "Multiple finds of the same item should receive errSecItemNotFound when metadata keys are invalid");

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findQuery2, &result),
                   errSecItemNotFound, @"Should not be able to find corrupt second item");
    XCTAssertNil((__bridge id)result, @"Should have received no data back from SICM for corrupt item");

    // Updating the now-corrupt item should fail
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)updateQuery,
                                 (__bridge CFDictionaryRef)@{ (id)kSecValueData: [@"otherpassword" dataUsingEncoding:NSUTF8StringEncoding] }),
                   errSecItemNotFound,
                   "Should not be able to update a corrupt item");

    // Re-add the item (should succeed)
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery, &result), errSecSuccess, @"Should have succeeded in adding test item to keychain");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemAdd");
    CFReleaseNull(result);

    // And we can find it again
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findQuery, &result), errSecSuccess, @"Should be able to find item");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemAdd");
    CFReleaseNull(result);

    // And update it
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)updateQuery,
                                 (__bridge CFDictionaryRef)@{ (id)kSecValueData: [@"otherpassword" dataUsingEncoding:NSUTF8StringEncoding] }),
                   errSecSuccess,
                   "Should be able to update a fixed item");

    /////////////
    // And our second item, which is wrapped under an old key, can't be found
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findQuery2, &result),
                   errSecItemNotFound, @"Should not be able to find corrupt second item");
    XCTAssertNil((__bridge id)result, @"Should have received no data back from SICM for corrupt item");

    // But can be re-added
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)addQuery2, &result),
                   errSecSuccess,
                   @"Should have succeeded in adding test2 item to keychain after corruption");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemAdd for item 2 (after corruption)");
    CFReleaseNull(result);

    // And we can find the second item again
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findQuery2, &result),
                   errSecSuccess, @"Should be able to find second item after re-add");
    XCTAssertNotNil((__bridge id)result, @"Should have received a dictionary back from SecItemCopyMatching for item 2 (after re-add)");
    CFReleaseNull(result);

    [mockSecDbKeychainMetadataKeyStore stopMocking];
}

// If a metadata key is created during a database transaction which is later rolled back, it shouldn't be cached for use later.
- (void)testMetadataKeyDoesntOutliveTxionRollback {
    NSString* testAccount = @"TestAccount";
    NSString* otherAccount = @"OtherAccount";
    NSString* thirdAccount = @"ThirdAccount";
    [self addTestItemExpecting:errSecSuccess account:testAccount accessible:(id)kSecAttrAccessibleAfterFirstUnlock];
    [self checkDatabaseExistenceOfMetadataKey:key_class_ck shouldExist:true value:nil];
    [self checkDatabaseExistenceOfMetadataKey:key_class_cku shouldExist:false value:nil];

    // This should fail, and not create a CKU metadata key
    [self addTestItemExpecting:errSecDuplicateItem account:testAccount accessible:(id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly];
    [self checkDatabaseExistenceOfMetadataKey:key_class_ck shouldExist:true value:nil];
    [self checkDatabaseExistenceOfMetadataKey:key_class_cku shouldExist:false value:nil];

    // But successfully creating a new CKU item should create the key
    [self addTestItemExpecting:errSecSuccess account:otherAccount accessible:(id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly];
    [self checkDatabaseExistenceOfMetadataKey:key_class_ck shouldExist:true value:nil];
    [self checkDatabaseExistenceOfMetadataKey:key_class_cku shouldExist:true value:nil];

    // Drop all metadata key caches
    [SecDbKeychainMetadataKeyStore resetSharedStore];

    [self findTestItemExpecting:errSecSuccess account:testAccount];
    [self findTestItemExpecting:errSecSuccess account:otherAccount];

    // Adding another CKU item now should be fine
    [self addTestItemExpecting:errSecSuccess account:thirdAccount accessible:(id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly];
    [self checkDatabaseExistenceOfMetadataKey:key_class_ck shouldExist:true value:nil];
    [self checkDatabaseExistenceOfMetadataKey:key_class_cku shouldExist:true value:nil];

    // Drop all metadata key caches once more, to ensure we can find all three items from the persisted keys
    [SecDbKeychainMetadataKeyStore resetSharedStore];

    [self findTestItemExpecting:errSecSuccess account:testAccount];
    [self findTestItemExpecting:errSecSuccess account:otherAccount];
    [self findTestItemExpecting:errSecSuccess account:thirdAccount];
}

- (void)testRecoverDataFromBadKeyclassStorage
{
    NSDictionary* metadataAttributesInput = @{@"TestMetadata" : @"TestValue"};
    SecDbKeychainSerializedItemV7* serializedItem = [self serializedItemWithPassword:@"password" metadataAttributes:metadataAttributesInput];
    serializedItem.keyclass = (serializedItem.keyclass | key_class_last + 1);
    
    NSError* error = nil;
    SecDbKeychainItemV7* item = [[SecDbKeychainItemV7 alloc] initWithData:serializedItem.data decryptionKeybag:0 error:&error];
    NSDictionary* metadataAttributesOut = [item metadataAttributesWithError:&error];
    XCTAssertEqualObjects(metadataAttributesOut, metadataAttributesInput, @"failed to retrieve metadata with error: %@", error);
    XCTAssertNil(error, @"error encountered attempting to retrieve metadata: %@", error);
}

- (NSData*)performItemEncryptionWithAccessibility:(CFStringRef)accessibility
{
    SecAccessControlRef ac = NULL;
    CFDataRef enc = NULL;
    CFErrorRef error = NULL;
    
    NSDictionary* secretData = @{(id)kSecValueData : @"secret here"};
    CFDictionaryRef emptyDict = (__bridge CFDictionaryRef)@{};
    
    ac = SecAccessControlCreate(NULL, &error);
    XCTAssertNotNil((__bridge id)ac, @"failed to create access control with error: %@", (__bridge id)error);
    XCTAssertNil((__bridge id)error, @"encountered error attempting to create access control: %@", (__bridge id)error);
    XCTAssertTrue(SecAccessControlSetProtection(ac, accessibility, &error), @"failed to set access control protection with error: %@", error);
    XCTAssertNil((__bridge id)error, @"encountered error attempting to set access control protection: %@", (__bridge id)error);
    
    XCTAssertTrue(ks_encrypt_data(KEYBAG_DEVICE, ac, NULL, (__bridge CFDictionaryRef)secretData, emptyDict, emptyDict, &enc, true, &error), @"failed to encrypt data with error: %@", error);
    XCTAssertTrue(enc != NULL, @"failed to get encrypted data from encryption function");
    XCTAssertNil((__bridge id)error, @"encountered error attempting to encrypt data: %@", (__bridge id)error);
    CFReleaseNull(ac);
    
    return (__bridge_transfer NSData*)enc;
}

- (void)performMetadataDecryptionOfData:(NSData*)encryptedData verifyingAccessibility:(CFStringRef)accessibility
{
    CFErrorRef error = NULL;
    CFMutableDictionaryRef attributes = NULL;
    uint32_t version = 0;
    
    SecAccessControlRef ac = SecAccessControlCreate(NULL, &error);
    XCTAssertNotNil((__bridge id)ac, @"failed to create access control with error: %@", (__bridge id)error);
    XCTAssertNil((__bridge id)error, @"encountered error attempting to create access control: %@", (__bridge id)error);
    XCTAssertTrue(SecAccessControlSetProtection(ac, accessibility, &error), @"failed to set access control protection with error: %@", error);
    XCTAssertNil((__bridge id)error, @"encountered error attempting to set access control protection: %@", (__bridge id)error);
    
    keyclass_t keyclass = 0;
    NSData* dummyACM = [NSData dataWithBytes:"dummy" length:5];
    const SecDbClass* class = kc_class_with_name(kSecClassGenericPassword);
    NSArray* dummyArray = [NSArray array];

    XCTAssertTrue(ks_decrypt_data(KEYBAG_DEVICE, kAKSKeyOpDecrypt, &ac, (__bridge CFDataRef _Nonnull)dummyACM, (__bridge CFDataRef)encryptedData, class, (__bridge CFArrayRef)dummyArray, &attributes, &version, false, &keyclass, &error), @"failed to decrypt data with error: %@", error);
    XCTAssertNil((__bridge id)error, @"encountered error attempting to decrypt data: %@", (__bridge id)error);
    XCTAssertEqual(keyclass & key_class_last, parse_keyclass(accessibility), @"failed to get back the keyclass from decryption");
    
    CFReleaseNull(error);
}

- (void)performMetadataEncryptDecryptWithAccessibility:(CFStringRef)accessibility
{
    NSData* encryptedData = [self performItemEncryptionWithAccessibility:accessibility];
    
    [SecDbKeychainMetadataKeyStore resetSharedStore];
    
    [self performMetadataDecryptionOfData:encryptedData verifyingAccessibility:accessibility];
}

- (void)testMetadataClassKeyDecryptionWithSimulatedAKSRolledKeys
{
    self.simulateRolledAKSKey = YES;
    
    [self performMetadataEncryptDecryptWithAccessibility:kSecAttrAccessibleWhenUnlocked];
    XCTAssertEqual(self.keyclassUsedForAKSDecryption, key_class_ak | key_class_last + 1);
    
    [self performMetadataEncryptDecryptWithAccessibility:kSecAttrAccessibleAfterFirstUnlock];
    XCTAssertEqual(self.keyclassUsedForAKSDecryption, key_class_ck | key_class_last + 1);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [self performMetadataEncryptDecryptWithAccessibility:kSecAttrAccessibleAlways];
    XCTAssertEqual(self.keyclassUsedForAKSDecryption, key_class_dk | key_class_last + 1);
#pragma clang diagnostic pop
    
    [self performMetadataEncryptDecryptWithAccessibility:kSecAttrAccessibleWhenUnlockedThisDeviceOnly];
    XCTAssertEqual(self.keyclassUsedForAKSDecryption, key_class_aku | key_class_last + 1);
    
    [self performMetadataEncryptDecryptWithAccessibility:kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly];
    XCTAssertEqual(self.keyclassUsedForAKSDecryption, key_class_cku | key_class_last + 1);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [self performMetadataEncryptDecryptWithAccessibility:kSecAttrAccessibleAlwaysThisDeviceOnly];
    XCTAssertEqual(self.keyclassUsedForAKSDecryption, key_class_dku | key_class_last + 1);
#pragma clang diagnostic pop
    
    [self performMetadataEncryptDecryptWithAccessibility:kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly];
    XCTAssertEqual(self.keyclassUsedForAKSDecryption, key_class_akpu | key_class_last + 1);
}

- (void)testUpgradingMetadataKeyEntry
{
    // first, force the creation of a metadata key
    NSData* encryptedData = [self performItemEncryptionWithAccessibility:kSecAttrAccessibleWhenUnlocked];

    // now let's jury-rig this metadata key to look like an old one with no actualKeyclass information
    __block CFErrorRef error = NULL;
    __block bool ok = true;
    ok &= kc_with_dbt(true, &error, ^bool(SecDbConnectionRef dbt) {
        if (checkV12DevEnabled()) { // item is in new format, turn it into an old format item
            NSString* sql = [NSString stringWithFormat:@"SELECT metadatakeydata FROM metadatakeys WHERE keyclass = %d", key_class_ak];
            __block NSData* key;
            ok &= SecDbPrepare(dbt, (__bridge CFStringRef)sql, &error, ^(sqlite3_stmt* stmt) {
                ok &= SecDbStep(dbt, stmt, &error, ^(bool *stop) {
                    NSData* wrappedKey = [[NSData alloc] initWithBytes:sqlite3_column_blob(stmt, 0) length:sqlite3_column_bytes(stmt, 0)];
                    SecDbKeychainSerializedMetadataKey* mdkdata = [[SecDbKeychainSerializedMetadataKey alloc] initWithData:wrappedKey];
                    key = mdkdata.akswrappedkey;
                });
            });
            sql = [NSString stringWithFormat:@"UPDATE metadatakeys SET actualKeyclass = 0, data = ?, metadatakeydata = ? WHERE keyclass = %d", key_class_ak];
            ok &= SecDbPrepare(dbt, (__bridge CFStringRef)sql, &error, ^(sqlite3_stmt *stmt) {
                ok &= SecDbBindBlob(stmt, 1, key.bytes, key.length, SQLITE_TRANSIENT, &error);
                ok &= SecDbStep(dbt, stmt, &error, NULL);
            });
        } else {
            NSString* sql = [NSString stringWithFormat:@"UPDATE metadatakeys SET actualKeyclass = %d WHERE keyclass = %d", 0, key_class_ak];
            ok &= SecDbPrepare(dbt, (__bridge CFStringRef)sql, &error, ^(sqlite3_stmt* stmt) {
                ok &= SecDbStep(dbt, stmt, &error, NULL);
            });
        }
        return ok;
    });

    // now, let's simulate AKS rejecting the decryption, and see if we recover and also update the database
    self.simulateRolledAKSKey = YES;
    [SecDbKeychainMetadataKeyStore resetSharedStore];
    [self performMetadataDecryptionOfData:encryptedData verifyingAccessibility:kSecAttrAccessibleWhenUnlocked];
}

@end

#endif
