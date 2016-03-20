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

#ifndef kc_30_xara_key_helpers_h
#define kc_30_xara_key_helpers_h

#if TARGET_OS_MAC

static CFMutableDictionaryRef makeBaseKeyDictionary() {
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassKey);
    return query;
}

static CFMutableDictionaryRef makeQueryKeyDictionary(SecKeychainRef kc, CFStringRef keyClass) {
    CFMutableDictionaryRef query = makeBaseKeyDictionary();

    CFMutableArrayRef searchList = (CFMutableArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue((CFMutableArrayRef)searchList, kc);
    CFDictionarySetValue(query, kSecMatchSearchList, searchList);

    CFDictionarySetValue(query, kSecAttrKeyClass, keyClass);

    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    return query;
}

static CFMutableDictionaryRef makeAddKeyDictionary(SecKeychainRef kc, CFStringRef keyClass, CFStringRef label) {
    CFMutableDictionaryRef query = makeBaseKeyDictionary();
    CFDictionaryAddValue(query, kSecUseKeychain, kc);

    CFDictionarySetValue(query, kSecAttrLabel, label);
    CFDictionarySetValue(query, kSecAttrApplicationLabel, CFSTR("test_application")); // without setting this, it uses the current datetime.

    int32_t n = 0;
    if(CFEqual(keyClass, kSecAttrKeyClassSymmetric)) {
        CFDictionarySetValue(query, kSecAttrKeyType, kSecAttrKeyTypeAES);
        n = 128;
    } else if(CFEqual(keyClass, kSecAttrKeyClassPublic) ||
              CFEqual(keyClass, kSecAttrKeyClassPrivate)) {
        CFDictionarySetValue(query, kSecAttrKeyType, kSecAttrKeyTypeRSA);
        n = 1024;
    }
    CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &n);
    CFDictionarySetValue(query, kSecAttrKeySizeInBits, num);

    return query;
}

static SecKeyRef makeCustomKey(const char* name, SecKeychainRef kc, CFStringRef label) {
    CFMutableDictionaryRef query = makeAddKeyDictionary(kc, kSecAttrKeyClassSymmetric, label);

    CFErrorRef error = NULL;
    SecKeyRef item = SecKeyGenerateSymmetric(query, &error);
    ok(item != NULL, "%s: SecKeyGenerateSymmetric errored: %ld", name, error ? CFErrorGetCode(error) : -1);

    CFReleaseNull(query);
    return item;
}
#define makeCustomKeyTests 1

static SecKeyRef makeKey(const char* name, SecKeychainRef kc) {
    return makeCustomKey(name, kc, CFSTR("test_key"));
}
#define makeKeyTests makeCustomKeyTests


static void makeCustomKeyWithIntegrity(const char* name, SecKeychainRef kc, CFStringRef label, CFStringRef expectedHash) {
    SecKeyRef key = makeCustomKey(name, kc, label);
    checkIntegrityHash(name, (SecKeychainItemRef) key, expectedHash);
    CFReleaseNull(key);
 }
#define makeCustomKeyWithIntegrityTests (makeCustomKeyTests + checkIntegrityHashTests)

static void makeKeyWithIntegrity(const char* name, SecKeychainRef kc, CFStringRef expectedHash) {
    makeCustomKeyWithIntegrity(name, kc, CFSTR("test_key"), expectedHash);
 }
#define makeKeyWithIntegrityTests makeCustomKeyWithIntegrityTests

static void makeCustomKeyPair(const char* name, SecKeychainRef kc, CFStringRef label, SecKeyRef* aPub, SecKeyRef* aPriv) {
    CFMutableDictionaryRef query = makeAddKeyDictionary(kc, kSecAttrKeyClassPublic, label);

    CFErrorRef error = NULL;
    SecKeyRef pub;
    SecKeyRef priv;
    ok_status(SecKeyGeneratePair(query, &pub, &priv), "%s: SecKeyGeneratePair returned a result", name);

    if(aPub) {
        *aPub = pub;
    }
    if(aPriv) {
        *aPriv = priv;
    }

    CFReleaseNull(query);
}
#define makeCustomKeyPairTests 1

static void makeKeyPair(const char* name, SecKeychainRef kc, SecKeyRef* aPub, SecKeyRef* aPriv) {
    makeCustomKeyPair(name, kc, CFSTR("test_key"), aPub, aPriv);
}
#define makeKeyPairTests makeCustomKeyPairTests

// Note that this is nearly useless, as key pairs will never have stable hashes
static void makeKeyPairWithIntegrity(const char* name, SecKeychainRef kc, CFStringRef expectedPubHash, CFStringRef expectedPrivHash) {
    SecKeyRef pub;
    SecKeyRef priv;
    makeKeyPair(name, kc, &pub, &priv);

    checkIntegrityHash(name, (SecKeychainItemRef) pub, expectedPubHash);
    checkIntegrityHash(name, (SecKeychainItemRef) priv, expectedPrivHash);
}
#define makeKeyPairWithIntegrityTests (makeKeyTests + checkIntegrityHashTests)

// This only works for symmetric keys; key pairs cannot ever generate a duplicate (due to setting kSecKeyLabel to the hash of the public key)
static void makeCustomDuplicateKey(const char* name, SecKeychainRef kc, CFStringRef label) {
    CFMutableDictionaryRef query;

    query = makeAddKeyDictionary(kc, kSecAttrKeyClassSymmetric, label);
    CFErrorRef error = NULL;
    SecKeyRef item = SecKeyGenerateSymmetric(query, &error);
    is(CFErrorGetCode(error), errSecDuplicateItem, "%s: SecKeyGenerateSymmetric (duplicate) errored: %ld", name, error ? CFErrorGetCode(error) : -1);

    CFReleaseNull(query);
}
#define makeCustomDuplicateKeyTests 1

static void makeDuplicateKey(const char* name, SecKeychainRef kc) {
    makeCustomDuplicateKey(name, kc, CFSTR("test_key"));
}
#define makeDuplicateKeyTests makeCustomDuplicateKeyTests

static SecKeyRef makeCustomFreeKey(const char* name, SecKeychainRef kc, CFStringRef label) {
    SecKeyRef symkey;

    ok_status(SecKeyGenerate(
                NULL,
                CSSM_ALGID_AES, 128,
                0, /* contextHandle */
                CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
                CSSM_KEYATTR_EXTRACTABLE,
                NULL, /* initialAccess */
                &symkey), "%s: SecKeyGenerate", name);;

    CFMutableDictionaryRef query = makeAddKeyDictionary(kc, kSecAttrKeyClassSymmetric, label);

    CFMutableArrayRef itemList = (CFMutableArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue((CFMutableArrayRef)itemList, symkey);

    CFDictionarySetValue(query, kSecUseItemList, itemList);

    CFTypeRef result = NULL;
    ok_status(SecItemAdd(query, &result), "%s: SecItemAdd", name);
    ok(result != NULL, "%s: SecItemAdd returned a result", name);
    CFReleaseNull(symkey);
    return (SecKeyRef) result;
}
#define makeCustomFreeKeyTests 3

static SecKeyRef makeFreeKey(const char* name, SecKeychainRef kc) {
    return makeCustomFreeKey(name, kc, CFSTR("test_free_key"));
}
#define makeFreeKeyTests makeCustomFreeKeyTests

static SecKeyRef makeCustomDuplicateFreeKey(const char* name, SecKeychainRef kc, CFStringRef label) {
    SecKeyRef symkey;

    ok_status(SecKeyGenerate(
                NULL,
                CSSM_ALGID_AES, 128,
                0, /* contextHandle */
                CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
                CSSM_KEYATTR_EXTRACTABLE,
                NULL, /* initialAccess */
                &symkey), "%s: SecKeyGenerate", name);;

    CFMutableDictionaryRef query = makeAddKeyDictionary(kc, kSecAttrKeyClassSymmetric, label);

    CFMutableArrayRef itemList = (CFMutableArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue((CFMutableArrayRef)itemList, symkey);

    CFDictionarySetValue(query, kSecUseItemList, itemList);

    CFTypeRef result = NULL;
    is(SecItemAdd(query, &result), errSecDuplicateItem, "%s: SecItemAdd (duplicate)", name);
    CFReleaseNull(symkey);
    return (SecKeyRef) result;
}
#define makeCustomDuplicateFreeKeyTests 2

static SecKeyRef makeDuplicateFreeKey(const char* name, SecKeychainRef kc) {
    return makeCustomFreeKey(name, kc, CFSTR("test_free_key"));
}
#define makeDuplicateFreeKeyTests makeCustomDuplicateFreeKeyTests


// And now for the actual tests

static void testAddKey(CFStringRef expectedHash) {
    char* name = "testAddKey";
    SecKeychainRef kc = newKeychain(name);
    makeKeyWithIntegrity(name, kc, expectedHash);
    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
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
}
//#define testAddFreeKeyTests (newKeychainTests + makeFreeKeyTests + checkIntegrityHashTests + 1)
#define testAddFreeKeyTests 0

static void testCopyMatchingKey(CFStringRef expectedHash) {
    char* name = "testCopyMatchingKey";
    secdebugfunc("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 0);

    makeKeyWithIntegrity(name, kc, expectedHash);

    SecKeyRef item = (SecKeyRef) checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    checkIntegrityHash(name, (SecKeychainItemRef) item, expectedHash);
    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
}
#define testCopyMatchingKeyTests (newKeychainTests + checkNTests + makeKeyWithIntegrityTests + checkNTests + checkIntegrityHashTests + 1)


static void testUpdateKey(CFStringRef expectedHash, CFStringRef expectedHashAfter) {
    char * name = "testUpdateKey";
    secdebugfunc("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    makeKeyWithIntegrity(name, kc, expectedHash);
    SecKeychainItemRef item = checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    CFReleaseNull(item);

    CFStringRef label = CFSTR("a modified label");

    CFMutableDictionaryRef query = makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric);
    CFMutableDictionaryRef update = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(update, kSecAttrLabel, label);
    ok_status(SecItemUpdate(query, update), "%s: SecItemUpdate", name);

    CFReleaseNull(query);
    CFReleaseNull(update);

    // Find the item again.
    query = makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric);
    CFDictionarySetValue(query, kSecAttrLabel, label);
    item = checkN(name, query, 1);
    checkIntegrityHash(name, item, expectedHashAfter);
    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
}
#define testUpdateKeyTests (newKeychainTests + makeKeyWithIntegrityTests + checkNTests + 1 + checkNTests + checkIntegrityHashTests + 1)

// Key Pairs have non-predictable hashes, since they receive an attribute kSecKeyLabel that is
// the hash of the public key. Since the key is generated randomly, so is the label.
//
// We can't do our normal "add", "copymatching", "update" tests here, so do
// something special...

static void testKeyPair() {
    char* name = "testKeyPair";
    secdebugfunc("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 0);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 0);

    SecKeyRef pub;
    SecKeyRef priv;
    makeKeyPair(name, kc, &pub, &priv);

    // Now that we have the key pair, make sure we can pull the individual keys
    // out (and the hashes match)

    SecKeyRef item;
    item = (SecKeyRef) checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    checkHashesMatch(name, (SecKeychainItemRef)pub, (SecKeychainItemRef)item);

    item = (SecKeyRef) checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);
    checkHashesMatch(name, (SecKeychainItemRef)priv, (SecKeychainItemRef)item);

    // TODO: is there a way to test SecItemUpdate?

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
}
#define testKeyPairTests (newKeychainTests + checkNTests + checkNTests + makeKeyPairTests \
        + checkNTests + checkHashesMatchTests \
        + checkNTests + checkHashesMatchTests + 1)

static void testAddDuplicateKey(CFStringRef expectedHash) {
    char * name = "testAddDuplicateKey";
    secdebugfunc("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 0);

    makeKeyWithIntegrity(name, kc, expectedHash);

    SecKeychainItemRef item = checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);
    makeDuplicateKey(name, kc);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
}
#define testAddDuplicateKeyTests (newKeychainTests + checkNTests +makeKeyWithIntegrityTests + checkNTests + makeDuplicateKeyTests + checkNTests + 1)

static void testAddDuplicateFreeKey(CFStringRef expectedHash) {
    // Due to <rdar://problem/8431281> SecItemAdd() will not add a generated symmetric key to the keychain
    // we can't actually run this test. Code is included here as a reference.
    //char * name = "testAddDuplicateFreeKey";
    //secdebugfunc("integrity", "************************************* %s", name);
    //SecKeychainRef kc = newKeychain(name);
    //checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 0);

    //SecKeyRef key = makeFreeKey(name, kc);
    //checkIntegrityHash(name, (SecKeychainItemRef) key, expectedHash);
    //SecKeychainItemRef item = checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);

    //makeDuplicateFreeKey(name, kc);
    //checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassSymmetric), 1);

    //ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
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
    secdebugfunc("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 0);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 0);

    SecKeyRef pub;
    SecKeyRef priv;
    makeKeyPair(name, kc, &pub, &priv);

    // Now that we have the key pair, make sure we can pull the individual keys out

    SecKeyRef item;
    item = (SecKeyRef) checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);
    checkHashesMatch(name, (SecKeychainItemRef)pub, (SecKeychainItemRef)item);

    item = (SecKeyRef) checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);
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
    ok_status(SecKeychainItemDelete((SecKeychainItemRef)pub), "%s: SecKeychainDelete", name);;
    CFRelease(pub);
    pub = NULL;

    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 0);
    ok_status(SecItemImport(keyData, NULL, NULL, NULL, kSecItemPemArmour, &keyParams, kc, &items), "%s: SecItemImport", name);
    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPublic), 1);


    ok_status(SecItemExport(priv, kSecFormatPEMSequence, kSecItemPemArmour, &keyParams, &keyData), "%s: SecItemExport", name);
    ok_status(SecKeychainItemDelete((SecKeychainItemRef)priv), "%s: SecKeychainDelete", name);;
    CFRelease(priv);
    priv = NULL;

    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 0);

    ok_status(SecItemImport(keyData, NULL, NULL, NULL, kSecItemPemArmour, &keyParams, kc, &items), "%s: SecItemImport", name);

    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);

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

    checkN(name, makeQueryKeyDictionary(kc, kSecAttrKeyClassPrivate), 1);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
}
#define testExportImportKeyPairTests (newKeychainTests + checkNTests + checkNTests + makeKeyPairTests \
        + checkNTests + checkHashesMatchTests \
        + checkNTests + checkHashesMatchTests \
        + 5 + checkNTests + 1 + checkNTests \
        + 2 + checkNTests + 1 + checkNTests + 1 + 1 + 1 + checkNTests\
        + 1)


#else

#endif /* TARGET_OS_MAC */


#endif /* kc_30_xara_key_helpers_h */
