/*
 * Copyright (c) 2006-2007,2009-2010,2012-2013 Apple Inc. All Rights Reserved.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecItem.h>
#include <Security/SecBase.h>
#include <utilities/array_size.h>
#include <stdlib.h>
#include <unistd.h>

#include "Security_regressions.h"

/* Test basic add delete update copy matching stuff. */
static void tests(void)
{
#ifndef NO_SERVER
    plan_skip_all("No testing against server.");
#else
    const void *keys[] = {
        kSecClass,
    };
    const void *values[] = {
        kSecClassInternetPassword,
    };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values,
    array_size(keys), NULL, NULL);
    CFTypeRef results = NULL;
    is_status(SecItemCopyMatching(query, &results), errSecItemNotFound,
    "find nothing");
    is(results, NULL, "results still NULL?");
    if (results) {
        CFRelease(results);
        results = NULL;
    }

    if (query) {
        CFRelease(query);
        query = NULL;
    }
#endif
}

int si_00_find_nothing(int argc, char *const *argv)
{
    plan_tests(2);
	tests();

	return 0;
}
