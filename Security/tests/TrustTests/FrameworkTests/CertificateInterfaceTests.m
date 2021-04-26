/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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
 *
 */

#include <AssertMacros.h>
#import <XCTest/XCTest.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include "OSX/utilities/SecCFWrappers.h"
#include <Security/SecTrustSettings.h>
#include <Security/SecTrustSettingsPriv.h>
#include "OSX/sec/Security/SecFramework.h"
#include "OSX/utilities/SecCFWrappers.h"
#include "trust/trustd/OTATrustUtilities.h"
#include <stdlib.h>
#include <unistd.h>
#include <utilities/SecCFRelease.h>
#include "../TestMacroConversions.h"
#include "TrustFrameworkTestCase.h"
#include <libDER/oids.h>

#include "CertificateInterfaceTests_data.h"

@interface CertificateInterfaceTests : TrustFrameworkTestCase

@end

@implementation CertificateInterfaceTests

- (void)testCTInterfaces {
    SecCertificateRef certF = NULL;

    CFDataRef precertTBS = NULL;
    CFArrayRef proofs = NULL;
    CFDataRef spkiDigest = NULL;

    isnt(certF = SecCertificateCreateWithBytes(NULL, serverF_cert_der, sizeof(serverF_cert_der)), NULL, "create certF");

    isnt(precertTBS = SecCertificateCopyPrecertTBS(certF), NULL, "copy precertTBS");
    XCTAssertEqualObjects((__bridge NSData *)precertTBS, [NSData dataWithBytes:serverF_pre_cert_der length:sizeof(serverF_pre_cert_der)],
                          "Pre-cert TBS for serverF incorrect");

    isnt(spkiDigest = SecCertificateCopySubjectPublicKeyInfoSHA256Digest(certF), NULL, "copy SPKI digest");
    XCTAssertEqualObjects((__bridge NSData *)spkiDigest, [NSData dataWithBytes:serverF_SPKI_hash length:sizeof(serverF_SPKI_hash)],
                          "SPKI digest for serverF incorrect");

    isnt(proofs = SecCertificateCopySignedCertificateTimestamps(certF), NULL, "copy SCTs");
    NSArray *expectedProofs = @[[NSData dataWithBytes:serverF_sct length:sizeof(serverF_sct)]];
    XCTAssertEqualObjects((__bridge NSArray*)proofs, expectedProofs , "SCT array for serverF incorrect");

    CFReleaseSafe(certF);
    CFReleaseSafe(precertTBS);
    CFReleaseSafe(proofs);
    CFReleaseSafe(spkiDigest);
}

- (void)testCreation {
    SecCertificateRef cert0 = NULL, cert2 = NULL, cert4 = NULL;
    isnt(cert0 = SecCertificateCreateWithBytes(NULL, _c0, sizeof(_c0)),
         NULL, "create cert0");
    
    CFDataRef cert2Data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                                      _c2, sizeof(_c2), kCFAllocatorNull);
    isnt(cert2 = SecCertificateCreateWithData(kCFAllocatorDefault, cert2Data),
         NULL, "create cert2");
    CFReleaseNull(cert2Data);
    
    CFDataRef cert4data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                                      pem, sizeof(pem), kCFAllocatorNull);
    ok(cert4 = SecCertificateCreateWithPEM(NULL, cert4data), "create cert from pem");

    uint8_t random[32];
    (void)SecRandomCopyBytes(kSecRandomDefault, sizeof(random), random);
    NSData *randomData = [[NSData alloc] initWithBytes:random length:sizeof(random)];
    XCTAssert(NULL == SecCertificateCreateWithPEM(NULL, (__bridge CFDataRef)randomData));

    CFReleaseNull(cert4data);
    CFReleaseNull(cert0);
    CFReleaseNull(cert2);
    CFReleaseNull(cert4);
}

- (void)testSelfSignedCA {
    SecCertificateRef cert0 = NULL, cert1 = NULL, cert5 = NULL;
    isnt(cert0 = SecCertificateCreateWithBytes(NULL, _c0, sizeof(_c0)),
         NULL, "create cert0");
    isnt(cert1 = SecCertificateCreateWithBytes(NULL, _c1, sizeof(_c1)),
         NULL, "create cert1");
    isnt(cert5 = SecCertificateCreateWithBytes(NULL, _elektron_v1_cert_der,
                                               sizeof(_elektron_v1_cert_der)), NULL, "create cert5");
    
    ok(SecCertificateIsSelfSignedCA(cert0), "cert0 is CA");
    ok(!SecCertificateIsSelfSignedCA(cert1), "cert1 is not CA");
    ok(SecCertificateIsSelfSignedCA(cert5), "cert5 is v1 CA");
    
    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(cert5);
}

- (void)testSummary {
    SecCertificateRef cert1 = NULL, cert3 = NULL, cert4 = NULL;
    CFStringRef subjectSummary = NULL, issuerSummary = NULL;
    
    isnt(cert1 = SecCertificateCreateWithBytes(NULL, _c1, sizeof(_c1)),
         NULL, "create cert1");
    isnt(cert3 = SecCertificateCreateWithBytes(NULL, _phased_c3, sizeof(_phased_c3)),
         NULL, "create cert3");
    
    CFDataRef cert4data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                                      pem, sizeof(pem), kCFAllocatorNull);
    ok(cert4 = SecCertificateCreateWithPEM(NULL, cert4data), "create cert from pem");
    CFReleaseNull(cert4data);
    
    isnt(subjectSummary = SecCertificateCopySubjectSummary(cert1), NULL,
         "cert1 has a subject summary");
    isnt(issuerSummary = SecCertificateCopyIssuerSummary(cert1), NULL,
         "cert1 has an issuer summary");
    
    ok(subjectSummary && CFEqual(subjectSummary, CFSTR("www.verisign.com/CPS Incorp.by Ref. LIABILITY LTD.(c)97 VeriSign, VeriSign International Server CA - Class 3, VeriSign, Inc.")),
       "subject summary is \"www.verisign.com/CPS Incorp.by Ref. LIABILITY LTD.(c)97 VeriSign, VeriSign International Server CA - Class 3, VeriSign, Inc.\"");
    ok(issuerSummary && CFEqual(issuerSummary,
                                CFSTR("Class 3 Public Primary Certification Authority")),
       "issuer summary is \"Class 3 Public Primary Certification Authority\"");
    CFReleaseNull(subjectSummary);
    
    isnt(subjectSummary = SecCertificateCopySubjectSummary(cert3), NULL,
         "cert3 has a subject summary");
    /* @@@ this caused a double free without an extra retain in obtainSummaryFromX501Name():
     summary->description = string = copyDERThingDescription(kCFAllocatorDefault, value, true); */
    CFReleaseNull(subjectSummary);
    
    isnt(subjectSummary = SecCertificateCopySubjectSummary(cert4), NULL,
         "cert4 has a subject summary");
    ok(subjectSummary && CFEqual(subjectSummary, CFSTR("S5L8900 Secure Boot")),
       "cert4 is S5L8900 Secure Boot");
    
    CFReleaseNull(subjectSummary);
    CFReleaseNull(issuerSummary);
    CFReleaseNull(cert1);
    CFReleaseNull(cert3);
    CFReleaseNull(cert4);
}

- (void)testNTPrincipalName {
    SecCertificateRef cert2 = NULL;
    CFArrayRef ntPrincipalNames = NULL;
    
    isnt(cert2 = SecCertificateCreateWithBytes(NULL, _c2, sizeof(_c2)),
         NULL, "create cert2");
    
    ok(ntPrincipalNames = SecCertificateCopyNTPrincipalNames(cert2),
       "SecCertificateCopyNTPrincipalNames");
    is(CFArrayGetCount(ntPrincipalNames), 1, "we got 1 princialname back");
    CFStringRef principal = (CFStringRef)CFArrayGetValueAtIndex(ntPrincipalNames, 0);
    ok(CFEqual(principal, CFSTR("kmm6b@Virginia.EDU")),
       "first principal is kmm6b@Virginia.EDU");
    CFReleaseNull(ntPrincipalNames);
    CFReleaseNull(cert2);
}

- (void)testDescription {
    CFStringRef desc = NULL;
    SecCertificateRef cert4 = NULL;
    
    CFDataRef cert4data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                                      pem, sizeof(pem), kCFAllocatorNull);
    ok(cert4 = SecCertificateCreateWithPEM(NULL, cert4data), "create cert from pem");
    CFReleaseNull(cert4data);
    
    ok(desc = CFCopyDescription(cert4), "cert4 CFCopyDescription works");
    CFReleaseNull(desc);
    CFReleaseNull(cert4);
}

- (void)testHashes {
    SecCertificateRef cert0 = NULL;
    isnt(cert0 = SecCertificateCreateWithBytes(NULL, _c0, sizeof(_c0)),
         NULL, "create cert0");
    CFDataRef spki1Hash = SecCertificateCopySubjectPublicKeyInfoSHA1Digest(cert0);
    isnt(spki1Hash, NULL, "cert0 has a SHA-1 subject public key info hash");
    CFReleaseSafe(spki1Hash);
    
    CFDataRef spki2Hash = SecCertificateCopySubjectPublicKeyInfoSHA256Digest(cert0);
    isnt(spki2Hash, NULL, "cert0 has a SHA-256 subject public key info hash");
    CFReleaseSafe(spki2Hash);
    CFReleaseNull(cert0);
}

- (void)testCommonName {
    SecCertificateRef cert = NULL;
    CFStringRef commonName = NULL;
    
    XCTAssert(cert = SecCertificateCreateWithBytes(NULL, two_common_names, sizeof(two_common_names)), "failed to create cert");
    XCTAssertEqual(errSecSuccess, SecCertificateCopyCommonName(cert, &commonName),
                   "failed to copy common names");
    is(CFStringCompare(commonName, CFSTR("certxauthsplit"), 0), kCFCompareEqualTo, "copy common name got the wrong name");

    CFReleaseSafe(commonName);
    CFReleaseSafe(cert);
}

- (void)testCopyEmailAddresses {
    SecCertificateRef cert = SecCertificateCreateWithBytes(NULL, mail_google_com, sizeof (mail_google_com));
    CFArrayRef array = NULL;
    CFStringRef name = NULL;
    
    ok_status(SecCertificateCopyCommonName(cert, &name), "Failed to get common name from cert");
    ok(name, "Failed to get common name");
    ok(CFEqual(name, CFSTR("mail.google.com")), "Got wrong common name");
    
    ok_status(SecCertificateCopyEmailAddresses (cert, &array), "Failed to get email addresses from cert");
    ok(array, "Failed to get email address array");
    is(CFArrayGetCount(array), 0, "Found unexpected email addresses");
    
    CFReleaseNull(cert);
    CFReleaseNull(name);
    CFReleaseNull(array);
}

- (void)testCopyExtensionValue {
    SecCertificateRef cert = SecCertificateCreateWithBytes(NULL, mail_google_com, sizeof(mail_google_com));
    CFDataRef extension = NULL, expected = NULL, oid = NULL;
    bool critical = false;
    
    /* parameter fails */
    is(extension = SecCertificateCopyExtensionValue(NULL, CFSTR("1.2.3.4"), &critical), NULL,
       "NULL cert input succeeded");
    is(extension = SecCertificateCopyExtensionValue(cert, NULL, &critical), NULL,
       "NULL OID input succeeded");
    
    /* Extension not present */
    is(extension = SecCertificateCopyExtensionValue(cert, CFSTR("1.2.3.4"), &critical), NULL,
       "Got extension value for non-present extension OID");
    
    /* Using decimal OID, extension present and critical */
    isnt(extension = SecCertificateCopyExtensionValue(cert, CFSTR("2.5.29.19"), &critical), NULL,
         "Failed to get extension for present extension OID");
    is(critical, true, "Got wrong criticality for critical extension");
    uint8_t basic_constraints_value[2] = { 0x30, 0x00 };
    expected = CFDataCreate(NULL, basic_constraints_value, sizeof(basic_constraints_value));
    ok(CFEqual(extension, expected), "Got wrong extension value for basic constraints");
    CFReleaseNull(extension);
    CFReleaseNull(expected);
    
    /* Using binary OID, extension present and non critical */
    uint8_t eku_oid[3] = { 0x55, 0x01d, 0x25 };
    oid = CFDataCreate(NULL, eku_oid, sizeof(eku_oid));
    isnt(extension = SecCertificateCopyExtensionValue(cert, oid, &critical), NULL,
         "Failed to get extension for present extension OID");
    is(critical, false, "Got wrong criticality for non-critical extension");
    uint8_t eku_value[] = {
        0x30, 0x1f, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01, 0x06, 0x08, 0x2b, 0x06,
        0x01, 0x05, 0x05, 0x07, 0x03, 0x02, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x42, 0x04,
        0x01
    };
    expected = CFDataCreate(NULL, eku_value, sizeof(eku_value));
    ok(CFEqual(extension, expected), "Got wrong extension value for extended key usage");
    CFReleaseNull(oid);
    CFReleaseNull(extension);
    CFReleaseNull(expected);
    
    /* No critical output */
    isnt(extension = SecCertificateCopyExtensionValue(cert, CFSTR("2.5.29.19"), NULL), NULL,
         "Failed to get extension for present extension OID");
    CFReleaseNull(extension);
    
    /* messed up binary OIDs */
    is(extension = SecCertificateCopyExtensionValue(cert, CFSTR("abcd"), NULL), NULL,
       "letters in OID");
    is(extension = SecCertificateCopyExtensionValue(cert, CFSTR("8.1.1.2"), NULL), NULL,
       "bad first arc");
    is(extension = SecCertificateCopyExtensionValue(cert, CFSTR("10.1.1.1"), NULL), NULL,
       "longer bad first arc");
    is(extension = SecCertificateCopyExtensionValue(cert, CFSTR(""), NULL), NULL,
       "empty string");
    is(extension = SecCertificateCopyExtensionValue(cert, CFSTR("1.2.1099511627776."), NULL), NULL,
       "six byte component");
    
    CFReleaseNull(cert);
}

- (void)testCopySerialNumber {
    SecCertificateRef cert1 = NULL;
    CFDataRef c1_serial = NULL, serial = NULL;

    isnt(cert1 = SecCertificateCreateWithBytes(NULL, _c1, sizeof(_c1)),
         NULL, "create cert1");

    c1_serial = CFDataCreate(NULL, _c1_serial, sizeof(_c1_serial));
    CFErrorRef error = NULL;
    ok(serial = SecCertificateCopySerialNumberData(cert1, &error), "copy cert1 serial");
    CFReleaseNull(error);
    ok(CFEqual(c1_serial, serial), "serial matches");

    CFReleaseNull(serial);
    CFReleaseNull(c1_serial);
    CFReleaseNull(cert1);
}

#if !TARGET_OS_BRIDGE // bridgeOS doesn't have a CT log list
-(void)testCopyTrustedCTLogs {
    __block CFDictionaryRef trustedLogs = NULL;
    __block int matched = 0;

    require_action(trustedLogs = SecCertificateCopyTrustedCTLogs(),
                   errOut, fail("failed to copy trusted CT logs"));
    /* look for some known CT log ids to ensure functionality */
    for (int ix = 0; ix < CTLOG_KEYID_COUNT; ix++) {
        CFDataRef logIDData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, _ctlogids[ix], CTLOG_KEYID_LENGTH, kCFAllocatorNull);
        NSString *logIDKey = [(__bridge NSData *)logIDData base64EncodedStringWithOptions:0];
        CFDictionaryRef logData = CFDictionaryGetValue(trustedLogs, (__bridge CFStringRef)logIDKey);
        if (logData) {
            ++matched;
        }
        CFReleaseSafe(logIDData);
    }
    require_action(matched > 0,
                   errOut, fail("failed to match known CT log ids"));
errOut:
    CFReleaseSafe(trustedLogs);
}
#endif // !TARGET_OS_BRIDGE

#if !TARGET_OS_BRIDGE // bridgeOS doesn't have a CT log list
-(void)testCopyCTLogForKeyID {
    int matched = 0;
    /* look for some known CT log ids to ensure functionality */
    for (int ix = 0; ix < CTLOG_KEYID_COUNT; ix++) {
        CFDataRef logIDData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, _ctlogids[ix], CTLOG_KEYID_LENGTH, kCFAllocatorNull);
        CFDictionaryRef logDict = NULL;
        const void *operator = NULL;
        require_action(logDict = SecCertificateCopyCTLogForKeyID(logIDData),
                       errContinue, fail("failed to match CT log"));
        require_action(isDictionary(logDict),
                       errContinue, fail("returned CT log is not a dictionary"));
        require_action(CFDictionaryGetValueIfPresent(logDict, CFSTR("operator"), &operator),
                       errContinue, fail("operator value is not present"));
        require_action(isString(operator),
                       errContinue, fail("operator value is not a string"));

        ++matched;

errContinue:
        CFReleaseNull(logDict);
        CFReleaseNull(logIDData);
    }
    require_action(matched > 0,
                   errOut, fail("failed to match known CT log ids"));
errOut:
    return;
}
#endif // !TARGET_OS_BRIDGE

- (void)testDeveloperIdDate {
    SecCertificateRef old_devid = SecCertificateCreateWithBytes(NULL, _old_developer_cert, sizeof(_old_developer_cert));
    SecCertificateRef new_devid = SecCertificateCreateWithBytes(NULL, _new_developer_cert, sizeof(_new_developer_cert));

    CFErrorRef error = NULL;
    CFAbsoluteTime time;
    is(SecCertificateGetDeveloperIDDate(old_devid, &time, &error), false, "old Developer ID cert returned date");
    is(CFErrorGetCode(error), errSecMissingRequiredExtension, "old Developer ID cert failed with wrong error code");
    CFReleaseNull(error);

    ok(SecCertificateGetDeveloperIDDate(new_devid, &time, &error), "new developer ID cert failed to copy date");
    is(time, 573436800.0, "date in certificate wasn't 2019-03-05 00:00:00Z");

    CFReleaseNull(old_devid);
    CFReleaseNull(new_devid);
}

- (void)testSecFrameworkIsDNSName {
    const char *valid_names[] = {
        "apple.com",
        "127.0.0.1.example.com",
        "1a.example.com",
        "a1.example.com",
        "example.xn--3hcrj9c",
        "example.xn--80ao21a",
    };

    const char *invalid_names[] = {
        /* Error: All-numeric TLD. */
        "apple.com.1",
        "127.0.0.1",
        /* Error: Label begins with a hyphen. */
        "-a.example.com",
        /* Error: Label ends with a hyphen. */
        "a-.example.com",
        /* Error: TLD begins with a hyphen. */
        "example.--3hcrj9c",
        /* Error: TLD ends with a hyphen. */
        "example.80ao21a--",
        /* Error: Label has invalid characters. */
        "_foo.example.com",
    };

    size_t i;
    for (i = 0; i < sizeof(valid_names) / sizeof(valid_names[0]); i++) {
        CFStringRef name = CFStringCreateWithCString(NULL, valid_names[i], kCFStringEncodingUTF8);
        XCTAssertTrue(name != NULL);
        XCTAssertTrue(SecFrameworkIsDNSName(name), "Valid host name '%s' failed to be parsed as such.", valid_names[i]);
        CFRelease(name);
    }

    for (i = 0; i < sizeof(invalid_names) / sizeof(invalid_names[0]); i++) {
        CFStringRef name = CFStringCreateWithCString(NULL, invalid_names[i], kCFStringEncodingUTF8);
        XCTAssertTrue(name != NULL);
        XCTAssertFalse(SecFrameworkIsDNSName(name), "Invalid host name '%s' failed to be parsed as such.", invalid_names[i]);
        CFRelease(name);
    }
}

- (void)testSecFrameworkIsIPAddress {
    const char *valid_addrs[] = {
        "127.0.0.1", /* localhost IPv4 */
        "162.159.36.1", /* WAN IPv4 */
        "::",  /* all-zeros IPv6 address */
        "::1", /* localhost IPv6, leading zero expansion */
        "cafe:feed:face::1", /* inline zero expansion */
        "cafe:FEED:FACE::", /* trailing zero expansion */
        "2606:4700:4700::1111", /* compressed */
        "2606:4700:4700:0:0:0:0:1111", /* uncompressed */
        "[2606:4700:4700:0:0:0:0:1111]", /* literal form (for URLs) */
    };

    const char *invalid_addrs[] = {
        "apple.com.1", /* invalid characters */
        "284.321.1.1", /* IPv4 octet values > 255 */
        "192.168.254.0/24", /* CIDR notation; not for SAN values */
        "2606:4700:4700::1111::1" /* multiple IPv6 expansions */
        "[2606:4700:4700:[0:0:0:0]:1111]", /* too many brackets */
        "...1", /* not enough fields for IPv4 */
        "23.45.56.67.78", /* too many fields */
        ":1", /* not enough fields for IPv6 */
        "100:90:80:70:60:50:40:30:20:10", /* too many fields */
        "2600:f00000000d::1", /* invalid IPv6 octet value */
        "cov:fefe::1", /* invalid characters */
    };

    size_t i;
    for (i = 0; i < sizeof(valid_addrs) / sizeof(valid_addrs[0]); i++) {
        CFStringRef addr = CFStringCreateWithCString(NULL, valid_addrs[i], kCFStringEncodingUTF8);
        XCTAssertTrue(addr != NULL);
        XCTAssertTrue(SecFrameworkIsIPAddress(addr), "Valid IP address '%s' failed to be parsed as such.", valid_addrs[i]);
        CFRelease(addr);
    }

    for (i = 0; i < sizeof(invalid_addrs) / sizeof(invalid_addrs[0]); i++) {
        CFStringRef addr = CFStringCreateWithCString(NULL, invalid_addrs[i], kCFStringEncodingUTF8);
        XCTAssertTrue(addr != NULL);
        XCTAssertFalse(SecFrameworkIsIPAddress(addr), "Invalid IP address '%s' failed to be parsed as such.", invalid_addrs[i]);
        CFRelease(addr);
    }
}

- (void)testSecFrameworkCopyIPAddressData {
    unsigned char ipv4_data[4] = {
        0xA2, 0x9F, 0x84, 0x35 };
    unsigned char ipv6_data[16] = {
        0x26, 0x06, 0x47, 0x00, 0x47, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11 };

    CFDataRef data4 = NULL, result4 = NULL;
    CFDataRef data6 = NULL, result6 = NULL;

    data4 = CFDataCreate(NULL, ipv4_data, sizeof(ipv4_data));
    XCTAssertTrue(data4 != NULL);
    result4 = SecFrameworkCopyIPAddressData(CFSTR("162.159.132.53"));
    XCTAssertTrue(result4 != NULL);
    XCTAssertTrue(CFEqual(result4, data4));
    CFRelease(data4);
    CFRelease(result4);

    data6 = CFDataCreate(NULL, ipv6_data, sizeof(ipv6_data));
    XCTAssertTrue(data6 != NULL);
    result6 = SecFrameworkCopyIPAddressData(CFSTR("2606:4700:4700::1111"));
    XCTAssertTrue(result6 != NULL);
    XCTAssertTrue(CFEqual(result6, data6));
    CFRelease(data6);
    CFRelease(result6);
}

- (void)testCopyIPAddresses {
    SecCertificateRef cert = SecCertificateCreateWithBytes(NULL, _IPAddressCert, sizeof(_IPAddressCert));
    NSArray *ipAddresses = CFBridgingRelease(SecCertificateCopyIPAddresses(cert));
    XCTAssertNotNil(ipAddresses);
    XCTAssertEqual(ipAddresses.count, 1);
    XCTAssertEqualObjects(ipAddresses[0], @"10.0.0.1");
    CFReleaseNull(cert);

    cert = SecCertificateCreateWithBytes(NULL, _c1, sizeof(_c1));
    ipAddresses = CFBridgingRelease(SecCertificateCopyIPAddresses(cert));
    XCTAssertNil(ipAddresses);
    CFReleaseNull(cert);
}

- (void)testCopySubjectAttributeValue {
    // ATV not present
    SecCertificateRef devCert = SecCertificateCreateWithBytes(NULL, _new_developer_cert, sizeof(_new_developer_cert));
    NSString *locality = CFBridgingRelease(SecCertificateCopySubjectAttributeValue(devCert, (DERItem *)&oidLocalityName));
    XCTAssertNil(locality);

    // ATV present
    NSString *ou = CFBridgingRelease(SecCertificateCopySubjectAttributeValue(devCert, (DERItem *)&oidOrganizationalUnitName));
    XCTAssertNotNil(ou);
    XCTAssert([ou isEqualToString:@"PV45XFU466"]);
    CFReleaseNull(devCert);

    // pick the last value for multiple attributes
    SecCertificateRef multipleValues = SecCertificateCreateWithBytes(NULL, two_common_names, sizeof(two_common_names));
    NSString *commonName = CFBridgingRelease(SecCertificateCopySubjectAttributeValue(multipleValues, (DERItem *)&oidCommonName));
    XCTAssertNotNil(commonName);
    XCTAssert([commonName isEqualToString:@"certxauthsplit"]);
    CFReleaseNull(multipleValues);
}

- (void)testCopyProperties {
    SecCertificateRef cert = SecCertificateCreateWithBytes(NULL, _c1, sizeof(_c1));

    NSArray *properties = CFBridgingRelease(SecCertificateCopyProperties(cert));
    XCTAssertNotNil(properties);

    NSArray *localizedProperties = CFBridgingRelease(SecCertificateCopyLocalizedProperties(cert, true));
    XCTAssertNotNil(localizedProperties);

    // If we're on a en-us device, these should match
    XCTAssertEqualObjects(properties, localizedProperties);

    CFReleaseNull(cert);
}

- (void)testSignatureHashAlgorithm
{
    SecCertificateRef cert=NULL;
    SecSignatureHashAlgorithm alg=0;

    isnt(cert = SecCertificateCreateWithBytes(NULL, RSA_MD2, sizeof(RSA_MD2)),
         NULL, "create RSA_MD2");
    alg = SecCertificateGetSignatureHashAlgorithm(cert);
    ok(alg == kSecSignatureHashAlgorithmMD2, "expected kSecSignatureHashAlgorithmMD2 (got %d)", (int)alg);
    CFReleaseNull(cert);

    isnt(cert = SecCertificateCreateWithBytes(NULL, RSA_MD5, sizeof(RSA_MD5)),
         NULL, "create RSA_MD5");
    alg = SecCertificateGetSignatureHashAlgorithm(cert);
    ok(alg == kSecSignatureHashAlgorithmMD5, "expected kSecSignatureHashAlgorithmMD5 (got %d)", (int)alg);
    CFReleaseNull(cert);

    isnt(cert = SecCertificateCreateWithBytes(NULL, RSA_SHA1, sizeof(RSA_SHA1)),
         NULL, "create RSA_SHA1");
    alg = SecCertificateGetSignatureHashAlgorithm(cert);
    ok(alg == kSecSignatureHashAlgorithmSHA1, "expected kSecSignatureHashAlgorithmSHA1 (got %d)", (int)alg);
    CFReleaseNull(cert);

    isnt(cert = SecCertificateCreateWithBytes(NULL, RSA_SHA256, sizeof(RSA_SHA256)),
         NULL, "create RSA_SHA256");
    alg = SecCertificateGetSignatureHashAlgorithm(cert);
    ok(alg == kSecSignatureHashAlgorithmSHA256, "expected kSecSignatureHashAlgorithmSHA256 (got %d)", (int)alg);
    CFReleaseNull(cert);

    isnt(cert = SecCertificateCreateWithBytes(NULL, RSA_SHA512, sizeof(RSA_SHA512)),
         NULL, "create RSA_SHA512");
    alg = SecCertificateGetSignatureHashAlgorithm(cert);
    ok(alg == kSecSignatureHashAlgorithmSHA512, "expected kSecSignatureHashAlgorithmSHA512 (got %d)", (int)alg);
    CFReleaseNull(cert);

    isnt(cert = SecCertificateCreateWithBytes(NULL, DSA_SHA1, sizeof(DSA_SHA1)),
         NULL, "create DSA_SHA1");
    alg = SecCertificateGetSignatureHashAlgorithm(cert);
    ok(alg == kSecSignatureHashAlgorithmSHA1, "expected kSecSignatureHashAlgorithmSHA1 (got %d)", (int)alg);
    CFReleaseNull(cert);

    isnt(cert = SecCertificateCreateWithBytes(NULL, ECDSA_SHA1, sizeof(ECDSA_SHA1)),
         NULL, "create ECDSA_SHA1");
    alg = SecCertificateGetSignatureHashAlgorithm(cert);
    ok(alg == kSecSignatureHashAlgorithmSHA1, "expected kSecSignatureHashAlgorithmSHA1 (got %d)", (int)alg);
    CFReleaseNull(cert);

    isnt(cert = SecCertificateCreateWithBytes(NULL, ECDSA_SHA256, sizeof(ECDSA_SHA256)),
         NULL, "create ECDSA_SHA256");
    alg = SecCertificateGetSignatureHashAlgorithm(cert);
    ok(alg == kSecSignatureHashAlgorithmSHA256, "expected kSecSignatureHashAlgorithmSHA256 (got %d)", (int)alg);
    CFReleaseNull(cert);

    isnt(cert = SecCertificateCreateWithBytes(NULL, ECDSA_SHA384, sizeof(ECDSA_SHA384)),
         NULL, "create ECDSA_SHA384");
    alg = SecCertificateGetSignatureHashAlgorithm(cert);
    ok(alg == kSecSignatureHashAlgorithmSHA384, "expected kSecSignatureHashAlgorithmSHA384 (got %d)", (int)alg);
    CFReleaseNull(cert);

    /* %%% RSAPSS is not yet supported; change this test when it is. <rdar://19356971> */
    isnt(cert = SecCertificateCreateWithBytes(NULL, RSAPSS_SHA256, sizeof(RSAPSS_SHA256)),
         NULL, "create RSAPSS_SHA256");
    alg = SecCertificateGetSignatureHashAlgorithm(cert);
    ok(alg == kSecSignatureHashAlgorithmUnknown, "expected kSecSignatureHashAlgorithmUnknown (got %d)", (int)alg);
    CFReleaseNull(cert);
}


@end
