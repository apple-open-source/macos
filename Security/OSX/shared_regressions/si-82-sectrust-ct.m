/*
 *  si-82-sectrust-ct.c
 *  Security
 *
 *  Copyright (c) 2014 Apple Inc. All Rights Reserved.
 *
 */

#include <AssertMacros.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecPolicyPriv.h>
#include <stdlib.h>
#include <unistd.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecTrustSettings.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecFramework.h>

#if TARGET_OS_IPHONE
#include <Security/SecTrustStore.h>
#else
#include <Security/SecKeychain.h>
#endif

#include "shared_regressions.h"
#include "si-82-sectrust-ct.h"

//define this if you want to print clock time of SecTrustEvaluate call.
//define PRINT_SECTRUST_EVALUATE_TIME

static bool isCFTrue(CFTypeRef cf)
{
    return (cf == kCFBooleanTrue);
}

static void test_ct_trust(CFArrayRef certs, CFArrayRef scts, CFTypeRef ocspresponses, CFArrayRef anchors,
                          CFArrayRef trustedLogs, CFStringRef hostname, CFDateRef date,
                          bool ct_expected, bool ev_expected, bool ct_whitelist_expected,
                          const char *test_name)
{
    CFArrayRef policies=NULL;
    SecPolicyRef policy=NULL;
    SecTrustRef trust=NULL;
    SecTrustResultType trustResult;
    CFDictionaryRef results=NULL;
    CFArrayRef properties=NULL;



    isnt(policy = SecPolicyCreateSSL(true, hostname), NULL, "create policy");
    isnt(policies = CFArrayCreate(kCFAllocatorDefault, (const void **)&policy, 1, &kCFTypeArrayCallBacks), NULL, "create policies");
    ok_status(SecTrustCreateWithCertificates(certs, policies, &trust), "create trust");

    assert(trust); // silence analyzer
    if(anchors) {
        ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set anchors");
    }

    if(scts) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
        ok_status(SecTrustSetSignedCertificateTimestamps(trust, scts), "set standalone SCTs");
#pragma clang diagnostic pop
    }

    if(trustedLogs) {
        ok_status(SecTrustSetTrustedLogs(trust, trustedLogs), "set trusted logs");
    }

    if(ocspresponses) {
        ok_status(SecTrustSetOCSPResponse(trust, ocspresponses), "set ocsp responses");
    }

    if (!date) { goto errOut; }
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");
#ifdef PRINT_SECTRUST_EVALUATE_TIME
    clock_t t0 = clock();
#endif
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
#ifdef PRINT_SECTRUST_EVALUATE_TIME
    clock_t t1 = clock() - t0;
#endif
    ok(trustResult == kSecTrustResultUnspecified, "trustResult 4 expected (got %d)",
       (int)trustResult);

    results = SecTrustCopyResult(trust);

    CFTypeRef ct = CFDictionaryGetValue(results, kSecTrustCertificateTransparency);
    CFTypeRef ev = CFDictionaryGetValue(results, kSecTrustExtendedValidation);
    CFTypeRef ct_whitelist = CFDictionaryGetValue(results, kSecTrustCertificateTransparencyWhiteList);


    ok((isCFTrue(ct) == ct_expected), "unexpected CT result (%s)", test_name);
    ok((isCFTrue(ev) == ev_expected), "unexpected EV result (%s)", test_name);
    ok((isCFTrue(ct_whitelist) == ct_whitelist_expected), "unexpected CT WhiteList result (%s)", test_name);
    /* Note that the CT whitelist has been removed due to the expiration of all contents. */

#ifdef PRINT_SECTRUST_EVALUATE_TIME
    printf("%s: %lu\n", test_name, t1);
#endif

    properties = SecTrustCopyProperties(trust);

errOut:
    CFReleaseSafe(policy);
    CFReleaseSafe(policies);
    CFReleaseSafe(trust);
    CFReleaseSafe(results);
    CFReleaseSafe(properties);
}

#import <Foundation/Foundation.h>

static
SecCertificateRef SecCertificateCreateFromResource(NSString *name)
{
    NSURL *url = [[NSBundle mainBundle] URLForResource:name withExtension:@".cer" subdirectory:@"si-82-sectrust-ct-data"];

    NSData *certData = [NSData dataWithContentsOfURL:url];

    SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)certData);

    return cert;
}

static
CFDataRef CFDataCreateFromResource(NSString *name)
{
    NSURL *url = [[NSBundle mainBundle] URLForResource:name withExtension:@".bin" subdirectory:@"si-82-sectrust-ct-data"];

    NSData *binData = [[NSData alloc] initWithContentsOfURL:url];

    return (__bridge_retained CFDataRef) binData;
}

static CFArrayRef CTTestsCopyTrustedLogs(void) {
    CFArrayRef trustedLogs=NULL;
    CFURLRef trustedLogsURL=NULL;

    trustedLogsURL = CFBundleCopyResourceURL(CFBundleGetMainBundle(),
                                             CFSTR("CTlogs"),
                                             CFSTR("plist"),
                                             CFSTR("si-82-sectrust-ct-data"));
    isnt(trustedLogsURL, NULL, "trustedLogsURL");
    trustedLogs = (CFArrayRef) CFPropertyListReadFromFile(trustedLogsURL);
    isnt(trustedLogs, NULL, "trustedLogs");

    CFReleaseNull(trustedLogsURL);
    return trustedLogs;
}

static void tests()
{
    SecCertificateRef certA=NULL, certD=NULL, certF=NULL, certCA_alpha=NULL, certCA_beta=NULL;
    CFDataRef proofD=NULL, proofA_1=NULL, proofA_2=NULL;
    SecCertificateRef www_digicert_com_2015_cert=NULL, www_digicert_com_2016_cert=NULL, digicert_sha2_ev_server_ca=NULL;
    SecCertificateRef pilot_cert_3055998=NULL, pilot_cert_3055998_issuer=NULL;
    SecCertificateRef whitelist_00008013=NULL, whitelist_5555bc4f=NULL, whitelist_aaaae152=NULL, whitelist_fff9b5f6=NULL;
    SecCertificateRef whitelist_00008013_issuer=NULL, whitelist_5555bc4f_issuer=NULL, whitelist_fff9b5f6_issuer=NULL;
    SecCertificateRef cfCert = NULL;
    CFMutableArrayRef certs=NULL;
    CFMutableArrayRef scts=NULL;
    CFMutableArrayRef anchors=NULL;
    CFDataRef valid_ocsp=NULL;
    CFDataRef invalid_ocsp=NULL;
    CFDataRef bad_hash_ocsp=NULL;

    CFArrayRef trustedLogs= CTTestsCopyTrustedLogs();

    isnt(certCA_alpha = SecCertificateCreateFromResource(@"CA_alpha"), NULL, "create ca-alpha cert");
    isnt(certCA_beta = SecCertificateCreateFromResource(@"CA_beta"), NULL, "create ca-beta cert");
    isnt(anchors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create anchors array");
    CFArrayAppendValue(anchors, certCA_alpha);
    CFArrayAppendValue(anchors, certCA_beta);
    isnt(certA = SecCertificateCreateFromResource(@"serverA"), NULL, "create certA");
    isnt(certD = SecCertificateCreateFromResource(@"serverD"), NULL, "create certD");
    isnt(certF = SecCertificateCreateFromResource(@"serverF"), NULL, "create certF");
    isnt(proofD = CFDataCreateFromResource(@"serverD_proof"), NULL, "creat proofD");
    isnt(proofA_1 = CFDataCreateFromResource(@"serverA_proof_Alfa_3"), NULL, "creat proofA_1");
    isnt(proofA_2 = CFDataCreateFromResource(@"serverA_proof_Bravo_3"), NULL, "creat proofA_2");
    isnt(www_digicert_com_2015_cert = SecCertificateCreateFromResource(@"www_digicert_com_2015"), NULL, "create www.digicert.com 2015 cert");
    isnt(www_digicert_com_2016_cert = SecCertificateCreateFromResource(@"www_digicert_com_2016"), NULL, "create www.digicert.com 2016 cert");
    isnt(digicert_sha2_ev_server_ca = SecCertificateCreateFromResource(@"digicert_sha2_ev_server_ca"), NULL, "create digicert.com subCA cert");
    isnt(valid_ocsp = CFDataCreateFromResource(@"valid_ocsp_response"), NULL, "create valid_ocsp");
    isnt(invalid_ocsp = CFDataCreateFromResource(@"invalid_ocsp_response"), NULL, "create invalid_ocsp");
    isnt(bad_hash_ocsp = CFDataCreateFromResource(@"bad_hash_ocsp_response"), NULL, "create bad_hash_ocsp");
    isnt(pilot_cert_3055998 = SecCertificateCreateFromResource(@"pilot_3055998"), NULL, "create pilot_cert_3055998 cert");
    isnt(pilot_cert_3055998_issuer = SecCertificateCreateFromResource(@"pilot_3055998_issuer"), NULL, "create pilot_cert_3055998 issuer cert");

    isnt(whitelist_00008013 = SecCertificateCreateFromResource(@"whitelist_00008013"), NULL, "create whitelist_00008013 cert");
    isnt(whitelist_5555bc4f = SecCertificateCreateFromResource(@"whitelist_5555bc4f"), NULL, "create whitelist_5555bc4f cert");
    isnt(whitelist_aaaae152 = SecCertificateCreateFromResource(@"whitelist_aaaae152"), NULL, "create whitelist_aaaae152 cert");
    isnt(whitelist_fff9b5f6 = SecCertificateCreateFromResource(@"whitelist_fff9b5f6"), NULL, "create whitelist_fff9b5f6 cert");
    isnt(whitelist_00008013_issuer = SecCertificateCreateFromResource(@"whitelist_00008013_issuer"), NULL, "create whitelist_00008013_issuer cert");
    isnt(whitelist_5555bc4f_issuer = SecCertificateCreateFromResource(@"whitelist_5555bc4f_issuer"), NULL, "create whitelist_5555bc4f_issuer cert");
    isnt(whitelist_fff9b5f6_issuer = SecCertificateCreateFromResource(@"whitelist_fff9b5f6_issuer"), NULL, "create whitelist_fff9b5f6_issuer cert");

    CFCalendarRef cal = NULL;
    CFAbsoluteTime at;
    CFDateRef date_20150307 = NULL; // Date for older set of tests.
    CFDateRef date_20160422 = NULL; // Date for newer set of tests.

    isnt(cal = CFCalendarCreateWithIdentifier(kCFAllocatorDefault, kCFGregorianCalendar), NULL, "create calendar");
    ok(CFCalendarComposeAbsoluteTime(cal, &at, "yMd", 2015, 3, 7), "create verify absolute time 20150307");
    isnt(date_20150307 = CFDateCreate(kCFAllocatorDefault, at), NULL, "create verify date 20150307");
    ok(CFCalendarComposeAbsoluteTime(cal, &at, "yMd", 2016, 4, 22), "create verify absolute time 20160422");
    isnt(date_20160422 = CFDateCreate(kCFAllocatorDefault, at), NULL, "create verify date 20160422");


    /* Case 1: coreos-ct-test embedded SCT - only 1 SCT - so not CT qualified */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, certF);
    test_ct_trust(certs, NULL, NULL, anchors, trustedLogs, NULL, date_20150307,
                  false, false, false, "coreos-ct-test 1");
    CFReleaseNull(certs);

    /* Case 2: coreos-ct-test standalone SCT - only 1 SCT - so not CT qualified  */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, certD);
    isnt(scts = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create SCT array");
    CFArrayAppendValue(scts, proofD);
    test_ct_trust(certs, scts, NULL, anchors, trustedLogs, NULL, date_20150307,
                  false, false, false, "coreos-ct-test 2");
    CFReleaseNull(certs);
    CFReleaseNull(scts);

    /* case 3: digicert : 2 embedded SCTs, but lifetime of cert is 24 month, so not CT qualified */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, www_digicert_com_2015_cert);
    CFArrayAppendValue(certs, digicert_sha2_ev_server_ca);
    test_ct_trust(certs, NULL, NULL, NULL, NULL, CFSTR("www.digicert.com"), date_20150307,
                  false, false, false, "digicert 2015");
    CFReleaseNull(certs);

    /* Case 4: coreos-ct-test standalone SCT -  2 SCTs - CT qualified  */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, certA);
    isnt(scts = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create SCT array");
    CFArrayAppendValue(scts, proofA_1);
    CFArrayAppendValue(scts, proofA_2);
    test_ct_trust(certs, scts, NULL, anchors, trustedLogs, NULL, date_20150307,
                  true,  false, false, "coreos-ct-test 3");
    CFReleaseNull(certs);
    CFReleaseNull(scts);

    /* Case 5: Test with an invalid OCSP response */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, certA);
    isnt(scts = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create SCT array");
    CFArrayAppendValue(scts, proofA_1);
    test_ct_trust(certs, scts, invalid_ocsp, anchors, trustedLogs, NULL, date_20150307,
                  false, false, false, "coreos-ct-test 4");
    CFReleaseNull(certs);
    CFReleaseNull(scts);

    /* Case 6: Test with a valid OCSP response */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, certA);
    isnt(scts = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create SCT array");
    CFArrayAppendValue(scts, proofA_1);
    test_ct_trust(certs, scts, valid_ocsp, anchors, trustedLogs, NULL, date_20150307,
                  false, false, false, "coreos-ct-test 5");
    CFReleaseNull(certs);
    CFReleaseNull(scts);

    /* Case 7: Test with a bad hash OCSP response */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, certA);
    isnt(scts = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create SCT array");
    CFArrayAppendValue(scts, proofA_1);
    test_ct_trust(certs, scts, bad_hash_ocsp, anchors, trustedLogs, NULL, date_20150307,
                  false, false, false, "coreos-ct-test 6");
    CFReleaseNull(certs);
    CFReleaseNull(scts);

    /* case 8: April 2016 www.digicert.com cert: 3 embedded SCTs, CT qualified, but OCSP doesn't respond */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, www_digicert_com_2016_cert);
    CFArrayAppendValue(certs, digicert_sha2_ev_server_ca);

    /* WatchOS doesn't require OCSP for EV flag, so even though the OCSP responder no longer responds for this cert,
     * it is EV on watchOS. */
#if TARGET_OS_WATCH
    test_ct_trust(certs, NULL, NULL, NULL, NULL, CFSTR("www.digicert.com"), date_20160422,
                  true, true, false, "digicert 2016");
#else
    test_ct_trust(certs, NULL, NULL, NULL, NULL, CFSTR("www.digicert.com"), date_20160422,
                  true, false, false, "digicert 2016");
#endif
    CFReleaseNull(certs);



#define TEST_CASE(x) \
 do { \
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array for " #x); \
    isnt(cfCert = SecCertificateCreateFromResource(@#x), NULL, "create cfCert from " #x); \
    CFArrayAppendValue(certs, cfCert); \
    test_ct_trust(certs, NULL, NULL, anchors, trustedLogs, NULL, date_20150307, true, false, false, #x); \
    CFReleaseNull(certs);  \
    CFReleaseNull(cfCert);  \
 } while (0)


    TEST_CASE(server_1601);
    TEST_CASE(server_1603);
    TEST_CASE(server_1604);
    TEST_CASE(server_1701);
    TEST_CASE(server_1704);
    TEST_CASE(server_1705);
    TEST_CASE(server_1801);
    TEST_CASE(server_1804);
    TEST_CASE(server_1805);
    TEST_CASE(server_2001);


    CFReleaseSafe(certCA_alpha);
    CFReleaseSafe(certCA_beta);
    CFReleaseSafe(anchors);
    CFReleaseSafe(certA);
    CFReleaseSafe(certD);
    CFReleaseSafe(certF);
    CFReleaseSafe(proofD);
    CFReleaseSafe(proofA_1);
    CFReleaseSafe(proofA_2);
    CFReleaseSafe(www_digicert_com_2015_cert);
    CFReleaseSafe(www_digicert_com_2016_cert);
    CFReleaseSafe(digicert_sha2_ev_server_ca);
    CFReleaseSafe(pilot_cert_3055998);
    CFReleaseSafe(pilot_cert_3055998_issuer);
    CFReleaseSafe(whitelist_00008013);
    CFReleaseSafe(whitelist_5555bc4f);
    CFReleaseSafe(whitelist_aaaae152);
    CFReleaseSafe(whitelist_fff9b5f6);
    CFReleaseSafe(whitelist_00008013_issuer);
    CFReleaseSafe(whitelist_5555bc4f_issuer);
    CFReleaseSafe(whitelist_fff9b5f6_issuer);
    CFReleaseSafe(trustedLogs);
    CFReleaseSafe(valid_ocsp);
    CFReleaseSafe(invalid_ocsp);
    CFReleaseSafe(bad_hash_ocsp);
    CFReleaseSafe(cal);
    CFReleaseSafe(date_20150307);
    CFReleaseSafe(date_20160422);

}

static void test_sct_serialization(void) {
    SecCertificateRef certA = NULL, certCA_alpha = NULL, certCA_beta = NULL;
    CFArrayRef trustedLogs= CTTestsCopyTrustedLogs();
    SecTrustRef trust = NULL, deserializedTrust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, NULL);
    NSData *proofA_1 = NULL, *proofA_2 = NULL;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:447450000.0]; // March 7, 2015 at 11:40:00 AM PST
    CFErrorRef error = NULL;

    isnt(certA = SecCertificateCreateFromResource(@"serverA"), NULL, "create certA");
    isnt(certCA_alpha = SecCertificateCreateFromResource(@"CA_alpha"), NULL, "create ca-alpha cert");
    isnt(certCA_beta = SecCertificateCreateFromResource(@"CA_beta"), NULL, "create ca-beta cert");

    NSArray *anchors = @[ (__bridge id)certCA_alpha, (__bridge id)certCA_beta ];

    isnt(proofA_1 = CFBridgingRelease(CFDataCreateFromResource(@"serverA_proof_Alfa_3")), NULL, "creat proofA_1");
    isnt(proofA_2 = CFBridgingRelease(CFDataCreateFromResource(@"serverA_proof_Bravo_3")), NULL, "creat proofA_2");
    NSArray *scts = @[ proofA_1, proofA_2 ];

    /* Make a SecTrustRef and then serialize it */
    ok_status(SecTrustCreateWithCertificates(certA, policy, &trust), "failed to create trust object");
    ok_status(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), "failed to set anchors");
    ok_status(SecTrustSetTrustedLogs(trust, trustedLogs), "failed to set trusted logs");
    ok_status(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), "failed to set verify date");

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
    ok_status(SecTrustSetSignedCertificateTimestamps(trust, (__bridge CFArrayRef)scts), "failed to set SCTS");
#pragma clang diagnostic pop

    NSData *serializedTrust = CFBridgingRelease(SecTrustSerialize(trust, &error));
    isnt(serializedTrust, NULL, "failed to serialize trust: %@", error);

    /* Evaluate it to make sure it's CT */
    ok(SecTrustEvaluateWithError(trust, &error), "failed to evaluate trust: %@", error);
    NSDictionary *results = CFBridgingRelease(SecTrustCopyResult(trust));
    isnt(results[(__bridge NSString*)kSecTrustCertificateTransparency], NULL, "failed get CT result");
    ok([results[(__bridge NSString*)kSecTrustCertificateTransparency] boolValue], "CT failed");

    /* Make a new trust object by deserializing the previous trust object */
    ok(deserializedTrust = SecTrustDeserialize((__bridge CFDataRef)serializedTrust, &error), "failed to deserialize trust: %@", error);

    /* Evaluate the new one to make sure it's CT (because the SCTs were serialized) */
    ok(SecTrustEvaluateWithError(deserializedTrust, &error), "failed to evaluate trust: %@", error);
    results = CFBridgingRelease(SecTrustCopyResult(deserializedTrust));
    isnt(results[(__bridge NSString*)kSecTrustCertificateTransparency], NULL, "failed get CT result");
    ok([results[(__bridge NSString*)kSecTrustCertificateTransparency] boolValue], "CT failed");

    CFReleaseNull(certA);
    CFReleaseNull(certCA_alpha);
    CFReleaseNull(certCA_beta);
    CFReleaseNull(trustedLogs);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(deserializedTrust);
    CFReleaseNull(error);
}

static void testSetCTExceptions(void) {
    CFErrorRef error = NULL;
    const CFStringRef SecurityTestsAppID = CFSTR("com.apple.security.regressions");
    const CFStringRef AnotherAppID = CFSTR("com.apple.security.not-this-one");
    CFDictionaryRef copiedExceptions = NULL;

    /* Verify no exceptions set */
    is(copiedExceptions = SecTrustStoreCopyCTExceptions(NULL, NULL), NULL, "no exceptions set");
    if (copiedExceptions) {
        /* If we're starting out with exceptions set, a lot of the following will also fail, so just skip them */
        CFReleaseNull(copiedExceptions);
        return;
    }

    /* Set exceptions with specified AppID */
    NSDictionary *exceptions1 = @{
        (__bridge NSString*)kSecCTExceptionsDomainsKey: @[@"test.apple.com", @".test.apple.com"],
    };
    ok(SecTrustStoreSetCTExceptions(SecurityTestsAppID, (__bridge CFDictionaryRef)exceptions1, &error),
       "failed to set exceptions for SecurityTests: %@", error);

    /* Copy all exceptions (with only one set) */
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(NULL, &error),
       "failed to copy all exceptions: %@", error);
    ok([exceptions1 isEqualToDictionary:(__bridge NSDictionary*)copiedExceptions],
       "got the wrong exceptions back");
    CFReleaseNull(copiedExceptions);

    /* Copy this app's exceptions */
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(SecurityTestsAppID, &error),
       "failed to copy SecurityTests' exceptions: %@", error);
    ok([exceptions1 isEqualToDictionary:(__bridge NSDictionary*)copiedExceptions],
       "got the wrong exceptions back");
    CFReleaseNull(copiedExceptions);

    /* Copy a different app's exceptions */
    is(copiedExceptions = SecTrustStoreCopyCTExceptions(AnotherAppID, &error), NULL,
       "failed to copy different app's exceptions: %@", error);
    CFReleaseNull(copiedExceptions);

    /* Set different exceptions with implied AppID */
    CFDataRef leafHash = SecSHA256DigestCreate(NULL, _system_after_leafSPKI, sizeof(_system_after_leafSPKI));
    NSDictionary *leafException = @{ (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
                                     (__bridge NSString*)kSecCTExceptionsSPKIHashKey : (__bridge NSData*)leafHash,
    };

    NSDictionary *exceptions2 = @{
        (__bridge NSString*)kSecCTExceptionsDomainsKey: @[@".test.apple.com"],
        (__bridge NSString*)kSecCTExceptionsCAsKey : @[ leafException ]
    };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions2, &error),
       "failed to set exceptions for this app: %@", error);

    /* Ensure exceptions are replaced for SecurityTests */
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(SecurityTestsAppID, &error),
       "failed to copy SecurityTests' exceptions: %@", error);
    ok([exceptions2 isEqualToDictionary:(__bridge NSDictionary*)copiedExceptions],
       "got the wrong exceptions back");
    CFReleaseNull(copiedExceptions);

    /* Set exceptions with a different AppID */
    CFDataRef rootHash = SecSHA256DigestCreate(NULL, _system_rootSPKI, sizeof(_system_rootSPKI));
    NSDictionary *rootExceptions =  @{ (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
                                       (__bridge NSString*)kSecCTExceptionsSPKIHashKey : (__bridge NSData*)rootHash,
    };
    NSDictionary *exceptions3 = @{
        (__bridge NSString*)kSecCTExceptionsCAsKey : @[ rootExceptions ]
    };
    ok(SecTrustStoreSetCTExceptions(AnotherAppID, (__bridge CFDictionaryRef)exceptions3, &error),
       "failed to set exceptions for different app: %@", error);

    /* Copy only one of the app's exceptions */
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(SecurityTestsAppID, &error),
       "failed to copy SecurityTests' exceptions: %@", error);
    ok([exceptions2 isEqualToDictionary:(__bridge NSDictionary*)copiedExceptions],
       "got the wrong exceptions back");
    CFReleaseNull(copiedExceptions);

    /* Set empty exceptions */
    NSDictionary *empty = @{};
    ok(SecTrustStoreSetCTExceptions(SecurityTestsAppID, (__bridge CFDictionaryRef)empty, &error),
       "failed to set empty exceptions");

    /* Copy exceptiosn to ensure no change */
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(SecurityTestsAppID, &error),
       "failed to copy SecurityTests' exceptions: %@", error);
    ok([exceptions2 isEqualToDictionary:(__bridge NSDictionary*)copiedExceptions],
       "got the wrong exceptions back");
    CFReleaseNull(copiedExceptions);

    /* Copy all exceptions */
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(NULL, &error),
       "failed to copy all exceptions: %@", error);
    is(CFDictionaryGetCount(copiedExceptions), 2, "Got the wrong number of all exceptions");
    NSDictionary *nsCopiedExceptions = CFBridgingRelease(copiedExceptions);
    NSArray *domainExceptions = nsCopiedExceptions[(__bridge NSString*)kSecCTExceptionsDomainsKey];
    NSArray *caExceptions = nsCopiedExceptions[(__bridge NSString*)kSecCTExceptionsCAsKey];
    ok(domainExceptions && caExceptions, "Got both domain and CA exceptions");
    ok([domainExceptions count] == 1, "Got 1 domain exception");
    ok([caExceptions count] == 2, "Got 2 CA exceptions");
    ok([domainExceptions[0] isEqualToString:@".test.apple.com"], "domain exception is .test.apple.com");
    ok([caExceptions containsObject:leafException] && [caExceptions containsObject:rootExceptions], "got expected leaf and root CA exceptions");

    /* Reset other app's exceptions */
    ok(SecTrustStoreSetCTExceptions(AnotherAppID, NULL, &error),
       "failed to reset exceptions for different app: %@", error);
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(NULL, &error),
       "failed to copy all exceptions: %@", error);
    ok([exceptions2 isEqualToDictionary:(__bridge NSDictionary*)copiedExceptions],
       "got the wrong exceptions back");
    CFReleaseNull(copiedExceptions);

#define check_errSecParam \
    if (error) { \
        is(CFErrorGetCode(error), errSecParam, "bad input produced unxpected error code: %ld", (long)CFErrorGetCode(error)); \
    } else { \
        fail("expected failure to set NULL exceptions"); \
    }

    /* Set exceptions with bad inputs */
    NSDictionary *badExceptions = @{
       (__bridge NSString*)kSecCTExceptionsDomainsKey: @[@"test.apple.com", @".test.apple.com"],
       @"not a key": @"not a value",
    };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with unknown key");
    check_errSecParam

    badExceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey:@"test.apple.com" };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with bad value");
    check_errSecParam

    badExceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey: @[ @{} ] };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with bad array value");
    check_errSecParam

    badExceptions = @{ (__bridge NSString*)kSecCTExceptionsCAsKey: @[ @"test.apple.com" ] };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with bad array value");
    check_errSecParam

    badExceptions = @{ (__bridge NSString*)kSecCTExceptionsCAsKey: @[ @{
      (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
      @"not-a-key" : (__bridge NSData*)rootHash,
    }] };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with bad CA dictionary value");
    check_errSecParam

    badExceptions = @{ (__bridge NSString*)kSecCTExceptionsCAsKey: @[ @{
      (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
    }] };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with bad CA dictionary value");
    check_errSecParam

    badExceptions = @{ (__bridge NSString*)kSecCTExceptionsCAsKey: @[ @{
      (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
      (__bridge NSString*)kSecCTExceptionsSPKIHashKey : (__bridge NSData*)rootHash,
      @"not-a-key":@"not-a-value"
    }] };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with bad CA dictionary value");
    check_errSecParam

    badExceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey: @[ @".com" ] };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with TLD value");
    check_errSecParam
#undef check_errSecParam

    /* Remove exceptions using empty arrays */
    NSDictionary *emptyArrays = @{
        (__bridge NSString*)kSecCTExceptionsDomainsKey: @[],
        (__bridge NSString*)kSecCTExceptionsCAsKey : @[]
    };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)emptyArrays, &error),
       "failed to set empty array exceptions for this app: %@", error);
    is(copiedExceptions = SecTrustStoreCopyCTExceptions(NULL, NULL), NULL, "no exceptions set");

    CFReleaseNull(leafHash);
    CFReleaseNull(rootHash);
}

static void setup_for_user_trust(NSArray *in_certs, NSArray **deleteMeCertificates) {
#if TARGET_OS_OSX
    /* Since we're putting trust settings in the admin domain,
     * we need to add the certs to the system keychain. */
    SecKeychainRef kcRef = NULL;
    CFArrayRef certRef = NULL;
    NSDictionary *attrs = nil;
    NSMutableArray *persistenRefs = nil;

    SecKeychainOpen("/Library/Keychains/System.keychain", &kcRef);
    if (!kcRef) {
        return;
    }

    persistenRefs = [[NSMutableArray alloc] init];

    for (id cert in in_certs) {
        attrs = @{(__bridge NSString*)kSecValueRef: cert,
                  (__bridge NSString*)kSecUseKeychain: (__bridge id)kcRef,
                  (__bridge NSString*)kSecReturnPersistentRef: @YES};
        if (SecItemAdd((CFDictionaryRef)attrs, (void *)&certRef) == 0)
            [persistenRefs addObject:(__bridge NSArray *)certRef];
        CFReleaseNull(certRef);
    }

    CFReleaseNull(kcRef);
    *deleteMeCertificates = persistenRefs;
#endif
}

static void cleanup_keychain(NSArray *deleteMeCertificates) {
#if TARGET_OS_OSX
    if (!deleteMeCertificates) {
        return;
    }

    [deleteMeCertificates enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        SecItemDelete((CFDictionaryRef)@{ (__bridge NSString*)kSecValuePersistentRef: [obj objectAtIndex:0]});
    }];
#endif
}

static bool set_trust_settings_for_cert(SecCertificateRef cert) {
    bool ok = false;
    NSDictionary *settings = nil;
    if (!SecCertificateIsSelfSignedCA(cert)) {
        settings = @{ (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultTrustAsRoot)};
    }
#if TARGET_OS_IPHONE
    require_noerr_string(SecTrustStoreSetTrustSettings(SecTrustStoreForDomain(kSecTrustStoreDomainUser), cert,
                                                       (__bridge CFDictionaryRef)settings),
                         errOut, "failed to set trust settings");
#else
    require_noerr_string(SecTrustSettingsSetTrustSettings(cert, kSecTrustSettingsDomainAdmin,
                                                          (__bridge CFDictionaryRef)settings),
                         errOut, "failed to set trust settings");
    usleep(20000);
#endif
    ok = true;
errOut:
    return ok;
}

static bool remove_trust_settings_for_cert(SecCertificateRef cert) {
    bool ok = false;
#if TARGET_OS_IPHONE
    require_noerr_string(SecTrustStoreRemoveCertificate(SecTrustStoreForDomain(kSecTrustStoreDomainUser), cert),
                         errOut, "failed to remove trust settings");
#else
    require_noerr_string(SecTrustSettingsRemoveTrustSettings(cert, kSecTrustSettingsDomainAdmin),
                         errOut, "failed to remove trust settings");
#endif
    ok = true;
errOut:
    return ok;
}

static void test_enforcement(void) {
    SecCertificateRef system_root = NULL, user_root = NULL;
    SecCertificateRef system_server_before = NULL, system_server_after = NULL, system_server_after_with_CT = NULL;
    SecCertificateRef user_server_after = NULL;
    CFArrayRef trustedLogs = CTTestsCopyTrustedLogs();
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *anchors = nil, *keychain_certs = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    NSDate *expiredDate = [NSDate dateWithTimeIntervalSinceReferenceDate:570000000.0]; // January 24, 2019 at 12:20:00 AM EST
    CFErrorRef error = nil;
    CFDataRef exceptions = nil;

    require_action(system_root = SecCertificateCreateFromResource(@"enforcement_system_root"),
                   errOut, fail("failed to create system root"));
    require_action(user_root = SecCertificateCreateFromResource(@"enforcement_user_root"),
                   errOut, fail("failed to create user root"));
    require_action(system_server_before = SecCertificateCreateFromResource(@"enforcement_system_server_before"),
                   errOut, fail("failed to create system server cert issued before flag day"));
    require_action(system_server_after = SecCertificateCreateFromResource(@"enforcement_system_server_after"),
                   errOut, fail("failed to create system server cert issued after flag day"));
    require_action(system_server_after_with_CT = SecCertificateCreateFromResource(@"enforcement_system_server_after_scts"),
                   errOut, fail("failed to create system server cert issued after flag day with SCTs"));
    require_action(user_server_after = SecCertificateCreateFromResource(@"enforcement_user_server_after"),
                   errOut, fail("failed to create user server cert issued after flag day"));

    /* set up the user and system roots to be used with trust settings */
    setup_for_user_trust(@[(__bridge id)system_root, (__bridge id)user_root, (__bridge id)system_server_after],
                         &keychain_certs);
    anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

#if 0 // Disable this test until we can mock MobileAsset and force asset to be out-of-date
    // Out-of-date asset, test system cert after date without CT passes
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert failed with out-of-date asset");
#endif

    // test system cert after date without CT fails
    require_noerr_action(SecTrustSetTrustedLogs(trust, trustedLogs), errOut, fail("failed to set trusted logs")); // set trusted logs to trigger enforcing behavior
    is(SecTrustEvaluateWithError(trust, &error), false, "system post-flag-date non-CT cert with in-date asset succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecVerifyActionFailed, "got wrong error code for non-ct cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), errSecVerifyActionFailed);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }

    // test expired system cert after date without CT passes with only expiration error
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)expiredDate), errOut, fail("failed to set verify date"));
    ok(SecTrustIsExpiredOnly(trust), "expired system post-flag-date non-CT cert had non-expiration errors");
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    // test exceptions for failing cert passes
    exceptions = SecTrustCopyExceptions(trust);
    ok(SecTrustSetExceptions(trust, exceptions), "failed to set exceptions for failing non-CT cert");
    CFReleaseNull(exceptions);
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert failed with exceptions set");
    SecTrustSetExceptions(trust, NULL);

    // test system cert + enterprise anchor after date without CT fails
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull" /* required because of <rdar://32627101> */
    require_noerr_action(SecTrustSetAnchorCertificates(trust, NULL), errOut, fail("failed to unset anchors"));
#pragma clang diagnostic pop
    require_action(set_trust_settings_for_cert(system_root), errOut, fail("failed to set trust settings for system_root"));
    is(SecTrustEvaluateWithError(trust, &error), false, "system post-flag date non-CT cert with enterprise root trust succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecVerifyActionFailed, "got wrong error code for non-ct cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), errSecVerifyActionFailed);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }
    require_action(remove_trust_settings_for_cert(system_root), errOut, fail("failed to remove trust settings for system_root"));

    // test app anchor for failing cert passes
    anchors = @[ (__bridge id)system_server_after ];
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchors"));
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert failed with server cert app anchor");

    // test trust settings for failing cert passes
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull" /* required because of <rdar://32627101> */
    require_noerr_action(SecTrustSetAnchorCertificates(trust, NULL), errOut, fail("failed to remove anchors"));
#pragma clang diagnostic pop
    require_action(set_trust_settings_for_cert(system_server_after), errOut, fail("failed to set trust settings for system_server_after"));
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert failed with server cert enterprise anchor");
    require_action(remove_trust_settings_for_cert(system_server_after), errOut, fail("failed to remove trust settings for system_server_after"));

    // EAP, test system cert after date without CT passes
    anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchors"));
    CFReleaseNull(policy);
    policy = SecPolicyCreateEAP(true, NULL);
    require_noerr_action(SecTrustSetPolicies(trust, policy), errOut, fail("failed to set EAP policy"));
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert failed with EAP cert");

    // Test pinning policy name
    CFReleaseNull(policy);
    policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    require_noerr_action(SecTrustSetPolicies(trust, policy), errOut, fail("failed to set SSL policy"));
    require_noerr_action(SecTrustSetPinningPolicyName(trust, CFSTR("a-policy-name")), errOut, fail("failed to set policy name"));
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert failed with pinning policy name");
    CFReleaseNull(trust);

    // test system cert after date with CT passes
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after_with_CT, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, trustedLogs), errOut, fail("failed to set trusted logs"));
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date CT cert failed");
    CFReleaseNull(trust);

    // test system cert before date without CT passes
    require_noerr_action(SecTrustCreateWithCertificates(system_server_before, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, trustedLogs), errOut, fail("failed to set trusted logs"));
    ok(SecTrustEvaluateWithError(trust, NULL), "system pre-flag-date non-CT cert failed");
    CFReleaseNull(trust);

    // test enterprise (non-public) after date without CT passes
    require_action(set_trust_settings_for_cert(user_root), errOut, fail("failed to set trust settings for user_root"));
    require_noerr_action(SecTrustCreateWithCertificates(user_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, trustedLogs), errOut, fail("failed to set trusted logs"));
    ok(SecTrustEvaluateWithError(trust, NULL), "non-system post-flag-date non-CT cert failed with enterprise anchor");
    require_action(remove_trust_settings_for_cert(user_root), errOut, fail("failed to remove trust settings for user_root"));

    // test app anchor (non-public) after date without CT passes
    anchors = @[ (__bridge id)user_root ];
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchors"));
    ok(SecTrustEvaluateWithError(trust, NULL), "non-system post-flag-date non-CT cert failed with app anchor");
    CFReleaseNull(trust);
    CFReleaseNull(policy);

errOut:
    cleanup_keychain(keychain_certs);
    CFReleaseNull(system_root);
    CFReleaseNull(user_root);
    CFReleaseNull(system_server_before);
    CFReleaseNull(system_server_after);
    CFReleaseNull(system_server_after_with_CT);
    CFReleaseNull(user_server_after);
    CFReleaseNull(trustedLogs);
    CFReleaseNull(trust);
    CFReleaseNull(policy);
}

static void test_apple_enforcement_exceptions(void) {
    SecCertificateRef appleRoot = NULL, appleServerAuthCA = NULL, apple_server_after = NULL;
    SecCertificateRef geoTrustRoot = NULL, appleISTCA8G1 = NULL, deprecatedSSLServer = NULL;
    CFArrayRef trustedLogs = CTTestsCopyTrustedLogs();
    SecTrustRef trust = NULL;
    SecPolicyRef policy = NULL;
    NSArray *anchors = nil, *certs = nil;
    NSDate *date1 = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    NSDate *date2 = [NSDate dateWithTimeIntervalSinceReferenceDate:576000000.0]; // April 3, 2019 at 9:00:00 AM PDT

    require_action(appleRoot = SecCertificateCreateFromResource(@"enforcement_apple_root"),
                   errOut, fail("failed to create apple root"));
    require_action(appleServerAuthCA = SecCertificateCreateFromResource(@"enforcement_apple_ca"),
                   errOut, fail("failed to create apple server auth CA"));
    require_action(apple_server_after = SecCertificateCreateFromResource(@"enforcement_apple_server_after"),
                   errOut, fail("failed to create apple server cert issued after flag day"));
    require_action(geoTrustRoot = SecCertificateCreateFromResource(@"GeoTrustPrimaryCAG2"),
                   errOut, fail("failed to create GeoTrust root"));
    require_action(appleISTCA8G1 = SecCertificateCreateFromResource(@"AppleISTCA8G1"),
                   errOut, fail("failed to create apple IST CA"));
    require_action(deprecatedSSLServer = SecCertificateCreateFromResource(@"deprecatedSSLServer"),
                   errOut, fail("failed to create livability cert"));

    // test apple anchor after date without CT passes
    policy = SecPolicyCreateSSL(true, CFSTR("bbasile-test.scv.apple.com"));
    certs = @[ (__bridge id)apple_server_after, (__bridge id)appleServerAuthCA ];
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date1), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, trustedLogs), errOut, fail("failed to set trusted logs"));
    ok(SecTrustEvaluateWithError(trust, NULL), "apple root post-flag-date non-CT cert failed");
    CFReleaseNull(trust);
    CFReleaseNull(policy);

    // test apple ca after date without CT passes
    policy = SecPolicyCreateSSL(true, CFSTR("bbasile-test.scv.apple.com"));
    certs = @[ (__bridge id)deprecatedSSLServer, (__bridge id)appleISTCA8G1 ];
    anchors = @[ (__bridge id)geoTrustRoot ];
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date2), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, trustedLogs), errOut, fail("failed to set trusted logs"));
    ok(SecTrustEvaluateWithError(trust, NULL), "apple public post-flag-date non-CT cert failed");

errOut:
    CFReleaseNull(appleRoot);
    CFReleaseNull(appleServerAuthCA);
    CFReleaseNull(apple_server_after);
    CFReleaseNull(geoTrustRoot);
    CFReleaseNull(appleISTCA8G1);
    CFReleaseNull(deprecatedSSLServer);
    CFReleaseNull(trustedLogs);
    CFReleaseNull(trust);
    CFReleaseNull(policy);
}

static void test_google_enforcement_exception(void) {
    SecCertificateRef globalSignRoot = NULL, googleIAG3 = NULL, google = NULL;
    CFArrayRef trustedLogs = CTTestsCopyTrustedLogs();
    SecTrustRef trust = NULL;
    SecPolicyRef policy = NULL;
    NSArray *anchors = nil, *certs = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT

    require_action(globalSignRoot = SecCertificateCreateFromResource(@"GlobalSignRootCAR2"),
                   errOut, fail("failed to create GlobalSign root"));
    require_action(googleIAG3 = SecCertificateCreateFromResource(@"GoogleIAG3"),
                   errOut, fail("failed to create Google IA CA"));
    require_action(google = SecCertificateCreateFromResource(@"google"),
                   errOut, fail("failed to create google server cert"));

    // test google ca after date without CT passes
    policy = SecPolicyCreateSSL(true, CFSTR("www.google.com"));
    certs = @[ (__bridge id)google, (__bridge id)googleIAG3];
    anchors = @[ (__bridge id)globalSignRoot ];
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, trustedLogs), errOut, fail("failed to set trusted logs"));
    ok(SecTrustEvaluateWithError(trust, NULL), "google public post-flag-date non-CT cert failed");

errOut:
    CFReleaseNull(globalSignRoot);
    CFReleaseNull(googleIAG3);
    CFReleaseNull(google);
    CFReleaseNull(trustedLogs);
    CFReleaseNull(trust);
    CFReleaseNull(policy);
}

static void test_precerts_fail(void) {
    SecCertificateRef precert = NULL, system_root = NULL;
    SecTrustRef trust = NULL;
    NSArray *anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:561540800.0]; // October 18, 2018 at 12:33:20 AM PDT
    CFErrorRef error = NULL;

    require_action(system_root = SecCertificateCreateFromResource(@"enforcement_system_root"),
                   errOut, fail("failed to create system root"));
    require_action(precert = SecCertificateCreateFromResource(@"precert"),
                   errOut, fail("failed to create precert"));

    anchors = @[(__bridge id)system_root];
    require_noerr_action(SecTrustCreateWithCertificates(precert, NULL, &trust), errOut, fail("failed to create trust object"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchor certificate"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    is(SecTrustEvaluateWithError(trust, &error), false, "SECURITY: trust evaluation of precert succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecUnknownCriticalExtensionFlag, "got wrong error code for precert, got %ld, expected %d",
           (long)CFErrorGetCode(error), errSecUnknownCriticalExtensionFlag);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }


errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(precert);
    CFReleaseNull(error);
}

#define evalTrustExpectingError(errCode, ...) \
    is(SecTrustEvaluateWithError(trust, &error), false, __VA_ARGS__); \
    if (error) { \
        is(CFErrorGetCode(error), errCode, "got wrong error code, got %ld, expected %d", \
            (long)CFErrorGetCode(error), errCode); \
    } else { \
        fail("expected trust evaluation to fail and it did not."); \
    } \
    CFReleaseNull(error);

static void test_specific_domain_exceptions(void) {
    SecCertificateRef system_root = NULL, system_server_after = NULL, system_server_after_with_CT = NULL;
    CFArrayRef trustedLogs = CTTestsCopyTrustedLogs();
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;
    NSDictionary *exceptions = nil;

    require_action(system_root = SecCertificateCreateFromResource(@"enforcement_system_root"),
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = SecCertificateCreateFromResource(@"enforcement_system_server_after"),
                   errOut, fail("failed to create system server cert issued after flag day"));
    require_action(system_server_after_with_CT = SecCertificateCreateFromResource(@"enforcement_system_server_after_scts"),
                   errOut, fail("failed to create system server cert issued after flag day with SCTs"));

    anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, trustedLogs), errOut, fail("failed to set trusted logs")); // set trusted logs to trigger enforcing behavior

    /* superdomain exception without CT fails */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@"test.apple.com"] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    evalTrustExpectingError(errSecVerifyActionFailed, "superdomain exception unexpectedly succeeded");

    /* subdomain exceptions without CT fails */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@"one.ct.test.apple.com"] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    evalTrustExpectingError(errSecVerifyActionFailed, "subdomain exception unexpectedly succeeded")

    /* no match without CT fails */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@"example.com"] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    evalTrustExpectingError(errSecVerifyActionFailed, "unrelated domain exception unexpectedly succeeded");

    /* matching domain without CT succeeds */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@"ct.test.apple.com"] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    is(SecTrustEvaluateWithError(trust, &error), true, "exact match domain exception did not apply");

    /* matching domain with CT succeeds */
    CFReleaseNull(trust);
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after_with_CT, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, trustedLogs), errOut, fail("failed to set trusted logs")); // set trusted logs to trigger enforcing behavior
    is(SecTrustEvaluateWithError(trust, &error), true, "ct cert should always pass");

    ok(SecTrustStoreSetCTExceptions(NULL, NULL, &error), "failed to reset exceptions: %@", error);

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(system_server_after_with_CT);
    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(error);
    CFReleaseNull(trustedLogs);
}

static void test_subdomain_exceptions(void) {
    SecCertificateRef system_root = NULL, system_server_after = NULL, system_server_after_with_CT = NULL;
    CFArrayRef trustedLogs = CTTestsCopyTrustedLogs();
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;
    NSDictionary *exceptions = nil;

    require_action(system_root = SecCertificateCreateFromResource(@"enforcement_system_root"),
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = SecCertificateCreateFromResource(@"enforcement_system_server_after"),
                   errOut, fail("failed to create system server cert issued after flag day"));
    require_action(system_server_after_with_CT = SecCertificateCreateFromResource(@"enforcement_system_server_after_scts"),
                   errOut, fail("failed to create system server cert issued after flag day with SCTs"));

    anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, trustedLogs), errOut, fail("failed to set trusted logs")); // set trusted logs to trigger enforcing behavior

    /* superdomain exception without CT succeeds */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@".test.apple.com"] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    is(SecTrustEvaluateWithError(trust, &error), true, "superdomain exception did not apply");

    /* exact domain exception without CT succeeds */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@".ct.test.apple.com"] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    is(SecTrustEvaluateWithError(trust, &error), true, "exact domain exception did not apply");

    /* no match without CT fails */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@".example.com"] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    evalTrustExpectingError(errSecVerifyActionFailed, "unrelated domain exception unexpectedly succeeded");

    /* subdomain without CT fails */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@".one.ct.test.apple.com"] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    evalTrustExpectingError(errSecVerifyActionFailed, "subdomain exception unexpectedly succeeded");

    ok(SecTrustStoreSetCTExceptions(NULL, NULL, &error), "failed to reset exceptions: %@", error);

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(system_server_after_with_CT);
    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(error);
    CFReleaseNull(trustedLogs);
}

static void test_mixed_domain_exceptions(void) {
    SecCertificateRef system_root = NULL, system_server_after = NULL, system_server_after_with_CT = NULL;
    CFArrayRef trustedLogs = CTTestsCopyTrustedLogs();
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;
    NSDictionary *exceptions = nil;

    require_action(system_root = SecCertificateCreateFromResource(@"enforcement_system_root"),
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = SecCertificateCreateFromResource(@"enforcement_system_server_after"),
                   errOut, fail("failed to create system server cert issued after flag day"));
    require_action(system_server_after_with_CT = SecCertificateCreateFromResource(@"enforcement_system_server_after_scts"),
                   errOut, fail("failed to create system server cert issued after flag day with SCTs"));

    anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, trustedLogs), errOut, fail("failed to set trusted logs")); // set trusted logs to trigger enforcing behavior

    /* specific domain exception without CT succeeds */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@"ct.test.apple.com", @".example.com" ] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    is(SecTrustEvaluateWithError(trust, &error), true, "one of exact domain exception did not apply");

    /* super domain exception without CT succeeds */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@".apple.com", @"example.com" ] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    is(SecTrustEvaluateWithError(trust, &error), true, "one of superdomain exception did not apply");

    /* both super domain and specific domain exceptions without CT succeeds */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@"ct.test.apple.com", @".apple.com" ] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    is(SecTrustEvaluateWithError(trust, &error), true, "both domain exception did not apply");

    /* neither specific domain nor super domain exceptions without CT fails */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@"apple.com", @".example.com" ] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    evalTrustExpectingError(errSecVerifyActionFailed, "no match domain unexpectedly succeeded");

    ok(SecTrustStoreSetCTExceptions(NULL, NULL, &error), "failed to reset exceptions: %@", error);

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(system_server_after_with_CT);
    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(error);
    CFReleaseNull(trustedLogs);
}



static void test_ct_domain_exceptions(void) {
    test_specific_domain_exceptions();
    test_subdomain_exceptions();
    test_mixed_domain_exceptions();
}

static void test_ct_leaf_exceptions(void) {
    SecCertificateRef system_root = NULL, system_server_after = NULL, system_server_after_with_CT = NULL;
    CFArrayRef trustedLogs = CTTestsCopyTrustedLogs();
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;
    NSDictionary *leafException = nil, *exceptions = nil;
    NSData *leafHash = nil;

    require_action(system_root = SecCertificateCreateFromResource(@"enforcement_system_root"),
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = SecCertificateCreateFromResource(@"enforcement_system_server_after"),
                   errOut, fail("failed to create system server cert issued after flag day"));
    require_action(system_server_after_with_CT = SecCertificateCreateFromResource(@"enforcement_system_server_after_scts"),
                   errOut, fail("failed to create system server cert issued after flag day with SCTs"));

    anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, trustedLogs), errOut, fail("failed to set trusted logs")); // set trusted logs to trigger enforcing behavior

    /* set exception on leaf cert without CT */
    leafHash = CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(system_server_after));
    leafException = @{ (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
                   (__bridge NSString*)kSecCTExceptionsSPKIHashKey : leafHash,
    };
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsCAsKey : @[ leafException ] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error),
       "failed to set exceptions: %@", error);
    is(SecTrustEvaluateWithError(trust, &error), true, "leaf public key exception did not apply");

    /* set exception on leaf cert with CT */
    leafHash = CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(system_server_after_with_CT));
    leafException = @{ (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
                       (__bridge NSString*)kSecCTExceptionsSPKIHashKey : leafHash,
                       };
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsCAsKey : @[ leafException ] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error),
       "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    evalTrustExpectingError(errSecVerifyActionFailed, "leaf cert with no public key exceptions succeeded");

    /* matching public key with CT succeeds */
    CFReleaseNull(trust);
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after_with_CT, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, trustedLogs), errOut, fail("failed to set trusted logs")); // set trusted logs to trigger enforcing behavior
    is(SecTrustEvaluateWithError(trust, &error), true, "ct cert should always pass");

    ok(SecTrustStoreSetCTExceptions(NULL, NULL, &error), "failed to reset exceptions: %@", error);

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(system_server_after_with_CT);
    CFReleaseNull(trustedLogs);
    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(error);
}

static void test_ct_unconstrained_ca_exceptions(void) {
    SecCertificateRef root = NULL, subca = NULL;
    SecCertificateRef server_matching = NULL, server_matching_with_CT = NULL, server_partial = NULL, server_no_match = NULL, server_no_org = NULL;
    CFArrayRef trustedLogs = CTTestsCopyTrustedLogs();
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *anchors = nil, *certs = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;
    NSDictionary *caException = nil, *exceptions = nil;
    NSData *caHash = nil;

    require_action(root = SecCertificateCreateFromResource(@"enforcement_system_root"),
                   errOut, fail("failed to create system root"));
    require_action(subca = SecCertificateCreateFromResource(@"enforcement_system_unconstrained_subca"),
                   errOut, fail("failed to create subca"));
    require_action(server_matching = SecCertificateCreateFromResource(@"enforcement_system_server_matching_orgs"),
                   errOut, fail("failed to create server cert with matching orgs"));
    require_action(server_matching_with_CT = SecCertificateCreateFromResource(@"enforcement_system_server_matching_orgs_scts"),
                   errOut, fail("failed to create server cert with matching orgs and scts"));
    require_action(server_partial = SecCertificateCreateFromResource(@"enforcement_system_server_partial_orgs"),
                   errOut, fail("failed to create server cert with partial orgs"));
    require_action(server_no_match = SecCertificateCreateFromResource(@"enforcement_system_server_nonmatching_orgs"),
                   errOut, fail("failed to create server cert with non-matching orgs"));
    require_action(server_no_org = SecCertificateCreateFromResource(@"enforcement_system_server_no_orgs"),
                   errOut, fail("failed to create server cert with no orgs"));

    anchors = @[ (__bridge id)root ];

#define createTrust(certs) \
    CFReleaseNull(trust); \
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), errOut, fail("failed to create trust")); \
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchors")); \
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date")); \
    require_noerr_action(SecTrustSetTrustedLogs(trust, trustedLogs), errOut, fail("failed to set trusted logs"));

    /* Set exception on the subCA */
    caHash = CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(subca));
    caException = @{ (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
                       (__bridge NSString*)kSecCTExceptionsSPKIHashKey : caHash,
                       };
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsCAsKey : @[ caException ] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error),
       "failed to set exceptions: %@", error);

    /* Verify that non-CT cert with Orgs matching subCA passes */
    certs = @[ (__bridge id)server_matching, (__bridge id)subca];
    createTrust(certs);
    is(SecTrustEvaluateWithError(trust, &error), true, "matching org subca exception did not apply: %@", error);

    /* Verify that CT cert with Orgs matching subCA passes */
    certs = @[ (__bridge id)server_matching_with_CT, (__bridge id)subca];
    createTrust(certs);
    is(SecTrustEvaluateWithError(trust, &error), true, "CT matching org subca exception did not apply: %@", error);

    /* Verify that non-CT cert with partial Org match fails */
    certs = @[ (__bridge id)server_partial, (__bridge id)subca];
    createTrust(certs);
    evalTrustExpectingError(errSecVerifyActionFailed, "partial matching org leaf succeeded");

    /* Verify that a non-CT cert with non-matching Org fails */
    certs = @[ (__bridge id)server_no_match, (__bridge id)subca];
    createTrust(certs);
    evalTrustExpectingError(errSecVerifyActionFailed, "non-matching org leaf succeeded");

    /* Verify that a non-CT cert with no Org fails */
    certs = @[ (__bridge id)server_no_org, (__bridge id)subca];
    createTrust(certs);
    evalTrustExpectingError(errSecVerifyActionFailed, "no org leaf succeeded");

    ok(SecTrustStoreSetCTExceptions(NULL, NULL, &error), "failed to reset exceptions: %@", error);

#undef createTrust

errOut:
    CFReleaseNull(root);
    CFReleaseNull(subca);
    CFReleaseNull(server_matching);
    CFReleaseNull(server_matching_with_CT);
    CFReleaseNull(server_partial);
    CFReleaseNull(server_no_match);
    CFReleaseNull(server_no_org);
    CFReleaseNull(trustedLogs);
    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(error);
}

static void test_ct_constrained_ca_exceptions(void) {
    SecCertificateRef root = NULL, org_constrained_subca = NULL;
    SecCertificateRef constraint_pass_server = NULL, constraint_pass_server_ct = NULL, constraint_fail_server = NULL;
    SecCertificateRef dn_constrained_subca = NULL, dn_constrained_server = NULL, dn_constrained_server_mismatch = NULL;
    SecCertificateRef dns_constrained_subca = NULL, dns_constrained_server = NULL, dns_constrained_server_mismatch = NULL;
    CFArrayRef trustedLogs = CTTestsCopyTrustedLogs();
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *anchors = nil, *certs = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;
    NSDictionary *caException = nil, *exceptions = nil;
    NSMutableArray *caExceptions = [NSMutableArray array];
    NSData *caHash = nil;

    require_action(root = SecCertificateCreateFromResource(@"enforcement_system_root"),
                   errOut, fail("failed to create system root"));
    require_action(org_constrained_subca = SecCertificateCreateFromResource(@"enforcement_system_constrained_subca"),
                   errOut, fail("failed to create org-constrained subca"));
    require_action(constraint_pass_server = SecCertificateCreateFromResource(@"enforcement_system_constrained_server"),
                   errOut, fail("failed to create constrained non-CT leaf"));
    require_action(constraint_pass_server_ct = SecCertificateCreateFromResource(@"enforcement_system_constrained_server_scts"),
                   errOut, fail("failed to create constrained CT leaf"));
    require_action(constraint_fail_server= SecCertificateCreateFromResource(@"enforcement_system_constrained_fail_server"),
                   errOut, fail("failed to create constraint failure leaf"));
    require_action(dn_constrained_subca = SecCertificateCreateFromResource(@"enforcement_system_constrained_no_org_subca"),
                   errOut, fail("failed to create dn-constrained subca"));
    require_action(dn_constrained_server = SecCertificateCreateFromResource(@"enforcement_system_constrained_no_org_server"),
                   errOut, fail("failed to create dn-constrained leaf"));
    require_action(dn_constrained_server_mismatch = SecCertificateCreateFromResource(@"enforcement_system_constrained_no_org_server_mismatch"),
                   errOut, fail("failed to create dn-constrained leaf with mismatched orgs"));
    require_action(dns_constrained_subca = SecCertificateCreateFromResource(@"enforcement_system_constrained_no_dn_subca"),
                   errOut, fail("failed to create dns-constrained subca"));
    require_action(dns_constrained_server = SecCertificateCreateFromResource(@"enforcement_system_constrained_no_dn_server"),
                   errOut, fail("failed to create dns-constrained leaf"));
    require_action(dns_constrained_server_mismatch = SecCertificateCreateFromResource(@"enforcement_system_constrained_no_dn_server_mismatch"),
                   errOut, fail("failed to create dns-constrained leaf with mismatched orgs"));

    anchors = @[ (__bridge id)root ];

    /* Set exception on the subCAs */
    caHash = CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(org_constrained_subca));
    caException = @{ (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
                     (__bridge NSString*)kSecCTExceptionsSPKIHashKey : caHash,
                     };
    [caExceptions addObject:caException];

    caHash = CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(dn_constrained_subca));
    caException = @{ (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
                     (__bridge NSString*)kSecCTExceptionsSPKIHashKey : caHash,
                     };
    [caExceptions addObject:caException];

    caHash = CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(dns_constrained_subca));
    caException = @{ (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
                     (__bridge NSString*)kSecCTExceptionsSPKIHashKey : caHash,
                     };
    [caExceptions addObject:caException];
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsCAsKey : caExceptions };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error),
       "failed to set exceptions: %@", error);

#define createTrust(certs) \
    CFReleaseNull(trust); \
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), errOut, fail("failed to create trust")); \
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut, fail("failed to set anchors")); \
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date")); \
    require_noerr_action(SecTrustSetTrustedLogs(trust, trustedLogs), errOut, fail("failed to set trusted logs"));

    /* Verify org-constrained non-CT leaf passes */
    certs = @[ (__bridge id)constraint_pass_server, (__bridge id)org_constrained_subca ];
    createTrust(certs);
    is(SecTrustEvaluateWithError(trust, &error), true, "org constrained exception did not apply: %@", error);

    /* Verify org-constrained CT leaf passes */
    certs = @[ (__bridge id)constraint_pass_server_ct, (__bridge id)org_constrained_subca ];
    createTrust(certs);
    is(SecTrustEvaluateWithError(trust, &error), true, "org constrained exception did not apply: %@", error);

    /* Verify org-constrained non-CT leaf with wrong org fails */
    certs = @[ (__bridge id)constraint_fail_server, (__bridge id)org_constrained_subca ];
    createTrust(certs);
    evalTrustExpectingError(errSecInvalidName, "leaf failing name constraints succeeded");

    /* Verify dn-constrained (but not with org) non-CT leaf with matching orgs succeeds */
    certs = @[ (__bridge id)dn_constrained_server, (__bridge id)dn_constrained_subca ];
    createTrust(certs);
    is(SecTrustEvaluateWithError(trust, &error), true, "org match exception did not apply: %@", error);

    /* Verify dn-constrained (but not with org) non-CT leaf without matching orgs fails */
    certs = @[ (__bridge id)dn_constrained_server_mismatch, (__bridge id)dn_constrained_subca ];
    createTrust(certs);
    evalTrustExpectingError(errSecVerifyActionFailed, "dn name constraints with no org succeeded");

    /* Verify dns-constrained (no DN constraints) non-CT leaf with matching orgs succeeds */
    certs = @[ (__bridge id)dns_constrained_server, (__bridge id)dns_constrained_subca ];
    createTrust(certs);
    is(SecTrustEvaluateWithError(trust, &error), true, "org match exception did not apply: %@", error);

    /* Verify dns-constrained (no DN constraints) non-CT leaf without matching orgs fails*/
    certs = @[ (__bridge id)dns_constrained_server_mismatch, (__bridge id)dns_constrained_subca ];
    createTrust(certs);
    evalTrustExpectingError(errSecVerifyActionFailed, "dns name constraints with no DN constraint succeeded");

    ok(SecTrustStoreSetCTExceptions(NULL, NULL, &error), "failed to reset exceptions: %@", error);

#undef createTrust

errOut:
    CFReleaseNull(root);
    CFReleaseNull(org_constrained_subca);
    CFReleaseNull(constraint_pass_server);
    CFReleaseNull(constraint_pass_server_ct);
    CFReleaseNull(constraint_fail_server);
    CFReleaseNull(dn_constrained_subca);
    CFReleaseNull(dn_constrained_server);
    CFReleaseNull(dn_constrained_server_mismatch);
    CFReleaseNull(dns_constrained_subca);
    CFReleaseNull(dns_constrained_server);
    CFReleaseNull(dns_constrained_server_mismatch);
    CFReleaseNull(trustedLogs);
    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(error);
}

static void test_ct_key_exceptions(void) {
    test_ct_leaf_exceptions();
    test_ct_unconstrained_ca_exceptions();
    test_ct_constrained_ca_exceptions();
}

static void test_ct_exceptions(void) {
    test_ct_domain_exceptions();
    test_ct_key_exceptions();
}

int si_82_sectrust_ct(int argc, char *const *argv)
{
	plan_tests(431);

	tests();
    test_sct_serialization();
    testSetCTExceptions();
    test_enforcement();
    test_apple_enforcement_exceptions();
    test_google_enforcement_exception();
    test_precerts_fail();
    test_ct_exceptions();

	return 0;
}
