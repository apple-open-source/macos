/*
 * Copyright (c) 2006-2010 Apple Inc. All Rights Reserved.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecBase.h>
#include <utilities/array_size.h>
#include <stdlib.h>
#include <unistd.h>

#include "Security_regressions.h"

/* Test basic add delete update copy matching stuff. */
static void tests(void)
{
    const char *v_data = "test";
    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)v_data, strlen(v_data));
    const void *keys[] = {
		kSecClass,
		kSecAttrAccount,
		kSecAttrService,
		kSecValueData
    };
    const void *values[] = {
		kSecClassGenericPassword,
		CFSTR("dankeen"),
		CFSTR("iTools"),
		pwdata
    };
    CFDictionaryRef item = CFDictionaryCreate(NULL, keys, values,
	array_size(keys), NULL, NULL);
    ok_status(SecItemAdd(item, NULL), "add generic password");

    CFTypeRef results = NULL;
    /* Create a dict with all attrs except the data. */
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values,
	(array_size(keys)) - 1, NULL, NULL);
    ok_status(SecItemCopyMatching(query, &results), "find generic password");
    if (results) {
        CFRelease(results);
        results = NULL;
    }

    /* Modify the data of the item. */
    const char *v_data2 = "different password data this time";
    CFDataRef pwdata2 = CFDataCreate(NULL, (UInt8 *)v_data2, strlen(v_data2));
    CFMutableDictionaryRef changes = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionarySetValue(changes, kSecValueData, pwdata2);
    ok_status(SecItemUpdate(query, changes), "update generic password");

    CFDictionarySetValue(changes, kSecValueData, NULL);
    is_status(SecItemUpdate(query, changes), errSecParam, "update NULL data");

    /* This seems to be what we were seeing in
       <rdar://problem/6940041> 466 Crashes in securityd: kc_encrypt_data (SecItemServer.c:971).
       which is now fixed. */
    CFDictionarySetValue(changes, kSecValueData, CFSTR("bogus"));
    is_status(SecItemUpdate(query, changes), errSecParam, "update string data");

    ok_status(SecItemDelete(query), "delete generic password");

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

    if (pwdata) {
        CFRelease(pwdata);
        pwdata = NULL;
    }

    if (pwdata2) {
        CFRelease(pwdata2);
        pwdata = NULL;
    }
}

int si_11_update_data(int argc, char *const *argv)
{
	plan_tests(6);

	tests();

	return 0;
}
