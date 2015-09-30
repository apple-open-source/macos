/*
 * Copyright (c) 2006-2010,2014 Apple Inc. All Rights Reserved.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecItem.h>
#include <Security/SecBase.h>
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>
#include <stdlib.h>
#include <unistd.h>

#include "Security_regressions.h"

/* Test regression from <rdar://problem/16461008> REGRESSION (Security-1601?): Preferences crashes when trying to view Safari > Saved Credit Cards (SecItemCopyMatching returns empty dictionaries when kSecReturnAttributes is not true) */

static void tests(void)
{
    /*
     security item -g class=genp agrp=com.apple.safari.credit-cards sync=1
     acct       : 3B1ED3EC-A558-43F0-B337-6E891000466B
     agrp       : com.apple.safari.credit-cards
     cdat       : 2014-01-24 22:58:17 +0000
     icmt       : This keychain item is used by Safari to automatically fill credit card information in web forms.
     labl       : Safari Credit Card Entry:
     mdat       : 2014-03-31 05:22:53 +0000
     pdmn       : ak
     svce       : SafariCreditCardEntries
     sync       : 1
     tomb       : 0
     v_Data     : 62706C6973743030D30102030405065E43617264686F6C6465724E616D655F1010436172644E616D655549537472696E675A436172644E756D6265725B4D69747A2050657474656C505F101031323334353637383938373636353535080F1E313C4849000000000000010100000000000000070000000000000000000000000000005C
     
     We actually use altered data and attributes to not mess up with real CreditCard data when running tests on system
     where data exist.
     */
    static const uint8_t vdata[] = {
        0x62, 0x70, 0x6c, 0x69, 0x73, 0x74, 0x30, 0x30, 0xd3, 0x01, 0x02, 0x03,
        0x04, 0x05, 0x06, 0x5e, 0x43, 0x61, 0x72, 0x64, 0x68, 0x6f, 0x6c, 0x64,
        0x65, 0x72, 0x4e, 0x61, 0x6d, 0x65, 0x5f, 0x10, 0x10, 0x43, 0x61, 0x72,
        0x64, 0x4e, 0x61, 0x6d, 0x65, 0x55, 0x49, 0x53, 0x74, 0x72, 0x69, 0x6e,
        0x67, 0x5a, 0x43, 0x61, 0x72, 0x64, 0x4e, 0x75, 0x6d, 0x62, 0x65, 0x72,
        0x5b, 0x4d, 0x69, 0x74, 0x7a, 0x20, 0x50, 0x65, 0x74, 0x74, 0x65, 0x6c,
        0x50, 0x5f, 0x10, 0x10, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
        0x39, 0x38, 0x37, 0x36, 0x36, 0x35, 0x35, 0x35, 0x08, 0x0f, 0x1e, 0x31,
        0x3c, 0x48, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5c
    };
    CFDataRef data = CFDataCreate(NULL, vdata, sizeof(vdata));
    CFDictionaryRef item = CFDictionaryCreateForCFTypes(NULL,
                                                        kSecClass, kSecClassGenericPassword,
                                                        kSecAttrAccount, CFSTR("3B1ED3EC-A558-43F0-B337-6E891000466B-test"),
                                                        kSecAttrService, CFSTR("SafariCreditCardEntries-test"),
                                                        kSecValueData, data,
                                                        NULL);
    ok_status(SecItemAdd(item, NULL), "add generic password");

    /*
     agrp = "com.apple.safari.credit-cards";
     class = genp;
     "m_Limit" = "m_LimitAll";
     "r_Data" = 1;
     "r_PersistentRef" = 1;
     svce = SafariCreditCardEntries;
     */
    CFDictionaryRef query = CFDictionaryCreateForCFTypes(NULL,
                                                         kSecClass, kSecClassGenericPassword,
                                                         kSecAttrService, CFSTR("SafariCreditCardEntries-test"),
                                                         kSecMatchLimit, kSecMatchLimitAll,
                                                         kSecReturnData, kCFBooleanTrue,
                                                         kSecReturnPersistentRef, kCFBooleanTrue,
                                                         NULL);
    CFTypeRef result;
    ok_status(SecItemCopyMatching(query, &result), "query generic password");
    ok(isArray(result) && CFArrayGetCount(result) == 1, "return 1-sized array of results");
    CFDictionaryRef row = CFArrayGetValueAtIndex(result, 0);
    ok(isDictionary(row), "array row is dictionary");
    ok(isData(CFDictionaryGetValue(row, kSecValuePersistentRef)), "result contains valid persistentref");
    ok(isData(CFDictionaryGetValue(row, kSecValueData)), "result contains data");
    ok(CFEqual(CFDictionaryGetValue(row, kSecValueData), data), "returned data are correct");

    CFRelease(result);
    CFRelease(query);
    query = CFDictionaryCreateForCFTypes(NULL,
                                         kSecClass, kSecClassGenericPassword,
                                         kSecAttrService, CFSTR("SafariCreditCardEntries-test"),
                                         kSecMatchLimit, kSecMatchLimitAll,
                                         kSecReturnData, kCFBooleanTrue,
                                         NULL);
    ok_status(SecItemCopyMatching(query, &result), "query generic password");
    ok(isArray(result) && CFArrayGetCount(result) == 1, "return 1-sized array of results");
    row = CFArrayGetValueAtIndex(result, 0);
    ok(isData(row), "result contains data");
    ok(CFEqual(row, data), "returned data are correct");

    CFRelease(result);
    CFRelease(query);
    query = CFDictionaryCreateForCFTypes(NULL,
                                         kSecClass, kSecClassGenericPassword,
                                         kSecAttrService, CFSTR("SafariCreditCardEntries-test"),
                                         kSecMatchLimit, kSecMatchLimitAll,
                                         kSecReturnPersistentRef, kCFBooleanTrue,
                                         NULL);
    ok_status(SecItemCopyMatching(query, &result), "query generic password");
    ok(isArray(result) && CFArrayGetCount(result) == 1, "return 1-sized array of results");
    row = CFArrayGetValueAtIndex(result, 0);
    ok(isData(row), "result contains data");

    CFRelease(query);
    query = CFDictionaryCreateForCFTypes(NULL,
                                         kSecClass, kSecClassGenericPassword,
                                         kSecAttrService, CFSTR("SafariCreditCardEntries-test"),
                                         NULL);
    ok_status(SecItemDelete(query), "delete testing item");

    CFRelease(query);
    CFRelease(data);
    CFRelease(item);
}

int si_78_query_attrs(int argc, char *const *argv)
{
    plan_tests(15);

    tests();

    return 0;
}
