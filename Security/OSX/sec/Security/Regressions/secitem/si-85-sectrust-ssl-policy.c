/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
 */

#include <Security/SecPolicyPriv.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecCertificatePriv.h>
#include <AssertMacros.h>
#include <utilities/SecCFWrappers.h>

#include "shared_regressions.h"

#include "si-85-sectrust-ssl-policy.h"

static void runTestForDictionary (const void *test_key, const void *test_value, void *context) {
    CFDictionaryRef test_info = test_value;
    CFStringRef test_name = test_key, file = NULL, reason = NULL, expectedResult = NULL, failReason = NULL;
    CFURLRef cert_file_url = NULL;
    CFDataRef cert_data = NULL;
    bool expectTrustSuccess = false;

    SecCertificateRef leaf = NULL, root = NULL;
    CFStringRef hostname = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef anchor_array = NULL;
    CFDateRef date = NULL;

    /* Note that this is built without many of the test convenience macros
     * in order to ensure there's only one "test" per test.
     */

    /* get filename in test dictionary */
    file = CFDictionaryGetValue(test_info, CFSTR("Filename"));
    require_action_quiet(file, cleanup, fail("%@: Unable to load filename from plist", test_name));

    /* get leaf certificate from file */
    cert_file_url = CFBundleCopyResourceURL(CFBundleGetMainBundle(), file, CFSTR("cer"), CFSTR("ssl-policy-certs"));
    require_action_quiet(cert_file_url, cleanup, fail("%@: Unable to get url for cert file %@",
                                                      test_name, file));
    
    SInt32 errorCode;
    require_action_quiet(CFURLCreateDataAndPropertiesFromResource(NULL, cert_file_url, &cert_data, NULL, NULL, &errorCode),
                         cleanup,
                         fail("%@: Could not create cert data for %@ with error %d",
                              test_name, file, (int)errorCode));

    /* create certificates */
    leaf = SecCertificateCreateWithData(NULL, cert_data);
    root = SecCertificateCreateWithBytes(NULL, _SSLTrustPolicyTestRootCA, sizeof(_SSLTrustPolicyTestRootCA));
    CFRelease(cert_data);
    require_action_quiet(leaf && root, cleanup, fail("%@: Unable to create certificates", test_name));

    /* create policy */
    hostname = CFDictionaryGetValue(test_info, CFSTR("Hostname"));
    require_action_quiet(hostname, cleanup, fail("%@: Unable to load hostname from plist", test_name));

    policy = SecPolicyCreateSSL(true, hostname);
    require_action_quiet(policy, cleanup, fail("%@: Unable to create SSL policy with hostname %@",
                                               test_name, hostname));

    /* create trust ref */
    OSStatus err = SecTrustCreateWithCertificates(leaf, policy, &trust);
    CFRelease(policy);
    require_noerr_action(err, cleanup, ok_status(err, "SecTrustCreateWithCertificates"));

    /* set anchor in trust ref */
    anchor_array = CFArrayCreate(NULL, (const void **)&root, 1, &kCFTypeArrayCallBacks);
    require_action_quiet(anchor_array, cleanup, fail("%@: Unable to create anchor array", test_name));
    err = SecTrustSetAnchorCertificates(trust, anchor_array);
    require_noerr_action(err, cleanup, ok_status(err, "SecTrustSetAnchorCertificates"));

    /* set date in trust ref to 4 Sep 2015 */
    date = CFDateCreate(NULL, 463079909.0);
    require_action_quiet(date, cleanup, fail("%@: Unable to create verify date", test_name));
    err = SecTrustSetVerifyDate(trust, date);
    CFRelease(date);
    require_noerr_action(err, cleanup, ok_status(err, "SecTrustSetVerifyDate"));

    /* evaluate */
    SecTrustResultType actualResult = 0;
    err = SecTrustEvaluate(trust, &actualResult);
    require_noerr_action(err, cleanup, ok_status(err, "SecTrustEvaluate"));
    bool is_valid = (actualResult == kSecTrustResultProceed || actualResult == kSecTrustResultUnspecified);
    if (!is_valid) failReason = SecTrustCopyFailureDescription(trust);
    
    /* get expected result for test */
    expectedResult = CFDictionaryGetValue(test_info, CFSTR("Result"));
    require_action_quiet(expectedResult, cleanup, fail("%@: Unable to get expected result",test_name));
    if (!CFStringCompare(expectedResult, CFSTR("kSecTrustResultUnspecified"), 0) ||
        !CFStringCompare(expectedResult, CFSTR("kSecTrustResultProceed"), 0)) {
        expectTrustSuccess = true;
    }

    /* process results */
    if(!CFDictionaryGetValueIfPresent(test_info, CFSTR("Reason"), (const void **)&reason)) {
        /* not a known failure */
        ok(is_valid == expectTrustSuccess, "%s %@%@",
           expectTrustSuccess ? "REGRESSION" : "SECURITY",
           test_name,
           failReason ? failReason : CFSTR(""));
    }
    else if(reason) {
        /* known failure */
        todo(CFStringGetCStringPtr(reason, kCFStringEncodingUTF8));
        ok(is_valid == expectTrustSuccess, "%@%@",
           test_name, expectTrustSuccess ? (failReason ? failReason : CFSTR("")) : CFSTR(" valid"));
    }
    else {
        fail("%@: unable to get reason for known failure", test_name);
    }

cleanup:
    CFReleaseNull(cert_file_url);
    CFReleaseNull(leaf);
    CFReleaseNull(root);
    CFReleaseNull(trust);
    CFReleaseNull(anchor_array);
    CFReleaseNull(failReason);
}

static void tests(void)
{
    CFDataRef plist_data = NULL;
    CFArrayRef plist = NULL;
    CFPropertyListRef tests_dictionary = NULL;

    plist = CFBundleCopyResourceURLsOfType(CFBundleGetMainBundle(), CFSTR("plist"), CFSTR("ssl-policy-certs"));
    if (CFArrayGetCount(plist) != 1) {
        fail("Incorrect number of plists found in ssl-policy-certs");
        goto exit;
    }

    SInt32 errorCode;
    if(!CFURLCreateDataAndPropertiesFromResource(NULL, CFArrayGetValueAtIndex(plist, 0), &plist_data, NULL, NULL, &errorCode)) {
        fail("Could not create data from plist with error %d", (int)errorCode);
        goto exit;
    }

    CFErrorRef err;
    tests_dictionary = CFPropertyListCreateWithData(NULL, plist_data, kCFPropertyListImmutable, NULL, &err);
    if(!tests_dictionary || (CFGetTypeID(tests_dictionary) != CFDictionaryGetTypeID())) {
        fail("Failed to create tests dictionary from plist");
        goto exit;
    }

    CFDictionaryApplyFunction(tests_dictionary, runTestForDictionary, NULL);

exit:
    CFReleaseNull(plist);
    CFReleaseNull(plist_data);
    CFReleaseNull(tests_dictionary);
}

int si_85_sectrust_ssl_policy(int argc, char *const *argv)
{
    plan_tests(37);

    tests();

    return 0;
}
