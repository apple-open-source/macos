/*
 * Copyright (c) 2006-2010,2012-2013 Apple Inc. All Rights Reserved.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecItem.h>
#include <Security/SecBase.h>
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>
#include <stdlib.h>
#include <unistd.h>

#include "Security_regressions.h"

// TODO: Test whether limit's works.

/* Test basic add delete update copy matching stuff. */
static void tests(void)
{
    int v_eighty = 80;
    CFNumberRef eighty = CFNumberCreate(NULL, kCFNumberSInt32Type, &v_eighty);
    const char *v_data = "test";
    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)v_data, strlen(v_data));
    const void *keys[] = {
		kSecClass,
		kSecAttrServer,
		kSecAttrAccount,
		kSecAttrPort,
		kSecAttrProtocol,
		kSecAttrAuthenticationType,
		kSecValueData
    };
    const void *values[] = {
		kSecClassInternetPassword,
		CFSTR("members.spamcop.net"),
		CFSTR("smith"),
		eighty,
		CFSTR("http"),
		CFSTR("dflt"),
		pwdata
    };
    CFDictionaryRef item = CFDictionaryCreate(NULL, keys, values,
	array_size(keys), NULL, NULL);
    ok_status(SecItemAdd(item, NULL), "add internet password");
    is_status(SecItemAdd(item, NULL), errSecDuplicateItem,
	"add internet password again");

    CFTypeRef results = NULL;
    /* Create a dict with all attrs except the data. */
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values,
	(array_size(keys)) - 1, NULL, NULL);
    ok_status(SecItemCopyMatching(query, &results), "find internet password");
    CFReleaseNull(results);

    CFMutableDictionaryRef query2 = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, query);
    CFDictionarySetValue(query2, kSecReturnAttributes, kCFBooleanTrue);
    CFDictionarySetValue(query2, kSecMatchLimit , kSecMatchLimitOne);
    ok_status(SecItemCopyMatching(query2, &results), "find internet password, return attributes");
    CFReleaseNull(query2);
    query2 = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, results);
    CFDictionarySetValue(query2, kSecClass, kSecClassInternetPassword);
    CFDictionarySetValue(query2, kSecReturnData, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(query2, &results), "find internet password using returned attributes");
    CFReleaseSafe(query2);
    ok(isData(results) && CFEqual(results, pwdata), "retrieved data correctly");
    CFReleaseNull(results);

    /* Modify the server attr of the item. */
    const void *ch_keys[] = {
        kSecAttrServer,
		kSecClass,
    };
    const void *ch_values[] = {
		CFSTR("www.spamcop.net"),
		kSecClassInternetPassword,
    };
    CFDictionaryRef changes = CFDictionaryCreate(NULL, ch_keys, ch_values,
		1, NULL, NULL);
    ok_status(SecItemUpdate(query, changes), "update internet password");

    is_status(SecItemUpdate(query, changes), errSecItemNotFound,
		"update non-exisiting internet password again");

    /* Delete the original item (which should fail since we modified it). */
    is_status(SecItemDelete(query), errSecItemNotFound,
		"delete non existing internet password");
    ok_status(SecItemAdd(item, NULL), "add unmodified password");
    ok_status(SecItemDelete(query), "delete internet password");

    ok_status(SecItemAdd(item, NULL), "add unmodified password");
    is_status(SecItemUpdate(query, changes), errSecDuplicateItem,
		"update internet password causing dupe");
    ok_status(SecItemDelete(query), "delete internet password");

    CFDictionaryRef changed_item = CFDictionaryCreate(NULL, ch_keys, ch_values,
		array_size(ch_keys), NULL, NULL);
    ok_status(SecItemDelete(changed_item), "delete changed internet password");

	if (changed_item) {
		CFRelease(changed_item);
		changed_item = NULL;
	}

    if (changes) {
        CFRelease(changes);
        changes = NULL;
    }

    if (item) {
        CFRelease(item);
        item = NULL;
    }

    if (query) {
        CFRelease(query);
        query = NULL;
    }

    if (eighty) {
        CFRelease(eighty);
        eighty = NULL;
    }
    if (pwdata) {
        CFRelease(pwdata);
        pwdata = NULL;
    }
}

int si_10_find_internet(int argc, char *const *argv)
{
	plan_tests(15);

	tests();

	return 0;
}
