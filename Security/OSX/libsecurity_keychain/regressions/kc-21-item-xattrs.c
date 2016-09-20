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
#include "keychain_regressions.h"
#include "kc-helpers.h"
#include "kc-keychain-file-helpers.h"
#include "test/testenv.h"
//
//  testKeychainXattrs.c
//
//  Basic test of SecKeychainItemExtendedAttributes functionality
//  to store arbitrary data in the extended attributes of a keychain
//  item.
//

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <Security/Security.h>
#include <Security/SecKeychainItemExtendedAttributes.h> /* private */

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>
#include <sys/param.h>

static int TestAddItems(SecKeychainRef keychain)
{
	int result = 0;
    OSStatus status;
    SecKeychainItemRef item = NULL;
    CFDataRef blob = NULL;

    /* add generic password item */
	status = SecKeychainAddGenericPassword(keychain,
                                           strlen("Test Cloud Service 42"), "Test Cloud Service 42",
                                           strlen("nobody"), "nobody",
                                           strlen("weakpass"), "weakpass",
                                           &item);
    ok_status(status, "%s: SecKeychainAddGenericPassword", testName);

	if (status && status != errSecDuplicateItem) { // ignore error if duplicate
		result++;
	}
    /* add an extended CFDataRef attribute to this item */
    UInt8 buf1[6] = { 's', 'e', 'c', 'r', 'e', 't' };
    blob = CFDataCreate(NULL, buf1, sizeof(buf1));
    status = SecKeychainItemSetExtendedAttribute(item, CFSTR("CloudyGoodness"), blob);
    ok_status(status, "%s: SecKeychainItemSetExtendedAttribute (generic)", testName);

    if (status) {
		result++;
    }
    if (blob) {
        CFRelease(blob);
        blob = NULL;
    }
    if (item) {
        CFRelease(item);
        item = NULL;
    }

    /* add internet password item */
	status = SecKeychainAddInternetPassword(keychain,
                                            strlen("test42.icloud.com"), "test42.icloud.com",
                                            0, NULL,
                                            strlen("nobody"), "nobody",
                                            0, NULL,
                                            80, kSecProtocolTypeHTTP, kSecAuthenticationTypeDefault,
                                            strlen("weakpass"), "weakpass",
                                            &item);
    ok_status(status, "%s: SecKeychainAddInternetPassword", testName);
	if (status && status != errSecDuplicateItem) { // ignore error if duplicate
		result++;
	}
    /* add an extended CFDataRef attribute to this item */
    UInt8 buf2[5] = { 'm', 'a', 'g', 'i', 'c' };
    blob = CFDataCreate(NULL, buf2, sizeof(buf2));
    status = SecKeychainItemSetExtendedAttribute(item, CFSTR("CloudyGoodness"), blob);
    ok_status(status, "%s: SecKeychainItemSetExtendedAttribute (internet)", testName);

    if (status) {
		result++;
    }
    if (blob) {
        CFRelease(blob);
        blob = NULL;
    }
    if (item) {
        CFRelease(item);
        item = NULL;
    }


	return result;
}

static int TestFindItems(SecKeychainRef keychain)
{
	int result = 0;

    CFMutableArrayRef searchList = (CFMutableArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue((CFMutableArrayRef)searchList, keychain);

    /* find generic password we added previously */
    {
        const void *keys[] = {
            kSecMatchSearchList,
            kSecClass,
            kSecAttrAccount,
            kSecAttrService,
            kSecMatchLimit,
            kSecReturnRef
        };
        const void *values[] = {
            searchList,
            kSecClassGenericPassword,
            CFSTR("nobody"),
            CFSTR("Test Cloud Service 42"),
            kSecMatchLimitOne,
            kCFBooleanTrue
        };

        OSStatus status = noErr;
        CFTypeRef results = NULL;
        CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values,
                                                   sizeof(keys) / sizeof(*keys),
                                                   &kCFTypeDictionaryKeyCallBacks,
                                                   &kCFTypeDictionaryValueCallBacks);

        status = SecItemCopyMatching(query, &results);
        ok_status(status, "%s: SecItemCopyMatching (generic password)", testName);

        if (status) {
            fprintf(stderr, "Unable to find \"Test Cloud Service 42\" generic password: error %d\n", (int)status);
            result++;
        }
        if (results) {
            /* found the item; since we asked for one item and a ref, this is a SecKeychainItemRef */
            SecKeychainItemRef item = (SecKeychainItemRef) results;
            CFDataRef blob = NULL;
            status = SecKeychainItemCopyExtendedAttribute(item, CFSTR("CloudyGoodness"), &blob);
            ok_status(status, "%s: SecKeychainItemCopyExtendedAttribute", testName);

            if (status) {
                fprintf(stderr, "Unable to retrieve xattr from \"Test Cloud Service 42\" generic password: error %d\n", (int)status);
                result++;
            }
            else {
                const UInt8 *dataPtr = CFDataGetBytePtr(blob);

                eq_stringn( (const char *) dataPtr, strlen((const char *)dataPtr), "secret", strlen("secret"), "%s: Retrieved xattr value matches expected value", testName);
                if (memcmp(dataPtr, "secret", strlen("secret"))) {
                    result++;
                }
            }
            if (blob) {
                CFRelease(blob);
            }
            CFRelease(results);
        }
        if (query) {
            CFRelease(query);
        }
    }

    /* find internet password we added previously */
    {
        const void *keys[] = {
            kSecMatchSearchList,
            kSecClass,
            kSecAttrAccount,
            kSecAttrServer,
            kSecMatchLimit,
            kSecReturnRef
        };
        const void *values[] = {
            searchList,
            kSecClassInternetPassword,
            CFSTR("nobody"),
            CFSTR("test42.icloud.com"),
            kSecMatchLimitOne,
            kCFBooleanTrue
        };

        OSStatus status = noErr;
        CFTypeRef results = NULL;
        CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values,
                                                   sizeof(keys) / sizeof(*keys),
                                                   &kCFTypeDictionaryKeyCallBacks,
                                                   &kCFTypeDictionaryValueCallBacks);

        status = SecItemCopyMatching(query, &results);
        ok_status(status, "%s: SecItemCopyMatching (internet password)", testName);
        if (status) {
            fprintf(stderr, "Unable to find \"test42.icloud.com\" internet password: error %d\n", (int)status);
            result++;
        }
        if (results) {
            /* found the item; since we asked for one item and a ref, this is a SecKeychainItemRef */
            SecKeychainItemRef item = (SecKeychainItemRef) results;
            CFDataRef blob = NULL;
            status = SecKeychainItemCopyExtendedAttribute(item, CFSTR("CloudyGoodness"), &blob);
            ok_status(status, "%s: SecKeychainItemCopyExtendedAttribute", testName);
            if (status) {
                fprintf(stderr, "Unable to retrieve xattr from \"test42.icloud.com2\" internet password: error %d\n", (int)status);
                result++;
            }
            else {
                const UInt8 *dataPtr = CFDataGetBytePtr(blob);
                eq_stringn( (const char *) dataPtr, strlen((const char *)dataPtr), "magic", strlen("magic"), "%s: Retrieved xattr value matches expected value", testName);

                if (memcmp(dataPtr, "magic", strlen("magic"))) {
                    fprintf(stderr, "Retrieved xattr value did not match expected value!\n");
                    result++;
                }
            }
            if (blob) {
                CFRelease(blob);
            }
            CFRelease(results);
        }
        if (query) {
            CFRelease(query);
        }
    }

	return result;
}

static int TestDeleteItems(SecKeychainRef keychain)
{
	int result = 0;

    CFMutableArrayRef searchList = (CFMutableArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue((CFMutableArrayRef)searchList, keychain);

    /* find generic password we added previously */
    {
        const void *keys[] = {
            kSecMatchSearchList,
            kSecClass,
            kSecAttrAccount,
            kSecAttrService,
            kSecMatchLimit,
            kSecReturnRef
        };
        const void *values[] = {
            searchList,
            kSecClassGenericPassword,
            CFSTR("nobody"),
            CFSTR("Test Cloud Service 42"),
            kSecMatchLimitOne,
            kCFBooleanTrue
        };

        OSStatus status = noErr;
        CFTypeRef results = NULL;
        CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values,
                                                   sizeof(keys) / sizeof(*keys),
                                                   &kCFTypeDictionaryKeyCallBacks,
                                                   &kCFTypeDictionaryValueCallBacks);

        status = SecItemCopyMatching(query, &results);
        ok_status(status, "%s: SecItemCopyMatching (Test Cloud Service 42)", testName);

        if (status) {
            fprintf(stderr, "Unable to find \"Test Cloud Service 42\" generic password: error %d\n", (int)status);
            result++;
        }
        if (results) {
            /* found the item; since we asked for one item and a ref, this is a SecKeychainItemRef */
            SecKeychainItemRef item = (SecKeychainItemRef) results;

            /* set the xattr to NULL in order to delete it */
            status = SecKeychainItemSetExtendedAttribute(item, CFSTR("CloudyGoodness"), NULL);
            ok_status( status, "%s: SecKeychainItemSetExtendedAttribute (generic password, null data)", testName);

            if (status) {
                fprintf(stderr, "Unable to remove xattr from \"Test Cloud Service 42\" generic password: error %d\n", (int)status);
                result++;
            }

            /* delete the item itself */
            status = SecKeychainItemDelete(item);
            ok_status(status, "%s: SecKeychainItemDelete (generic password)", testName);

            if (status) {
                fprintf(stderr, "Unable to delete \"Test Cloud Service 42\" generic password: error %d\n", (int)status);
                result++;
            }

            CFRelease(results);
        }
        if (query) {
            CFRelease(query);
        }
    }

    /* find internet password we added previously */
    {
        const void *keys[] = {
            kSecMatchSearchList,
            kSecClass,
            kSecAttrAccount,
            kSecAttrServer,
            kSecMatchLimit,
            kSecReturnRef
        };
        const void *values[] = {
            searchList,
            kSecClassInternetPassword,
            CFSTR("nobody"),
            CFSTR("test42.icloud.com"),
            kSecMatchLimitOne,
            kCFBooleanTrue
        };

        OSStatus status = noErr;
        CFTypeRef results = NULL;
        CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values,
                                                   sizeof(keys) / sizeof(*keys),
                                                   &kCFTypeDictionaryKeyCallBacks,
                                                   &kCFTypeDictionaryValueCallBacks);

        status = SecItemCopyMatching(query, &results);
        ok_status(status, "%s: SecItemCopyMatching (test42.icloud.com)", testName);

        if (status) {
            fprintf(stderr, "Unable to find \"test42.icloud.com\" internet password: error %d\n", (int)status);
            result++;
        }
        if (results) {
            /* found the item; since we asked for one item and a ref, this is a SecKeychainItemRef */
            SecKeychainItemRef item = (SecKeychainItemRef) results;

            /* set the xattr to NULL in order to delete it */
            status = SecKeychainItemSetExtendedAttribute(item, CFSTR("CloudyGoodness"), NULL);
            ok_status( status, "%s: SecKeychainItemSetExtendedAttribute (internet password, null data)", testName);

            if (status) {
                fprintf(stderr, "Unable to remove xattr from \"test42.icloud.com2\" internet password: error %d\n", (int)status);
                result++;
            }

            /* delete the item itself */
            status = SecKeychainItemDelete(item);
            ok_status(status, "%s: SecKeychainItemDelete (generic password)", testName);

            if (status) {
                fprintf(stderr, "Unable to delete \"test42.icloud.com2\" internet password: error %d\n", (int)status);
                result++;
            }

            CFRelease(results);
        }
        if (query) {
            CFRelease(query);
        }
    }

	return result;
}

int kc_21_item_xattrs(int argc, char *const *argv)
{
    plan_tests(21);
    initializeKeychainTests(__FUNCTION__);

    SecKeychainRef keychain = getPopulatedTestKeychain();

    TestAddItems(keychain);
    TestFindItems(keychain);
    TestDeleteItems(keychain);

    ok_status(SecKeychainDelete(keychain), "%s: SecKeychainDelete", testName);
    CFReleaseNull(keychain);
    checkPrompts(0, "No prompts");

    deleteTestFiles();
    return 0;
}
