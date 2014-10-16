/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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


//
//  si-68-secmatchissuer.c
//  regressions
//
//
//
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <libDER/libDER.h>
#include <libDER/DER_Decode.h>
#include <libDER/asn1Types.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecItem.h>
#include <Security/SecInternal.h>
#include <utilities/array_size.h>

#include "Security_regressions.h"
#include <test/testcert.h>

/*
static OSStatus add_item_to_keychain(CFTypeRef item, CFDataRef * persistent_ref)
{
    const void *keys[] = { kSecValueRef, kSecReturnPersistentRef };
    const void *vals[] = { item, kCFBooleanTrue };
    CFDictionaryRef add_query = CFDictionaryCreate(NULL, keys, vals, array_size(keys), NULL, NULL);
    OSStatus status = errSecAllocate;
    if (add_query) {
        status = SecItemAdd(add_query, (CFTypeRef *)persistent_ref);
        CFRelease(add_query);
    }
    return status;
}

static OSStatus remove_item_from_keychain(CFTypeRef item, CFDataRef * persistent_ref)
{
    const void *keys[] = { kSecValueRef, kSecReturnPersistentRef };
    const void *vals[] = { item, kCFBooleanTrue };
    CFDictionaryRef add_query = CFDictionaryCreate(NULL, keys, vals, array_size(keys), NULL, NULL);
    OSStatus status = errSecAllocate;
    if (add_query) {
        status = SecItemAdd(add_query, (CFTypeRef *)persistent_ref);
        CFRelease(add_query);
    }
    return status;
}
*/

static OSStatus add_item(CFTypeRef item)
{
    CFDictionaryRef add_query = CFDictionaryCreate(NULL, &kSecValueRef, &item, 1, NULL, NULL);
    OSStatus status = SecItemAdd(add_query, NULL);
    CFRelease(add_query);
    return status;
}

static OSStatus remove_item(CFTypeRef item)
{
    CFDictionaryRef remove_query = CFDictionaryCreate(NULL, &kSecValueRef, &item, 1, NULL, NULL);
    OSStatus status = SecItemDelete(remove_query);
    CFRelease(remove_query);
    return status;
}

static void tests(void)
{

// MARK: test SecDistinguishedNameCopyNormalizedContent

    unsigned char example_dn[] = {
        0x30, 0x4f, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
        0x02, 0x43, 0x5a, 0x31, 0x27, 0x30, 0x25, 0x06, 0x03, 0x55, 0x04, 0x0a,
        0x0c, 0x1e, 0x4d, 0x69, 0x6e, 0x69, 0x73, 0x74, 0x65, 0x72, 0x73, 0x74,
        0x76, 0x6f, 0x20, 0x73, 0x70, 0x72, 0x61, 0x76, 0x65, 0x64, 0x6c, 0x6e,
        0x6f, 0x73, 0x74, 0x69, 0x20, 0xc4, 0x8c, 0x52, 0x31, 0x17, 0x30, 0x15,
        0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0e, 0x4d, 0x53, 0x70, 0x20, 0x52,
        0x6f, 0x6f, 0x74, 0x20, 0x43, 0x41, 0x20, 0x30, 0x31
    };
    unsigned int example_dn_len = 81;

    CFDataRef normalized_dn = NULL, dn = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, example_dn, example_dn_len, kCFAllocatorNull);
    ok(dn, "got dn as data");
    ok(normalized_dn = SecDistinguishedNameCopyNormalizedContent(dn), "convert to normalized form");
    //CFShow(dn);
    //CFShow(normalized_dn);
    CFReleaseNull(dn);
    CFReleaseNull(normalized_dn);
    
// MARK: generate certificate hierarchy

	SecKeyRef public_key = NULL, private_key = NULL;
    ok_status(test_cert_generate_key(512, kSecAttrKeyTypeRSA, &private_key, &public_key), "generate keypair");
	// make organization random uuid to avoid previous run to spoil the fun

    CFUUIDRef UUID = CFUUIDCreate(kCFAllocatorDefault);
    CFStringRef uuidString = CFUUIDCreateString(kCFAllocatorDefault, UUID);
    CFStringRef root_authority_name = CFStringCreateWithFormat(kCFAllocatorDefault, 0, CFSTR("O=%@,CN=Root CA"), uuidString);
    CFStringRef intermediate_authority_name = CFStringCreateWithFormat(kCFAllocatorDefault, 0, CFSTR("O=%@,CN=Intermediate CA"), uuidString);
    CFStringRef leaf_name = CFStringCreateWithFormat(kCFAllocatorDefault, 0, CFSTR("O=%@,CN=Client"), uuidString);
    CFRelease(uuidString);
    CFRelease(UUID);

    SecIdentityRef ca_identity =
        test_cert_create_root_certificate(root_authority_name, public_key, private_key);
    CFRelease(root_authority_name);

    SecCertificateRef ca_cert = NULL;
    SecIdentityCopyCertificate(ca_identity, &ca_cert);
    //CFShow(ca_cert);
    
	SecCertificateRef intermediate_cert =
        test_cert_issue_certificate(ca_identity, public_key, intermediate_authority_name, 42, kSecKeyUsageKeyCertSign);
    CFRelease(intermediate_authority_name);
    SecIdentityRef intermediate_identity = SecIdentityCreate(kCFAllocatorDefault, intermediate_cert, private_key);
    
    ok_status(add_item(intermediate_cert), "add intermediate");
    //CFShow(intermediate_cert);
    
	SecCertificateRef leaf_cert = test_cert_issue_certificate(intermediate_identity, public_key,
          leaf_name, 4242, kSecKeyUsageDigitalSignature);
    CFRelease(leaf_name);
    SecIdentityRef leaf_identity = SecIdentityCreate(kCFAllocatorDefault, leaf_cert, private_key);
    
    ok_status(add_item(leaf_identity), "add leaf");
    //CFShow(leaf_cert);

    // this is already canonical - see if we can get the raw one
    CFDataRef issuer = SecCertificateGetNormalizedIssuerContent(intermediate_cert);
    ok(CFDataGetLength(issuer) < 128, "max 127 bytes of content - or else you'll need to properly encode issuer sequence");
    CFMutableDataRef canonical_issuer = CFDataCreateMutable(kCFAllocatorDefault, CFDataGetLength(issuer) + 2);
    CFDataSetLength(canonical_issuer, CFDataGetLength(issuer) + 2);
    uint8_t * ptr = CFDataGetMutableBytePtr(canonical_issuer);
    memcpy(ptr+2, CFDataGetBytePtr(issuer), CFDataGetLength(issuer));
    ptr[0] = 0x30;
    ptr[1] = CFDataGetLength(issuer);

    CFMutableArrayRef all_distinguished_names = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(all_distinguished_names, canonical_issuer);

    {
        CFReleaseNull(canonical_issuer);
        const void *keys[] = { kSecClass, kSecReturnRef, kSecMatchLimit, kSecMatchIssuers };
        const void *vals[] = { kSecClassIdentity, kCFBooleanTrue, kSecMatchLimitAll, all_distinguished_names };
        CFDictionaryRef all_identities_query = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, array_size(keys), NULL, NULL);
        CFTypeRef all_matching_identities = NULL;
        ok_status(SecItemCopyMatching(all_identities_query, &all_matching_identities), "find all identities matching");
        CFReleaseNull(all_identities_query);
        ok(((CFArrayGetTypeID() == CFGetTypeID(all_matching_identities)) && (CFArrayGetCount(all_matching_identities) == 2)), "return 2");
        //CFShow(all_matching_identities);
    }

    {
        int limit = 0x7fff; // To regress-test <rdar://problem/14603111>
        CFNumberRef cfLimit = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &limit);
        const void *keys[] = { kSecClass, kSecReturnRef, kSecMatchLimit, kSecMatchIssuers };
        const void *vals[] = { kSecClassCertificate, kCFBooleanTrue, cfLimit, all_distinguished_names };
        CFDictionaryRef all_identities_query = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, array_size(keys), NULL, NULL);
        CFTypeRef all_matching_certificates = NULL;
        ok_status(SecItemCopyMatching(all_identities_query, &all_matching_certificates), "find all certificates matching");
        CFReleaseNull(all_identities_query);
        ok(((CFArrayGetTypeID() == CFGetTypeID(all_matching_certificates)) && (CFArrayGetCount(all_matching_certificates) == 2)), "return 2");
        //CFShow(all_matching_certificates);
        CFReleaseSafe(cfLimit);
    }

    remove_item(leaf_identity);
    CFRelease(leaf_identity);
    CFRelease(leaf_cert);
    
    remove_item(intermediate_cert);
    CFRelease(intermediate_cert);
    CFRelease(intermediate_identity);
    
    CFRelease(ca_cert);
    CFRelease(ca_identity);
    
    CFRelease(public_key);
    CFRelease(private_key);
    
}

int si_68_secmatchissuer(int argc, char *const *argv)
{
	plan_tests(10);
    
	tests();
    
	return 0;
}
