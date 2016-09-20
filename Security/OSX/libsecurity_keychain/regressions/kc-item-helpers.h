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

#include "kc-helpers.h"
#include "utilities/SecCFRelease.h"

#ifndef kc_item_helpers_h
#define kc_item_helpers_h

#if TARGET_OS_MAC

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"

static CFMutableDictionaryRef makeBaseDictionary(CFStringRef itemclass) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(query, kSecReturnRef, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecClass, itemclass);

    return query;
}

static CFMutableDictionaryRef convertToQuery(CFMutableDictionaryRef query, SecKeychainRef kc) {
    CFMutableArrayRef searchList = (CFMutableArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue((CFMutableArrayRef)searchList, kc);
    CFDictionarySetValue(query, kSecMatchSearchList, searchList);

    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);

    return query;
}

static CFMutableDictionaryRef addLabel(CFMutableDictionaryRef query, CFStringRef label) {
    CFDictionarySetValue(query, kSecAttrLabel, label);
    return query;
}

static CFMutableDictionaryRef makeBaseItemDictionary(CFStringRef itemclass, CFStringRef service) {
    CFMutableDictionaryRef query = makeBaseDictionary(itemclass);

    if(CFEqual(itemclass, kSecClassInternetPassword)) {
        CFDictionarySetValue(query, kSecAttrServer, service == NULL ? CFSTR("test_service") : service);
        CFDictionarySetValue(query, kSecAttrAuthenticationType, CFSTR("dflt")); // Default, I guess?
    } else {
        // Generic passwords have services
        CFDictionarySetValue(query, kSecAttrService, service == NULL ? CFSTR("test_service") : service);
    }
    return query;
}

static CFMutableDictionaryRef makeQueryItemDictionaryWithService(SecKeychainRef kc, CFStringRef itemclass, CFStringRef service) {
    return convertToQuery(makeBaseItemDictionary(itemclass, service), kc);
}
static CFMutableDictionaryRef makeQueryItemDictionary(SecKeychainRef kc, CFStringRef itemclass) {
    return makeQueryItemDictionaryWithService(kc, itemclass, NULL);
}

static CFMutableDictionaryRef makeBaseQueryDictionary(SecKeychainRef kc, CFStringRef itemclass) {
    return convertToQuery(makeBaseDictionary(itemclass), kc);
}

static CFMutableDictionaryRef makeQueryCustomItemDictionaryWithService(SecKeychainRef kc, CFStringRef itemclass, CFStringRef label, CFStringRef service) {
    CFMutableDictionaryRef query = makeQueryItemDictionaryWithService(kc, itemclass, service);
    CFDictionarySetValue(query, kSecAttrLabel, label);
    return query;
}
static CFMutableDictionaryRef makeQueryCustomItemDictionary(SecKeychainRef kc, CFStringRef itemclass, CFStringRef label) {
    return makeQueryCustomItemDictionaryWithService(kc, itemclass, label, NULL);
}

static CFMutableDictionaryRef makeAddCustomItemDictionaryWithService(SecKeychainRef kc, CFStringRef itemclass, CFStringRef label, CFStringRef account, CFStringRef service) {
    CFMutableDictionaryRef query = makeBaseItemDictionary(itemclass, service);

    CFDictionaryAddValue(query, kSecUseKeychain, kc);
    CFDictionarySetValue(query, kSecAttrAccount, account);
    CFDictionarySetValue(query, kSecAttrComment, CFSTR("a comment"));
    CFDictionarySetValue(query, kSecAttrLabel, label);
    CFDictionarySetValue(query, kSecValueData, CFDataCreate(NULL, (void*)"data", 4));
    return query;
}
static CFMutableDictionaryRef makeAddCustomItemDictionary(SecKeychainRef kc, CFStringRef itemclass, CFStringRef label, CFStringRef account) {
    return makeAddCustomItemDictionaryWithService(kc, itemclass, label, account, NULL);
}

static CFMutableDictionaryRef makeAddItemDictionary(SecKeychainRef kc, CFStringRef itemclass, CFStringRef label) {
    return makeAddCustomItemDictionary(kc, itemclass, label, CFSTR("test_account"));
}

static SecKeychainItemRef makeCustomItem(const char* name, SecKeychainRef kc, CFDictionaryRef addDictionary) {
    CFTypeRef result = NULL;
    ok_status(SecItemAdd(addDictionary, &result), "%s: SecItemAdd", name);
    ok(result != NULL, "%s: SecItemAdd returned a result", name);

    SecKeychainItemRef item = (SecKeychainItemRef) result;
    ok(item != NULL, "%s: Couldn't convert into SecKeychainItemRef", name);

    return item;
}
#define makeCustomItemTests 3

static SecKeychainItemRef makeItem(const char* name, SecKeychainRef kc, CFStringRef itemclass, CFStringRef label) {
    CFMutableDictionaryRef query = makeAddItemDictionary(kc, itemclass, label);

    SecKeychainItemRef item = makeCustomItem(name, kc, query);

    CFReleaseNull(query);
    return item;
}
#define makeItemTests makeCustomItemTests

static void makeCustomDuplicateItem(const char* name, SecKeychainRef kc, CFStringRef itemclass, CFStringRef label) {
    CFMutableDictionaryRef query = makeAddItemDictionary(kc, itemclass, label);

    CFTypeRef result = NULL;
    is(SecItemAdd(query, &result), errSecDuplicateItem, "%s: SecItemAdd (duplicate)", name);

    CFReleaseNull(query);
}
#define makeCustomDuplicateItemTests 1

static void makeDuplicateItem(const char* name, SecKeychainRef kc, CFStringRef itemclass) {
    return makeCustomDuplicateItem(name, kc, itemclass, CFSTR("test_label"));
}
#define makeDuplicateItemTests makeCustomDuplicateItemTests


#pragma clang pop
#else

#endif /* TARGET_OS_MAC */

#endif /* kc_item_helpers_h */
