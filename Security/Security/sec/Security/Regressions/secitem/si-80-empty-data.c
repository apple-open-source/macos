/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecItem.h>
#include <Security/SecBase.h>
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>
#include <stdlib.h>
#include <unistd.h>

#include "Security_regressions.h"

static void tests(void)
{
    CFDictionaryRef item = CFDictionaryCreateForCFTypes(NULL,
                                                        kSecClass, kSecClassGenericPassword,
                                                        kSecAttrAccount, CFSTR("empty-data-account-test"),
                                                        kSecAttrService, CFSTR("empty-data-svce-test"),
                                                        NULL);
    ok_status(SecItemAdd(item, NULL), "add generic password");

    CFDictionaryRef query = CFDictionaryCreateForCFTypes(NULL,
                                                         kSecClass, kSecClassGenericPassword,
                                                         kSecAttrService, CFSTR("empty-data-svce-test"),
                                                         kSecMatchLimit, kSecMatchLimitAll,
                                                         kSecReturnData, kCFBooleanTrue,
                                                         kSecReturnAttributes, kCFBooleanTrue,
                                                         NULL);
    CFTypeRef result;
    ok_status(SecItemCopyMatching(query, &result), "query generic password");
    ok(isArray(result) && CFArrayGetCount(result) == 1, "return 1-sized array of results");
    CFDictionaryRef row = CFArrayGetValueAtIndex(result, 0);
    ok(isDictionary(row), "array row is dictionary");
    ok(CFDictionaryGetValue(row, kSecValueData) == NULL, "result contains no data");
    ok(CFEqual(CFDictionaryGetValue(row, kSecAttrService), CFSTR("empty-data-svce-test")), "svce attribute is returned");
    ok(CFEqual(CFDictionaryGetValue(row, kSecAttrAccount), CFSTR("empty-data-account-test")), "account attribute is returned");

    CFRelease(result);
    CFRelease(query);
    query = CFDictionaryCreateForCFTypes(NULL,
                                         kSecClass, kSecClassGenericPassword,
                                         kSecAttrService, CFSTR("empty-data-svce-test"),
                                         NULL);
    ok_status(SecItemDelete(query), "delete testing item");

    CFRelease(query);
    CFRelease(item);
}

int si_80_empty_data(int argc, char *const *argv)
{
    plan_tests(8);

    tests();

    return 0;
}
