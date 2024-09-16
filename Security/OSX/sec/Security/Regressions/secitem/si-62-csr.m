/*
 * Copyright (c) 2008-2017 Apple Inc. All Rights Reserved.
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

#import <AssertMacros.h>
#import <Foundation/Foundation.h>

#include <Security/SecKey.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecCMS.h>
#include <Security/SecCertificateRequest.h>
#include <Security/SecSCEP.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentityPriv.h>
#include <utilities/array_size.h>

#include <Security/SecInternal.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdlib.h>
#include <unistd.h>

#include "shared_regressions.h"

#include <fcntl.h>
__unused static inline void write_data(const char * path, CFDataRef data)
{
    int data_file = open(path, O_CREAT|O_WRONLY|O_TRUNC, 0644);
    write(data_file, CFDataGetBytePtr(data), CFDataGetLength(data));
    close(data_file);
}


static void tests(void)
{
    SecKeyRef phone_publicKey = NULL, phone_privateKey = NULL;
    SecKeyRef ca_publicKey = NULL, ca_privateKey = NULL;

    int keysize = 2048;
    CFNumberRef key_size_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &keysize);
    const void *keygen_keys[] = { kSecAttrKeyType, kSecAttrKeySizeInBits };
    const void *keygen_vals[] = { kSecAttrKeyTypeRSA, key_size_num };
    CFDictionaryRef parameters = CFDictionaryCreate(kCFAllocatorDefault,
        keygen_keys, keygen_vals, array_size(keygen_vals),
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
    CFReleaseNull(key_size_num);

    CFMutableDictionaryRef subject_alt_names = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(subject_alt_names, kSecSubjectAltNameDNSName, CFSTR("xey.nl"));

    int key_usage = kSecKeyUsageDigitalSignature | kSecKeyUsageKeyEncipherment;
    CFNumberRef key_usage_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &key_usage);

    CFMutableDictionaryRef random_extensions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    const void *key[] = { kSecCSRChallengePassword, kSecSubjectAltName, kSecCertificateKeyUsage, kSecCertificateExtensions };
    const void *val[] = { CFSTR("magic"), subject_alt_names, key_usage_num, random_extensions };
    CFDictionaryRef csr_parameters = CFDictionaryCreate(kCFAllocatorDefault,
        key, val, array_size(key), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    SecATV cn_phone[] = { { kSecOidCommonName, SecASN1PrintableString, CFSTR("My iPhone") }, {} };
    SecATV c[]  = { { kSecOidCountryName, SecASN1PrintableString, CFSTR("US") }, {} };
    SecATV st[] = { { kSecOidStateProvinceName, SecASN1PrintableString, CFSTR("CA") }, {} };
    SecATV l[]  = { { kSecOidLocalityName, SecASN1PrintableString, CFSTR("Cupertino") }, {} };
    SecATV o[]  = { { CFSTR("2.5.4.10"), SecASN1PrintableString, CFSTR("Apple Inc.") }, {} };
    SecATV ou[] = { { kSecOidOrganizationalUnit, SecASN1PrintableString, CFSTR("iPhone") }, {} };

    SecRDN atvs_phone[] = { cn_phone, c, st, l, o, ou, NULL };

    ok_status(SecKeyGeneratePair(parameters, &phone_publicKey, &phone_privateKey), "generate key pair");
    ok_status(SecKeyGeneratePair(parameters, &ca_publicKey, &ca_privateKey), "generate key pair");

    int self_key_usage = kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign;
    CFNumberRef self_key_usage_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &self_key_usage);
    int path_len = 0;
    CFNumberRef path_len_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &path_len);
    const void *self_key[] = { kSecCertificateKeyUsage, kSecCSRBasicContraintsPathLen };
    const void *self_val[] = { self_key_usage_num, path_len_num };
    CFDictionaryRef self_signed_parameters = CFDictionaryCreate(kCFAllocatorDefault,
        self_key, self_val, array_size(self_key), NULL, NULL);

    const void * ca_o[] = { kSecOidOrganization, CFSTR("Apple Inc.") };
    const void * ca_cn[] = { kSecOidCommonName, CFSTR("Root CA") };
    CFArrayRef ca_o_dn = CFArrayCreate(kCFAllocatorDefault, ca_o, 2, NULL);
    CFArrayRef ca_cn_dn = CFArrayCreate(kCFAllocatorDefault, ca_cn, 2, NULL);
    const void *ca_dn_array[2];
    ca_dn_array[0] = CFArrayCreate(kCFAllocatorDefault, (const void **)&ca_o_dn, 1, NULL);
    ca_dn_array[1] = CFArrayCreate(kCFAllocatorDefault, (const void **)&ca_cn_dn, 1, NULL);
    CFArrayRef ca_rdns = CFArrayCreate(kCFAllocatorDefault, ca_dn_array, 2, NULL);

    SecCertificateRef ca_cert = SecGenerateSelfSignedCertificate(ca_rdns,
        self_signed_parameters, ca_publicKey, ca_privateKey);
	SecCertificateRef ca_cert_phone_key =
		SecGenerateSelfSignedCertificate(ca_rdns, self_signed_parameters, phone_publicKey, phone_privateKey);

	CFReleaseSafe(self_signed_parameters);
    CFReleaseSafe(self_key_usage_num);
    CFReleaseSafe(path_len_num);
    CFReleaseNull(ca_o_dn);
    CFReleaseNull(ca_cn_dn);
    CFReleaseNull(ca_dn_array[0]);
    CFReleaseNull(ca_dn_array[1]);
    CFReleaseNull(ca_rdns);

    isnt(ca_cert, NULL, "got back a cert");
    ok(SecCertificateIsSelfSignedCA(ca_cert), "cert is self-signed ca cert");
    isnt(ca_cert_phone_key, NULL, "got back a cert");
    ok(SecCertificateIsSelfSignedCA(ca_cert_phone_key), "cert is self-signed ca cert");
    CFDataRef data = SecCertificateCopyData(ca_cert);
    //write_data("/tmp/ca_cert.der", data);
    CFReleaseSafe(data);

    SecIdentityRef ca_identity = SecIdentityCreate(kCFAllocatorDefault, ca_cert, ca_privateKey);
	SecIdentityRef ca_identity_phone_key = SecIdentityCreate(kCFAllocatorDefault, ca_cert_phone_key, phone_privateKey);
    isnt(ca_identity, NULL, "got a identity");
    isnt(ca_identity_phone_key, NULL, "got a identity");
    CFDictionaryRef dict = CFDictionaryCreate(NULL, (const void **)&kSecValueRef, (const void **)&ca_identity, 1, NULL, NULL);
    ok_status(SecItemAdd(dict, NULL), "add ca identity");
    CFReleaseSafe(dict);
#if TARGET_OS_IPHONE
    TODO: {
        todo("Adding a cert with the same issuer/serial but a different key should return something other than errSecDuplicateItem");
#else
    {
#endif
        dict = CFDictionaryCreate(NULL, (const void **)&kSecValueRef, (const void **)&ca_identity_phone_key, 1, NULL, NULL);
        is_status(errSecDuplicateItem, SecItemAdd(dict, NULL), "add ca identity");
        CFReleaseSafe(dict);
    }

    CFDataRef csr = SecGenerateCertificateRequestWithParameters(atvs_phone, NULL, phone_publicKey, phone_privateKey);
    isnt(csr, NULL, "got back a csr");
    CFReleaseNull(csr);

	//dict[kSecSubjectAltName, dict[ntPrincipalName, "foo@bar.org"]]
	CFStringRef nt_princ_name_val = CFSTR("foo@bar.org");
	CFDictionaryRef nt_princ = CFDictionaryCreate(NULL, (const void **)&kSecSubjectAltNameNTPrincipalName, (const void **)&nt_princ_name_val, 1, NULL, NULL);
	CFDictionaryRef params = CFDictionaryCreate(NULL, (const void **)&kSecSubjectAltName, (const void **)&nt_princ, 1, NULL, NULL);

    csr = SecGenerateCertificateRequestWithParameters(atvs_phone, params, phone_publicKey, phone_privateKey);
    isnt(csr, NULL, "got back a csr");
	//write_data("/var/tmp/csr-nt-princ", csr);
    CFReleaseNull(csr);
	CFReleaseNull(params);
	CFReleaseNull(nt_princ);

    csr = SecGenerateCertificateRequestWithParameters(atvs_phone, csr_parameters, phone_publicKey, phone_privateKey);
    isnt(csr, NULL, "csr w/ params");
    //write_data("/tmp/csr", csr);
    CFDataRef subject, extensions;
    CFStringRef challenge;
    ok(SecVerifyCertificateRequest(csr, NULL, &challenge, &subject, &extensions), "verify csr");
    CFReleaseNull(csr);

    uint8_t serialno_byte = 42;
    CFDataRef serialno = CFDataCreate(kCFAllocatorDefault, &serialno_byte, sizeof(serialno_byte));
    SecCertificateRef cert = SecIdentitySignCertificate(ca_identity, serialno,
        phone_publicKey, subject, extensions);
    data = SecCertificateCopyData(cert);
    //write_data("/tmp/iphone_cert.der", data);
    CFReleaseNull(data);
    CFReleaseNull(subject);
    CFReleaseNull(extensions);
    CFReleaseNull(challenge);

    const void * email[] = { CFSTR("1.2.840.113549.1.9.1"), CFSTR("foo@bar.biz") };
    const void * cn[] = { CFSTR("2.5.4.3"), CFSTR("S/MIME Baby") };
    CFArrayRef email_dn = CFArrayCreate(kCFAllocatorDefault, email, 2, NULL);
    CFArrayRef cn_dn = CFArrayCreate(kCFAllocatorDefault, cn, 2, NULL);
    const void *dn_array[2];
    dn_array[0] = CFArrayCreate(kCFAllocatorDefault, (const void **)&email_dn, 1, NULL);
    dn_array[1] = CFArrayCreate(kCFAllocatorDefault, (const void **)&cn_dn, 1, NULL);
    CFArrayRef rdns = CFArrayCreate(kCFAllocatorDefault, dn_array, 2, NULL);
    CFDictionarySetValue(subject_alt_names, CFSTR("rfc822name"), CFSTR("mongo@pawn.org"));

    uint8_t random_extension_data[] = { 0xde, 0xad, 0xbe, 0xef };
    CFDataRef random_extension_value = CFDataCreate(kCFAllocatorDefault, random_extension_data, sizeof(random_extension_data));
    CFDictionarySetValue(random_extensions, CFSTR("1.2.840.113635.100.6.1.2"), random_extension_value);  // APPLE_FDR_ACCESS_OID
    CFDictionarySetValue(random_extensions, CFSTR("1.2.840.113635.100.6.1.3"), CFSTR("that guy"));  // APPLE_FDR_CLIENT_IDENTIFIER_OID
    CFReleaseNull(random_extension_value);

    csr = SecGenerateCertificateRequest(rdns, csr_parameters, phone_publicKey, phone_privateKey);
    isnt(csr, NULL, "csr w/ params");
    //write_data("/tmp/csr_neu", csr);
    CFReleaseNull(csr);
    CFReleaseNull(subject_alt_names);
    CFDictionaryRemoveAllValues(random_extensions);

    CFDataRef scep_request = SecSCEPGenerateCertificateRequest(rdns,
        csr_parameters, phone_publicKey, phone_privateKey, NULL, ca_cert);
    isnt(scep_request, NULL, "got scep blob");
    //write_data("/tmp/scep_request.der", scep_request);

    CFReleaseNull(email_dn);
    CFReleaseNull(cn_dn);
    CFReleaseNull(dn_array[0]);
    CFReleaseNull(dn_array[1]);
    CFReleaseNull(rdns);

    CFDataRef scep_reply = SecSCEPCertifyRequest(scep_request, ca_identity, serialno, false);
    isnt(scep_reply, NULL, "produced scep reply");
    //write_data("/tmp/scep_reply.der", scep_reply);

    CFArrayRef issued_certs = NULL;
    ok(issued_certs = SecSCEPVerifyReply(scep_request, scep_reply, ca_cert, NULL), "verify scep reply");

    // take the issued cert and CA cert and pretend it's a RA/CA couple
    CFMutableArrayRef scep_certs = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, issued_certs);
    CFArrayAppendValue(scep_certs, ca_cert);
    SecCertificateRef ca_certificate = NULL, ra_signing_certificate = NULL, ra_encryption_certificate = NULL;

    ok_status(SecSCEPValidateCACertMessage(scep_certs, NULL,
        &ca_certificate, &ra_signing_certificate,
        &ra_encryption_certificate), "pull apart array again");
    ok(CFEqual(ca_cert, ca_certificate), "found ca");
    ok(CFArrayContainsValue(issued_certs, CFRangeMake(0, CFArrayGetCount(issued_certs)), ra_signing_certificate), "found ra");
    ok(!ra_encryption_certificate, "no separate encryption cert");

    CFReleaseSafe(ca_certificate);
    CFReleaseSafe(ra_signing_certificate);
    CFReleaseSafe(scep_certs);

    CFReleaseSafe(scep_request);
    CFReleaseSafe(scep_reply);
    CFReleaseSafe(issued_certs);

    // cleanups
    dict = CFDictionaryCreate(NULL, (const void **)&kSecValueRef, (const void **)&ca_identity, 1, NULL, NULL);
    ok_status(SecItemDelete(dict), "delete ca identity");
	CFReleaseSafe(dict);
    dict = CFDictionaryCreate(NULL, (const void **)&kSecValueRef, (const void **)&phone_privateKey, 1, NULL, NULL);
    ok_status(SecItemDelete(dict), "delete phone private key");
	CFReleaseSafe(dict);

    CFReleaseSafe(serialno);

    CFReleaseSafe(cert);
    CFReleaseSafe(ca_identity);
    CFReleaseSafe(ca_cert);
	CFReleaseSafe(ca_identity_phone_key);
	CFReleaseSafe(ca_cert_phone_key);
    CFReleaseSafe(csr_parameters);
    CFReleaseSafe(random_extensions);
    CFReleaseSafe(parameters);
    CFReleaseSafe(ca_publicKey);
    CFReleaseSafe(ca_privateKey);
    CFReleaseSafe(phone_publicKey);
    CFReleaseSafe(phone_privateKey);
}

static void test_ec_csr(void) {
    SecKeyRef ecPublicKey = NULL, ecPrivateKey = NULL;

    int keysize = 256;
    CFNumberRef key_size_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &keysize);

    const void *keyParamsKeys[] = { kSecAttrKeyType, kSecAttrKeySizeInBits };
    const void *keyParamsValues[] = { kSecAttrKeyTypeECSECPrimeRandom,  key_size_num};
    CFDictionaryRef keyParameters = CFDictionaryCreate(NULL, keyParamsKeys, keyParamsValues, 2,
                                                       &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    ok_status(SecKeyGeneratePair(keyParameters, &ecPublicKey, &ecPrivateKey),
              "unable to generate EC key");

    SecATV cn_phone[] = { { kSecOidCommonName, SecASN1PrintableString, CFSTR("My iPhone") }, {} };
    SecATV c[]  = { { kSecOidCountryName, SecASN1PrintableString, CFSTR("US") }, {} };
    SecATV st[] = { { kSecOidStateProvinceName, SecASN1PrintableString, CFSTR("CA") }, {} };
    SecATV l[]  = { { kSecOidLocalityName, SecASN1PrintableString, CFSTR("Cupertino") }, {} };
    SecATV o[]  = { { CFSTR("2.5.4.10"), SecASN1PrintableString, CFSTR("Apple Inc.") }, {} };
    SecATV ou[] = { { kSecOidOrganizationalUnit, SecASN1PrintableString, CFSTR("iPhone") }, {} };

    SecRDN atvs_phone[] = { cn_phone, c, st, l, o, ou, NULL };

    CFMutableDictionaryRef subject_alt_names = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(subject_alt_names, kSecSubjectAltNameDNSName, CFSTR("xey.nl"));

    int key_usage = kSecKeyUsageDigitalSignature | kSecKeyUsageKeyEncipherment;
    CFNumberRef key_usage_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &key_usage);

    CFMutableDictionaryRef random_extensions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    const void *key[] = { kSecCSRChallengePassword, kSecSubjectAltName, kSecCertificateKeyUsage, kSecCertificateExtensions };
    const void *val[] = { CFSTR("magic"), subject_alt_names, key_usage_num, random_extensions };
    CFDictionaryRef csr_parameters = CFDictionaryCreate(kCFAllocatorDefault,
                                                        key, val, array_size(key), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);


    CFDataRef csr = SecGenerateCertificateRequestWithParameters(atvs_phone, csr_parameters, ecPublicKey, ecPrivateKey);
    isnt(csr, NULL, "csr w/ params");
    //write_data("/tmp/csr", csr);
    CFDataRef subject, extensions;
    CFStringRef challenge;
    ok(SecVerifyCertificateRequest(csr, NULL, &challenge, &subject, &extensions), "verify csr");

    CFReleaseNull(csr);
    CFReleaseNull(key_size_num);
    CFReleaseNull(keyParameters);
    CFReleaseNull(ecPublicKey);
    CFReleaseNull(ecPrivateKey);
    CFReleaseNull(subject_alt_names);
    CFReleaseNull(key_usage_num);
    CFReleaseNull(random_extensions);
    CFReleaseNull(csr_parameters);
    CFReleaseNull(subject);
    CFReleaseNull(extensions);
    CFReleaseNull(challenge);
}

static bool test_csr_create_sign_verify(SecKeyRef ca_priv, SecKeyRef leaf_priv,
                         CFStringRef cert_hashing_alg, CFStringRef csr_hashing_alg) {
    bool status = false;
    SecCertificateRef ca_cert = NULL, leaf_cert1 = NULL, leaf_cert2 = NULL;
    SecIdentityRef ca_identity = NULL;
    NSArray *leaf_rdns = nil, *anchors = nil;
    NSDictionary *leaf_parameters = nil;
    NSData *csr = nil, *serial_no = nil;
    SecKeyRef csr_pub_key = NULL;
    CFDataRef csr_subject = NULL, csr_extensions = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    SecTrustResultType trustResult = kSecTrustResultInvalid;

    /* Generate a self-signed cert */
    NSString *common_name = [NSString stringWithFormat:@"CSR Test Root: %@", cert_hashing_alg];
    NSArray *ca_rdns = @[
         @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
         @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc."]],
         @[@[(__bridge NSString*)kSecOidCommonName, common_name]]
     ];
    NSDictionary *ca_parameters = @{
        (__bridge NSString *)kSecCMSSignHashAlgorithm: (__bridge NSString*)cert_hashing_alg,
        (__bridge NSString *)kSecCSRBasicContraintsPathLen: @0,
        (__bridge NSString *)kSecCertificateKeyUsage: @(kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign)
    };
    ca_cert = SecGenerateSelfSignedCertificate((__bridge CFArrayRef)ca_rdns,
                                               (__bridge CFDictionaryRef)ca_parameters,
                                               NULL, ca_priv);
    require(ca_cert, out);
    ca_identity = SecIdentityCreate(NULL, ca_cert, ca_priv);
    require(ca_identity, out);

    /* Generate a CSR */
    leaf_rdns = @[
        @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
        @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc"]],
        @[@[(__bridge NSString*)kSecOidCommonName, @"Leaf 1"]]
    ];
    leaf_parameters = @{
        (__bridge NSString*)kSecCMSSignHashAlgorithm: (__bridge NSString*)csr_hashing_alg,
        (__bridge NSString*)kSecSubjectAltName: @{
            (__bridge NSString*)kSecSubjectAltNameDNSName : @[ @"valid.apple.com",
                                                              @"valid-qa.apple.com",
                                                              @"valid-uat.apple.com"]
        },
        (__bridge NSString*)kSecCertificateKeyUsage : @(kSecKeyUsageDigitalSignature)
    };
    csr = CFBridgingRelease(SecGenerateCertificateRequest((__bridge CFArrayRef)leaf_rdns,
                                                                  (__bridge CFDictionaryRef)leaf_parameters,
                                                                  NULL, leaf_priv));
    require(csr, out);

    /* Verify that CSR */
    require(SecVerifyCertificateRequest((__bridge CFDataRef)csr, &csr_pub_key, NULL, &csr_subject, &csr_extensions), out);
    require(csr_pub_key && csr_extensions && csr_subject, out);

    /* Sign that CSR */
    uint8_t serial_no_bytes[] = { 0xbb, 0x01 };
    serial_no = [NSData dataWithBytes:serial_no_bytes length:sizeof(serial_no_bytes)];
    leaf_cert1 = SecIdentitySignCertificateWithAlgorithm(ca_identity, (__bridge CFDataRef)serial_no,
                                                         csr_pub_key, csr_subject, csr_extensions, cert_hashing_alg);
    require(leaf_cert1, out);

    CFReleaseNull(csr_pub_key);
    CFReleaseNull(csr_subject);
    CFReleaseNull(csr_extensions);

    /* Generate a CSR "with parameters" SPI */
    SecATV c[]  = { { kSecOidCountryName, SecASN1PrintableString, CFSTR("US") }, {} };
    SecATV o[]  = { { kSecOidOrganization, SecASN1PrintableString, CFSTR("Apple Inc.") }, {} };
    SecATV cn[] = { { kSecOidCommonName, SecASN1PrintableString, CFSTR("Leaf 2") }, {} };

    SecRDN atvs_leaf2[] = { c, o, cn, NULL };
    csr = CFBridgingRelease(SecGenerateCertificateRequestWithParameters(atvs_leaf2, (__bridge CFDictionaryRef)leaf_parameters, NULL, leaf_priv));
    require(csr, out);

    /* Verify that CSR */
    require(SecVerifyCertificateRequest((__bridge CFDataRef)csr, &csr_pub_key, NULL, &csr_subject, &csr_extensions), out);
    require(csr_pub_key && csr_extensions && csr_subject, out);

    /* Sign that CSR */
    uint8_t serial_no_bytes2[] = { 0xbb, 0x02 };
    serial_no = [NSData dataWithBytes:serial_no_bytes2 length:sizeof(serial_no_bytes2)];
    leaf_parameters = @{(__bridge NSString*)kSecCMSSignHashAlgorithm: (__bridge NSString*)csr_hashing_alg};
    leaf_cert2 = SecIdentitySignCertificateWithParameters(ca_identity, (__bridge CFDataRef)serial_no,
                                                         csr_pub_key, csr_subject, csr_extensions,
                                                          (__bridge CFDictionaryRef)leaf_parameters);
    require(leaf_cert2, out);

    /* Verify the signed leaf certs chain to the root */
    require(policy = SecPolicyCreateBasicX509(), out);
    require_noerr(SecTrustCreateWithCertificates(leaf_cert1, policy, &trust), out);
    anchors = @[ (__bridge id)ca_cert ];
    require_noerr(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), out);
    require_noerr(SecTrustEvaluate(trust, &trustResult), out);
    require(trustResult == kSecTrustResultUnspecified || trustResult == kSecTrustResultProceed, out);
    CFReleaseNull(trust);

    require_noerr(SecTrustCreateWithCertificates(leaf_cert2, policy, &trust), out);
    require_noerr(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), out);
    require_noerr(SecTrustEvaluate(trust, &trustResult), out);
    require(trustResult == kSecTrustResultUnspecified || trustResult == kSecTrustResultProceed, out);
    CFReleaseNull(trust);

    status = true;
out:
    CFReleaseNull(ca_cert);
    CFReleaseNull(ca_identity);
    CFReleaseNull(leaf_cert1);
    CFReleaseNull(leaf_cert2);
    CFReleaseNull(csr_pub_key);
    CFReleaseNull(csr_subject);
    CFReleaseNull(csr_extensions);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    return status;
}

static void test_algs(void) {
    SecKeyRef ca_rsa_key = NULL, ca_ec_key = NULL;
    SecKeyRef leaf_rsa_key = NULL, leaf_ec_key = NULL;
    SecKeyRef publicKey = NULL;
    NSDictionary *rsa_parameters = nil, *ec_parameters = nil;

    rsa_parameters = @{
        (__bridge NSString*)kSecAttrKeyType: (__bridge NSString*)kSecAttrKeyTypeRSA,
        (__bridge NSString*)kSecAttrKeySizeInBits : @2048,
    };
    ok_status(SecKeyGeneratePair((__bridge CFDictionaryRef)rsa_parameters, &publicKey, &ca_rsa_key),
              "Failed to generate CA RSA key");
    CFReleaseNull(publicKey);
    ok_status(SecKeyGeneratePair((__bridge CFDictionaryRef)rsa_parameters, &publicKey, &leaf_rsa_key),
              "Failed to generate leaf RSA key");
    CFReleaseNull(publicKey);

    ec_parameters = @{
        (__bridge NSString*)kSecAttrKeyType: (__bridge NSString*)kSecAttrKeyTypeECSECPrimeRandom,
        (__bridge NSString*)kSecAttrKeySizeInBits : @384,
    };
    ok_status(SecKeyGeneratePair((__bridge CFDictionaryRef)ec_parameters, &publicKey, &ca_ec_key),
              "Failed to generate CA EC key");
    CFReleaseNull(publicKey);
    ok_status(SecKeyGeneratePair((__bridge CFDictionaryRef)ec_parameters, &publicKey, &leaf_ec_key),
              "Failed to generate leaf EC key");
    CFReleaseNull(publicKey);

    /* Single algorithm tests */
    ok(test_csr_create_sign_verify(ca_rsa_key, leaf_rsa_key, kSecCMSHashingAlgorithmSHA1, kSecCMSHashingAlgorithmSHA1),
       "Failed to run csr test with RSA SHA-1");
    ok(test_csr_create_sign_verify(ca_rsa_key, leaf_rsa_key, kSecCMSHashingAlgorithmSHA256, kSecCMSHashingAlgorithmSHA256),
       "Failed to run csr test with RSA SHA-256");
    ok(test_csr_create_sign_verify(ca_rsa_key, leaf_rsa_key, kSecCMSHashingAlgorithmSHA384, kSecCMSHashingAlgorithmSHA384),
       "Failed to run csr test with RSA SHA-384");
    ok(test_csr_create_sign_verify(ca_rsa_key, leaf_rsa_key, kSecCMSHashingAlgorithmSHA512, kSecCMSHashingAlgorithmSHA512),
       "Failed to run csr test with RSA SHA-512");
    ok(test_csr_create_sign_verify(ca_ec_key, leaf_ec_key, kSecCMSHashingAlgorithmSHA256, kSecCMSHashingAlgorithmSHA256),
       "Failed to run csr test with EC SHA-256");
    ok(test_csr_create_sign_verify(ca_ec_key, leaf_ec_key, kSecCMSHashingAlgorithmSHA384, kSecCMSHashingAlgorithmSHA384),
       "Failed to run csr test with EC SHA-384");
    ok(test_csr_create_sign_verify(ca_ec_key, leaf_ec_key, kSecCMSHashingAlgorithmSHA512, kSecCMSHashingAlgorithmSHA512),
       "Failed to run csr test with EC SHA-512");

    /* Mix and match */
    ok(test_csr_create_sign_verify(ca_rsa_key, leaf_ec_key, kSecCMSHashingAlgorithmSHA256, kSecCMSHashingAlgorithmSHA384),
       "Failed to run csr test with RSA CA, EC leaf, SHA256 certs, SHA384 csrs");
    ok(test_csr_create_sign_verify(ca_rsa_key, leaf_rsa_key, kSecCMSHashingAlgorithmSHA256, kSecCMSHashingAlgorithmSHA1),
       "Failed to run csr test with RSA keys, SHA256 certs, SHA1 csrs");
    ok(test_csr_create_sign_verify(ca_ec_key, leaf_ec_key, kSecCMSHashingAlgorithmSHA384, kSecCMSHashingAlgorithmSHA256),
       "Failed to run csr test with EC keys, SHA384 certs, SHA256 csrs");

    CFReleaseNull(ca_rsa_key);
    CFReleaseNull(ca_ec_key);
    CFReleaseNull(leaf_rsa_key);
    CFReleaseNull(leaf_ec_key);
}

    static void test_lifetimes(void) {
        SecKeyRef ca_priv = NULL;
        SecKeyRef leaf_priv = NULL;
        NSDictionary *ec_parameters = nil;

        ec_parameters = @{
            (__bridge NSString*)kSecAttrKeyType: (__bridge NSString*)kSecAttrKeyTypeECSECPrimeRandom,
            (__bridge NSString*)kSecAttrKeySizeInBits : @384,
        };
        ca_priv = SecKeyCreateRandomKey((__bridge CFDictionaryRef)ec_parameters, NULL);
        isnt(NULL, ca_priv, "Failed to generate CA EC key");
        leaf_priv = SecKeyCreateRandomKey((__bridge CFDictionaryRef)ec_parameters, NULL);
        isnt(NULL, leaf_priv, "Failed to generate leaf EC key");

        for (int i = 0; i < 2; i++) {
            int64_t ca_lifetime = i == 0 ? 0 : 300; // 0 to represent "default"
            int64_t leaf_lifetime = i == 1 ? 300 : 0;

            SecCertificateRef ca_cert = NULL, leaf_cert = NULL;
            SecIdentityRef ca_identity = NULL;
            NSArray *leaf_rdns = nil;
            NSDictionary *leaf_parameters = nil;
            NSData *csr = nil, *serial_no = nil;
            SecKeyRef csr_pub_key = NULL;
            CFDataRef csr_subject = NULL, csr_extensions = NULL;

            /* Generate a self-signed cert */
            NSString *common_name = [NSString stringWithFormat:@"CSR Test Root: %lld seconds", ca_lifetime];
            NSArray *ca_rdns = @[
                 @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
                 @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc."]],
                 @[@[(__bridge NSString*)kSecOidCommonName, common_name]]
             ];
            NSMutableDictionary *ca_parameters = [@{
                (__bridge NSString *)kSecCSRBasicContraintsPathLen: @0,
                (__bridge NSString *)kSecCertificateKeyUsage: @(kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign)
            } mutableCopy];
            if (ca_lifetime) {
                ca_parameters[(__bridge NSString*)kSecCertificateLifetime] = @(ca_lifetime);
            }
            ca_cert = SecGenerateSelfSignedCertificate((__bridge CFArrayRef)ca_rdns,
                                                       (__bridge CFDictionaryRef)ca_parameters,
                                                       NULL, ca_priv);
            require_action(ca_cert, out, fail("failed to make ca_cert"));
            ca_identity = SecIdentityCreate(NULL, ca_cert, ca_priv);
            require_action(ca_identity, out, fail("failed to make ca_identity"));

            /* Generate a CSR */
            common_name = [NSString stringWithFormat:@"CSR Leaf: %lld seconds", leaf_lifetime];
            leaf_rdns = @[
                @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
                @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc"]],
                @[@[(__bridge NSString*)kSecOidCommonName, common_name]]
            ];
            leaf_parameters = @{
                (__bridge NSString*)kSecSubjectAltName: @{
                    (__bridge NSString*)kSecSubjectAltNameDNSName : @"valid.apple.com"
                },
                (__bridge NSString*)kSecCertificateKeyUsage : @(kSecKeyUsageDigitalSignature)
            };
            csr = CFBridgingRelease(SecGenerateCertificateRequest((__bridge CFArrayRef)leaf_rdns,
                                                                          (__bridge CFDictionaryRef)leaf_parameters,
                                                                          NULL, leaf_priv));
            require_action(csr, out, fail("failed to make csr"));

            /* Verify that CSR */
            require_action(SecVerifyCertificateRequest((__bridge CFDataRef)csr, &csr_pub_key, NULL, &csr_subject, &csr_extensions), out,  fail("failed to verify csr"));
            require_action(csr_pub_key && csr_extensions && csr_subject, out, fail("failed to get csr details"));

            /* Sign that CSR */
            uint8_t serial_no_bytes[] = { 0xbb, 0x01 };
            serial_no = [NSData dataWithBytes:serial_no_bytes length:sizeof(serial_no_bytes)];
            if (leaf_lifetime) {
                leaf_parameters = @{ (__bridge NSString*)kSecCertificateLifetime : @(leaf_lifetime) };
            } else {
                leaf_parameters = @{};
            }
            leaf_cert = SecIdentitySignCertificateWithParameters(ca_identity, (__bridge CFDataRef)serial_no,
                                                                 csr_pub_key, csr_subject, csr_extensions, (__bridge CFDictionaryRef) leaf_parameters);
            require_action(leaf_cert, out, fail("failed to make leaf_cert"));

            /* Check the lifetimes */
            if (ca_lifetime) {
                is(ca_lifetime, SecCertificateNotValidAfter(ca_cert) - SecCertificateNotValidBefore(ca_cert), "configured ca lifetime does not match");
            } else {
                is(3600*24*365, SecCertificateNotValidAfter(ca_cert) - SecCertificateNotValidBefore(ca_cert), "default ca lifetime does not match");
            }

            if (leaf_lifetime) {
                is(leaf_lifetime, SecCertificateNotValidAfter(leaf_cert) - SecCertificateNotValidBefore(leaf_cert), "configured leaf lifetime does not match");
            } else {
                is(3600*24*365, SecCertificateNotValidAfter(leaf_cert) - SecCertificateNotValidBefore(leaf_cert), "default leaf lifetime does not match");
            }

            /* Check the lifetimes with API */
            CFDateRef caValidBefore = SecCertificateCopyNotValidBeforeDate(ca_cert);
            CFDateRef caValidAfter = SecCertificateCopyNotValidAfterDate(ca_cert);
            CFDateRef leafValidBefore = SecCertificateCopyNotValidBeforeDate(leaf_cert);
            CFDateRef leafValidAfter = SecCertificateCopyNotValidAfterDate(leaf_cert);

            if (ca_lifetime) {
                is(ca_lifetime, CFDateGetAbsoluteTime(caValidAfter) - CFDateGetAbsoluteTime(caValidBefore), "configured ca lifetime does not match");
            } else {
                is(3600*24*365, CFDateGetAbsoluteTime(caValidAfter) - CFDateGetAbsoluteTime(caValidBefore), "default ca lifetime does not match");
            }

            if (leaf_lifetime) {
                is(leaf_lifetime, CFDateGetAbsoluteTime(leafValidAfter) - CFDateGetAbsoluteTime(leafValidBefore), "configured leaf lifetime does not match");
            } else {
                is(3600*24*365, CFDateGetAbsoluteTime(leafValidAfter) - CFDateGetAbsoluteTime(leafValidBefore), "default leaf lifetime does not match");
            }

            out:
            CFReleaseNull(ca_cert);
            CFReleaseNull(ca_identity);
            CFReleaseNull(leaf_cert);
            CFReleaseNull(csr_pub_key);
            CFReleaseNull(csr_subject);
            CFReleaseNull(csr_extensions);
            CFReleaseNull(caValidBefore);
            CFReleaseNull(caValidAfter);
            CFReleaseNull(leafValidBefore);
            CFReleaseNull(leafValidAfter);
        }

        CFReleaseNull(ca_priv);
        CFReleaseNull(leaf_priv);
    }

    static void test_ekus(void) {
        SecKeyRef cert_priv = NULL;
        SecCertificateRef cert = NULL;
        NSDictionary *ec_parameters = @{
            (__bridge NSString*)kSecAttrKeyType: (__bridge NSString*)kSecAttrKeyTypeECSECPrimeRandom,
            (__bridge NSString*)kSecAttrKeySizeInBits : @384,
        };
        cert_priv = SecKeyCreateRandomKey((__bridge CFDictionaryRef)ec_parameters, NULL);
        isnt(NULL, cert_priv, "Failed to generate CA EC key");

        NSArray *rdns = @[
             @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
             @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc."]],
             @[@[(__bridge NSString*)kSecOidCommonName, @"Self-signed EKUs Test"]]
         ];

        /* Value of EKU parameter incorrect type */
        NSDictionary *parameters = @{
            (__bridge NSString*)kSecCertificateKeyUsage: @(kSecKeyUsageDigitalSignature),
            (__bridge NSString*)kSecCertificateExtendedKeyUsage : (__bridge NSString*)kSecEKUServerAuth
        };
        cert = SecGenerateSelfSignedCertificate((__bridge CFArrayRef)rdns,
                                                   (__bridge CFDictionaryRef)parameters,
                                                   NULL, cert_priv);
        is(NULL, cert);
        CFReleaseNull(cert);

        /* Bad EKU */
        parameters = @{
            (__bridge NSString*)kSecCertificateKeyUsage: @(kSecKeyUsageDigitalSignature),
            (__bridge NSString*)kSecCertificateExtendedKeyUsage : @[@"not an OID"]
        };
        cert = SecGenerateSelfSignedCertificate((__bridge CFArrayRef)rdns,
                                                   (__bridge CFDictionaryRef)parameters,
                                                   NULL, cert_priv);
        is(NULL, cert);
        CFReleaseNull(cert);

        /* One EKU */
        parameters = @{
            (__bridge NSString*)kSecCertificateKeyUsage: @(kSecKeyUsageDigitalSignature),
            (__bridge NSString*)kSecCertificateExtendedKeyUsage : @[(__bridge NSString*)kSecEKUServerAuth]
        };
        cert = SecGenerateSelfSignedCertificate((__bridge CFArrayRef)rdns,
                                                   (__bridge CFDictionaryRef)parameters,
                                                   NULL, cert_priv);
        isnt(NULL, cert);
        NSArray *ekus = CFBridgingRelease(SecCertificateCopyExtendedKeyUsage(cert));
        ok(ekus != nil);
        is([ekus count], 1);
        CFReleaseNull(cert);

        /* Multiple EKUs */
        parameters = @{
            (__bridge NSString*)kSecCertificateKeyUsage: @(kSecKeyUsageDigitalSignature),
            (__bridge NSString*)kSecCertificateExtendedKeyUsage : @[(__bridge NSString*)kSecEKUClientAuth,
                                                                    (__bridge NSString*)kSecEKUEmailProtection]
        };
        cert = SecGenerateSelfSignedCertificate((__bridge CFArrayRef)rdns,
                                                   (__bridge CFDictionaryRef)parameters,
                                                   NULL, cert_priv);
        isnt(NULL, cert);
        ekus = CFBridgingRelease(SecCertificateCopyExtendedKeyUsage(cert));
        ok(ekus != nil);
        is([ekus count], 2);
        CFReleaseNull(cert);

        /* Custom EKUs */
        parameters = @{
            (__bridge NSString*)kSecCertificateKeyUsage: @(kSecKeyUsageDigitalSignature),
            (__bridge NSString*)kSecCertificateExtendedKeyUsage : @[@"1.2.840.113635.100.4.100",
                                                                    (__bridge NSString*)kSecEKUEmailProtection]
        };
        cert = SecGenerateSelfSignedCertificate((__bridge CFArrayRef)rdns,
                                                   (__bridge CFDictionaryRef)parameters,
                                                   NULL, cert_priv);
        isnt(NULL, cert);
        ekus = CFBridgingRelease(SecCertificateCopyExtendedKeyUsage(cert));
        ok(ekus != nil);
        is([ekus count], 2);

        CFReleaseNull(cert_priv);
        CFReleaseNull(cert);
    }

    static void test_ca_extensions(void) {
        SecKeyRef ca_priv = NULL, leaf_priv = NULL, leaf_pub = NULL;
        SecCertificateRef ca_cert = NULL, leaf_cert = NULL;
        SecIdentityRef ca_identity = NULL;
        NSDictionary *leaf_parameters = nil;
        NSArray *result_dns_names = nil;

        uint8_t serial_no_bytes[] = { 0xbb, 0x01 };
        NSData *serial_no = [NSData dataWithBytes:serial_no_bytes length:sizeof(serial_no_bytes)];

        NSArray *leaf_subject = @[
            @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
            @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc"]],
            @[@[(__bridge NSString*)kSecOidCommonName, @"Extensions Signing Test Leaf"]]
        ];

        NSDictionary *ec_parameters = @{
            (__bridge NSString*)kSecAttrKeyType: (__bridge NSString*)kSecAttrKeyTypeECSECPrimeRandom,
            (__bridge NSString*)kSecAttrKeySizeInBits : @384,
        };
        ca_priv = SecKeyCreateRandomKey((__bridge CFDictionaryRef)ec_parameters, NULL);
        isnt(NULL, ca_priv, "Failed to generate CA EC key");
        leaf_priv = SecKeyCreateRandomKey((__bridge CFDictionaryRef)ec_parameters, NULL);
        isnt(NULL, leaf_priv, "Failed to generate leaf EC key");
        leaf_pub = SecKeyCopyPublicKey(leaf_priv);
        isnt(NULL, leaf_pub, "Failed to get leaf public key");

        /* Generate a self-signed cert */
        NSArray *ca_rdns = @[
             @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
             @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc."]],
             @[@[(__bridge NSString*)kSecOidCommonName, @"Extensions Signing Test Root"]]
         ];
        NSDictionary *ca_parameters = @{
            (__bridge NSString *)kSecCSRBasicContraintsPathLen: @0,
            (__bridge NSString *)kSecCertificateKeyUsage: @(kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign)
        };
        ca_cert = SecGenerateSelfSignedCertificate((__bridge CFArrayRef)ca_rdns,
                                                   (__bridge CFDictionaryRef)ca_parameters,
                                                   NULL, ca_priv);
        require_action(ca_cert, out, fail("failed to make ca_cert"));
        ca_identity = SecIdentityCreate(NULL, ca_cert, ca_priv);
        require_action(ca_identity, out, fail("failed to make ca_identity"));

        /* Create leaf certs with no extensions */
        leaf_cert = SecIdentitySignCertificateWithParameters(ca_identity, (__bridge CFDataRef)serial_no, leaf_pub, (__bridge CFArrayRef)leaf_subject,
                                                             nil, nil);
        isnt(NULL, leaf_cert, "failed to make leaf_cert with no extensions or parameters");
        CFReleaseNull(leaf_cert);

        /* No extensions but parameters with other entries */
        leaf_parameters = @{ (__bridge NSString*)kSecCertificateLifetime : @(1024) };
        leaf_cert = SecIdentitySignCertificateWithParameters(ca_identity, (__bridge CFDataRef)serial_no, leaf_pub, (__bridge CFArrayRef)leaf_subject,
                                                             nil, (__bridge CFDictionaryRef)leaf_parameters);
        isnt(NULL, leaf_cert, "failed to make leaf_cert with no extensions or parameters");
        CFReleaseNull(leaf_cert);

        /* Request extensions of wrong type */
        leaf_cert = SecIdentitySignCertificateWithParameters(ca_identity, (__bridge CFDataRef)serial_no, leaf_pub, (__bridge CFArrayRef)leaf_subject,
                                                             (__bridge CFArrayRef)leaf_subject, nil);
        is(NULL, leaf_cert, "Created cert with request extensions of unsupported CFType");
        CFReleaseNull(leaf_cert);

        /* Request extensions of dictionary type */
        leaf_parameters = @{
            (__bridge NSString*)kSecSubjectAltName: @{
                (__bridge NSString*)kSecSubjectAltNameDNSName : @"valid.apple.com"
            },
            (__bridge NSString*)kSecCertificateKeyUsage : @(kSecKeyUsageDigitalSignature)
        };
        leaf_cert = SecIdentitySignCertificateWithParameters(ca_identity, (__bridge CFDataRef)serial_no, leaf_pub, (__bridge CFArrayRef)leaf_subject,
                                                             (__bridge CFDictionaryRef)leaf_parameters, nil);
        isnt(NULL, leaf_cert, "failed to make leaf_cert with no extensions or parameters");
        result_dns_names = CFBridgingRelease(SecCertificateCopyDNSNames(leaf_cert));
        ok(nil != result_dns_names, "failed to set DNS names in leaf cert");
        is([result_dns_names count], 1);
        CFReleaseNull(leaf_cert);

        /* "CA" extensions override request extensions */
        leaf_cert = SecIdentitySignCertificateWithParameters(ca_identity, (__bridge CFDataRef)serial_no, leaf_pub, (__bridge CFArrayRef)leaf_subject,
                                                             (__bridge CFDictionaryRef)ca_parameters, (__bridge CFDictionaryRef)leaf_parameters);
        isnt(NULL, leaf_cert, "failed to make leaf_cert with no extensions or parameters");
        result_dns_names = CFBridgingRelease(SecCertificateCopyDNSNames(leaf_cert));
        ok(nil != result_dns_names, "failed to set DNS names in leaf cert");
        is([result_dns_names count], 1);
        is(false, SecCertificateIsCA(leaf_cert));
        CFReleaseNull(leaf_cert);

        out:
        CFReleaseNull(ca_cert);
        CFReleaseNull(ca_identity);
        CFReleaseNull(ca_priv);
        CFReleaseNull(leaf_priv);
        CFReleaseNull(leaf_pub);
    }

    static void test_self_signed_failure(NSArray *rdns, NSDictionary *parameters, SecKeyRef priv) {
        CFErrorRef error = NULL;
        SecCertificateRef cert = SecGenerateSelfSignedCertificateWithError((__bridge CFArrayRef)rdns,
                                                                           (__bridge CFDictionaryRef)parameters,
                                                                           NULL, priv, 
                                                                           &error);
        is(NULL, cert);
        isnt(NULL, error);
        CFReleaseNull(cert);
        CFReleaseNull(error);
    }

    static void test_gen_self_signed_with_error(void) {
        SecKeyRef cert_priv = NULL;
        NSDictionary *ec_parameters = @{
            (__bridge NSString*)kSecAttrKeyType: (__bridge NSString*)kSecAttrKeyTypeECSECPrimeRandom,
            (__bridge NSString*)kSecAttrKeySizeInBits : @384,
        };
        cert_priv = SecKeyCreateRandomKey((__bridge CFDictionaryRef)ec_parameters, NULL);
        isnt(NULL, cert_priv, "Failed to generate CA EC key");

        NSDictionary *parameters = @{
            (__bridge NSString*)kSecCertificateKeyUsage: @(kSecKeyUsageDigitalSignature),
        };

        // wrong data type in ATV OID
        NSArray *rdns = @[
             @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
             @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc."]],
             @[@[@[], @"Self-signed WithError Test"]]
         ];
        test_self_signed_failure(rdns, parameters, cert_priv);

        // only one ATV
        rdns = @[
             @[@[(__bridge NSString*)kSecOidCommonName]]
         ];
        test_self_signed_failure(rdns, parameters, cert_priv);

        rdns = @[
             @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
             @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc."]],
             @[@[(__bridge NSString*)kSecOidCommonName, @"Self-signed WithError Test"]]
         ];

        // wrong SAN key type
        parameters = @{
            (__bridge NSString*)kSecSubjectAltName: @{
                @[] : @"valid.apple.com"
            },
        };
        test_self_signed_failure(rdns, parameters, cert_priv);

        // eku is not OID string
        parameters = @{
            (__bridge NSString*)kSecCertificateExtendedKeyUsage : @[@"not an OID"]
        };
        test_self_signed_failure(rdns, parameters, cert_priv);

        // custom extension with wrong value type
        parameters = @{
            (__bridge NSString*)kSecCertificateExtensions : @{
                @"1.2.840.113635.6.10000": @[@"not an extension value"]
            },
        };
        test_self_signed_failure(rdns, parameters, cert_priv);

        // path len + CA=false
        parameters = @{
            (__bridge NSString *)kSecCSRBasicContraintsPathLen: @1,
            (__bridge NSString *)kSecCSRBasicConstraintsCA: @NO
        };
        test_self_signed_failure(rdns, parameters, cert_priv);

        // key usage wrong type
        parameters = @{
            (__bridge NSString*)kSecCertificateKeyUsage: @"not a key usage",
        };
        test_self_signed_failure(rdns, parameters, cert_priv);

        parameters = @{
            (__bridge NSString*)kSecCertificateKeyUsage: @(kSecKeyUsageDigitalSignature),
        };

        // unsupported ec public key curve? or key type
        ec_parameters = @{
            (__bridge NSString*)kSecAttrKeyType: (__bridge NSString*)kSecAttrKeyTypeECSECPrimeRandom,
            (__bridge NSString*)kSecAttrKeySizeInBits : @224,
        };
        SecKeyRef unsupported_priv = SecKeyCreateRandomKey((__bridge CFDictionaryRef)ec_parameters, NULL);
        test_self_signed_failure(rdns, parameters, unsupported_priv);
        CFReleaseNull(unsupported_priv);

        // unsupported signature algorithm (ECDSA with SHA1)
        parameters = @{
            (__bridge NSString*)kSecCMSSignHashAlgorithm: (__bridge NSString*)kSecCMSHashingAlgorithmSHA1,
        };
        test_self_signed_failure(rdns, parameters, cert_priv);

        CFReleaseNull(cert_priv);
    }

int si_62_csr(int argc, char *const *argv)
{
	plan_tests(95);

	tests();
    test_ec_csr();
    test_algs();
    test_lifetimes();
    test_ekus();
    test_ca_extensions();
    test_gen_self_signed_with_error();

	return 0;
}
