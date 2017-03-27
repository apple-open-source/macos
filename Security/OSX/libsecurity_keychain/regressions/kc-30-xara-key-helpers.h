/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

#include "kc-30-xara-helpers.h"
#include "kc-key-helpers.h"

#ifndef kc_30_xara_key_helpers_h
#define kc_30_xara_key_helpers_h

#if TARGET_OS_MAC

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"

static void makeCustomKeyWithIntegrity(const char* name, SecKeychainRef kc, CFStringRef label, CFStringRef expectedHash) {
    SecKeyRef key = createCustomKey(name, kc, label);
    checkIntegrityHash(name, (SecKeychainItemRef) key, expectedHash);
    checkPartitionIDs(name, (SecKeychainItemRef) key, 1);
    CFReleaseNull(key);
 }
#define makeCustomKeyWithIntegrityTests (createCustomKeyTests + checkIntegrityHashTests + checkPartitionIDsTests)

static void makeKeyWithIntegrity(const char* name, SecKeychainRef kc, CFStringRef expectedHash) {
    makeCustomKeyWithIntegrity(name, kc, CFSTR("test_key"), expectedHash);
 }
#define makeKeyWithIntegrityTests makeCustomKeyWithIntegrityTests

// Note that this is nearly useless, as key pairs will never have stable hashes
static void makeKeyPairWithIntegrity(const char* name, SecKeychainRef kc, CFStringRef expectedPubHash, CFStringRef expectedPrivHash) {
    SecKeyRef pub;
    SecKeyRef priv;
    makeKeyPair(name, kc, &pub, &priv);

    checkIntegrityHash(name, (SecKeychainItemRef) pub, expectedPubHash);
    checkIntegrityHash(name, (SecKeychainItemRef) priv, expectedPrivHash);
}
#define makeKeyPairWithIntegrityTests (makeKeyTests + checkIntegrityHashTests)

// And now for the actual tests

static void testAddKey(CFStringRef expectedHash) {
    char* name = "testAddKey";
    SecKeychainRef kc = newKeychain(name);
    makeKeyWithIntegrity(name, kc, expectedHash);
    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
}
#define testAddKeyTests (newKeychainTests + makeKeyWithIntegrityTests + 1)

static void testAddFreeKey(CFStringRef expectedHash) {
    // Due to <rdar://problem/8431281> SecItemAdd() will not add a generated symmetric key to the keychain
    // we can't actually run this test. Code is included here as a reference.
    //char* name = "testAddFreeKey";
    //SecKeychainRef kc = newKeychain(name);

    //SecKeyRef key = makeFreeKey(name, kc);
    //checkIntegrityHash(name, (SecKeychainItemRef) key, expectedHash);

    //ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    //CFReleaseNull(kc);
}
//#define testAddFreeKeyTests (newKeychainTests + makeFreeKeyTests + checkIntegrityHashTests + 1)
#define testAddFreeKeyTests 0

static void testCopyMatchingKey(CFStringRef expectedHash) {
    char* name = "testCopyMatchingKey";
    secnotice("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 0);

    makeKeyWithIntegrity(name, kc, expectedHash);

    SecKeyRef item = (SecKeyRef) checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    checkIntegrityHash(name, (SecKeychainItemRef) item, expectedHash);
    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(item);
    CFReleaseNull(kc);
}
#define testCopyMatchingKeyTests (newKeychainTests + checkNTests + makeKeyWithIntegrityTests + checkNTests + checkIntegrityHashTests + 1)


static void testUpdateKey(CFStringRef expectedHash, CFStringRef expectedHashAfter) {
    char * name = "testUpdateKey";
    secnotice("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    makeKeyWithIntegrity(name, kc, expectedHash);
    SecKeychainItemRef item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    CFReleaseNull(item);

    CFStringRef label = CFSTR("a modified label");

    CFMutableDictionaryRef query = createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric);
    CFMutableDictionaryRef update = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(update, kSecAttrLabel, label);
    ok_status(SecItemUpdate(query, update), "%s: SecItemUpdate", name);

    CFReleaseNull(query);
    CFReleaseNull(update);

    // Find the item again.
    query = createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric);
    CFDictionarySetValue(query, kSecAttrLabel, label);
    item = checkNCopyFirst(name, query, 1);
    checkIntegrityHash(name, item, expectedHashAfter);
    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
}
#define testUpdateKeyTests (newKeychainTests + makeKeyWithIntegrityTests + checkNTests + 1 + checkNTests + checkIntegrityHashTests + 1)

// Key Pairs have non-predictable hashes, since they receive an attribute kSecKeyLabel that is
// the hash of the public key. Since the key is generated randomly, so is the label.
//
// We can't do our normal "add", "copymatching", "update" tests here, so do
// something special...

static void testKeyPair() {
    char* name = "testKeyPair";
    secnotice("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 0);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 0);

    SecKeyRef pub;
    SecKeyRef priv;
    makeKeyPair(name, kc, &pub, &priv);

    // Now that we have the key pair, make sure we can pull the individual keys
    // out (and the hashes match)

     CFStringRef label = CFSTR("a modified label");
    CFMutableDictionaryRef query;
    CFMutableDictionaryRef update;

    // Ensure that the public key still exists and can be updated

    SecKeyRef item;
    item = (SecKeyRef) checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    checkHashesMatch(name, (SecKeychainItemRef)pub, (SecKeychainItemRef)item);

    query = createQueryKeyDictionary(kc, kSecAttrKeyClassPublic);
    CFDictionarySetValue(query, kSecAttrLabel, label);
    item = (SecKeyRef) checkNCopyFirst(name, query, 0);

    query = createQueryKeyDictionary(kc, kSecAttrKeyClassPublic);
    update = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(update, kSecAttrLabel, label);
    ok_status(SecItemUpdate(query, update), "%s: SecItemUpdate (public key)", name);

    query = createQueryKeyDictionary(kc, kSecAttrKeyClassPublic);
    CFDictionarySetValue(query, kSecAttrLabel, label);
    item = (SecKeyRef) checkNCopyFirst(name, query, 1);
    CFReleaseNull(item);

    // Ensure that the private key still exists and can be updated

    item = (SecKeyRef) checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);
    checkHashesMatch(name, (SecKeychainItemRef)priv, (SecKeychainItemRef)item);

    query = createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate);
    CFDictionarySetValue(query, kSecAttrLabel, label);
    item = (SecKeyRef) checkNCopyFirst(name, query, 0);

    query = createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate);
    update = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(update, kSecAttrLabel, label);
    ok_status(SecItemUpdate(query, update), "%s: SecItemUpdate (private key)", name);

    query = createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate);
    CFDictionarySetValue(query, kSecAttrLabel, label);
    item = (SecKeyRef) checkNCopyFirst(name, query, 1);
    CFReleaseNull(item);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
}
#define testKeyPairTests (newKeychainTests + checkNTests + checkNTests + makeKeyPairTests \
        + checkNTests + checkHashesMatchTests \
        + checkNTests + 1 + checkNTests \
        + checkNTests + checkHashesMatchTests \
        + checkNTests + 1 + checkNTests \
        + 1)

static void testAddDuplicateKey(CFStringRef expectedHash) {
    char * name = "testAddDuplicateKey";
    secnotice("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 0);

    makeKeyWithIntegrity(name, kc, expectedHash);

    SecKeychainItemRef item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    makeDuplicateKey(name, kc);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    CFReleaseNull(item);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
}
#define testAddDuplicateKeyTests (newKeychainTests + checkNTests +makeKeyWithIntegrityTests + checkNTests + makeDuplicateKeyTests + checkNTests + 1)

static void testAddDuplicateFreeKey(CFStringRef expectedHash) {
    // Due to <rdar://problem/8431281> SecItemAdd() will not add a generated symmetric key to the keychain
    // we can't actually run this test. Code is included here as a reference.
    //char * name = "testAddDuplicateFreeKey";
    //secnotice("integrity", "************************************* %s", name);
    //SecKeychainRef kc = newKeychain(name);
    //checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 0);

    //SecKeyRef key = makeFreeKey(name, kc);
    //checkIntegrityHash(name, (SecKeychainItemRef) key, expectedHash);
    //SecKeychainItemRef item = checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);

    //makeDuplicateFreeKey(name, kc);
    //checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);

    //ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    //CFReleaseNull(kc);
}
//#define testAddDuplicateFreeKeyTests (newKeychainTests + checkNTests + makeFreeKeyTests + checkIntegrityHashTests + checkNTests \
//        + makeDuplicateKeyTests + checkNTests + 1)
#define testAddDuplicateFreeKeyTests 0

// testAddDuplicateKeyPair:
//
// By use of the Sec* APIs, you will almost certainly never get an
// errSecDuplicateItem out of SecKeychainGeneratePair. Since it sets a primary
// key attribute as the hash of the public key, it just will never generate a
// duplicate item.

static void testExportImportKeyPair() {
    char* name = "testExportImportKeyPair";
    secnotice("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 0);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 0);

    SecKeyRef pub;
    SecKeyRef priv;
    makeKeyPair(name, kc, &pub, &priv);

    // Now that we have the key pair, make sure we can pull the individual keys out

    SecKeyRef item;
    item = (SecKeyRef) checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    checkHashesMatch(name, (SecKeychainItemRef)pub, (SecKeychainItemRef)item);

    item = (SecKeyRef) checkNCopyFirst(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);
    checkHashesMatch(name, (SecKeychainItemRef)priv, (SecKeychainItemRef)item);

    CFMutableArrayRef applications = (CFMutableArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 1, &kCFTypeArrayCallBacks);
    SecTrustedApplicationRef app = NULL;

    ok_status(SecTrustedApplicationCreateFromPath(NULL, &app), "%s: SecTrustedApplicationCreateFromPath", name);
    CFArrayAppendValue(applications, app);

    ok_status(SecTrustedApplicationCreateFromPath("/usr/bin/codesign", &app), "%s: SecTrustedApplicationCreateFromPath", name);
    CFArrayAppendValue(applications, app);

    SecAccessRef accessRef = NULL;
    ok_status(SecAccessCreate(CFSTR("accessDescription"), applications, &accessRef), "%s: SecAccessCreate", name);

    const SecItemImportExportKeyParameters keyParams =
    {
        SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION,
        0,
        CFSTR( "" ),
        CFSTR( "" ),
        CFSTR( "" ),
        accessRef, 0, 0
    };
    CFArrayRef items = NULL;

    CFDataRef keyData = NULL;
    ok_status(SecItemExport(pub, kSecFormatPEMSequence, kSecItemPemArmour, &keyParams, &keyData), "%s: SecItemExport", name);
    ok_status(SecKeychainItemDelete((SecKeychainItemRef)pub), "%s: SecKeychainItemDelete", name);;
    CFRelease(pub);
    pub = NULL;

    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 0);
    ok_status(SecItemImport(keyData, NULL, NULL, NULL, kSecItemPemArmour, &keyParams, kc, &items), "%s: SecItemImport", name);
    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);


    ok_status(SecItemExport(priv, kSecFormatPEMSequence, kSecItemPemArmour, &keyParams, &keyData), "%s: SecItemExport", name);
    ok_status(SecKeychainItemDelete((SecKeychainItemRef)priv), "%s: SecKeychainItemDelete", name);;
    CFRelease(priv);
    priv = NULL;

    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 0);

    ok_status(SecItemImport(keyData, NULL, NULL, NULL, kSecItemPemArmour, &keyParams, kc, &items), "%s: SecItemImport", name);

    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);

    SecAccessRef newRef = NULL;
    ok_status(SecKeychainItemCopyAccess((SecKeychainItemRef) CFArrayGetValueAtIndex(items, 0), &newRef), "%s:SecKeychainItemCopyAccess", name);

    SecKeyRef importedKey = items && CFArrayGetCount(items) > 0 ? (SecKeyRef)CFArrayGetValueAtIndex(items, 0) : NULL;
    if (importedKey) {
        CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(query, kSecClass, kSecClassKey);
        CFDictionaryAddValue(query, kSecValueRef, importedKey);

        CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(attrs, kSecAttrLabel, CFSTR("private key custom label"));

        ok_status( SecItemUpdate(query, attrs), "%s: SecItemUpdate", name);
    } else {
        fail("%s: Didn't have an item to update", name);
    }

    ok_status(SecKeychainItemCopyAccess((SecKeychainItemRef) CFArrayGetValueAtIndex(items, 0), &newRef), "%s:SecKeychainItemCopyAccess", name);
    // TODO: should probably check this AccessRef object to make sure it's simple

    checkN(name, createQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
}
#define testExportImportKeyPairTests (newKeychainTests + checkNTests + checkNTests + makeKeyPairTests \
        + checkNTests + checkHashesMatchTests \
        + checkNTests + checkHashesMatchTests \
        + 5 + checkNTests + 1 + checkNTests \
        + 2 + checkNTests + 1 + checkNTests + 1 + 1 + 1 + checkNTests\
        + 1)

#pragma clang diagnostic pop

#else

#endif /* TARGET_OS_MAC */


#endif /* kc_30_xara_key_helpers_h */
