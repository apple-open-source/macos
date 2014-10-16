/*
 *  si-43-persistent.c
 *  Security
 *
 *  Copyright (c) 2008-2010,2012-2013 Apple Inc. All Rights Reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecInternal.h>
#include <Security/SecItemPriv.h>
#include <utilities/array_size.h>
#include <stdlib.h>
#include <unistd.h>

#include "Security_regressions.h"

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
		kSecReturnPersistentRef,
		kSecValueData
    };
    const void *values[] = {
		kSecClassInternetPassword,
		CFSTR("zuigt.nl"),
		CFSTR("frtnbf"),
		eighty,
		CFSTR("http"),
		CFSTR("dflt"),
		kCFBooleanTrue,
		pwdata
    };
    CFDictionaryRef item = CFDictionaryCreate(NULL, keys, values,
        array_size(keys), NULL, NULL);

    CFTypeRef persist = NULL;
    ok_status(SecItemAdd(item, &persist), "add internet password");
    ok(persist, "got back persistent ref");

    /* Create a dict with all attrs except the data. */
    keys[(array_size(keys)) - 2] = kSecReturnAttributes;
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values,
        (array_size(keys)) - 1, NULL, NULL);
    CFTypeRef results = NULL;
    ok_status(SecItemCopyMatching(query, &results), "find internet password by attr");

    const void *keys_persist[] = {
		kSecReturnAttributes,
		kSecValuePersistentRef
    };
    const void *values_persist[] = {
        kCFBooleanTrue,
        persist
    };
    CFDictionaryRef query2 = CFDictionaryCreate(NULL, keys_persist, values_persist,
	(array_size(keys_persist)), NULL, NULL);
    CFTypeRef results2 = NULL;
    ok_status(SecItemCopyMatching(query2, &results2), "find internet password by persistent ref");
    ok(CFEqual(results, results2 ? results2 : CFSTR("")), "same item (attributes)");

    CFReleaseNull(results);
    CFReleaseNull(results2);

    ok_status(SecItemDelete(query), "delete internet password");

    ok_status(!SecItemCopyMatching(query, &results),
        "don't find internet password by attributes");
    ok(!results, "no results");

    /* clean up left over from aborted run */
    if (results) {
        CFDictionaryRef cleanup = CFDictionaryCreate(NULL, &kSecValuePersistentRef,
            &results, 1, NULL, NULL);
        SecItemDelete(cleanup);
        CFRelease(results);
        CFRelease(cleanup);
    }

    ok_status(!SecItemCopyMatching(query2, &results2),
        "don't find internet password by persistent ref anymore");
    ok(!results2, "no results");

    CFReleaseNull(persist);

    /* Add a new item and get its persistent ref. */
    ok_status(SecItemAdd(item, &persist), "add internet password");
    CFTypeRef persist2 = NULL;
    CFMutableDictionaryRef item2 = CFDictionaryCreateMutableCopy(NULL, 0, item);
    CFDictionarySetValue(item2, kSecAttrAccount, CFSTR("johndoe"));
    ok_status(SecItemAdd(item2, &persist2), "add second internet password");
    is(CFGetTypeID(persist), CFDataGetTypeID(), "result is a CFData");
    CFMutableDictionaryRef query3 = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(query3, kSecValuePersistentRef, persist);
    CFMutableDictionaryRef update = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(update, kSecAttrServer, CFSTR("zuigt.com"));
    ok_status(SecItemUpdate(query3, update), "update via persitant ref");

    /* Verify that the update really worked. */
    CFDictionaryAddValue(query3, kSecReturnAttributes, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(query3, &results2), "find updated internet password by persistent ref");
    CFStringRef server = CFDictionaryGetValue(results2, kSecAttrServer);
    ok(CFEqual(server, CFSTR("zuigt.com")), "verify attribute was modified by update");
    CFReleaseNull(results2);
    CFDictionaryRemoveValue(query3, kSecReturnAttributes);

    /* Verify that item2 wasn't affected by the update. */
    CFMutableDictionaryRef query4 = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(query4, kSecValuePersistentRef, persist2);
    CFDictionaryAddValue(query4, kSecReturnAttributes, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(query4, &results2), "find non updated internet password by persistent ref");
    server = CFDictionaryGetValue(results2, kSecAttrServer);
    ok(CFEqual(server, CFSTR("zuigt.nl")), "verify second items attribute was not modified by update");
    CFReleaseNull(results2);

    /* Delete the item via persitant ref. */
    ok_status(SecItemDelete(query3), "delete via persitant ref");
    is_status(SecItemCopyMatching(query3, &results2), errSecItemNotFound,
        "don't find deleted internet password by persistent ref");
    CFReleaseNull(results2);
    ok_status(SecItemCopyMatching(query4, &results2),
        "find non deleted internet password by persistent ref");
    CFReleaseNull(results2);
    ok_status(SecItemDelete(query4),
              "delete internet password by persistent ref");

    CFRelease(query);
    CFRelease(query2);
    CFRelease(query3);
    CFRelease(query4);
    CFRelease(update);
    CFReleaseNull(item);
    CFReleaseNull(item2);
    CFReleaseNull(eighty);
    CFReleaseNull(pwdata);
    CFReleaseNull(persist);
    CFReleaseNull(persist2);
}

int si_43_persistent(int argc, char *const *argv)
{
    plan_tests(22);


    tests();

    return 0;
}
