/*
 *  si-82-sectrust-ct.c
 *  Security
 *
 *  Copyright (c) 2014 Apple Inc. All Rights Reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecPolicy.h>
#include <stdlib.h>
#include <unistd.h>
#include <utilities/SecCFWrappers.h>

#include "shared_regressions.h"

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



    isnt(policy = SecPolicyCreateSSL(false, hostname), NULL, "create policy");
    isnt(policies = CFArrayCreate(kCFAllocatorDefault, (const void **)&policy, 1, &kCFTypeArrayCallBacks), NULL, "create policies");
    ok_status(SecTrustCreateWithCertificates(certs, policies, &trust), "create trust");

    assert(trust); // silence analyzer
    if(anchors) {
        ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set anchors");
    }

    if(scts) {
        ok_status(SecTrustSetSignedCertificateTimestamps(trust, scts), "set standalone SCTs");;
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
    NSURL *url = [[NSBundle mainBundle] URLForResource:name withExtension:@".crt" subdirectory:@"si-82-sectrust-ct-data"];

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

static void tests()
{
    SecCertificateRef certA=NULL, certD=NULL, certF=NULL, certCA_alpha=NULL, certCA_beta=NULL;
    CFDataRef proofD=NULL, proofA_1=NULL, proofA_2=NULL;
    SecCertificateRef www_digicert_com_2015_cert=NULL, www_digicert_com_2016_cert=NULL, digicert_sha2_ev_server_ca=NULL;
    SecCertificateRef www_paypal_com_cert=NULL, www_paypal_com_issuer_cert=NULL;
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

    CFArrayRef trustedLogs=NULL;
    CFURLRef trustedLogsURL=NULL;

    trustedLogsURL = CFBundleCopyResourceURL(CFBundleGetMainBundle(),
                                             CFSTR("CTlogs"),
                                             CFSTR("plist"),
                                             CFSTR("si-82-sectrust-ct-data"));
    isnt(trustedLogsURL, NULL, "trustedLogsURL");
    trustedLogs = (CFArrayRef) CFPropertyListReadFromFile(trustedLogsURL);
    isnt(trustedLogs, NULL, "trustedLogs");

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
    isnt(www_paypal_com_cert = SecCertificateCreateFromResource(@"www_paypal_com"), NULL, "create www.paypal.com cert");
    isnt(www_paypal_com_issuer_cert = SecCertificateCreateFromResource(@"www_paypal_com_issuer"), NULL, "create www.paypal.com issuer cert");
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
    test_ct_trust(certs, NULL, NULL, anchors, trustedLogs, CFSTR("coreos-ct-test.apple.com"), date_20150307,
                  false, false, false, "coreos-ct-test 1");
    CFReleaseNull(certs);

    /* Case 2: coreos-ct-test standalone SCT - only 1 SCT - so not CT qualified  */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, certD);
    isnt(scts = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create SCT array");
    CFArrayAppendValue(scts, proofD);
    test_ct_trust(certs, scts, NULL, anchors, trustedLogs, CFSTR("coreos-ct-test.apple.com"), date_20150307,
                  false, false, false, "coreos-ct-test 2");
    CFReleaseNull(certs);
    CFReleaseNull(scts);

    /* case 3: digicert : 2 embedded SCTs, but lifetime of cert is 24 month, so not CT qualified, but is whitelisted */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, www_digicert_com_2015_cert);
    CFArrayAppendValue(certs, digicert_sha2_ev_server_ca);
    test_ct_trust(certs, NULL, NULL, NULL, NULL, CFSTR("www.digicert.com"), date_20150307,
                  false, true, true, "digicert 2015");
    CFReleaseNull(certs);

    /* case 4: paypal.com cert - not CT, but EV */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, www_paypal_com_cert);
    CFArrayAppendValue(certs, www_paypal_com_issuer_cert);
    test_ct_trust(certs, NULL, NULL, NULL, trustedLogs, CFSTR("www.paypal.com"), date_20150307,
                  false, true, false, "paypal");
    CFReleaseNull(certs);

    /* Case 5: coreos-ct-test standalone SCT -  2 SCTs - CT qualified  */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, certA);
    isnt(scts = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create SCT array");
    CFArrayAppendValue(scts, proofA_1);
    CFArrayAppendValue(scts, proofA_2);
    test_ct_trust(certs, scts, NULL, anchors, trustedLogs, CFSTR("coreos-ct-test.apple.com"), date_20150307,
                  true,  false, false, "coreos-ct-test 3");
    CFReleaseNull(certs);
    CFReleaseNull(scts);


    /* Case 6: Test with an invalid OCSP response */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, certA);
    isnt(scts = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create SCT array");
    CFArrayAppendValue(scts, proofA_1);
    test_ct_trust(certs, scts, invalid_ocsp, anchors, trustedLogs, CFSTR("coreos-ct-test.apple.com"), date_20150307,
                  false, false, false, "coreos-ct-test 4");
    CFReleaseNull(certs);
    CFReleaseNull(scts);

    /* Case 7: Test with a valid OCSP response */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, certA);
    isnt(scts = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create SCT array");
    CFArrayAppendValue(scts, proofA_1);
    test_ct_trust(certs, scts, valid_ocsp, anchors, trustedLogs, CFSTR("coreos-ct-test.apple.com"), date_20150307,
                  false, false, false, "coreos-ct-test 5");
    CFReleaseNull(certs);
    CFReleaseNull(scts);

    /* Case 8: Test with a bad hash OCSP response */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, certA);
    isnt(scts = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create SCT array");
    CFArrayAppendValue(scts, proofA_1);
    test_ct_trust(certs, scts, bad_hash_ocsp, anchors, trustedLogs, CFSTR("coreos-ct-test.apple.com"), date_20150307,
                  false, false, false, "coreos-ct-test 6");
    CFReleaseNull(certs);
    CFReleaseNull(scts);

    /* Case 9: Previously WhiteListed EV cert (expired in Feb 2016, so not on final whitelist)*/
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, pilot_cert_3055998);
    CFArrayAppendValue(certs, pilot_cert_3055998_issuer);
    test_ct_trust(certs, NULL, NULL, NULL, NULL, CFSTR("www.ssbwingate.com"), date_20150307,
                  false, true, false, "previously whitelisted cert");
    CFReleaseNull(certs);

    /* Case 10-13: WhiteListed EV cert */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, whitelist_00008013);
    CFArrayAppendValue(certs, whitelist_00008013_issuer);
    test_ct_trust(certs, NULL, NULL, NULL, NULL, CFSTR("clava.com"), date_20150307,
                  false, true, true, "whitelisted cert 00008013");
    CFReleaseNull(certs);

    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, whitelist_5555bc4f);
    CFArrayAppendValue(certs, whitelist_5555bc4f_issuer);
    test_ct_trust(certs, NULL, NULL, NULL, NULL, CFSTR("lanai.dartmouth.edu"),
                  date_20150307, false, true, true, "whitelisted cert 5555bc4f");
    CFReleaseNull(certs);

    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, whitelist_aaaae152);
    CFArrayAppendValue(certs, whitelist_5555bc4f_issuer); // Same issuer (Go Daddy) as above
    test_ct_trust(certs, NULL, NULL, NULL, NULL, CFSTR("www.falymusic.com"),
                  date_20150307, false, true, true, "whitelisted cert aaaae152");
    CFReleaseNull(certs);

    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, whitelist_fff9b5f6);
    CFArrayAppendValue(certs, whitelist_fff9b5f6_issuer);
    test_ct_trust(certs, NULL, NULL, NULL, NULL, CFSTR("www.defencehealth.com.au"),
                  date_20150307, false, true, true, "whitelisted cert fff9b5f6");
    CFReleaseNull(certs);


    /* case 14: Current (April 2016) www.digicert.com cert: 3 embedded SCTs, CT qualified */
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    CFArrayAppendValue(certs, www_digicert_com_2016_cert);
    CFArrayAppendValue(certs, digicert_sha2_ev_server_ca);
    test_ct_trust(certs, NULL, NULL, NULL, NULL, CFSTR("www.digicert.com"), date_20160422,
                  true, true, false, "digicert 2016");
    CFReleaseNull(certs);



#define TEST_CASE(x) \
 do { \
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array for " #x); \
    isnt(cfCert = SecCertificateCreateFromResource(@#x), NULL, "create cfCert from " #x); \
    CFArrayAppendValue(certs, cfCert); \
    test_ct_trust(certs, NULL, NULL, anchors, trustedLogs, CFSTR("coreos-ct-test.apple.com"), date_20150307, true, false, false, #x); \
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
    CFReleaseSafe(www_paypal_com_cert);
    CFReleaseSafe(www_paypal_com_issuer_cert);
    CFReleaseSafe(pilot_cert_3055998);
    CFReleaseSafe(pilot_cert_3055998_issuer);
    CFReleaseSafe(whitelist_00008013);
    CFReleaseSafe(whitelist_5555bc4f);
    CFReleaseSafe(whitelist_aaaae152);
    CFReleaseSafe(whitelist_fff9b5f6);
    CFReleaseSafe(whitelist_00008013_issuer);
    CFReleaseSafe(whitelist_5555bc4f_issuer);
    CFReleaseSafe(whitelist_fff9b5f6_issuer);
    CFReleaseSafe(trustedLogsURL);
    CFReleaseSafe(trustedLogs);
    CFReleaseSafe(valid_ocsp);
    CFReleaseSafe(invalid_ocsp);
    CFReleaseSafe(bad_hash_ocsp);
    CFReleaseSafe(cal);
    CFReleaseSafe(date_20150307);
    CFReleaseSafe(date_20160422);

}


int si_82_sectrust_ct(int argc, char *const *argv)
{
	plan_tests(329);

	tests();

	return 0;
}
