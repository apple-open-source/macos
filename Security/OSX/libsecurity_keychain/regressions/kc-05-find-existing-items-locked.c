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
#include "kc-item-helpers.h"
#include "kc-key-helpers.h"

static void tests()
{
    SecKeychainRef kc = getPopulatedTestKeychain();

    CFMutableDictionaryRef query = NULL;
    SecKeychainItemRef item = NULL;

    // Perform keychain upgrade so future calls will check integrity, then lock keychain
    query = createQueryCustomItemDictionaryWithService(kc, kSecClassInternetPassword, CFSTR("test_service"), CFSTR("test_service"));
    item = checkNCopyFirst(testName, query, 1);
    CFReleaseNull(item);

    ok_status(SecKeychainLock(kc), "%s: SecKeychainLock", testName);

    // Find passwords
    query = createQueryCustomItemDictionaryWithService(kc, kSecClassInternetPassword, CFSTR("test_service"), CFSTR("test_service"));
    item = checkNCopyFirst(testName, query, 1);
    readPasswordContentsWithResult(item, errSecAuthFailed, NULL); // keychain is locked; AuthFailed is what securityd throws if UI access is not allowed
    CFReleaseNull(item);
    checkPrompts(0, "after reading a password in locked keychain without UI"); // this should be 1, but is 0 due to how denying UI access works in Credentials

    query = createQueryCustomItemDictionaryWithService(kc, kSecClassInternetPassword, CFSTR("test_service_restrictive_acl"), CFSTR("test_service_restrictive_acl"));
    item = checkNCopyFirst(testName, query, 1);
    readPasswordContentsWithResult(item, errSecAuthFailed, NULL);
    CFReleaseNull(item);
    checkPrompts(0, "trying to read password in locked keychain without UI");

    query = createQueryCustomItemDictionaryWithService(kc, kSecClassGenericPassword, CFSTR("test_service"), CFSTR("test_service"));
    item = checkNCopyFirst(testName, query, 1);
    readPasswordContentsWithResult(item, errSecAuthFailed, NULL); // keychain is locked
    CFReleaseNull(item);
    checkPrompts(0, "after reading a password in locked keychain without UI");

    query = createQueryCustomItemDictionaryWithService(kc, kSecClassGenericPassword, CFSTR("test_service_restrictive_acl"), CFSTR("test_service_restrictive_acl"));
    item = checkNCopyFirst(testName, query, 1);
    readPasswordContentsWithResult(item, errSecAuthFailed, NULL); // we don't expect to be able to read this
    CFReleaseNull(item);
    checkPrompts(0, "trying to read password in locked keychain without UI");

    // Find symmetric keys
    query = createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric);
    item = checkNCopyFirst(testName, query, 2);
    CFReleaseNull(item);

    // Find asymmetric keys
    query = createQueryKeyDictionary(kc, kSecAttrKeyClassPublic);
    item = checkNCopyFirst(testName, query, 2);
    CFReleaseNull(item);

    query = createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate);
    item = checkNCopyFirst(testName, query, 2);
    CFReleaseNull(item);

    // Find certificates
    query = makeBaseQueryDictionary(kc, kSecClassCertificate);
    item = checkNCopyFirst(testName, query, 3);
    CFReleaseNull(item);

    // ensure we can pull data from a certificate
    query = makeBaseQueryDictionary(kc, kSecClassCertificate);
    CFDictionarySetValue(query, kSecMatchSubjectWholeString, CFSTR("test_codesigning"));
    item = checkNCopyFirst(testName, query, 1);
    const unsigned char expectedSHA1[] = { 0x94, 0xdf, 0x22, 0x4a, 0x4d, 0x49, 0x33, 0x27, 0x9e, 0xc5, 0x7e, 0x91, 0x95, 0xcc, 0xbd, 0x51, 0x3d, 0x59, 0xae, 0x34 };
    CFDataRef expectedSHAData = CFDataCreateWithBytesNoCopy(NULL, expectedSHA1, sizeof(expectedSHA1), kCFAllocatorNull);
    eq_cf(SecCertificateGetSHA1Digest((SecCertificateRef) item), expectedSHAData, "%s: expected SHA1 of certificate does not match", testName);
    CFReleaseNull(item);
    CFReleaseNull(expectedSHAData);

    checkPrompts(0, "searching keys and certificates");

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", testName);
    CFReleaseNull(kc);
}
#define nTests (getPopulatedTestKeychainTests + checkNTests + 1 + \
checkNTests + readPasswordContentsWithResultTests + checkPromptsTests + \
checkNTests + readPasswordContentsWithResultTests + checkPromptsTests + \
checkNTests + readPasswordContentsWithResultTests + checkPromptsTests + \
checkNTests + readPasswordContentsWithResultTests + checkPromptsTests + \
checkNTests + \
checkNTests + \
checkNTests + \
checkNTests + \
checkNTests + 1 + \
checkPromptsTests + 1)

int kc_05_find_existing_items_locked(int argc, char *const *argv)
{
    plan_tests(nTests);
    initializeKeychainTests(__FUNCTION__);

    tests();

    deleteTestFiles();

    return 0;
}
