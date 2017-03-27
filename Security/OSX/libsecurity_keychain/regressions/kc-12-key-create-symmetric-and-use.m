/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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
 * limitations under the xLicense.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import <Security/Security.h>
#import <Security/SecCertificatePriv.h>

#include "keychain_regressions.h"
#include "kc-helpers.h"
#define nullptr NULL

#import <Foundation/Foundation.h>

static NSString * const EncryptionKeyLabel = @"Test Encryption Key";

//
// This function allows finer-grained access control settings;
// the given application list is trusted only for one specific authorization
// (e.g. kSecACLAuthorizationDecrypt). Note that if trustedApplications
// is NULL, this means "allow any application", while an empty (zero-length)
// list means "no applications have access".
// Returns true if the ACL was modified successfully, otherwise false.
//
static bool setTrustedApplicationsForACLAuthorization(SecAccessRef access,
                                                      CFTypeRef authorizationTag, NSArray* trustedApplications)
{
    // get the access control list for this authorization tag)
    CFArrayRef aclList = SecAccessCopyMatchingACLList(access, authorizationTag);
    if (!aclList)
        return false;

    // get the first entry in the access control list
    SecACLRef aclRef=(SecACLRef)CFArrayGetValueAtIndex(aclList, 0);
    CFArrayRef appList=nil;
    CFStringRef promptDescription=nil;
    SecKeychainPromptSelector promptSelector;
    OSStatus err = SecACLCopyContents(aclRef, &appList, &promptDescription, &promptSelector);
    ok_status(err, "%s: SecACLCopyContents", testName);

    if (!trustedApplications) // "allow all applications to access this item"
    {
        // change the ACL to not require the passphrase, and have a nil application list.
        promptSelector &= ~kSecKeychainPromptRequirePassphase;
        err = SecACLSetContents(aclRef, NULL, promptDescription, promptSelector);
        ok_status(err, "%s: SecACLSetContents (allow all)", testName);
    }
    else // "allow access by these applications"
    {
        // modify the application list
        err = SecACLSetContents(aclRef, (CFArrayRef)trustedApplications, promptDescription, promptSelector);
        ok_status(err, "%s: SecACLSetContents", testName);
    }

    if (appList) CFRelease(appList);
    if (promptDescription) CFRelease(promptDescription);

    CFRelease(aclList);
    return (!err);
}

//
// This function returns a SecAccessRef, which the caller owns and must release.
// Note that if the provided item is not NULL, its existing access reference is returned,
// otherwise a new access reference is created.
//
static SecAccessRef createAccess(SecKeychainItemRef item, NSString *accessLabel, BOOL allowAny)
{
    OSStatus err;
    SecAccessRef access=nil;
    NSArray *trustedApplications=nil;

    if (!allowAny) // use default access ("confirm access")
    {
        // make an exception list of applications you want to trust,
        // which are allowed to access the item without requiring user confirmation
        SecTrustedApplicationRef myself, someOther;
        err = SecTrustedApplicationCreateFromPath(NULL, &myself);
        ok_status(err, "%s: SecTrustedApplicationCreateFromPath (1)", testName);
        err = SecTrustedApplicationCreateFromPath("/Applications/Safari.app", &someOther);
        ok_status(err, "%s: SecTrustedApplicationCreateFromPath (2)", testName);
        trustedApplications = [NSArray arrayWithObjects:(__bridge_transfer id)myself, (__bridge_transfer id)someOther, nil];
    }

    // If the keychain item already exists, use its access reference; otherwise, create a new one
    if (item) {
        err = SecKeychainItemCopyAccess(item, &access);
        ok_status(err, "%s: SecKeychainItemCopyAccess", testName);
    } else {
        err = SecAccessCreate((CFStringRef)accessLabel, (CFArrayRef)trustedApplications, &access);
        ok_status(err, "%s: SecAccessCreate", testName);
    }

    if (err) return nil;

    // At this point we have a SecAccessRef which permits "decrypt" access to the item
    // only by apps in our trustedApplications list. We could return at this point.
    //
    // To set up other types of access, we need to do more work.
    // In this example, we'll explicitly set the access control for decrypt and encrypt operations.
    //
    setTrustedApplicationsForACLAuthorization(access, kSecACLAuthorizationEncrypt, (allowAny) ? NULL : trustedApplications);
    setTrustedApplicationsForACLAuthorization(access, kSecACLAuthorizationDecrypt, (allowAny) ? NULL : trustedApplications);

    return access;
}

static SecKeyRef findExistingEncryptionKey(SecKeychainRef kc)
{
    SecKeyRef key;
    NSArray* searchList = @[(__bridge id) kc];
    NSDictionary *query = @{ (__bridge id)kSecMatchSearchList: searchList,
                             (__bridge id)kSecClass: (__bridge id)kSecClassKey,
                             (__bridge id)kSecAttrApplicationLabel: EncryptionKeyLabel,
                             (__bridge id)kSecReturnRef: @YES };
    if (!SecItemCopyMatching((__bridge CFDictionaryRef)query, (CFTypeRef*)&key))
        return key;

    return nullptr;
}

static SecKeyRef generateEncryptionKey(SecKeychainRef kc)
{
    SecAccessRef access = createAccess(nil, EncryptionKeyLabel, false);
    if (!access) {
        NSLog(@"Creating an access object failed.");
        return nullptr;
    }

    CFErrorRef error = NULL;
    NSDictionary *keyParameters = @{ (__bridge id)kSecUseKeychain: (__bridge id)kc,
                                     (__bridge id)kSecAttrKeyType: (__bridge id)kSecAttrKeyTypeAES,
                                     (__bridge id)kSecAttrKeySizeInBits: @(256),
                                     (__bridge id)kSecAttrCanEncrypt: @YES,
                                     (__bridge id)kSecAttrCanDecrypt: @YES,
                                     (__bridge id)kSecAttrIsPermanent: @YES,
                                     (__bridge id)kSecAttrAccess: (__bridge id)access,
                                     (__bridge id)kSecAttrLabel: EncryptionKeyLabel,
                                     (__bridge id)kSecAttrApplicationLabel: EncryptionKeyLabel };
    SecKeyRef key = SecKeyGenerateSymmetric((__bridge CFDictionaryRef)keyParameters, &error);

    is(error, NULL, "%s: SecKeyGenerateSymmetric", testName);
    ok(key, "%s: SecKeyGenerateSymmetric returned a key", testName);

    if (!key)
        NSLog(@"Creating encryption key failed: %@", error);

    CFRelease(access);
    return key;
}

static SecKeyRef findOrGenerateEncryptionKey(SecKeychainRef kc)
{
    SecKeyRef key = findExistingEncryptionKey(kc);
    if (key)
        return key;

    return generateEncryptionKey(kc);
}

static SecKeyRef encryptionKey(SecKeychainRef kc)
{
    static SecKeyRef key = NULL;
    if (!key) {
        key = findOrGenerateEncryptionKey(kc);
    }
    return key;
}

static NSData *encryptData(SecKeychainRef kc, NSData *plainTextData)
{
    SecTransformRef transform = SecEncryptTransformCreate(encryptionKey(kc), nullptr);
    SecTransformSetAttribute(transform, kSecPaddingKey, kSecPaddingPKCS7Key, nullptr);
    SecTransformSetAttribute(transform, kSecEncryptionMode, kSecModeCBCKey, nullptr);
    SecTransformSetAttribute(transform, kSecTransformInputAttributeName, (__bridge CFDataRef)plainTextData, nullptr);

    CFErrorRef error = 0;
    NSData *result = CFBridgingRelease(SecTransformExecute(transform, &error));
    CFRelease(transform);
    is(error, NULL, "%s: SecTransformExecute (encrypt)", testName);

    if (!result) {
        NSLog(@"Encrypting data failed: %@", error);
        CFRelease(error);
        return nil;
    }

    return result;
}

static NSData *decryptData(SecKeychainRef kc, NSData *cipherTextData)
{
    SecTransformRef transform = SecDecryptTransformCreate(encryptionKey(kc), nullptr);
    SecTransformSetAttribute(transform, kSecPaddingKey, kSecPaddingPKCS7Key, nullptr);
    SecTransformSetAttribute(transform, kSecEncryptionMode, kSecModeCBCKey, nullptr);
    SecTransformSetAttribute(transform, kSecTransformInputAttributeName, (__bridge CFDataRef)cipherTextData, nullptr);

    CFErrorRef error = 0;
    NSData *result = CFBridgingRelease(SecTransformExecute(transform, &error));
    is(error, NULL, "%s: SecTransformExecute (decrypt)", testName);

    CFRelease(transform);
    if (!result) {
        NSLog(@"Decrypting data failed: %@", error);
        CFRelease(error);
        return nil;
    }

    return result;
}

int kc_12_key_create_symmetric_and_use(int argc, char *const *argv)
{
    plan_tests(17);
    initializeKeychainTests(__FUNCTION__);

    SecKeychainRef kc = getPopulatedTestKeychain();

    @autoreleasepool {
        NSData *data = [@"Hello, world!" dataUsingEncoding:NSUTF8StringEncoding];
        NSLog(@" Original: %@", data);
        NSData *encryptedData = encryptData(kc, data);
        NSLog(@"Encrypted: %@", encryptedData);
        if (encryptedData) {
            NSData *roundtrippedData = decryptData(kc, encryptedData);

            eq_cf((__bridge CFTypeRef) roundtrippedData, (__bridge CFTypeRef) data, "%s: Round-tripped data does not match original data", testName);
            NSLog(@"Decrypted: %@", roundtrippedData);
        }
    }

    checkPrompts(0, "no prompts during test");

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", testName);
    CFReleaseNull(kc);

    deleteTestFiles();
    return 0;
}

