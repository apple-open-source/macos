/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecFramework.h>
#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecECKey.h>
#include <Security/SecAccessControl.h>
#include <Security/SecAccessControlPriv.h>
#include <Security/SecInternal.h>
#include <Security/SecCertificatePriv.h>
#include <utilities/SecFileLocations.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFRelease.h>
#include <utilities/der_plist.h>
#include <Security/SecBase64.h>

#include <os/feature_private.h>

#include <libaks_acl_cf_keys.h>

#include <ctkclient/ctkclient_test.h>
#include <coreauthd_spi.h>

#include "secd_regressions.h"

#include "SecdTestKeychainUtilities.h"
#include "SecKeybagSupport.h"

const char *cert1 = "MIIFQzCCBCugAwIBAgIBAjANBgkqhkiG9w0BAQsFADAyMQswCQYDVQQGEwJVUzENMAsGA1UEChMETklTVDEUMBIGA1UEAxMLUElWIFRlc3QgQ0EwHhcNMTUwOTE2MDAwMDAwWhcNMTYwOTE2MjM1OTU5WjBlMQswCQYDVQQGEwJVUzEbMBkGA1UEChMST2JlcnR1clRlY2hub2xvZ2llMRowGAYDVQQLExFJZGVudGl0eSBEaXZpc2lvbjEdMBsGA1UEAxMUSUQtT25lIFBJViBUZXN0IENhcmQwgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBAN8DrET5AAQ4dVIP+RD3XATFaBYpG9b2H0tV82gVGOv/t5cxOszAMxzsw7xlY/tMRrx5yz7IUUvueylHl98e7yMefP69vwqwSc4DWSELSqHOLMHd/uPLYLINIFqEW8Nq4Q02V2IxqBbiwtZeeSOqY3gQ2kiCd4cF8Itlr3UePJrlAgMBAAGjggKzMIICrzAfBgNVHSMEGDAWgBTr2hnSCEKN9N4lh2nJu6sM05YwATApBgNVHQ4EIgQg5YNVxRTOC13qs9cVUuvDIp6AH+jitdjhWJfai2bfP3QwDgYDVR0PAQH/BAQDAgeAMBYGA1UdJQEB/wQMMAoGCGCGSAFlAwYIMBcGA1UdIAQQMA4wDAYKYIZIAWUDAgEDETCBtAYDVR0fBIGsMIGpMIGmoIGjoIGghkRodHRwOi8vZmljdGl0aW91cy5uaXN0Lmdvdi9maWN0aXRpb3VzQ1JMZGlyZWN0b3J5L2ZpY3RpdGlvdXNDUkwxLmNybIZYbGRhcDovL3NtaW1lMi5uaXN0Lmdvdi9jbj1Hb29kJTIwQ0Esbz1UZXN0JTIwQ2VydGlmaWNhdGVzLGM9VVM/Y2VydGlmaWNhdGVSZXZvY2F0aW9uTGlzdDCCASEGCCsGAQUFBwEBBIIBEzCCAQ8wPgYIKwYBBQUHMAGGMmh0dHA6Ly9maWN0aXRpb3VzLm5pc3QuZ292L2ZpY3RpdGlvdXNPQ1NQTG9jYXRpb24vMF4GCCsGAQUFBzAChlJodHRwOi8vZmljdGl0aW91cy5uaXN0Lmdvdi9maWN0aXRpb3VzQ2VydHNPbmx5Q01TZGlyZWN0b3J5L2NlcnRzSXNzdWVkVG9Hb29kQ0EucDdjMG0GCCsGAQUFBzAChmFsZGFwOi8vc21pbWUyLm5pc3QuZ292L2NuPUdvb2QlMjBDQSxvPVRlc3QlMjBDZXJ0aWZpY2F0ZXMsYz1VUz9jQUNlcnRpZmljYXRlLGNyb3NzQ2VydGlmaWNhdGVQYWlyMDIGA1UdEQQrMCmgJwYIYIZIAWUDBgagGwQZ1Oc52nOc7TnOc52haFoIySreCmGE5znD4jAQBglghkgBZQMGCQEEAwEBADANBgkqhkiG9w0BAQsFAAOCAQEAVVGMeep+1wpVFdXFIXUTkxy9RjdOO3SmMGVomfVXofVOBfVzooaI+RV5UCURnoqoHYziBidxc9YKW6n9mX6p27KfrC1roHg6wu5xVEHJ93hju35g3WAXTnqNFiQpB+GU7UvJJEhkcTU2rChuYNS5SeFZ0pv1Gyzw7WjLfh9rdAPBfRg4gxpho9SMCUnI+p5KbEiptmimtPfsVq6htT3P+m2V4UXIT6sr7T6IpnPteMppsH43NKXNM6iPCkRCUPQ0d+lpfXAYGSFIzx2WesjSmrs/CHXfwmhnbrJNPCx9zlcCMmmfGcZGyufF+10wF9gv9qx+PUwi2xMKhwuKR1LoCg==";
const char *cert2 =
"MIIFCTCCA/GgAwIBAgIBAzANBgkqhkiG9w0BAQsFADAyMQswCQYDVQQGEwJVUzENMAsGA1UEChMETklTVDEUMBIGA1UEAxMLUElWIFRlc3QgQ0EwHhcNMTUwOTE2MDAwMDAwWhcNMTYwOTE2MjM1OTU5WjBlMQswCQYDVQQGEwJVUzEbMBkGA1UEChMST2JlcnR1clRlY2hub2xvZ2llMRowGAYDVQQLExFJZGVudGl0eSBEaXZpc2lvbjEdMBsGA1UEAxMUSUQtT25lIFBJViBUZXN0IENhcmQwgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBAKij0LIQlW0VKahBGF4tu/xdwGWN+KTLLGyMQmuuG+NNG+vQMSsdXD1pd00YMBiGn3sC5b+G7lQLZ85mDQfO+eI8GDjG+Sh8W8Cghku20sxZnQ+kZOLOr//R2/ZXonVaxoBR/9tBPh0MIEIVzRS8JmltZVfhkbIR6Wiox3jVEAsPAgMBAAGjggJ5MIICdTAfBgNVHSMEGDAWgBTr2hnSCEKN9N4lh2nJu6sM05YwATApBgNVHQ4EIgQga85kaqoMEaV+E04P1gZ2OUlbCbvr623fC30WhBZn3bMwDgYDVR0PAQH/BAQDAgbAMBcGA1UdIAQQMA4wDAYKYIZIAWUDAgEDDTCBtAYDVR0fBIGsMIGpMIGmoIGjoIGghkRodHRwOi8vZmljdGl0aW91cy5uaXN0Lmdvdi9maWN0aXRpb3VzQ1JMZGlyZWN0b3J5L2ZpY3RpdGlvdXNDUkwxLmNybIZYbGRhcDovL3NtaW1lMi5uaXN0Lmdvdi9jbj1Hb29kJTIwQ0Esbz1UZXN0JTIwQ2VydGlmaWNhdGVzLGM9VVM/Y2VydGlmaWNhdGVSZXZvY2F0aW9uTGlzdDCCASEGCCsGAQUFBwEBBIIBEzCCAQ8wPgYIKwYBBQUHMAGGMmh0dHA6Ly9maWN0aXRpb3VzLm5pc3QuZ292L2ZpY3RpdGlvdXNPQ1NQTG9jYXRpb24vMF4GCCsGAQUFBzAChlJodHRwOi8vZmljdGl0aW91cy5uaXN0Lmdvdi9maWN0aXRpb3VzQ2VydHNPbmx5Q01TZGlyZWN0b3J5L2NlcnRzSXNzdWVkVG9Hb29kQ0EucDdjMG0GCCsGAQUFBzAChmFsZGFwOi8vc21pbWUyLm5pc3QuZ292L2NuPUdvb2QlMjBDQSxvPVRlc3QlMjBDZXJ0aWZpY2F0ZXMsYz1VUz9jQUNlcnRpZmljYXRlLGNyb3NzQ2VydGlmaWNhdGVQYWlyMCIGA1UdEQQbMBmBF2NvbW1vbl9uYW1lQHBpdmRlbW8ub3JnMA0GCSqGSIb3DQEBCwUAA4IBAQANg1tGsgO32fVXDyRPHFeqDa0QmQ4itHrh6BAK6n94QL8383wuPDFkPy1TfVYVdYm0Gne6hyH/Z13ycw1XXNddooT7+OiYK5F1TEhfQNiRhzTqblB/yc2lv6Ho0EsOrwPhaBRaO3EFUyjeNMxsvG8Dr9Y5u2B38ESB4OsLKHq0eD/WZjEAlyGx16Qi7YlLiHGfLMorgkg9Mbp73guNO1PItDTAnqHUUOlQ01ThNug0sR5ua1zlNFx6AIPoX4yAPrtlEMZtbsevsXlgDpO1zc26p5icBmQHYT7uzdTEEN4tmcxXg6Z/dGB63GCluf+Pc+ovRt/MMt2EbcIuwJ9C516H";
const char *cert3 =
"MIICETCCAbigAwIBAgIJANiM7uTufLiKMAkGByqGSM49BAEwgZIxCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTESMBAGA1UEBxMJQ3VwZXJ0aW5vMRMwEQYDVQQKEwpBcHBsZSBJbmMuMQ8wDQYDVQQLEwZDb3JlT1MxGjAYBgNVBAMTEUFwcGxlIFRlc3QgQ0EyIEVDMSAwHgYJKoZIhvcNAQkBFhF2a3V6ZWxhQGFwcGxlLmNvbTAeFw0xNjA1MTIxMTMyMzlaFw0yNjA1MTAxMTMyMzlaMGYxEDAOBgNVBAMTB3NldG9rZW4xCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTESMBAGA1UEBxMJQ3VwZXJ0aW5vMRMwEQYDVQQKEwpBcHBsZSBJbmMuMQ8wDQYDVQQLEwZDb3JlT1MwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAQHKxfYgbqRHmThPnO9yQX5KrL/EPa6dZU52Wys5gC3/Mk0dNt9dLhpWblAVaeBzkos4juN3cxbnoB9MsC4bvoLoyMwITAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwICBDAJBgcqhkjOPQQBA0gAMEUCIQClcoYLhEA/xIGU94ZBcup26Pb7pXaWaaOM3+9z510TRwIgV/iprC051SuQzkqXA5weVliJOohFYjO+gUoH/6MJpDg=";

extern void LASetErrorCodeBlock(CFErrorRef (^newCreateErrorBlock)(void));

static void test_item_add(void) {

    static const UInt8 data[] = { 0x01, 0x02, 0x03, 0x04 };
    CFDataRef valueData = CFDataCreate(NULL, data, sizeof(data));
    __block NSUInteger objIDIdx = 0;

    CFMutableDictionaryRef attrs = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                                           kSecClass, kSecClassGenericPassword,
                                                                           kSecAttrTokenID, CFSTR("tokenid"),
                                                                           kSecAttrService, CFSTR("ctktest-service"),
                                                                           kSecValueData, valueData,
                                                                           kSecReturnAttributes, kCFBooleanTrue,
                                                                           NULL);
    // Setup token hook.
    __block int phase = 0;
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        phase++;
        eq_cf(CFDictionaryGetValue(attributes, kSecAttrTokenID), CFSTR("tokenid"));

        blocks->createOrUpdateObject = ^CFDataRef(CFDataRef objectID, CFMutableDictionaryRef at, CFErrorRef *error) {
            phase++;
            is(objectID, NULL);
            eq_cf(CFDictionaryGetValue(at, kSecClass), kSecClassGenericPassword);
            eq_cf(CFDictionaryGetValue(at, kSecAttrService), CFDictionaryGetValue(attrs, kSecAttrService));
            eq_cf(CFDictionaryGetValue(at, kSecAttrTokenID), CFSTR("tokenid"));
            eq_cf(CFDictionaryGetValue(at, kSecValueData), valueData);
            CFDictionaryRemoveValue(at, kSecValueData);
            ++objIDIdx;
            return (__bridge_retained CFDataRef)[NSData dataWithBytes:&objIDIdx length:sizeof(objIDIdx)];
        };

        blocks->copyObjectAccessControl = ^CFDataRef(CFDataRef oid, CFErrorRef *error) {
            phase++;
            SecAccessControlRef ac = SecAccessControlCreate(NULL, NULL);
            SecAccessControlSetProtection(ac, kSecAttrAccessibleAlwaysPrivate, NULL);
            test_IsTrue(SecAccessControlAddConstraintForOperation(ac, kAKSKeyOpDefaultAcl, kCFBooleanTrue, NULL));
            CFDataRef acData = SecAccessControlCopyData(ac);
            CFRelease(ac);
            return acData;
        };

        blocks->copyObjectData = ^CFTypeRef(CFDataRef oid, CFErrorRef *error) {
            phase++;
            return CFRetain(valueData);
        };
    });

    CFTypeRef result = NULL;
    ok_status(SecItemAdd(attrs, &result));
    eq_cf(CFDictionaryGetValue(result, kSecAttrService), CFSTR("ctktest-service"));
    eq_cf(CFDictionaryGetValue(result, kSecAttrTokenID), CFSTR("tokenid"));
    is(CFDictionaryGetValue(result, kSecValueData), NULL);
    CFReleaseNull(result);

    is(phase, 3);

    phase = 0;
    CFDictionarySetValue(attrs, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(attrs, kSecAttrService, CFSTR("ctktest-service1"));
    ok_status(SecItemAdd(attrs, &result));
    eq_cf(CFDictionaryGetValue(result, kSecAttrService), CFSTR("ctktest-service1"));
    eq_cf(CFDictionaryGetValue(result, kSecAttrTokenID), CFSTR("tokenid"));
    eq_cf(CFDictionaryGetValue(result, kSecValueData), valueData);
    CFReleaseNull(result);

    is(phase, 4);

    phase = 0;
    CFDictionaryRemoveValue(attrs, kSecReturnAttributes);
    CFDictionarySetValue(attrs, kSecAttrAccount, CFSTR("2nd"));
    ok_status(SecItemAdd(attrs, &result));
    eq_cf(result, valueData);
    CFReleaseNull(result);
    is(phase, 4);

    CFRelease(attrs);
    CFRelease(valueData);
}

static void test_item_query() {
    static const UInt8 data[] = { 0x01, 0x02, 0x03, 0x04 };
    CFDataRef valueData = CFDataCreate(NULL, data, sizeof(data));
    CFDataRef valueData2 = CFDataCreate(NULL, data, sizeof(data) - 1);

    __block int phase = 0;
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        phase++;
        eq_cf(CFDictionaryGetValue(attributes, kSecAttrTokenID), CFSTR("tokenid"));

        blocks->copyObjectData = ^CFTypeRef(CFDataRef oid, CFErrorRef *error) {
            phase++;
            return CFRetain(valueData);
        };
    });

    // Add non-token item with the same service, to test queries returning mixed results.
    CFMutableDictionaryRef attrs = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                                           kSecClass, kSecClassGenericPassword,
                                                                           kSecAttrService, CFSTR("ctktest-service"),
                                                                           kSecValueData, valueData2,
                                                                           NULL);
    ok_status(SecItemAdd(attrs, NULL));
    CFRelease(attrs);

    // Query with service.
    CFMutableDictionaryRef query;
    query = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                    kSecClass, kSecClassGenericPassword,
                                                    kSecAttrService, CFSTR("ctktest-service"),
                                                    kSecReturnAttributes, kCFBooleanTrue,
                                                    kSecReturnData, kCFBooleanTrue,
                                                    NULL);

    phase = 0;
    CFTypeRef result = NULL;
    ok_status(SecItemCopyMatching(query, &result));
    is(phase, 2);
    is(CFGetTypeID(result), CFDictionaryGetTypeID());
    eq_cf(CFDictionaryGetValue(result, kSecValueData), valueData);
    is(CFGetTypeID(CFDictionaryGetValue(result, kSecAttrAccessControl)), SecAccessControlGetTypeID());
    eq_cf(CFDictionaryGetValue(result, kSecAttrService), CFSTR("ctktest-service"));
    CFReleaseSafe(result);

    phase = 0;
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    ok_status(SecItemCopyMatching(query, &result));
    is(phase, 2);
    is(CFGetTypeID(result), CFArrayGetTypeID());
    is(CFArrayGetCount(result), 2);
    CFReleaseSafe(result);

    phase = 0;
    CFDictionaryRemoveValue(query, kSecMatchLimit);
    CFDictionaryRemoveValue(query, kSecReturnData);
    ok_status(SecItemCopyMatching(query, &result));
    is(phase, 0);
    is(CFGetTypeID(result), CFDictionaryGetTypeID());
    is(CFDictionaryGetValue(result, kSecValueData), NULL);
    CFReleaseSafe(result);

    phase = 0;
    CFDictionaryRemoveValue(query, kSecReturnAttributes);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecAttrTokenID, CFSTR("tokenid"));
    ok_status(SecItemCopyMatching(query, &result));
    is(phase, 2);
    eq_cf(result, valueData);
    CFReleaseSafe(result);

    static const uint8_t tk_persistent_ref_id[] = {'t', 'k', 'p', 'r'};
    NSData *persistentRefId = [NSData dataWithBytes:tk_persistent_ref_id length:sizeof(tk_persistent_ref_id)];
    phase = 0;
    ok_status(SecItemCopyMatching((__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassGenericPassword,
                                                               (id)kSecAttrService : @"ctktest-service",
                                                               (id)kSecReturnPersistentRef : @YES }, &result));
    is(phase, 0);
    NSData *persistentRef = (__bridge NSData *)result;
    is(CFEqual((__bridge CFDataRef)persistentRefId, (__bridge CFDataRef)[persistentRef subdataWithRange:NSMakeRange(0, 4)]), TRUE);
    CFReleaseSafe(result);

    phase = 0;
    ok_status(SecItemCopyMatching((__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassGenericPassword,
                                                               (id)kSecAttrService : @"ctktest-service",
                                                               (id)kSecReturnData : @YES,
                                                               (id)kSecReturnPersistentRef : @YES }, &result));
    is(phase, 2);
    persistentRef = ((__bridge NSDictionary *)result)[(id)kSecValuePersistentRef];
    is(CFEqual((__bridge CFDataRef)persistentRefId, (__bridge CFDataRef)[persistentRef subdataWithRange:NSMakeRange(0, 4)]), TRUE);
    CFReleaseSafe(result);

    phase = 0;
    ok_status(SecItemCopyMatching((__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassGenericPassword,
                                                               (id)kSecAttrService : @"ctktest-service",
                                                               (id)kSecReturnAttributes : @YES,
                                                               (id)kSecReturnPersistentRef : @YES }, &result));
    is(phase, 0);
    persistentRef = ((__bridge NSDictionary *)result)[(id)kSecValuePersistentRef];
    is(CFEqual((__bridge CFDataRef)persistentRefId, (__bridge CFDataRef)[persistentRef subdataWithRange:NSMakeRange(0, 4)]), TRUE);
    CFReleaseSafe(result);

    phase = 0;
    ok_status(SecItemCopyMatching((__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassGenericPassword,
                                                               (id)kSecAttrService : @"ctktest-service",
                                                               (id)kSecReturnData : @YES,
                                                               (id)kSecReturnAttributes : @YES,
                                                               (id)kSecReturnPersistentRef : @YES }, &result));
    is(phase, 2);
    persistentRef = ((__bridge NSDictionary *)result)[(id)kSecValuePersistentRef];
    is(CFEqual((__bridge CFDataRef)persistentRefId, (__bridge CFDataRef)[persistentRef subdataWithRange:NSMakeRange(0, 4)]), TRUE);
    CFReleaseSafe(result);

    CFRelease(query);
    CFRelease(valueData);
    CFRelease(valueData2);
}

static void test_item_update() {
    static const UInt8 data[] = { 0x01, 0x02, 0x03, 0x04 };
    CFDataRef valueData2 = CFDataCreate(NULL, data, sizeof(data) - 1);
    CFTypeRef result = NULL;

    CFMutableDictionaryRef query, attrs;

    // Setup token hook.
    __block int phase = 0;
    __block bool store_value = false;
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        phase++;
        eq_cf(CFDictionaryGetValue(attributes, kSecAttrTokenID), CFSTR("tokenid"));

        blocks->createOrUpdateObject = ^CFDataRef(CFDataRef objectID, CFMutableDictionaryRef at, CFErrorRef *error) {
            phase++;
            eq_cf(CFDictionaryGetValue(at, kSecValueData), valueData2);
            if (!store_value) {
                CFDictionaryRemoveValue(at, kSecValueData);
            }
            return CFRetainSafe(objectID);
        };

        blocks->copyObjectAccessControl = ^CFDataRef(CFDataRef oid, CFErrorRef *error) {
            phase++;
            SecAccessControlRef ac = SecAccessControlCreate(NULL, NULL);
            SecAccessControlSetProtection(ac, kSecAttrAccessibleAlwaysPrivate, NULL);
            test_IsTrue(SecAccessControlAddConstraintForOperation(ac, kAKSKeyOpDefaultAcl, kCFBooleanTrue, NULL));
            CFDataRef acData = SecAccessControlCopyData(ac);
            CFRelease(ac);
            return acData;
        };

        blocks->copyObjectData = ^CFTypeRef(CFDataRef oid, CFErrorRef *error) {
            phase++;
            return CFRetain(valueData2);
        };
    });

    query = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                    kSecClass, kSecClassGenericPassword,
                                                    kSecAttrTokenID, CFSTR("tokenid"),
                                                    kSecAttrService, CFSTR("ctktest-service"),
                                                    NULL);

    attrs = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                    kSecValueData, valueData2,
                                                    NULL);

    ok_status(SecItemUpdate(query, attrs));
    is(phase, 3);

    phase = 0;
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(query, &result));
    eq_cf(valueData2, result);
    CFRelease(result);
    is(phase, 2);

    phase = 0;
    store_value = true;
    CFDictionaryRemoveValue(query, kSecReturnData);
    ok_status(SecItemUpdate(query, attrs));
    is(phase, 3);

    phase = 0;
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(query, &result));
    eq_cf(valueData2, result);
    CFRelease(result);
    is(phase, 0);

    phase = 0;
    CFDictionarySetValue(query, kSecAttrService, CFSTR("ctktest-service1"));
    CFDictionaryRemoveValue(query, kSecReturnData);
    ok_status(SecItemUpdate(query, attrs));
    is(phase, 5);

    phase = 0;
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(query, &result));
    is(phase, 0);
    is(CFGetTypeID(result), CFArrayGetTypeID());
    is(CFArrayGetCount(result), 2);
    eq_cf(CFArrayGetValueAtIndex(result, 0), valueData2);
    eq_cf(CFArrayGetValueAtIndex(result, 1), valueData2);

    CFRelease(query);
    CFRelease(attrs);
    CFRelease(valueData2);
}

static void test_item_delete(void) {

    CFMutableDictionaryRef query;
    CFTypeRef result;

    __block int phase = 0;
    __block CFErrorRef deleteError = NULL;
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        phase++;
        eq_cf(CFDictionaryGetValue(attributes, kSecAttrTokenID), CFSTR("tokenid"));

        blocks->copyObjectAccessControl = ^CFDataRef(CFDataRef oid, CFErrorRef *error) {
            phase++;
            SecAccessControlRef ac = SecAccessControlCreate(NULL, NULL);
            SecAccessControlSetProtection(ac, kSecAttrAccessibleAlwaysPrivate, NULL);
            test_IsTrue(SecAccessControlAddConstraintForOperation(ac, kAKSKeyOpDefaultAcl, kCFBooleanTrue, NULL));
            CFDataRef acData = SecAccessControlCopyData(ac);
            CFRelease(ac);
            return acData;
        };

        blocks->deleteObject = ^bool(CFDataRef objectID, CFErrorRef *error) {
            phase++;
            if (deleteError != NULL) {
                CFAssignRetained(*error, deleteError);
                deleteError = NULL;
                return false;
            }
            return true;
        };
    });

    query = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                    kSecClass, kSecClassGenericPassword,
                                                    kSecAttrTokenID, CFSTR("tokenid"),
                                                    kSecAttrService, CFSTR("ctktest-service"),
                                                    NULL);

    phase = 0;
    ok_status(SecItemDelete(query));
    is(phase, 2);

    phase = 0;
    is_status(SecItemCopyMatching(query, &result), errSecItemNotFound);
    is(phase, 0);

    phase = 0;
    CFDictionarySetValue(query, kSecAttrService, CFSTR("ctktest-service1"));
    ok_status(SecItemCopyMatching(query, &result));
    is(phase, 0);

    phase = 0;
#if LA_CONTEXT_IMPLEMENTED
    LASetErrorCodeBlock(^{ return (CFErrorRef)NULL; });
    deleteError = CFErrorCreate(NULL, CFSTR(kTKErrorDomain), kTKErrorCodeAuthenticationNeeded, NULL);
    ok_status(SecItemDelete(query), "delete multiple token items");
    is(phase, 6, "connect + delete-auth-fail + copyAccess + connect + delete + delete-2nd");
#else
    ok_status(SecItemDelete(query), "delete multiple token items");
    is(phase, 3, "connect + delete + delete");
#endif

    phase = 0;
    is_status(SecItemCopyMatching(query, &result), errSecItemNotFound);
    is(phase, 0);

    is_status(SecItemDelete(query), errSecItemNotFound);

    CFRelease(query);
    CFReleaseSafe(deleteError);
}

static void test_key_generate(int globalPersistence, int privatePersistence, int publicPersistence,
                              bool privateIsPersistent, bool publicIsPersistent) {
    __block int phase = 0;
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        phase |= 0x01;

        blocks->createOrUpdateObject = ^CFDataRef(CFDataRef objectID, CFMutableDictionaryRef at, CFErrorRef *error) {
            id privateKey;
            CFTypeRef keyClass = CFDictionaryGetValue(at, kSecAttrKeyClass) ?: kSecAttrKeyClassPrivate;
            eq_cf(keyClass, kSecAttrKeyClassPrivate, "only private keys can be created on token");
            NSDictionary *params = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256, (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate };
            if (objectID != NULL) {
                phase |= 0x20;
                privateKey = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)objectID, (CFDictionaryRef)params, NULL));
            } else {
                phase |= 0x02;
                privateKey = CFBridgingRelease(SecKeyCreateRandomKey((__bridge CFDictionaryRef)params, error));
            }
            NSDictionary *privKeyAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)privateKey));
            CFDictionarySetValue(at, kSecClass, kSecClassKey);
            CFDictionarySetValue(at, kSecAttrKeyType, kSecAttrKeyTypeECSECPrimeRandom);
            CFDictionarySetValue(at, kSecAttrKeySizeInBits, CFSTR("256"));
            CFDictionarySetValue(at, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
            return CFBridgingRetain(privKeyAttrs[(id)kSecValueData]);
        };

        blocks->copyObjectAccessControl = ^CFDataRef(CFDataRef oid, CFErrorRef *error) {
            phase |= 0x04;
            SecAccessControlRef ac = SecAccessControlCreate(NULL, NULL);
            SecAccessControlSetProtection(ac, kSecAttrAccessibleAlwaysPrivate, NULL);
            test_IsTrue(SecAccessControlAddConstraintForOperation(ac, kAKSKeyOpDefaultAcl, kCFBooleanTrue, NULL));
            CFDataRef acData = SecAccessControlCopyData(ac);
            CFRelease(ac);
            return acData;
        };

        blocks->copyPublicKeyData = ^CFDataRef(CFDataRef objectID, CFErrorRef *error) {
            phase |= 0x08;
            SecKeyRef privKey = SecKeyCreateECPrivateKey(NULL, CFDataGetBytePtr(objectID), CFDataGetLength(objectID), kSecKeyEncodingBytes);
            CFDataRef publicData;
            SecKeyCopyPublicBytes(privKey, &publicData);
            CFRelease(privKey);
            return publicData;
        };

        blocks->copyObjectData = ^CFTypeRef(CFDataRef oid, CFErrorRef *error) {
            phase |= 0x10;
            return kCFNull;
        };
    });

    NSMutableDictionary *params = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeEC,
                                     (id)kSecAttrKeySizeInBits: @"256",
                                     (id)kSecAttrTokenID: @"tokenid",
                                     }.mutableCopy;
    if (globalPersistence >= 0) {
        params[(id)kSecAttrIsPermanent] = globalPersistence ? @YES : @NO;
    }
    if (publicPersistence >= 0) {
        params[(id)kSecPublicKeyAttrs] = @{ (id)kSecAttrIsPermanent: publicPersistence ? @YES : @NO };
    }
    if (privatePersistence >= 0) {
        params[(id)kSecPrivateKeyAttrs] = @{ (id)kSecAttrIsPermanent: privatePersistence ? @YES : @NO };
    }

    NSError *error;
    phase = 0;
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((__bridge CFDictionaryRef)params, (void *)&error));
    isnt(privateKey, nil, "failed to generate token key, error %@", error);
    is(phase, privateIsPersistent ? 0x3f : 0x1f);
    id publicKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privateKey));
    isnt(publicKey, nil, "failed to get public key from private key");

    NSDictionary *query = @{ (id)kSecValueRef: privateKey,
                             (id)kSecReturnAttributes: @YES,
                             (id)kSecReturnRef: @YES,
                             (id)kSecReturnData: @YES };
    phase = 0;
    NSDictionary *result;
    if (privateIsPersistent) {
        ok_status(SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&result), "persistent private key not found in kc");
        is(phase, 0x19);
        is(result[(id)kSecValueData], nil);
        eq_cf((__bridge CFTypeRef)result[(id)kSecAttrTokenID], @"tokenid");
        NSDictionary *keyAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)result[(id)kSecValueRef]));
        eq_cf((__bridge CFTypeRef)keyAttrs[(id)kSecAttrApplicationLabel], (__bridge CFTypeRef)result[(id)kSecAttrApplicationLabel]);
        keyAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)publicKey));
        eq_cf((__bridge CFTypeRef)keyAttrs[(id)kSecAttrApplicationLabel], (__bridge CFTypeRef)result[(id)kSecAttrApplicationLabel]);
    } else {
        is_status(SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&result), errSecItemNotFound, "ephemeral private key found in kc");
        is(phase, 0x08);

        // Balancing test count from the branch above
        ok(true);
        ok(true);
        ok(true);
        ok(true);
    }

    query = @{ (id)kSecValueRef: publicKey,
               (id)kSecReturnAttributes: @YES,
               (id)kSecReturnRef: @YES,
               (id)kSecReturnData: @YES };
    phase = 0;
    result = nil;
    if (publicIsPersistent) {
        ok_status(SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&result), "persistent public key not found in kc");
        NSDictionary *keyAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)publicKey));
        eq_cf((__bridge CFTypeRef)keyAttrs[(id)kSecAttrApplicationLabel], (__bridge CFTypeRef)result[(id)kSecAttrApplicationLabel]);
    } else {
        is_status(SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&result), errSecItemNotFound, "ephemeral public key found in kc");

        // Balancing test count from the branch above
        ok(true);
    }

    // Get OID from the private key and try to create duplicate of the key using its OID and attributes.
    NSDictionary *attrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)privateKey));
    NSData *oid = attrs[(id)kSecAttrTokenOID];
    ok(oid != nil, "private key attributes need OID");
    phase = 0;
    id copyKey = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)[NSData data],
                                                        (CFDictionaryRef)@{ (id)kSecAttrTokenID: @"tokenid", (id)kSecAttrTokenOID: oid },
                                                        (void *)&error));
    ok(copyKey != nil, "copied key is created");
    is(phase, 0x21);
    attrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)copyKey));
    is(phase, 0x29);
    phase = 0;
    eq_cf((__bridge CFTypeRef)attrs[(id)kSecClass], kSecClassKey);
    eq_cf((__bridge CFTypeRef)attrs[(id)kSecAttrKeyClass], kSecAttrKeyClassPrivate);
    eq_cf((__bridge CFTypeRef)attrs[(id)kSecAttrKeyType], kSecAttrKeyTypeECSECPrimeRandom);
    eq_cf((__bridge CFTypeRef)attrs[(id)kSecAttrKeySizeInBits], CFSTR("256"));
    eq_cf((__bridge CFTypeRef)attrs[(id)kSecAttrTokenID], CFSTR("tokenid"));
    id copyPublicKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)copyKey));
    ok(copyPublicKey != nil);
    is(phase, 0x08);
    NSDictionary *pubAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)copyPublicKey));
    eq_cf((__bridge CFTypeRef)attrs[(id)kSecAttrApplicationLabel], (__bridge CFTypeRef)pubAttrs[(id)kSecAttrApplicationLabel]);
}

static void test_key_sign(void) {

    static const UInt8 data[] = { 0x01, 0x02, 0x03, 0x04 };
    CFDataRef valueData = CFDataCreate(NULL, data, sizeof(data));

    __block int phase = 0;
    __block CFErrorRef cryptoError = NULL;
    __block SecKeyOperationType cryptoOperation = -1;
    __block SecKeyAlgorithm cryptoAlgorithm = NULL;
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        phase++;

        blocks->copyPublicKeyData = ^CFDataRef(CFDataRef objectID, CFErrorRef *error) {
            phase++;
            SecKeyRef privKey = SecKeyCreateECPrivateKey(NULL, CFDataGetBytePtr(objectID), CFDataGetLength(objectID), kSecKeyEncodingBytes);
            CFDataRef publicData;
            ok_status(SecKeyCopyPublicBytes(privKey, &publicData));
            CFRelease(privKey);
            return publicData;
        };

        blocks->copyObjectAccessControl = ^CFDataRef(CFDataRef oid, CFErrorRef *error) {
            phase++;
            SecAccessControlRef ac = SecAccessControlCreate(NULL, NULL);
            SecAccessControlSetProtection(ac, kSecAttrAccessibleAlwaysPrivate, NULL);
            test_IsTrue(SecAccessControlAddConstraintForOperation(ac, kAKSKeyOpDefaultAcl, kCFBooleanTrue, NULL));
            CFDataRef acData = SecAccessControlCopyData(ac);
            CFRelease(ac);
            return acData;
        };

        blocks->copyOperationResult = ^CFTypeRef(CFDataRef objectID, CFIndex operation, CFArrayRef algorithms, CFIndex secKeyOperationMode, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
            SecKeyAlgorithm algorithm = CFArrayGetValueAtIndex(algorithms, CFArrayGetCount(algorithms) - 1);
            phase++;
            cryptoOperation = operation;
            cryptoAlgorithm = algorithm;
            if (cryptoError != NULL) {
                CFAssignRetained(*error, cryptoError);
                cryptoError = NULL;
                return NULL;
            }
            return CFRetainSafe(valueData);
        };

        blocks->copyObjectData = ^CFTypeRef(CFDataRef objectID, CFErrorRef *error) {
            phase++;
            return kCFNull;
        };
    });

    NSDictionary *query = @{ (id)kSecClass: (id)kSecClassKey, (id)kSecReturnRef: @YES };

    phase = 0;
    SecKeyRef privateKey = NULL;
    ok_status(SecItemCopyMatching((__bridge CFDictionaryRef)query, (CFTypeRef *)&privateKey));
    is(phase, 2);

    phase = 0;
    CFMutableDataRef sig = CFDataCreateMutable(NULL, 0);
    CFDataSetLength(sig, 256);
    size_t sigLen = CFDataGetLength(sig);
    ok_status(SecKeyRawSign(privateKey, kSecPaddingPKCS1, data, sizeof(data), CFDataGetMutableBytePtr(sig), &sigLen));
    is(phase, 1);
    is(cryptoAlgorithm, kSecKeyAlgorithmECDSASignatureDigestX962);
    is(cryptoOperation, kSecKeyOperationTypeSign);
    CFDataSetLength(sig, sigLen);
    is(CFDataGetLength(sig), CFDataGetLength(valueData));
    eq_cf(valueData, sig);

#if LA_CONTEXT_IMPLEMENTED
    phase = 0;
    CFDataSetLength(sig, 256);
    sigLen = CFDataGetLength(sig);
    LASetErrorCodeBlock(^{ return (CFErrorRef)NULL; });
    cryptoError = CFErrorCreate(NULL, CFSTR(kTKErrorDomain), kTKErrorCodeAuthenticationNeeded, NULL);
    ok_status(SecKeyRawSign(privateKey, kSecPaddingPKCS1, data, sizeof(data), CFDataGetMutableBytePtr(sig), &sigLen));
    is(phase, 4);
    is(cryptoError, NULL);
    CFDataSetLength(sig, sigLen);
    is(CFDataGetLength(sig), CFDataGetLength(valueData));
    eq_cf(valueData, sig);
#endif

    NSError *error;
    NSData *result;
    result = CFBridgingRelease(SecKeyCreateDecryptedData(privateKey, kSecKeyAlgorithmRSAEncryptionRaw,
                                                         valueData, (void *)&error));
    eq_cf((__bridge CFDataRef)result, valueData);
    is(cryptoAlgorithm, kSecKeyAlgorithmRSAEncryptionRaw);
    is(cryptoOperation, kSecKeyOperationTypeDecrypt);

    NSDictionary *params = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256 };
    SecKeyRef otherPrivKey = NULL, otherPubKey = NULL;
    ok_status(SecKeyGeneratePair((__bridge CFDictionaryRef)params, &otherPubKey, &otherPrivKey));

    error = nil;
    result = CFBridgingRelease(SecKeyCopyKeyExchangeResult(privateKey, kSecKeyAlgorithmECDHKeyExchangeCofactor,
                                                           otherPubKey, (CFDictionaryRef)@{}, (void *)&error));
    eq_cf((__bridge CFDataRef)result, valueData);
    is(cryptoAlgorithm, kSecKeyAlgorithmECDHKeyExchangeCofactor);
    is(cryptoOperation, kSecKeyOperationTypeKeyExchange);

    CFReleaseSafe(otherPrivKey);
    CFReleaseSafe(otherPubKey);
    CFReleaseSafe(cryptoError);
    CFRelease(sig);
    CFRelease(privateKey);
}

static void test_key_generate_with_params(void) {

    const UInt8 data[] = "foo";
    CFDataRef cred_ref = CFDataCreate(NULL, data, 4);
    __block int phase = 0;
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        phase++;
        eq_cf(CFDictionaryGetValue(attributes, kSecUseOperationPrompt), CFSTR("prompt"));
        is(CFDictionaryGetValue(attributes, kSecUseAuthenticationUI), NULL);
        eq_cf(CFDictionaryGetValue(attributes, kSecUseCredentialReference), cred_ref);

        blocks->createOrUpdateObject = ^CFDataRef(CFDataRef objectID, CFMutableDictionaryRef at, CFErrorRef *error) {
            phase++;
            SecCFCreateError(-4 /* kTKErrorCodeCanceledByUser */, CFSTR(kTKErrorDomain), CFSTR(""), NULL, error);
            return NULL;
        };
    });

    CFDictionaryRef prk_params = CFDictionaryCreateForCFTypes(NULL,
                                                              kSecAttrIsPermanent, kCFBooleanTrue,
                                                              NULL);

    CFMutableDictionaryRef params = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                                            kSecAttrKeyType, kSecAttrKeyTypeEC,
                                                                            kSecAttrKeySizeInBits, CFSTR("256"),
                                                                            kSecAttrTokenID, CFSTR("tokenid"),
                                                                            kSecPrivateKeyAttrs, prk_params,
                                                                            kSecUseOperationPrompt, CFSTR("prompt"),
                                                                            kSecUseAuthenticationUI, kSecUseAuthenticationUIAllow,
                                                                            kSecUseCredentialReference, cred_ref,
                                                                            NULL);
    CFRelease(prk_params);

    SecKeyRef publicKey = NULL, privateKey = NULL;
    phase = 0;
    is_status(SecKeyGeneratePair(params, &publicKey, &privateKey), errSecUserCanceled);
    is(phase, 2);

    CFReleaseSafe(publicKey);
    CFReleaseSafe(privateKey);
    CFRelease(params);
    CFRelease(cred_ref);
}

static void test_error_codes(void) {

    CFMutableDictionaryRef attrs = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                                           kSecClass, kSecClassGenericPassword,
                                                                           kSecAttrTokenID, CFSTR("tokenid"),
                                                                           NULL);
    // Setup token hook.
    __block OSStatus ctk_error = 0;
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        blocks->createOrUpdateObject = ^CFDataRef(CFDataRef objectID, CFMutableDictionaryRef at, CFErrorRef *error) {
            SecCFCreateError(ctk_error, CFSTR(kTKErrorDomain), CFSTR(""), NULL, error);
            return NULL;
        };
    });

    ctk_error = kTKErrorCodeBadParameter;
    is_status(SecItemAdd(attrs, NULL), errSecParam);

    ctk_error = kTKErrorCodeNotImplemented;
    is_status(SecItemAdd(attrs, NULL), errSecUnimplemented);

    ctk_error = kTKErrorCodeCanceledByUser;
    is_status(SecItemAdd(attrs, NULL), errSecUserCanceled);

    CFRelease(attrs);
}

static CFDataRef copy_certificate_data(const char *base64Cert)
{
    size_t size = SecBase64Decode(base64Cert, strnlen(base64Cert, 2048), NULL, 0);
    ok(size);
    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, size);
    CFDataSetLength(data, size);
    size = SecBase64Decode(base64Cert, strnlen(base64Cert, 2048), (char*)CFDataGetMutableBytePtr(data), CFDataGetLength(data));
    ok(size);
    CFDataSetLength(data, size);

    return data;
}

static CFMutableDictionaryRef copy_certificate_attributes(const char *base64Cert)
{
    CFDataRef data = copy_certificate_data(base64Cert);

    SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, data);
    ok(cert);
    CFDictionaryRef certAttributes = SecCertificateCopyAttributeDictionary(cert);
    ok(certAttributes);
    CFMutableDictionaryRef result = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, certAttributes);
    ok(result);

    if (certAttributes)
        CFRelease(certAttributes);
    if (data)
        CFRelease(data);
    if (cert)
        CFRelease(cert);

    return result;
}

static CFDictionaryRef copy_certificate_query(const char *base64cert, CFStringRef label, CFDataRef oid, CFStringRef tokenID)
{
    CFMutableDictionaryRef certAttributes = copy_certificate_attributes(base64cert);

    CFDictionarySetValue(certAttributes, kSecAttrLabel, label);
    CFDictionarySetValue(certAttributes, kSecAttrAccessible, kSecAttrAccessibleAlwaysPrivate);
    if (oid != NULL) {
        CFDictionarySetValue(certAttributes, kSecAttrTokenOID, oid);
    }
    CFDictionaryRemoveValue(certAttributes, kSecValueData);

    if (tokenID != NULL) {
        SecAccessControlRef acl = SecAccessControlCreate(kCFAllocatorDefault, NULL);
        ok(acl);
        CFTypeRef key[] = { kSecAttrTokenID };
        CFTypeRef value[] = { tokenID };
        CFDictionaryRef protection = CFDictionaryCreate(kCFAllocatorDefault, key, value, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        ok(SecAccessControlSetProtection(acl, protection, NULL));
        CFRelease(protection);
        ok(SecAccessControlAddConstraintForOperation(acl, kAKSKeyOpDefaultAcl, kCFBooleanTrue, NULL));
        CFDataRef aclData = SecAccessControlCopyData(acl);
        ok(aclData);
        if (aclData) {
            CFDictionarySetValue(certAttributes, kSecAttrAccessControl, aclData);
            CFRelease(aclData);
        }

        if (acl)
            CFRelease(acl);
    } else {
        NSData *certData = CFBridgingRelease(copy_certificate_data(base64cert));
        CFDictionarySetValue(certAttributes, kSecValueData, (__bridge CFDataRef)certData);
    }

    return certAttributes;
}

static CFDictionaryRef copy_key_query(CFDictionaryRef certAttributes, CFStringRef label, CFDataRef oid, CFStringRef tokenID)
{
    CFMutableDictionaryRef keyAttributes = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks) ;

    CFDictionarySetValue(keyAttributes, kSecClass, kSecClassKey);
    CFDictionarySetValue(keyAttributes, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
    CFDictionarySetValue(keyAttributes, kSecAttrKeyType, kSecAttrKeyTypeRSA);
    CFNumberRef keySize = CFNumberCreateWithCFIndex(kCFAllocatorDefault, 2048);
    CFDictionarySetValue(keyAttributes, kSecAttrKeySizeInBits, keySize);
    CFRelease(keySize);

    CFDictionarySetValue(keyAttributes, kSecAttrCanDecrypt, kCFBooleanTrue);
    CFDictionarySetValue(keyAttributes, kSecAttrCanSign, kCFBooleanTrue);
    CFDictionarySetValue(keyAttributes, kSecAttrCanUnwrap, kCFBooleanTrue);
    CFDictionarySetValue(keyAttributes, kSecAttrCanDerive, kCFBooleanFalse);
    CFDictionarySetValue(keyAttributes, kSecAttrIsPrivate, kCFBooleanTrue);

    CFDictionarySetValue(keyAttributes, kSecAttrLabel, label);
    CFDictionarySetValue(keyAttributes, kSecAttrAccessible, kSecAttrAccessibleAlwaysPrivate);
    CFDictionarySetValue(keyAttributes, kSecAttrTokenOID, oid);
    CFDictionarySetValue(keyAttributes, kSecAttrApplicationLabel, CFDictionaryGetValue(certAttributes, kSecAttrPublicKeyHash));

    SecAccessControlRef acl = SecAccessControlCreate(kCFAllocatorDefault, NULL);
    ok(acl);
    CFTypeRef key[] = { kSecAttrTokenID };
    CFTypeRef value[] = { tokenID };
    CFDictionaryRef protection = CFDictionaryCreate(kCFAllocatorDefault, key, value, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    ok(SecAccessControlSetProtection(acl, protection, NULL));
    CFRelease(protection);
    ok(SecAccessControlAddConstraintForOperation(acl, kAKSKeyOpDefaultAcl, kCFBooleanTrue, NULL));
    CFDataRef aclData = SecAccessControlCopyData(acl);
    ok(aclData);
    if (aclData) {
        CFDictionarySetValue(keyAttributes, kSecAttrAccessControl, aclData);
        CFRelease(aclData);
    }

    if (acl)
        CFRelease(acl);

    return keyAttributes;
}

static void check_array_for_type_id(CFArrayRef array, CFTypeID typeID)
{
    if (array && CFGetTypeID(array) == CFArrayGetTypeID()) {
        for (CFIndex i = 0; i < CFArrayGetCount(array); ++i) {
            ok(CFGetTypeID(CFArrayGetValueAtIndex(array, i)) == typeID);
        }
    }
}

static void test_propagate_token_items()
{
    if (!os_feature_enabled(CryptoTokenKit, UseTokens)) {
        // This test does not work if tokens cannot be used by keychain.
        return;
    }

    NSData *cert1OID = [@"oid1" dataUsingEncoding:NSUTF8StringEncoding];
    NSData *cert2OID = [@"oid2" dataUsingEncoding:NSUTF8StringEncoding];
    NSData *key1OID = [@"oid3" dataUsingEncoding:NSUTF8StringEncoding];
    NSData *key2OID = [@"oid4" dataUsingEncoding:NSUTF8StringEncoding];

    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        blocks->copyObjectData = ^CFTypeRef(CFDataRef oid, CFErrorRef *error) {
            if (CFEqual(oid, (__bridge CFDataRef)cert1OID)) {
                return copy_certificate_data(cert1);
            }
            else if (CFEqual(oid, (__bridge CFDataRef)cert2OID)) {
                return copy_certificate_data(cert2);
            }
            else if (CFEqual(oid, (__bridge CFDataRef)key1OID) || CFEqual(oid, (__bridge CFDataRef)key2OID)) {
                return kCFNull;
            }
            else {
                return NULL;
            }
        };
    });

    CFStringRef tokenID = CFSTR("com.apple.secdtest:propagate_test_token");

    CFMutableArrayRef items = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    CFDictionaryRef certQuery = copy_certificate_query(cert1, CFSTR("test_cert1"), (__bridge CFDataRef)cert1OID, tokenID);
    ok(certQuery);
    CFDictionaryRef keyQuery = copy_key_query(certQuery, CFSTR("test_key1"), (__bridge CFDataRef)key1OID, tokenID);
    ok(keyQuery);

    CFArrayAppendValue(items, certQuery);
    CFArrayAppendValue(items, keyQuery);
    CFReleaseSafe(certQuery);
    CFReleaseSafe(keyQuery);

    certQuery = copy_certificate_query(cert2, CFSTR("test_cert2"), (__bridge CFDataRef)cert2OID, tokenID);
    ok(certQuery);
    keyQuery = copy_key_query(certQuery, CFSTR("test_key2"), (__bridge CFDataRef)key2OID, tokenID);
    ok(keyQuery);

    CFArrayAppendValue(items, certQuery);
    CFArrayAppendValue(items, keyQuery);
    CFReleaseSafe(certQuery);
    CFReleaseSafe(keyQuery);

    OSStatus result;
    ok_status(result = SecItemUpdateTokenItemsForAccessGroups(tokenID, (__bridge CFArrayRef)@[(id)kSecAttrAccessGroupToken], NULL), "Failed to delete items.");

    ok_status(result = SecItemUpdateTokenItemsForAccessGroups(tokenID, (__bridge CFArrayRef)@[(id)kSecAttrAccessGroupToken], items), "Failed to propagate items.");
    CFRelease(items);

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassCertificate);
    CFDictionarySetValue(query, kSecAttrAccessGroup, CFSTR("com.apple.token"));
    CFDictionarySetValue(query, kSecReturnRef, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    CFTypeRef queryResult;
    ok_status(SecItemCopyMatching(query, &queryResult));
    ok(queryResult && CFGetTypeID(queryResult) == CFArrayGetTypeID() && CFArrayGetCount(queryResult) == 2, "Expect array with two certs");
    check_array_for_type_id(queryResult, SecCertificateGetTypeID());
    CFReleaseNull(queryResult);

    CFDictionarySetValue(query, kSecReturnRef, kCFBooleanFalse);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(query, &queryResult));
    ok(queryResult && CFGetTypeID(queryResult) == CFArrayGetTypeID() && CFArrayGetCount(queryResult) == 2, "Expect array with two certs");
    check_array_for_type_id(queryResult, CFDataGetTypeID());
    CFReleaseNull(queryResult);

    CFDictionarySetValue(query, kSecClass, kSecClassKey);
    CFDictionarySetValue(query, kSecReturnRef, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanFalse);
    ok_status(SecItemCopyMatching(query, &queryResult));
    ok(queryResult && CFGetTypeID(queryResult) == CFArrayGetTypeID() && CFArrayGetCount(queryResult) == 2, "Expect array with two keys");
    check_array_for_type_id(queryResult, SecKeyGetTypeID());
    CFReleaseNull(queryResult);

    CFDictionarySetValue(query, kSecReturnRef, kCFBooleanFalse);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(query, &queryResult));
    ok(queryResult && CFGetTypeID(queryResult) == CFArrayGetTypeID() && CFArrayGetCount(queryResult) == 0, "Expect empty array");
    CFReleaseNull(queryResult);

    CFDictionarySetValue(query, kSecClass, kSecClassIdentity);
    CFDictionarySetValue(query, kSecReturnRef, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanFalse);
    ok_status(SecItemCopyMatching(query, &queryResult));
    ok(queryResult && CFGetTypeID(queryResult) == CFArrayGetTypeID() && CFArrayGetCount(queryResult) == 2, "Expect array with two identities");
    check_array_for_type_id(queryResult, SecIdentityGetTypeID());
    CFReleaseNull(queryResult);

    CFDictionarySetValue(query, kSecReturnRef, kCFBooleanFalse);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(query, &queryResult));
    ok(queryResult && CFGetTypeID(queryResult) == CFArrayGetTypeID() && CFArrayGetCount(queryResult) == 0, "Expect empty array");
    CFReleaseNull(queryResult);

    ok_status(result = SecItemUpdateTokenItemsForAccessGroups(tokenID, (__bridge CFArrayRef)@[(id)kSecAttrAccessGroupToken], NULL), "Failed to delete items.");

    CFDictionarySetValue(query, kSecReturnRef, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanFalse);
    is_status(SecItemCopyMatching(query, &queryResult), errSecItemNotFound);
    CFReleaseNull(queryResult);
    CFRelease(query);
}

static void test_identity_on_two_tokens() {
    if (!os_feature_enabled(CryptoTokenKit, UseTokens)) {
        // This test does not work if tokens cannot be used by keychains.
        return;
    }

    @autoreleasepool {
        NSString *tokenID1 = @"com.apple.secdtest:identity_test_token1";
        NSString *tokenID2 = @"com.apple.secdtest:identity_test_token2";

        NSError *error;
        NSData *privKeyData = [[NSData alloc] initWithBase64EncodedString:@"BAcrF9iBupEeZOE+c73JBfkqsv8Q9rp1lTnZbKzmALf8yTR02310uGlZuUBVp4HOSiziO43dzFuegH0ywLhu+gtJj81RD8Rt+nLR6oTARkL+0l2/fzrIouleaEYpYmEp0A==" options:NSDataBase64DecodingIgnoreUnknownCharacters];
        id privKey = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)privKeyData, (CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate}, (void *)&error));
        id publicKey = CFBridgingRelease(SecKeyCopyPublicKey((__bridge SecKeyRef)privKey));
        NSData *pubKeyHash = CFBridgingRelease(SecKeyCopyPublicKeyHash((__bridge SecKeyRef)publicKey));

        NSData *cert3OID = [@"oid1" dataUsingEncoding:NSUTF8StringEncoding];
        TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {

            blocks->copyObjectData = ^CFTypeRef(CFDataRef oid, CFErrorRef *error) {
                if (CFEqual(oid, (__bridge CFDataRef)cert3OID))
                    return copy_certificate_data(cert3);
                else
                    return kCFNull;
            };

            blocks->copyPublicKeyData = ^CFDataRef(CFDataRef oid, CFErrorRef *error) {
                if ([privKeyData isEqualToData:(__bridge NSData *)oid]) {
                    return SecKeyCopyExternalRepresentation((SecKeyRef)publicKey, error);
                }
                return NULL;
            };

        });

        id ac = CFBridgingRelease(SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenUnlocked, 0, NULL));
        id acData = CFBridgingRelease(SecAccessControlCopyData((__bridge SecAccessControlRef)ac));
        NSDictionary *keyQuery = @{ (id)kSecClass: (id)kSecClassKey,
                                    (id)kSecAttrTokenID: tokenID1,
                                    (id)kSecAttrKeyType : (id)kSecAttrKeyTypeECSECPrimeRandom,
                                    (id)kSecAttrKeySizeInBits : @"256",
                                    (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
                                    (id)kSecAttrIsPrivate: @YES,
                                    (id)kSecAttrAccessControl: acData,
                                    (id)kSecAttrTokenOID : privKeyData,
                                    (id)kSecAttrApplicationLabel : pubKeyHash,
                                   };
        OSStatus result;
        ok_status(result = SecItemUpdateTokenItemsForAccessGroups((__bridge CFStringRef)tokenID1, (__bridge CFArrayRef)@[(id)kSecAttrAccessGroupToken], (__bridge CFArrayRef)@[keyQuery]), "Failed to propagate key item.");

        id privateKey;
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)@{(id)kSecClass: (id)kSecClassKey, (id)kSecAttrTokenID: tokenID1, (id)kSecAttrAccessGroup: (id)kSecAttrAccessGroupToken, (id)kSecReturnRef: @YES}, (void *)&privateKey));

        NSDictionary *certQuery = CFBridgingRelease(copy_certificate_query(cert3, CFSTR("test_cert3"), (__bridge CFDataRef)cert3OID, (__bridge CFStringRef)tokenID2));
        ok(certQuery);

        ok_status(result = SecItemUpdateTokenItemsForAccessGroups((__bridge CFStringRef)tokenID2, (__bridge CFArrayRef)@[(id)kSecAttrAccessGroupToken], (__bridge CFArrayRef)@[certQuery]), "Failed to propagate cert item.");

        CFTypeRef resultRef;
        NSDictionary *query = @{ (id)kSecClass : (id)kSecClassKey, (id)kSecAttrApplicationLabel : pubKeyHash, (id)kSecReturnRef : @YES, (id)kSecReturnAttributes : @YES,  (id)kSecAttrAccessGroup: (id)kSecAttrAccessGroupToken };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (CFTypeRef*)&resultRef));
        CFReleaseNull(resultRef);

        query = @{ (id)kSecClass : (id)kSecClassCertificate, (id)kSecAttrPublicKeyHash : pubKeyHash, (id)kSecReturnRef : @YES, (id)kSecReturnAttributes : @YES, (id)kSecAttrAccessGroup: (id)kSecAttrAccessGroupToken };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (CFTypeRef*)&resultRef));
        CFReleaseNull(resultRef);

        query = @{ (id)kSecClass : (id)kSecClassIdentity, (id)kSecAttrApplicationLabel : pubKeyHash, (id)kSecReturnRef : @YES, (id)kSecReturnAttributes : @YES, (id)kSecAttrAccessGroup: (id)kSecAttrAccessGroupToken };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (CFTypeRef*)&resultRef));
        CFReleaseNull(resultRef);

        NSData *persRef, *persRef2;
        id newRef;

        // Query persistent reference for key and verify that we can get key back using it.
        query = @{ (id)kSecValueRef: privateKey, (id)kSecReturnPersistentRef: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&persRef));
        query = @{ (id)kSecClass: (id)kSecClassKey, (id)kSecAttrApplicationLabel: pubKeyHash, (id)kSecReturnPersistentRef: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&persRef2));
        eq_cf((__bridge CFTypeRef)persRef, (__bridge CFTypeRef)persRef2);
        query = @{ (id)kSecValuePersistentRef: persRef, (id)kSecReturnRef: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&newRef));
        eq_cf((__bridge CFTypeRef)privateKey, (__bridge CFTypeRef)newRef);

        // Query persistent reference for certificate and verify that we can get certificate back using it.
        id certRef;
        query = @{ (id)kSecClass: (id)kSecClassCertificate, (id)kSecAttrPublicKeyHash: pubKeyHash, (id)kSecReturnRef: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&certRef));

        persRef = nil;
        persRef2 = nil;
        newRef = nil;
        query = @{ (id)kSecValueRef: certRef, (id)kSecReturnPersistentRef: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&persRef));
        query = @{ (id)kSecClass: (id)kSecClassCertificate, (id)kSecAttrPublicKeyHash: pubKeyHash, (id)kSecReturnPersistentRef: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&persRef2));
        eq_cf((__bridge CFTypeRef)persRef, (__bridge CFTypeRef)persRef2);
        query = @{ (id)kSecValuePersistentRef: persRef, (id)kSecReturnRef: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&newRef));
        eq_cf((__bridge CFTypeRef)certRef, (__bridge CFTypeRef)newRef);

        // Query persistent reference for identity and verify that we can get identity back using it.
        id identityRef;
        NSDictionary *attrs;
        query = @{ (id)kSecClass: (id)kSecClassIdentity, (id)kSecAttrApplicationLabel: pubKeyHash, (id)kSecReturnRef: @YES, (id)kSecReturnAttributes: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&attrs));
        identityRef = attrs[(id)kSecValueRef];
        eq_cf((__bridge CFTypeRef)attrs[(id)kSecAttrTokenID], (__bridge CFTypeRef)tokenID1);

        persRef = nil;
        persRef2 = nil;
        attrs = nil;
        query = @{ (id)kSecValueRef: identityRef, (id)kSecReturnPersistentRef: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&persRef));
        query = @{ (id)kSecClass: (id)kSecClassIdentity, (id)kSecAttrApplicationLabel: pubKeyHash, (id)kSecReturnPersistentRef: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&persRef2));
        eq_cf((__bridge CFTypeRef)persRef, (__bridge CFTypeRef)persRef2);
        query = @{ (id)kSecValuePersistentRef: persRef2, (id)kSecReturnRef: @YES, (id)kSecReturnAttributes: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&attrs));
        eq_cf((__bridge CFTypeRef)identityRef, (__bridge CFTypeRef)attrs[(id)kSecValueRef]);
        eq_cf((__bridge CFTypeRef)attrs[(id)kSecAttrTokenID], (__bridge CFTypeRef)tokenID1);

        // Remove certificate from token and add it as regular keychain item (non-token) one. Following tests
        // repeat identity test for key-on-token, certificate-non-token hybrid identities.
        ok_status(result = SecItemUpdateTokenItemsForAccessGroups((__bridge CFStringRef)tokenID2, (__bridge CFArrayRef)@[(id)kSecAttrAccessGroupToken], NULL));
        certQuery = CFBridgingRelease(copy_certificate_query(cert3, CFSTR("reg_cert3"), NULL, NULL));
        ok_status(result = SecItemAdd((__bridge CFDictionaryRef)certQuery, NULL));

        query = @{ (id)kSecClass: (id)kSecClassIdentity, (id)kSecAttrApplicationLabel: pubKeyHash, (id)kSecReturnRef: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&identityRef));
        isnt(identityRef, NULL);

        persRef = nil;
        persRef2 = nil;
        attrs = nil;
        query = @{ (id)kSecValueRef: identityRef, (id)kSecReturnPersistentRef: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&persRef));
        query = @{ (id)kSecClass: (id)kSecClassIdentity, (id)kSecAttrApplicationLabel: pubKeyHash, (id)kSecReturnPersistentRef: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&persRef2));
        eq_cf((__bridge CFTypeRef)persRef, (__bridge CFTypeRef)persRef2);
        query = @{ (id)kSecValuePersistentRef: persRef, (id)kSecReturnRef: @YES, (id)kSecReturnAttributes: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&attrs));
        eq_cf((__bridge CFTypeRef)identityRef,  (__bridge CFTypeRef)attrs[(id)kSecValueRef]);
        eq_cf((__bridge CFTypeRef)attrs[(id)kSecAttrTokenID], (__bridge CFTypeRef)tokenID1);

        // After removing token with key, getting identity from persistent reference must fail gracefully.
        ok_status(result = SecItemUpdateTokenItemsForAccessGroups((__bridge CFStringRef)tokenID1, (__bridge CFArrayRef)@[(id)kSecAttrAccessGroupToken], NULL));
        query = @{ (id)kSecValuePersistentRef: persRef, (id)kSecReturnRef: @YES };
        is_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&newRef), errSecItemNotFound);

        // Getting persistent reference to non-token item with token-like (but malformed) data must not crash.
        NSData *data = CFBridgingRelease(CFPropertyListCreateDERData(kCFAllocatorDefault, (__bridge CFPropertyListRef)@[], NULL));
        query = @{ (id)kSecClass: (id)kSecClassGenericPassword, (id)kSecAttrLabel: @"probe", (id)kSecValueData: data};
        ok_status(result = SecItemAdd((__bridge CFDictionaryRef)query, NULL));
        NSDictionary *dict;
        query = @{ (id)kSecClass: (id)kSecClassGenericPassword, (id)kSecAttrLabel: @"probe", (id)kSecReturnPersistentRef: @YES, (id)kSecReturnData: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&dict));
        NSData *newData;
        query = @{ (id)kSecValuePersistentRef: dict[(id)kSecValuePersistentRef], (id)kSecReturnData: @YES };
        ok_status(result = SecItemCopyMatching((__bridge CFDictionaryRef)query, (void *)&newData));
        eq_cf((__bridge CFTypeRef)data, (__bridge CFTypeRef)newData);
    }
}

static void test_ies(SecKeyRef privateKey, SecKeyAlgorithm algorithm) {
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {

        blocks->createOrUpdateObject = ^CFDataRef(CFDataRef objectID, CFMutableDictionaryRef at, CFErrorRef *error) {
            return CFBridgingRetain([@"oid" dataUsingEncoding:NSUTF8StringEncoding]);
        };

        blocks->copyPublicKeyData = ^CFDataRef(CFDataRef objectID, CFErrorRef *error) {
            SecKeyRef publicKey = SecKeyCopyPublicKey(privateKey);
            CFDataRef data = SecKeyCopyExternalRepresentation(publicKey, error);
            CFReleaseNull(publicKey);
            return data;
        };

        blocks->copyObjectData = ^CFTypeRef(CFDataRef oid, CFErrorRef *error) {
            return kCFNull;
        };

        blocks->copyObjectAccessControl = ^CFDataRef(CFDataRef oid, CFErrorRef *error) {
            SecAccessControlRef ac = SecAccessControlCreate(NULL, NULL);
            SecAccessControlSetProtection(ac, kSecAttrAccessibleAlwaysPrivate, NULL);
            test_IsTrue(SecAccessControlAddConstraintForOperation(ac, kAKSKeyOpDefaultAcl, kCFBooleanTrue, NULL));
            CFDataRef acData = SecAccessControlCopyData(ac);
            CFRelease(ac);
            return acData;
        };

        blocks->copyOperationResult = ^CFTypeRef(CFDataRef objectID, CFIndex operation, CFArrayRef algorithms, CFIndex secKeyOperationMode, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
            CFTypeRef result = kCFNull;
            CFTypeRef algorithm = CFArrayGetValueAtIndex(algorithms, CFArrayGetCount(algorithms) - 1);
            switch (operation) {
                case kSecKeyOperationTypeKeyExchange: {
                    if (CFEqual(algorithm, kSecKeyAlgorithmECDHKeyExchangeStandard) ||
                        CFEqual(algorithm, kSecKeyAlgorithmECDHKeyExchangeCofactor)) {
                        NSDictionary *attrs = CFBridgingRelease(SecKeyCopyAttributes(privateKey));
                        NSDictionary *params = @{
                                                 (id)kSecAttrKeyType: attrs[(id)kSecAttrKeyType],
                                                 (id)kSecAttrKeySizeInBits: attrs[(id)kSecAttrKeySizeInBits],
                                                 (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic,
                                                 };
                        SecKeyRef pubKey = SecKeyCreateWithData(in1, (CFDictionaryRef)params, error);
                        if (pubKey == NULL) {
                            return NULL;
                        }
                        result = SecKeyCopyKeyExchangeResult(privateKey, algorithm, pubKey, in2, error);
                        CFReleaseSafe(pubKey);
                    }
                    break;
                }
                case kSecKeyOperationTypeDecrypt: {
                    if (CFEqual(algorithm, kSecKeyAlgorithmRSAEncryptionRaw)) {
                        result = SecKeyCreateDecryptedData(privateKey, algorithm, in1, error);
                    }
                    break;
                }
                default:
                    break;
            }
            return result;
        };
    });

    NSDictionary *privateParams = CFBridgingRelease(SecKeyCopyAttributes(privateKey));
    NSDictionary *params = @{ (id)kSecAttrKeyType : privateParams[(id)kSecAttrKeyType],
                              (id)kSecAttrKeySizeInBits : privateParams[(id)kSecAttrKeySizeInBits],
                              (id)kSecAttrTokenID : @"tid-ies",
                              (id)kSecPrivateKeyAttrs : @{ (id)kSecAttrIsPermanent : @NO }
                              };
    NSError *error;
    SecKeyRef tokenKey = SecKeyCreateRandomKey((__bridge CFDictionaryRef)params, (void *)&error);
    ok(tokenKey != NULL, "create token-based key (err %@)", error);
    SecKeyRef tokenPublicKey = SecKeyCopyPublicKey(tokenKey);

    NSData *plaintext = [@"plaintext" dataUsingEncoding:NSUTF8StringEncoding];

    error = nil;
    NSData *ciphertext = CFBridgingRelease(SecKeyCreateEncryptedData(tokenPublicKey, algorithm, (CFDataRef)plaintext, (void *)&error));
    ok(ciphertext, "failed to encrypt IES, err %@", error);

    NSData *decrypted = CFBridgingRelease(SecKeyCreateDecryptedData(tokenKey, algorithm, (CFDataRef)ciphertext, (void *)&error));
    ok(decrypted, "failed to decrypt IES, err %@", error);

    eq_cf((__bridge CFDataRef)plaintext, (__bridge CFDataRef)decrypted, "decrypted(%@) != plaintext(%@)", decrypted, plaintext);

    CFReleaseNull(tokenKey);
    CFReleaseNull(tokenPublicKey);
}

static void test_ecies() {
    NSError *error;
    SecKeyRef privateKey = SecKeyCreateRandomKey((__bridge CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256}, (void *)&error);
    ok(privateKey != NULL, "failed to generate CPU EC key: %@", error);

    test_ies(privateKey, kSecKeyAlgorithmECIESEncryptionCofactorX963SHA256AESGCM);

    CFReleaseNull(privateKey);
}

static void test_rsawrap() {
    NSError *error;
    SecKeyRef privateKey = SecKeyCreateRandomKey((__bridge CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA, (id)kSecAttrKeySizeInBits: @2048}, (void *)&error);
    ok(privateKey != NULL, "failed to generate CPU RSA key: %@", error);

    test_ies(privateKey, kSecKeyAlgorithmRSAEncryptionOAEPSHA256AESGCM);

    CFReleaseNull(privateKey);
}

static void tests(void) {
    /* custom keychain dir */
    secd_test_setup_temp_keychain("secd_33_keychain_ctk", NULL);

    test_item_add();
    test_item_query();
    test_item_update();
    test_item_delete();

    // params: int globalPersistence, int privatePersistence, int publicPersistence, bool privateIsPersistent, bool publicIsPersistent
    test_key_generate(-1, -1, -1, true, false);
    test_key_generate(0, -1, -1, false, false);
    test_key_generate(1, -1, -1, true, true);
    test_key_generate(-1, 0, 0, false, false);
    test_key_generate(-1, 1, 0, true, false);
    test_key_generate(-1, 0, 1, false, true);
    test_key_generate(-1, 1, 1, true, true);
    test_key_generate(0, 1, 1, true, true);
    test_key_generate(1, 1, 1, true, true);

    test_key_sign();
    test_key_generate_with_params();
    test_error_codes();
    test_propagate_token_items();
    test_identity_on_two_tokens();
    test_rsawrap();
    test_ecies();

    secd_test_teardown_delete_temp_keychain("secd_33_keychain_ctk");
}

int secd_33_keychain_ctk(int argc, char *const *argv) {
    if (os_feature_enabled(CryptoTokenKit, UseTokens)) {
        plan_tests(539);
    } else {
        plan_tests(403);
    }

    tests();

    return 0;
}
