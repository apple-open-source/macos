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
#include "kc-item-helpers.h"

#ifndef kc_30_xara_item_helpers_h
#define kc_30_xara_item_helpers_h

#if TARGET_OS_MAC

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"

static void makeCustomItemWithIntegrity(const char* name, SecKeychainRef kc, CFStringRef itemclass, CFStringRef label, CFStringRef expectedHash) {
    SecKeychainItemRef item = makeItem(name, kc, itemclass, label);
    checkIntegrityHash(name, item, expectedHash);
    checkPartitionIDs(name, (SecKeychainItemRef) item, 1);

    CFReleaseNull(item);
}
#define makeCustomItemWithIntegrityTests (makeItemTests + checkIntegrityHashTests + checkPartitionIDsTests)

static void makeItemWithIntegrity(const char* name, SecKeychainRef kc, CFStringRef itemclass, CFStringRef expectedHash) {
    makeCustomItemWithIntegrity(name, kc, itemclass, CFSTR("test_label"), expectedHash);
}
#define makeItemWithIntegrityTests (makeCustomItemWithIntegrityTests)

static void testAddItem(CFStringRef itemclass, CFStringRef expectedHash) {
    char name[100];
    sprintf(name, "testAddItem[%s]", CFStringGetCStringPtr(itemclass, kCFStringEncodingUTF8));
    secnotice("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    makeItemWithIntegrity(name, kc, itemclass, expectedHash);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
}
#define testAddItemTests (newKeychainTests + makeItemWithIntegrityTests + 1)

static void testCopyMatchingItem(CFStringRef itemclass, CFStringRef expectedHash) {
    char name[100];
    sprintf(name, "testCopyMatchingItem[%s]", CFStringGetCStringPtr(itemclass, kCFStringEncodingUTF8));
    secnotice("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    makeItemWithIntegrity(name, kc, itemclass, expectedHash);

    SecKeychainItemRef item = checkNCopyFirst(name, createQueryItemDictionary(kc, itemclass), 1);
    checkIntegrityHash(name, item, expectedHash);
    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(item);
    CFReleaseNull(kc);
}
#define testCopyMatchingItemTests (newKeychainTests + makeItemWithIntegrityTests + checkNTests + checkIntegrityHashTests + 1)

static void testUpdateItem(CFStringRef itemclass, CFStringRef expectedHashOrig, CFStringRef expectedHashAfter) {
    char name[100];
    sprintf(name, "testUpdateItem[%s]", CFStringGetCStringPtr(itemclass, kCFStringEncodingUTF8));
    secnotice("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    makeItemWithIntegrity(name, kc, itemclass, expectedHashOrig);

    CFMutableDictionaryRef query = createQueryItemDictionary(kc, itemclass);
    CFMutableDictionaryRef update = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(update, kSecAttrComment, CFSTR("a modification"));
    CFDictionarySetValue(update, kSecAttrAccount, CFSTR("a account modification"));
    CFDictionarySetValue(update, kSecAttrLabel, CFSTR("a label modification"));
    ok_status(SecItemUpdate(query, update), "%s: SecItemUpdate", name);

    CFReleaseNull(update);

    SecKeychainItemRef item = checkNCopyFirst(name, createQueryItemDictionary(kc, itemclass), 1);
    checkIntegrityHash(name, item, expectedHashAfter);
    CFReleaseNull(item);

    // Check that updating data works
    update = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDataRef data = CFDataCreate(NULL, (void*)"data", 4);
    CFDictionarySetValue(update, kSecValueData, data);
    CFReleaseNull(data);
    ok_status(SecItemUpdate(query, update), "%s: SecItemUpdate", name);

    item = checkNCopyFirst(name, createQueryItemDictionary(kc, itemclass), 1);
    checkIntegrityHash(name, item, expectedHashAfter);
    checkPartitionIDs(name, item, 1);

    CFReleaseNull(query);
    CFReleaseNull(update);
    CFReleaseNull(item);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
}
#define testUpdateItemTests (newKeychainTests + makeItemWithIntegrityTests \
        + 1 + checkNTests + checkIntegrityHashTests \
        + 1 + checkNTests + checkIntegrityHashTests + checkPartitionIDsTests \
        + 1)

static void testAddDuplicateItem(CFStringRef itemclass, CFStringRef expectedHash) {
    char name[100];
    sprintf(name, "testAddDuplicateItem[%s]", CFStringGetCStringPtr(itemclass, kCFStringEncodingUTF8));
    secnotice("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    makeItemWithIntegrity(name, kc, itemclass, expectedHash);

    makeDuplicateItem(name, kc, itemclass);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
}
#define testAddDuplicateItemTests (newKeychainTests + makeItemWithIntegrityTests + makeDuplicateItemTests + 1)

static void testDeleteItem(CFStringRef itemclass, CFStringRef expectedHash) {
    char name[100];
    sprintf(name, "testDeleteItem[%s]", CFStringGetCStringPtr(itemclass, kCFStringEncodingUTF8));
    secnotice("integrity", "************************************* %s", name);

    SecKeychainRef kc = newKeychain(name);
    makeItemWithIntegrity(name, kc, itemclass, expectedHash);

    SecKeychainItemRef item = checkNCopyFirst(name, createQueryItemDictionary(kc, itemclass), 1);
    checkIntegrityHash(name, item, expectedHash);

    ok_status(SecKeychainItemDelete(item), "%s: SecKeychainItemDelete", name);
    checkN(name, createQueryItemDictionary(kc, itemclass), 0);
    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
    CFReleaseNull(item);
}
#define testDeleteItemTests (newKeychainTests + makeItemWithIntegrityTests + checkNTests + checkIntegrityHashTests + 1 + checkNTests + 1)

static void writeEmptyV512Keychain(const char* name, const char* keychainFile);

// This test is to find <rdar://problem/23515265> CrashTracer: accountsd at …curity: Security::KeychainCore::CCallbackMgr::consume + 387
//
// The issue was that items could remain in the Keychain cache, even after the
// actual object was freed. The main path involved updating an item so that it
// had the same primary key as an item which was in the cache but not in the
// database (this could be caused by another process deleting the item and us
// not receiving the notification).
//
// This test should pass. Failure is shown by crashing.
//
static void testUpdateRetainedItem(CFStringRef itemclass) {
    char name[100];
    sprintf(name, "testUpdateRetainedItem[%s]", CFStringGetCStringPtr(itemclass, kCFStringEncodingUTF8));
    secnotice("integrity", "************************************* %s", name);

    writeEmptyV512Keychain(name, keychainDbFile);
    SecKeychainRef kc = openCustomKeychain(name, keychainName, "password");

    SecKeychainItemRef item = createCustomItem(name, kc, createAddCustomItemDictionary(kc, itemclass, CFSTR("test_label"), CFSTR("account1")));

    checkN(name, createQueryCustomItemDictionary(kc, itemclass, CFSTR("test_label")), 1);

    cmp_ok(CFGetRetainCount(item), >=, 1, "%s: CFGetRetainCount(item)", name);

    // Bump our local database version number a few times, so we'll re-read the database when we reset it later
    CFReleaseSafe(createCustomItem(name, kc, createAddCustomItemDictionary(kc, itemclass, CFSTR("version"), CFSTR("version"))));
    CFReleaseSafe(createCustomItem(name, kc, createAddCustomItemDictionary(kc, itemclass, CFSTR("bump"), CFSTR("bump"))));

    // Simulate another process deleting the items we just made, and us not receiving the notification
    writeEmptyV512Keychain(name, keychainDbFile);

    // Generate some keychain notifications on a different keychain so the AppleDatabase will reload test.keychain
    SecKeychainRef kc2 = newCustomKeychain(name, "unrelated.keychain", "password");
    CFReleaseSafe(createCustomItem(name, kc2, createAddCustomItemDictionary(kc, itemclass, CFSTR("unrelated1_label"), CFSTR("unrelated1"))));
    ok_status(SecKeychainDelete(kc2), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc2);

    secnotice("integrity", "************************************* should reload database\n");

    SecKeychainItemRef item2 = createCustomItem(name, kc, createAddCustomItemDictionary(kc, itemclass, CFSTR("not_a_test_label"), CFSTR("account2")));
    checkN(name, createQueryCustomItemDictionary(kc, itemclass, CFSTR("not_a_test_label")), 1);
    cmp_ok(CFGetRetainCount(item2), >=, 1, "%s: CFGetRetainCount(item2)", name);

    // Now, update the second item so it would collide with the first
    CFMutableDictionaryRef query = createQueryCustomItemDictionary(kc, itemclass, CFSTR("not_a_test_label"));
    CFMutableDictionaryRef update = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(update, kSecAttrAccount, CFSTR("account1"));
    CFDictionarySetValue(update, kSecAttrLabel, CFSTR("test_label"));
    ok_status(SecItemUpdate(query, update), "%s: SecItemUpdate", name);

    cmp_ok(CFGetRetainCount(item), >=, 1, "%s: CFGetRetainCount(item)", name);
    CFReleaseNull(item);

    checkN(name, createQueryCustomItemDictionary(kc, itemclass, CFSTR("test_label")), 1);
    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", name);
    CFReleaseNull(kc);
}
#define testUpdateRetainedItemTests (openCustomKeychainTests + createCustomItemTests + checkNTests \
        + 1 + createCustomItemTests + createCustomItemTests \
        + newCustomKeychainTests + createCustomItemTests + 1 \
        + createCustomItemTests + checkNTests + 1 \
        + 1 + 1 + checkNTests + 1)

#pragma clang diagnostic pop
#else

#endif /* TARGET_OS_MAC */


#endif /* kc_30_xara_item_helpers_h */
