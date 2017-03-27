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
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef kc_helpers_h
#define kc_helpers_h

#include <stdlib.h>
#include <unistd.h>

#include <Security/Security.h>
#include <Security/SecKeychainPriv.h>
#include "utilities/SecCFRelease.h"

#include "kc-keychain-file-helpers.h"

/* redefine this since the headers are mixed up */
static inline bool CFEqualSafe(CFTypeRef left, CFTypeRef right)
{
    if (left == NULL || right == NULL)
        return left == right;
    else
        return CFEqual(left, right);
}

static char keychainFile[1000];
static char keychainDbFile[1000];
static char keychainTempFile[1000];
static char keychainName[1000];
static char testName[1000];
static uint32_t promptAttempts;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"

static void startTest(const char* thisTestName) {
    strlcpy(testName, thisTestName, sizeof(testName));
}

static void initializeKeychainTests(const char* thisTestName) {
    const char *home_dir = getenv("HOME");
    sprintf(keychainName, "test-%s.asdf", thisTestName);
    sprintf(keychainFile, "%s/Library/Keychains/%s", home_dir, keychainName);
    sprintf(keychainDbFile, "%s/Library/Keychains/%s-db", home_dir, keychainName);
    sprintf(keychainTempFile, "%s/Library/Keychains/test_temp", home_dir);

    deleteKeychainFiles(keychainFile);

    startTest(thisTestName);

    SecKeychainGetUserPromptAttempts(&promptAttempts);
    SecKeychainSetUserInteractionAllowed(FALSE);
}

// Use this at the bottom of every test to make sure everything is gone
static void deleteTestFiles() {
    deleteKeychainFiles(keychainFile);
}

static SecKeychainRef CF_RETURNS_RETAINED getPopulatedTestKeychain() {
    deleteKeychainFiles(keychainFile);

    writeFile(keychainFile, test_keychain, sizeof(test_keychain));

    SecKeychainRef kc = NULL;
    ok_status(SecKeychainOpen(keychainFile, &kc), "%s: getPopulatedTestKeychain: SecKeychainOpen", testName);
    ok_status(SecKeychainUnlock(kc, (UInt32) strlen(test_keychain_password), test_keychain_password, true), "%s: getPopulatedTestKeychain: SecKeychainUnlock", testName);
    return kc;
}
#define getPopulatedTestKeychainTests 2

static SecKeychainRef CF_RETURNS_RETAINED getEmptyTestKeychain() {
    deleteKeychainFiles(keychainFile);

    SecKeychainRef kc = NULL;
    ok_status(SecKeychainCreate(keychainFile, (UInt32) strlen(test_keychain_password), test_keychain_password, false, NULL, &kc), "%s: getPopulatedTestKeychain: SecKeychainCreate", testName);
    return kc;
}
#define getEmptyTestKeychainTests 1


static void addToSearchList(SecKeychainRef keychain) {
    CFArrayRef searchList = NULL;
    SecKeychainCopySearchList(&searchList);
    CFMutableArrayRef mutableSearchList = CFArrayCreateMutableCopy(NULL, CFArrayGetCount(searchList) + 1, searchList);
    CFArrayAppendValue(mutableSearchList, keychain);
    SecKeychainSetSearchList(mutableSearchList);
    CFRelease(searchList);
    CFRelease(mutableSearchList);
}


/* Checks to be sure there are N elements in this search, and returns the first
 * if it exists. */
static SecKeychainItemRef checkNCopyFirst(char* testName, const CFDictionaryRef CF_CONSUMED query, uint32_t n) {
    CFArrayRef results = NULL;
    if(n > 0) {
        ok_status(SecItemCopyMatching(query, (CFTypeRef*) &results), "%s: SecItemCopyMatching", testName);
    } else {
        is(SecItemCopyMatching(query, (CFTypeRef*) &results), errSecItemNotFound, "%s: SecItemCopyMatching (for no items)", testName);
    }

    SecKeychainItemRef item = NULL;
    if(results) {
        is(CFArrayGetCount(results), n, "%s: Wrong number of results", testName);
        if(n >= 1) {
            ok(item = (SecKeychainItemRef) CFArrayGetValueAtIndex(results, 0), "%s: Couldn't get item", testName);
        } else {
            pass("make test numbers match");
        }
    } else if((!results) && n == 0) {
        pass("%s: no results found (and none expected)", testName);
        pass("make test numbers match");
    } else {
        fail("%s: no results found (and %d expected)", testName, n);
        fflush(stdout); CFShow(query); fflush(stdout);
        pass("make test numbers match");
    }

    CFRetainSafe(item);
    CFReleaseNull(results);

    CFRelease(query);
    return item;
}

static void checkN(char* testName, const CFDictionaryRef CF_CONSUMED query, uint32_t n) {
    SecKeychainItemRef item = checkNCopyFirst(testName, query, n);
    CFReleaseSafe(item);
}

#define checkNTests 3


static void readPasswordContentsWithResult(SecKeychainItemRef item, OSStatus expectedResult, CFStringRef expectedContents) {
    if(!item) {
        fail("no item passed to readPasswordContentsWithResult");
        fail("Match test numbers");
        fail("Match test numbers");
        return;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);

    CFMutableArrayRef itemList = (CFMutableArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue((CFMutableArrayRef)itemList, item);
    CFDictionarySetValue(query, kSecUseItemList, itemList);

    CFTypeRef results = NULL;
    if(expectedContents) {
        is(SecItemCopyMatching(query, (CFTypeRef*) &results), expectedResult, "%s: readPasswordContents: SecItemCopyMatching", testName);
        CFReleaseNull(query);

        if(results) {
            ok(CFGetTypeID(results) == CFDataGetTypeID(), "%s: result is not a data", testName);

            CFDataRef data = (CFDataRef) results;
            CFStringRef str = CFStringCreateWithBytes(NULL, CFDataGetBytePtr(data), CFDataGetLength(data), kCFStringEncodingUTF8, false);
            eq_cf(str, expectedContents, "%s: contents do not match", testName);
            CFReleaseNull(str);
            CFReleaseNull(results);
        } else {
            fail("Didn't get any results");
            fail("Match test numbers");
        }
    } else {
        is(SecItemCopyMatching(query, (CFTypeRef*) &results), expectedResult, "%s: readPasswordContents: expecting error %d", testName, (int) expectedResult);
        pass("Match test numbers");
        pass("Match test numbers");
    }
}
#define readPasswordContentsWithResultTests 3

static void readPasswordContents(SecKeychainItemRef item, CFStringRef expectedContents) {
    return readPasswordContentsWithResult(item, errSecSuccess, expectedContents);
}
#define readPasswordContentsTests readPasswordContentsWithResultTests

static void changePasswordContents(SecKeychainItemRef item, CFStringRef newPassword) {
    if(!item) {
        fail("no item passed to changePasswordContents");
        return;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

    CFMutableArrayRef itemList = (CFMutableArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue((CFMutableArrayRef)itemList, item);
    CFDictionarySetValue(query, kSecUseItemList, itemList);

    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDataRef data = CFDataCreate(NULL, (const UInt8*) CFStringGetCStringPtr(newPassword, kCFStringEncodingUTF8), CFStringGetLength(newPassword));
    CFDictionarySetValue(attrs, kSecValueData, data);
    CFReleaseNull(data);

    ok_status(SecItemUpdate(query, attrs), "%s: SecItemUpdate", testName);
}
#define changePasswordContentsTests 1

static void deleteItem(SecKeychainItemRef item) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFMutableArrayRef itemList = (CFMutableArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue((CFMutableArrayRef)itemList, item);
    CFDictionarySetValue(query, kSecUseItemList, itemList);

    ok_status(SecItemDelete(query), "%s: SecItemDelete single item", testName);
    CFReleaseNull(query);
}
#define deleteItemTests 1

static void deleteItems(CFArrayRef items) {
    if(!items) {
        fail("no items passed to deleteItems");
        return;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecUseItemList, items);

    size_t count = (size_t) CFArrayGetCount(items);
    if(count > 0) {
        ok_status(SecItemDelete(query), "%s: SecItemDelete %ld items", testName, count);
    } else {
        is(SecItemDelete(query), errSecItemNotFound, "%s: SecItemDelete no items", testName);
    }
    CFReleaseNull(query);
}
#define deleteItemsTests 1

/* Checks in with securityd to see how many prompts were generated since the last call to this function, and tests against the number expected.
 Returns the number generated since the last call. */
static uint32_t checkPrompts(uint32_t expectedSinceLastCall, char* explanation) {
    uint32_t currentPrompts = UINT_MAX;
    uint32_t newPrompts = UINT_MAX;
    ok_status(SecKeychainGetUserPromptAttempts(&currentPrompts), "%s: SecKeychainGetUserPromptAttempts", testName);

    newPrompts = currentPrompts - promptAttempts;

    is(newPrompts, expectedSinceLastCall, "%s: wrong number of prompts: %s", testName, explanation);
    promptAttempts = currentPrompts;

    return newPrompts;
}
#define checkPromptsTests 2

#pragma clang diagnostic pop

#endif /* kc_helpers_h */
