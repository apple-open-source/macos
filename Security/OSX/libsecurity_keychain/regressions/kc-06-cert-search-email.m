/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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
 * limitations under the xLicense.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import <Security/Security.h>
#import <Security/SecCertificatePriv.h>

#include "keychain_regressions.h"
#include "kc-helpers.h"
#include "kc-item-helpers.h"
#include "kc-key-helpers.h"

#import <Foundation/Foundation.h>

#include <Security/SecCertificate.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecPolicySearch.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecIdentitySearch.h>
#include <Security/SecIdentitySearchPriv.h>
#include <Security/SecTrust.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainItemPriv.h>
#include <SecurityFoundation/SFCertificateData.h>
#include <Security/oidsalg.h>


static NSString* printDataAsHex(
	const CSSM_DATA *d)
{
	if (!d || !d->Data) return NULL;

	unsigned int i;
	CSSM_SIZE len = d->Length;
	uint8 *cp = d->Data;
	NSString *str = [NSString string];

	for(i=0; i<len; i++) {
		str = [str stringByAppendingFormat:@"%02X", ((unsigned char *)cp)[i]];
	}
	return str;
}

static NSString* printDigest(
	CSSM_ALGORITHMS digestAlgorithm,
	const CSSM_DATA* thingToDigest)
{
	CSSM_RETURN crtn;
	CSSM_DATA digest;
	uint8 buf[64]; // we really only expect 16 or 20 byte digests, but...

	digest.Data = buf;
	digest.Length = sizeof(buf);
	crtn = SecDigestGetData (digestAlgorithm, &digest, thingToDigest);

	if (crtn || !digest.Length) return NULL;
	return printDataAsHex(&digest);
}

static void printCertificate(SecCertificateRef certificate, SecPolicyRef policy, int ordinalValue)
{
    CSSM_DATA certData = { 0, nil };
    (void) SecCertificateGetData(certificate, &certData);
    NSString *digestStr = printDigest(CSSM_ALGID_MD5, &certData);
    const char *digest = [digestStr UTF8String];
    fprintf(stdout, "%3d) %s", ordinalValue, (digest) ? digest : "!-- unable to get md5 digest --!");

    CFStringRef label=nil;
    OSStatus status = SecCertificateInferLabel(certificate, &label);
    if (!status && label)
    {
        char buf[1024];
        if (!CFStringGetCString(label, buf, 1024-1, kCFStringEncodingUTF8))
            buf[0]=0;
        fprintf(stdout, " \"%s\"", buf);
        CFRelease(label);
    }

    // Default to X.509 Basic if no policy was specified
    if (!policy) {
        SecPolicySearchRef policySearch = NULL;
        if (SecPolicySearchCreate(CSSM_CERT_X_509v3, &CSSMOID_APPLE_X509_BASIC, NULL, &policySearch)==noErr) {
            SecPolicySearchCopyNext(policySearch, &policy);
        }
    }

    // Create a trust reference, given policy and certificates
    SecTrustRef trust=nil;
    NSArray *certificates = [NSArray arrayWithObject:(__bridge id)certificate];
    status = SecTrustCreateWithCertificates((CFArrayRef)certificates, policy, &trust);

    SFCertificateData *sfCertData = [[SFCertificateData alloc] initWithCertificate:certificate trust:trust parse:NO];
    const char *statusStr = [[sfCertData statusString] UTF8String];
    // Skip the status string if the certificate is valid, but print it otherwise
    if (statusStr && (strcmp(statusStr, "This certificate is valid") != 0))
        fprintf(stdout, " (%s)", statusStr);
    fprintf(stdout, "\n");
}

static BOOL certificateHasExpired(SecCertificateRef certificate)
{
    SFCertificateData *sfCertData = [[SFCertificateData alloc] initWithCertificate:certificate trust:nil parse:NO];
    BOOL result = [sfCertData expired];

    return result;
}

static void doCertificateSearchForEmailAddress(SecKeychainRef kc, const char *emailAddr, bool showAll)
{
    OSStatus status = errSecSuccess;

    // Enumerate matching certificates
    fprintf(stdout, "%s certificates matching \"%s\":\n", (showAll) ? "All" : "Valid", emailAddr);
    SecKeychainSearchRef searchRef;
    status = SecKeychainSearchCreateForCertificateByEmail(kc, emailAddr, &searchRef);
    ok_status(status, "%s: SecKeychainSearchCreateForCertificateByEmail", testName);

    SecCertificateRef preferredCert = nil;
    CFStringRef emailStr = (emailAddr) ? CFStringCreateWithCStringNoCopy(NULL, emailAddr, kCFStringEncodingUTF8, kCFAllocatorNull) : NULL;

	if (!status) {
        SecKeychainItemRef itemRef=nil;
        unsigned int i=0;
        while (SecKeychainSearchCopyNext(searchRef, &itemRef)==noErr) {
            if (showAll || !certificateHasExpired((SecCertificateRef)itemRef)) {
                printCertificate((SecCertificateRef)itemRef, nil, ++i);
            }

            // Set this certificate as preferred for this email address
            ok_status(SecCertificateSetPreferred((SecCertificateRef)itemRef, emailStr, 0), "%s: SecCertificateSetPreferred", testName);

            CFRelease(itemRef);
        }
        is(i, 1, "%s: Wrong number of certificates found", testName);

        CFRelease(searchRef);
    }

    // Check that our certificate is new preferred
    status = SecCertificateCopyPreference(emailStr, (CSSM_KEYUSE) 0, &preferredCert);
    ok_status(status, "%s: SecCertificateCopyPreference", testName);

    if (preferredCert)
        CFRelease(preferredCert);
    if (emailStr)
        CFRelease(emailStr);
}

int kc_06_cert_search_email(int argc, char *const *argv)
{
    bool showAll = false;

    plan_tests(7);
    initializeKeychainTests(__FUNCTION__);

    // Delete any existing preferences for our certificate, but don't test
    // status since maybe it doesn't exist yet
    CFMutableDictionaryRef q = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(q, kSecClass, kSecClassGenericPassword);
    q = addLabel(q, CFSTR("nobody_certificate@apple.com"));
    SecItemDelete(q);


    SecKeychainRef kc = getPopulatedTestKeychain();
    addToSearchList(kc);

    doCertificateSearchForEmailAddress(kc, "nobody_certificate@apple.com", showAll);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", testName);
    CFReleaseNull(kc);

    deleteTestFiles();
	return 0;
}
