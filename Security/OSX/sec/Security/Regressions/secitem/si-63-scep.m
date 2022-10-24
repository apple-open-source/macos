/*
 * Copyright (c) 2008,2012-2014 Apple Inc. All Rights Reserved.
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

#include <AssertMacros.h>
#include <Security/SecInternal.h>
#include <Security/SecCMS.h>
#include <Security/SecSCEP.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentityPriv.h>
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>

#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdlib.h>
#include <unistd.h>
#include <Foundation/Foundation.h>

#include "shared_regressions.h"
#include "test/testcert.h"

#include "si-63-scep.h"
#include "si-63-scep/getcacert-mdes.h"
#include "si-63-scep/getcacert-mdesqa.h"

#include <fcntl.h>
__unused static inline void write_data(const char * path, CFDataRef data)
{
    int data_file = open(path, O_CREAT|O_WRONLY|O_TRUNC, 0644);
    write(data_file, CFDataGetBytePtr(data), CFDataGetLength(data));
    close(data_file);
}

/* Test basic add delete update copy matching stuff. */
static void tests(void)
{
	CFDataRef getcacert_blob = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
		getcacert, getcacert_len, kCFAllocatorNull);
	CFArrayRef certificates = NULL;
	SecCertificateRef ca_certificate, ra_signing_certificate, ra_encryption_certificate;
	ca_certificate = ra_signing_certificate = ra_encryption_certificate = NULL;
	certificates = SecCMSCertificatesOnlyMessageCopyCertificates(getcacert_blob);
	isnt(certificates, NULL, "decode cert-only pkcs#7");
	CFDataRef sha1_fingerprint = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
		ruby_sha1_hash, sizeof(ruby_sha1_hash), kCFAllocatorNull);

	ok_status(SecSCEPValidateCACertMessage(certificates, sha1_fingerprint,
		&ca_certificate, &ra_signing_certificate, &ra_encryption_certificate),
			  "parse CA/RAse getcacert message");
	CFReleaseNull(sha1_fingerprint);
	isnt(ca_certificate, NULL, "got ca cert");
	isnt(ra_signing_certificate, NULL, "got ra signing cert");
	is(ra_encryption_certificate, NULL, "no separate ra encryption cert");

	/* these are always going to be true, but ensure replacement payloads are equivalent */
	ok(SecCertificateIsSelfSignedCA(ca_certificate), "self-signed ca cert");
	ok(SecCertificateGetKeyUsage(ra_signing_certificate) & kSecKeyUsageDigitalSignature, "can sign");

	CFReleaseNull(ca_certificate);
	CFReleaseNull(ra_signing_certificate);
	CFReleaseNull(ra_encryption_certificate);
	CFReleaseNull(getcacert_blob);
	CFReleaseNull(certificates);

    ca_certificate = ra_signing_certificate = ra_encryption_certificate = NULL;
	getcacert_blob = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, getcacert_mdes, getcacert_mdes_len, kCFAllocatorNull);
	certificates = SecCMSCertificatesOnlyMessageCopyCertificates(getcacert_blob);
    ok_status(SecSCEPValidateCACertMessage(certificates, NULL, &ca_certificate, &ra_signing_certificate, &ra_encryption_certificate), "parse WF MDES getcacert message");
    ok(ca_certificate && ra_signing_certificate && ra_encryption_certificate, "identify all 3 certs");

    CFReleaseNull(ca_certificate);
	CFReleaseNull(ra_signing_certificate);
	CFReleaseNull(ra_encryption_certificate);
	CFReleaseNull(getcacert_blob);
	CFReleaseNull(certificates);

    ca_certificate = ra_signing_certificate = ra_encryption_certificate = NULL;
    getcacert_blob = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, getcacert_mdesqa, getcacert_mdesqa_len, kCFAllocatorNull);
	certificates = SecCMSCertificatesOnlyMessageCopyCertificates(getcacert_blob);
    ok_status(SecSCEPValidateCACertMessage(certificates, NULL, &ca_certificate, &ra_signing_certificate, &ra_encryption_certificate), "parse WF MDESQA getcacert message");
    ok(ca_certificate && ra_signing_certificate && ra_encryption_certificate, "identify all 3 certs");

    CFReleaseNull(ca_certificate);
	CFReleaseNull(ra_signing_certificate);
	CFReleaseNull(ra_encryption_certificate);
	CFReleaseNull(getcacert_blob);
	CFReleaseNull(certificates);

	sha1_fingerprint = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
		msscep_md5_hash, sizeof(msscep_md5_hash), kCFAllocatorNull);
	CFDataRef msscep_getcacert_blob = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
		msscep_getcacert, msscep_getcacert_len, kCFAllocatorNull);
	CFReleaseNull(sha1_fingerprint);
	ca_certificate = ra_signing_certificate = ra_encryption_certificate = NULL;
	certificates = SecCMSCertificatesOnlyMessageCopyCertificates(msscep_getcacert_blob);
	ok_status(SecSCEPValidateCACertMessage(certificates, sha1_fingerprint,
		&ca_certificate, &ra_signing_certificate, &ra_encryption_certificate),
			  "parse CA/RAs/RAe msscep getcacert message");
	isnt(ca_certificate, NULL, "got ca cert");
	isnt(ra_signing_certificate, NULL, "got ra signing cert");
	isnt(ra_encryption_certificate, NULL, "got ra encryption cert");

	/* these are always going to be true, but ensure replacement payloads are equivalent */
	ok(SecCertificateIsSelfSignedCA(ca_certificate), "self-signed ca cert");
	ok(SecCertificateGetKeyUsage(ra_encryption_certificate) & kSecKeyUsageKeyEncipherment, "can sign");

	/*
	int ix;
	uint8_t md5_hash[CC_MD5_DIGEST_LENGTH];
	CFDataRef cert_data = SecCertificateCopyData(ca_certificate);
	CC_MD5(CFDataGetBytePtr(cert_data), CFDataGetLength(cert_data), md5_hash);
	for(ix = 0; ix < CC_MD5_DIGEST_LENGTH; ix++) fprintf(stdout, "0x%.02x, ", md5_hash[ix]); fprintf(stdout, "\n");
	uint8_t sha1_hash[CC_SHA1_DIGEST_LENGTH];
	CCDigest(kCCDigestSHA1, CFDataGetBytePtr(cert_data), CFDataGetLength(cert_data), sha1_hash);
	for(ix = 0; ix < CC_SHA1_DIGEST_LENGTH; ix++) fprintf(stdout, "0x%.02x, ", sha1_hash[ix]); fprintf(stdout, "\n");
	CFRelease(cert_data);
	*/

	CFReleaseNull(ca_certificate);
	CFReleaseNull(ra_signing_certificate);
	CFReleaseNull(ra_encryption_certificate);
	CFRelease(certificates);
	CFRelease(msscep_getcacert_blob);




	CFDataRef bmw_getcacert_blob = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
		bmw_scep_pkt, bmw_scep_pkt_len, kCFAllocatorNull);
	ca_certificate = ra_signing_certificate = ra_encryption_certificate = NULL;
	certificates = SecCMSCertificatesOnlyMessageCopyCertificates(bmw_getcacert_blob);
    CFMutableArrayRef certificates_mod = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, certificates);
    CFArrayRemoveValueAtIndex(certificates_mod, 2);
	ok_status(SecSCEPValidateCACertMessage(certificates_mod, NULL,
		&ca_certificate, &ra_signing_certificate, &ra_encryption_certificate),
			  "parse CA/RAs/RAe msscep getcacert message");
    CFRelease(certificates_mod);
	CFRelease(ca_certificate);
	CFRelease(ra_signing_certificate);
	CFRelease(ra_encryption_certificate);
    certificates_mod = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, certificates);
    CFArrayInsertValueAtIndex(certificates_mod, 0, CFArrayGetValueAtIndex(certificates_mod, 3));
    CFArrayRemoveValueAtIndex(certificates_mod, 4);
	ok_status(SecSCEPValidateCACertMessage(certificates_mod, NULL,
		&ca_certificate, &ra_signing_certificate, &ra_encryption_certificate),
			  "parse CA/RAs/RAe msscep getcacert message");
    CFRelease(certificates_mod);
	CFRelease(ca_certificate);
	CFRelease(ra_signing_certificate);
	CFRelease(ra_encryption_certificate);

	ok_status(SecSCEPValidateCACertMessage(certificates, NULL,
		&ca_certificate, &ra_signing_certificate, &ra_encryption_certificate),
			  "parse CA/RAs/RAe msscep getcacert message");
	isnt(ca_certificate, NULL, "got ca cert");
	isnt(ra_signing_certificate, NULL, "got ra signing cert");
	isnt(ra_encryption_certificate, NULL, "got ra encryption cert");

	/* these are always going to be true, but ensure replacement payloads are equivalent */
	ok(SecCertificateIsSelfSignedCA(ca_certificate), "self-signed ca cert");
	ok(SecCertificateGetKeyUsage(ra_encryption_certificate) & kSecKeyUsageKeyEncipherment, "can sign");

	CFReleaseSafe(ca_certificate);
	CFReleaseSafe(ra_signing_certificate);
	CFReleaseSafe(ra_encryption_certificate);
	CFReleaseSafe(certificates);
	CFReleaseSafe(bmw_getcacert_blob);

	uint32_t key_size_in_bits = 1024;
	CFNumberRef key_size = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &key_size_in_bits);
	const void *keygen_keys[] = { kSecAttrKeyType, kSecAttrKeySizeInBits };
	const void *keygen_vals[] = { kSecAttrKeyTypeRSA, key_size };
	CFDictionaryRef parameters = CFDictionaryCreate(kCFAllocatorDefault,
		keygen_keys, keygen_vals, array_size(keygen_vals),
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFRelease(key_size);

	SecKeyRef ca_publicKey = NULL, ca_privateKey = NULL;
	ok_status(SecKeyGeneratePair(parameters, &ca_publicKey, &ca_privateKey), "gen key");
	SecIdentityRef ca_identity = test_cert_create_root_certificate(CFSTR("O=Foo Bar Inc.,CN=Root CA"), ca_publicKey, ca_privateKey);
    CFRelease(ca_publicKey);
    CFRelease(ca_privateKey);

    SecKeyRef scep_ra_publicKey = NULL, scep_ra_privateKey = NULL;
    ok_status(SecKeyGeneratePair(parameters, &scep_ra_publicKey, &scep_ra_privateKey), "generate ra key pair");
	SecCertificateRef scep_ra_certificate =
		test_cert_issue_certificate(ca_identity, scep_ra_publicKey,
		CFSTR("O=Foo Bar Inc.,CN=SCEP RA"), 42,
		kSecKeyUsageKeyEncipherment|kSecKeyUsageDigitalSignature);
	ok(scep_ra_certificate, "got a ra cert");
	SecIdentityRef ra_identity = SecIdentityCreate(kCFAllocatorDefault, scep_ra_certificate, scep_ra_privateKey);
    CFRelease(scep_ra_publicKey);
    CFRelease(scep_ra_privateKey);

	// store encryption identity in the keychain because the decrypt function looks in there only
    CFDictionaryRef identity_add = CFDictionaryCreate(NULL,
        (const void **)&kSecValueRef, (const void **)&ra_identity, 1, NULL, NULL);
	ok_status(SecItemAdd(identity_add, NULL), "add encryption identity to keychain");

    SecKeyRef phone_publicKey = NULL, phone_privateKey = NULL;
    ok_status(SecKeyGeneratePair(parameters, &phone_publicKey, &phone_privateKey), "generate phone key pair");
	CFArrayRef subject = test_cert_string_to_subject(CFSTR("O=Foo Bar Inc.,CN=Shoes"));
	SecIdentityRef self_signed_identity = SecSCEPCreateTemporaryIdentity(phone_publicKey, phone_privateKey);
	CFStringRef magic = CFSTR("magic");
	CFDictionaryRef csr_params = CFDictionaryCreate(kCFAllocatorDefault,
		(const void **)&kSecCSRChallengePassword, (const void **)&magic, 1, NULL, NULL);
	CFDataRef request = SecSCEPGenerateCertificateRequest(NULL, csr_params, phone_publicKey, phone_privateKey, self_signed_identity, scep_ra_certificate);
	CFRelease(csr_params);
    CFRelease(phone_publicKey);
    CFRelease(phone_privateKey);
	isnt(request, NULL, "got a request");
	CFDataRef serialno = CFDataCreate(kCFAllocatorDefault, (uint8_t*)"\001", 1);
	CFDataRef pended_request = SecSCEPCertifyRequest(request, ra_identity, serialno, true);
    CFRelease(serialno);
	isnt(pended_request, NULL, "got a pended request (not failed)");
	CFErrorRef server_error = NULL;
	CFArrayRef issued_certs = NULL;
	issued_certs = SecSCEPVerifyReply(request, pended_request, scep_ra_certificate, &server_error);
    CFReleaseSafe(request);
	is(issued_certs, NULL, "no certs if pended");
	CFDataRef retry_get_cert_initial = NULL;
    isnt(server_error, NULL, "Should have gotten PENDING error");
	CFDictionaryRef error_dict = CFErrorCopyUserInfo(server_error);
	retry_get_cert_initial = SecSCEPGetCertInitial(scep_ra_certificate, subject, NULL, error_dict, self_signed_identity, scep_ra_certificate);
	isnt(retry_get_cert_initial, NULL, "got retry request");
	//write_data("/var/tmp/get_cert_initial", retry_get_cert_initial);
    CFRelease(subject);

    ok_status(SecItemDelete(identity_add), "delete encryption identity from keychain");
    CFReleaseSafe(identity_add);

    CFReleaseNull(parameters);
    CFReleaseNull(scep_ra_certificate);
    CFReleaseSafe(self_signed_identity);
	CFReleaseSafe(retry_get_cert_initial);
	CFReleaseSafe(server_error);
    CFReleaseSafe(pended_request);
    CFReleaseSafe(issued_certs);
    CFReleaseSafe(error_dict);
}

static bool test_scep_with_keys_algorithms(SecKeyRef ca_key, SecKeyRef leaf_key, CFStringRef hash_alg) {
    SecCertificateRef ca_cert = NULL;
    SecIdentityRef ca_identity = NULL;
    NSArray *ca_rdns = nil, *leaf_rdns = nil, *issued_certs = nil;
    NSDictionary *ca_parameters = nil, *leaf_parameters = nil, *ca_item_dict = nil, *leaf_item_dict = nil;
    NSData *scep_request = nil, *scep_reply = nil, *serial_no = nil;
    bool status = false;

    /* Generate CA cert */
    NSString *common_name = [NSString stringWithFormat:@"SCEP Test Root: %@", hash_alg];
    ca_rdns = @[
                @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
                @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc."]],
                @[@[(__bridge NSString*)kSecOidCommonName, common_name]]
                ];
    ca_parameters = @{
                      (__bridge NSString *)kSecCMSSignHashAlgorithm: (__bridge NSString*)hash_alg,
                      (__bridge NSString *)kSecCSRBasicContraintsPathLen: @0,
                      (__bridge NSString *)kSecCertificateKeyUsage: @(kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign)
                      };
    ca_cert = SecGenerateSelfSignedCertificate((__bridge CFArrayRef)ca_rdns,
                                               (__bridge CFDictionaryRef)ca_parameters,
                                               NULL, ca_key);
    require(ca_cert, out);
    ca_identity = SecIdentityCreate(NULL, ca_cert, ca_key);
    require(ca_identity, out);

    /* Generate leaf request - SHA-256 csr, SHA-256 CMS */
    leaf_rdns = @[
                  @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
                  @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc."]],
                  @[@[(__bridge NSString*)kSecOidCommonName, @"SCEP SHA-2 leaf"]]
                  ];
    leaf_parameters = @{
                        (__bridge NSString*)kSecCSRChallengePassword: @"magic",
                        (__bridge NSString*)kSecCMSSignHashAlgorithm: (__bridge NSString*)hash_alg,
                        (__bridge NSString*)kSecSubjectAltName: @{
                                (__bridge NSString*)kSecSubjectAltNameEmailAddress : @"test@apple.com"
                                },
                        (__bridge NSString*)kSecCertificateKeyUsage: @(kSecKeyUsageDigitalSignature),
                        (__bridge NSString*)kSecCMSBulkEncryptionAlgorithm : (__bridge NSString*)kSecCMSEncryptionAlgorithmAESCBC,
                        };
    scep_request = CFBridgingRelease(SecSCEPGenerateCertificateRequest((__bridge CFArrayRef)leaf_rdns,
                                                                       (__bridge CFDictionaryRef)leaf_parameters,
                                                                       NULL, leaf_key, NULL, ca_cert));
    require(scep_request, out);

    /* Add CA identity to keychain so CMS can decrypt */
    ca_item_dict = @{
                     (__bridge NSString*)kSecValueRef : (__bridge id)ca_identity,
                     (__bridge NSString*)kSecAttrLabel : @"SCEP CA Identity"
                     };
    require_noerr(SecItemAdd((__bridge CFDictionaryRef)ca_item_dict, NULL), out);

    /* Certify the request with SHA256, AES */
    uint8_t serial_no_bytes[] = { 0x12, 0x34 };
    serial_no = [NSData dataWithBytes:serial_no_bytes length:sizeof(serial_no_bytes)];
    scep_reply = CFBridgingRelease(SecSCEPCertifyRequestWithAlgorithms((__bridge CFDataRef)scep_request, ca_identity,
                                                                       (__bridge CFDataRef)serial_no, false,
                                                                       hash_alg,
                                                                       kSecCMSEncryptionAlgorithmAESCBC));
    require(scep_reply, out);

    /* Add leaf private key to keychain so CMS can decrypt */
    leaf_item_dict = @{
                       (__bridge NSString*)kSecClass : (__bridge NSString*)kSecClassKey,
                       (__bridge NSString*)kSecValueRef : (__bridge id)leaf_key,
                       (__bridge NSString*)kSecAttrApplicationLabel : @"SCEP Leaf Key"
                       };
    OSStatus addStatus = SecItemAdd((__bridge CFDictionaryRef)leaf_item_dict, NULL);
    if (addStatus == errSecDuplicateItem) {
        leaf_item_dict = nil; // Don't delete the key if we didn't add it
    }
    require(addStatus == errSecSuccess || addStatus == errSecDuplicateItem, out);

    /* Verify the reply */
    issued_certs = CFBridgingRelease(SecSCEPVerifyReply((__bridge CFDataRef)scep_request, (__bridge CFDataRef)scep_reply, ca_cert, nil));
    require(issued_certs, out);
    require([issued_certs count] == 1, out);

    status = true;

out:
    /* Remove from keychain */
    if (ca_item_dict) { SecItemDelete((__bridge CFDictionaryRef)ca_item_dict); }
    if (leaf_item_dict) { SecItemDelete((__bridge CFDictionaryRef)leaf_item_dict); }
    CFReleaseNull(ca_cert);
    CFReleaseNull(ca_identity);
    return status;
}

static void test_SCEP_algs(void) {
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

    /* Hash algorithms */
    ok(test_scep_with_keys_algorithms(ca_rsa_key, leaf_rsa_key, kSecCMSHashingAlgorithmSHA1),
       "Failed to run scep test with RSA SHA-1");
    ok(test_scep_with_keys_algorithms(ca_rsa_key, leaf_rsa_key, kSecCMSHashingAlgorithmSHA256),
       "Failed to run scep test with RSA SHA-256");
    ok(test_scep_with_keys_algorithms(ca_rsa_key, leaf_rsa_key, kSecCMSHashingAlgorithmSHA384),
       "Failed to run scep test with RSA SHA-384");
    ok(test_scep_with_keys_algorithms(ca_rsa_key, leaf_rsa_key, kSecCMSHashingAlgorithmSHA512),
       "Failed to run scep test with RSA SHA-512");

    /* Unsupported key algorithms */
    is(test_scep_with_keys_algorithms(ca_ec_key, leaf_ec_key, kSecCMSHashingAlgorithmSHA256), false,
       "Performed scep with EC ca and leaf");
    is(test_scep_with_keys_algorithms(ca_ec_key, leaf_rsa_key, kSecCMSHashingAlgorithmSHA256), false,
       "Performed scep with EC ca");
    is(test_scep_with_keys_algorithms(ca_rsa_key, leaf_ec_key, kSecCMSHashingAlgorithmSHA256), false,
       "Performed scep with EC leaf");

#if TARGET_OS_OSX
    // macOS "helpfully" added the keys we generated to the keychain, delete them now
    NSDictionary *key_dict = @{
                       (__bridge NSString*)kSecClass : (__bridge NSString*)kSecClassKey,
                       (__bridge NSString*)kSecValueRef : (__bridge id)ca_rsa_key,
                       };
    SecItemDelete((__bridge CFDictionaryRef)key_dict);
    key_dict = @{
                       (__bridge NSString*)kSecClass : (__bridge NSString*)kSecClassKey,
                       (__bridge NSString*)kSecValueRef : (__bridge id)ca_ec_key,
                       };
    SecItemDelete((__bridge CFDictionaryRef)key_dict);
    key_dict = @{
                       (__bridge NSString*)kSecClass : (__bridge NSString*)kSecClassKey,
                       (__bridge NSString*)kSecValueRef : (__bridge id)leaf_rsa_key,
                       };
    SecItemDelete((__bridge CFDictionaryRef)key_dict);
    key_dict = @{
                       (__bridge NSString*)kSecClass : (__bridge NSString*)kSecClassKey,
                       (__bridge NSString*)kSecValueRef : (__bridge id)leaf_ec_key,
                       };
    SecItemDelete((__bridge CFDictionaryRef)key_dict);
#endif

    CFReleaseNull(ca_rsa_key);
    CFReleaseNull(ca_ec_key);
    CFReleaseNull(leaf_rsa_key);
    CFReleaseNull(leaf_ec_key);
}

static void test_multiple_recipient_certs(void) {
    SecCertificateRef ca_cert = NULL, ra_cert = NULL;
    SecKeyRef ca_key = NULL, ra_key = NULL, leaf_key = NULL, publicKey = NULL;;
    SecIdentityRef ca_identity = NULL, ra_identity = NULL, temp_leaf_identity = NULL;
    NSArray *ca_rdns = nil, * ra_rdns = nil, *leaf_rdns = nil;
    NSDictionary *ca_parameters = nil, *ra_parameters = nil, *leaf_parameters = nil,
        *ca_item_dict = nil, *ra_item_dict = nil, *leaf_item_dict = nil;
    NSData *csr = nil, *scep_request = nil, *scep_reply = nil, *serial_no = nil;
    NSDictionary *rsa_parameters = nil;

    rsa_parameters = @{
                       (__bridge NSString*)kSecAttrKeyType: (__bridge NSString*)kSecAttrKeyTypeRSA,
                       (__bridge NSString*)kSecAttrKeySizeInBits : @2048,
                       };
    ok_status(SecKeyGeneratePair((__bridge CFDictionaryRef)rsa_parameters, &publicKey, &ca_key),
              "Failed to generate CA RSA key");
    CFReleaseNull(publicKey);
    ok_status(SecKeyGeneratePair((__bridge CFDictionaryRef)rsa_parameters, &publicKey, &ra_key),
              "Failed to generate CA RSA key");
    CFReleaseNull(publicKey);
    ok_status(SecKeyGeneratePair((__bridge CFDictionaryRef)rsa_parameters, &publicKey, &leaf_key),
              "Failed to generate leaf RSA key");
    temp_leaf_identity = SecSCEPCreateTemporaryIdentity(publicKey, leaf_key);
    CFReleaseNull(publicKey);

    /* Add leaf private key to keychain so CMS can decrypt */
    leaf_item_dict = @{
                       (__bridge NSString*)kSecClass : (__bridge NSString*)kSecClassKey,
                       (__bridge NSString*)kSecValueRef : (__bridge id)leaf_key,
                       (__bridge NSString*)kSecAttrApplicationLabel : @"SCEP Leaf Key"
                       };
    OSStatus addStatus = SecItemAdd((__bridge CFDictionaryRef)leaf_item_dict, NULL);
    if (addStatus == errSecDuplicateItem) {
        leaf_item_dict = nil; // Don't delete the key if we didn't add it
    }

    /* Generate CA cert */
    ca_rdns = @[
                @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
                @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc."]],
                @[@[(__bridge NSString*)kSecOidCommonName, @"SCEP Test Root"]]
                ];
    ca_parameters = @{
                      (__bridge NSString *)kSecCMSSignHashAlgorithm: (__bridge NSString*)kSecCMSHashingAlgorithmSHA256,
                      (__bridge NSString *)kSecCSRBasicContraintsPathLen: @0,
                      (__bridge NSString *)kSecCertificateKeyUsage: @(kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign)
                      };
    ca_cert = SecGenerateSelfSignedCertificate((__bridge CFArrayRef)ca_rdns,
                                               (__bridge CFDictionaryRef)ca_parameters,
                                               NULL, ca_key);
    ok(ca_cert);
    ca_identity = SecIdentityCreate(NULL, ca_cert, ca_key);
    ok(ca_identity);
    /* DONT save ca identity to keychain so we can test that RA key used to encrypt/decrypt */

    /* Generate RA CSR */
    ra_rdns = @[
                  @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
                  @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc."]],
                  @[@[(__bridge NSString*)kSecOidCommonName, @"SCEP Test RA"]]
                  ];
    ra_parameters = @{
                        (__bridge NSString*)kSecCSRChallengePassword: @"magic",
                        (__bridge NSString*)kSecCMSSignHashAlgorithm: (__bridge NSString*)kSecCMSHashingAlgorithmSHA256,
                        (__bridge NSString*)kSecCertificateKeyUsage: @(kSecKeyUsageKeyEncipherment),
                        (__bridge NSString*)kSecCMSBulkEncryptionAlgorithm : (__bridge NSString*)kSecCMSEncryptionAlgorithmAESCBC,
                        };
    publicKey = SecKeyCopyPublicKey(ra_key);
    csr = CFBridgingRelease(SecGenerateCertificateRequest((__bridge CFArrayRef)ra_rdns,
                                                          (__bridge CFDictionaryRef)ra_parameters,
                                                          publicKey, ra_key));
    CFReleaseNull(publicKey);
    ok(csr);

    /* Sign RA CSR */
    CFDataRef subject = NULL;
    CFDataRef extensions = NULL;
    ok(SecVerifyCertificateRequest((__bridge CFDataRef)csr, &publicKey, NULL, &subject, &extensions));
    uint8_t serial_no_bytes[] = { 0x12, 0x34 };
    serial_no = [NSData dataWithBytes:serial_no_bytes length:sizeof(serial_no_bytes)];
    ra_cert = SecIdentitySignCertificateWithParameters(ca_identity, (__bridge CFDataRef)serial_no,
                                                       publicKey, subject, extensions,
                                                       (__bridge CFDictionaryRef)ra_parameters);
    ok(ra_cert);
    ra_identity = SecIdentityCreate(NULL,ra_cert, ra_key);
    ok(ra_identity);
    CFReleaseNull(publicKey);
    CFReleaseNull(subject);
    CFReleaseNull(extensions);

    /* Add RA identity to keychain so CMS can decrypt */
    ra_item_dict = @{
                     (__bridge NSString*)kSecValueRef : (__bridge id)ra_identity,
                     (__bridge NSString*)kSecAttrLabel : @"SCEP RA Identity"
                     };
    ok_status(SecItemAdd((__bridge CFDictionaryRef)ra_item_dict, NULL));

    /* Generate leaf request - SHA-256 csr, SHA-256 CMS */
    leaf_rdns = @[
                  @[@[(__bridge NSString*)kSecOidCountryName, @"US"]],
                  @[@[(__bridge NSString*)kSecOidOrganization, @"Apple Inc."]],
                  @[@[(__bridge NSString*)kSecOidCommonName, @"SCEP SHA-2 leaf"]]
                  ];
    leaf_parameters = @{
                        (__bridge NSString*)kSecCSRChallengePassword: @"magic",
                        (__bridge NSString*)kSecCMSSignHashAlgorithm: (__bridge NSString*)kSecCMSHashingAlgorithmSHA256,
                        (__bridge NSString*)kSecSubjectAltName: @{
                                (__bridge NSString*)kSecSubjectAltNameEmailAddress : @"test@apple.com"
                                },
                        (__bridge NSString*)kSecCertificateKeyUsage: @(kSecKeyUsageDigitalSignature),
                        (__bridge NSString*)kSecCMSBulkEncryptionAlgorithm : (__bridge NSString*)kSecCMSEncryptionAlgorithmAESCBC,
                        };
    NSArray *recipients = @[ (__bridge id)ca_cert, (__bridge id)ra_cert ];
    scep_request = CFBridgingRelease(SecSCEPGenerateCertificateRequest((__bridge CFArrayRef)leaf_rdns,
                                                                       (__bridge CFDictionaryRef)leaf_parameters,
                                                                       NULL, leaf_key, temp_leaf_identity, (__bridge CFArrayRef)recipients));
    ok(scep_request);

    /* Create a pending certification response to leaf */
    serial_no_bytes[1] = 0x35;
    serial_no = [NSData dataWithBytes:serial_no_bytes length:sizeof(serial_no_bytes)];
    scep_reply = CFBridgingRelease(SecSCEPCertifyRequestWithAlgorithms((__bridge CFDataRef)scep_request, ca_identity,
                                                                       (__bridge CFDataRef)serial_no, true,
                                                                       kSecCMSHashingAlgorithmSHA256,
                                                                       kSecCMSEncryptionAlgorithmAESCBC));
    ok(scep_reply);

    /* Verify the pending reply */
    CFErrorRef server_error = NULL;
    is(NULL, SecSCEPVerifyReply((__bridge CFDataRef)scep_request, (__bridge CFDataRef)scep_reply, ca_cert, &server_error));
    ok(server_error);

    /* Create a retry message */
    CFDictionaryRef error_dict = CFErrorCopyUserInfo(server_error);
    scep_request = CFBridgingRelease(SecSCEPGetCertInitial(ca_cert, (__bridge CFArrayRef)leaf_rdns,
                                                         (__bridge CFDictionaryRef)leaf_parameters, error_dict, temp_leaf_identity, (__bridge CFArrayRef)recipients));
    CFReleaseNull(error_dict);
    CFReleaseNull(server_error);
    ok(SecSCEPVerifyGetCertInitial((__bridge CFDataRef)scep_request, ca_identity));

    /* Cleanup */
    if (ca_item_dict) { SecItemDelete((__bridge CFDictionaryRef)ca_item_dict); }
    if (ra_item_dict) { SecItemDelete((__bridge CFDictionaryRef)ra_item_dict); }
    if (leaf_item_dict) { SecItemDelete((__bridge CFDictionaryRef)leaf_item_dict); }
    CFReleaseNull(ca_cert);
    CFReleaseNull(ca_identity);
    CFReleaseNull(ca_key);
    CFReleaseNull(ra_cert);
    CFReleaseNull(ra_identity);
    CFReleaseNull(ra_key);
    CFReleaseNull(leaf_key);
    CFReleaseNull(temp_leaf_identity);
}

static void test_GetCACert(void)
{
    NSData *getCACertResponse = [NSData dataWithBytes:_getCACertReponse_97197618 length:sizeof(_getCACertReponse_97197618)];
    CFArrayRef certificates = SecCMSCertificatesOnlyMessageCopyCertificates((__bridge CFDataRef)getCACertResponse);
    isnt(certificates, NULL);
    is(CFArrayGetCount(certificates), 3);
    SecCertificateRef expected_signing_cert = (SecCertificateRef)CFArrayGetValueAtIndex(certificates, 0);
    SecCertificateRef expected_subCA_cert = (SecCertificateRef)CFArrayGetValueAtIndex(certificates, 1);
    SecCertificateRef expected_root_cert = (SecCertificateRef)CFArrayGetValueAtIndex(certificates, 2);
    SecCertificateRef ca_certificate = NULL, ra_signing_certificate = NULL, ra_encryption_certificate=NULL;

    // No fingerprint
    ok_status(SecSCEPValidateCACertMessage(certificates, NULL,
        &ca_certificate, &ra_signing_certificate, &ra_encryption_certificate),
              "parse CA/RA getcacert message");
    ok(CFEqualSafe(ra_signing_certificate, expected_signing_cert));
    ok(CFEqualSafe(ca_certificate, expected_root_cert)); // with no fingerprint specified, we should get the root cert in the chain
    is(ra_encryption_certificate, NULL, "no separate ra encryption cert");
    CFReleaseNull(ca_certificate);
    CFReleaseNull(ra_signing_certificate);
    CFReleaseNull(ra_encryption_certificate);

    const uint8_t _md5_ca_fingerprint[] = { // MD5 fingerprint of root
        0xd8, 0x3a, 0x86, 0xc3, 0x07, 0xf5, 0x70, 0xb0, 0x01, 0x49, 0x1b, 0x3c, 0x81, 0x65, 0x7b, 0x2c
    };

    // Invalid fingerprint length
    NSData *fingerprint = [NSData dataWithBytes:_md5_ca_fingerprint length:2];
    is(errSecInvalidDigestAlgorithm, SecSCEPValidateCACertMessage(certificates, (__bridge CFDataRef)fingerprint,
                                                                  &ca_certificate, &ra_signing_certificate, &ra_encryption_certificate));

    // MD5
    fingerprint = [NSData dataWithBytes:_md5_ca_fingerprint length:sizeof(_md5_ca_fingerprint)];
    ok_status(SecSCEPValidateCACertMessage(certificates, (__bridge CFDataRef)fingerprint,
        &ca_certificate, &ra_signing_certificate, &ra_encryption_certificate),
              "parse CA/RA getcacert message");
    ok(CFEqualSafe(ra_signing_certificate, expected_signing_cert));
    ok(CFEqualSafe(ca_certificate, expected_root_cert));
    is(ra_encryption_certificate, NULL, "no separate ra encryption cert");
    CFReleaseNull(ca_certificate);
    CFReleaseNull(ra_signing_certificate);
    CFReleaseNull(ra_encryption_certificate);

    // SHA1
    const uint8_t _sha1_ca_fingerprint[] = { // SHA1 fingerprint of RA signing cert
        0x0a, 0x3c, 0x1f, 0x32, 0x83, 0xca, 0xd1, 0x30, 0xc2, 0xb9, 0x71, 0xe2, 0x5b, 0xd7, 0x95, 0x41,
        0x0b, 0xca, 0xa4, 0x97
    };
    fingerprint = [NSData dataWithBytes:_sha1_ca_fingerprint length:sizeof(_sha1_ca_fingerprint)];
    ok_status(SecSCEPValidateCACertMessage(certificates, (__bridge CFDataRef)fingerprint,
        &ca_certificate, &ra_signing_certificate, &ra_encryption_certificate),
              "parse CA/RA getcacert message");
    ok(CFEqualSafe(ra_signing_certificate, expected_signing_cert));
    ok(CFEqualSafe(ca_certificate, expected_signing_cert));
    is(ra_encryption_certificate, NULL, "no separate ra encryption cert");
    CFReleaseNull(ca_certificate);
    CFReleaseNull(ra_signing_certificate);
    CFReleaseNull(ra_encryption_certificate);

    // SHA224
    const uint8_t _sha224_ca_fingerprint[] = { // SHA224 fingerprint of subCA
        0x50, 0x3b, 0x0d, 0xaf, 0x9b, 0x53, 0x8e, 0xaf, 0xaa, 0xec, 0x3f, 0x76, 0x86, 0xef, 0x88, 0x8f,
        0xe5, 0x4e, 0x54, 0xfc, 0xd4, 0x08, 0xf5, 0x84, 0x7c, 0xc2, 0x07, 0xbd
    };
    fingerprint = [NSData dataWithBytes:_sha224_ca_fingerprint length:sizeof(_sha224_ca_fingerprint)];
    ok_status(SecSCEPValidateCACertMessage(certificates, (__bridge CFDataRef)fingerprint,
        &ca_certificate, &ra_signing_certificate, &ra_encryption_certificate),
              "parse CA/RA getcacert message");
    ok(CFEqualSafe(ra_signing_certificate, expected_signing_cert));
    ok(CFEqualSafe(ca_certificate, expected_subCA_cert));
    is(ra_encryption_certificate, NULL, "no separate ra encryption cert");
    CFReleaseNull(ca_certificate);
    CFReleaseNull(ra_signing_certificate);
    CFReleaseNull(ra_encryption_certificate);

    // SHA256
    const uint8_t _sha256_ca_fingerprint[] = { // SHA256 fingerprint of RA signing cert
        0x74, 0x7e, 0x12, 0x04, 0xdd, 0xf0, 0x01, 0xca, 0x1f, 0x05, 0xfa, 0xb8, 0xea, 0xa3, 0x14, 0xad,
        0x87, 0x5c, 0xdf, 0x9b, 0x5e, 0xc6, 0x3f, 0x25, 0x9c, 0x0b, 0x3e, 0x21, 0xbc, 0x5e, 0xfe, 0xac
    };
    fingerprint = [NSData dataWithBytes:_sha256_ca_fingerprint length:sizeof(_sha256_ca_fingerprint)];
    ok_status(SecSCEPValidateCACertMessage(certificates, (__bridge CFDataRef)fingerprint,
        &ca_certificate, &ra_signing_certificate, &ra_encryption_certificate),
              "parse CA/RA getcacert message");
    ok(CFEqualSafe(ra_signing_certificate, expected_signing_cert));
    ok(CFEqualSafe(ca_certificate, expected_signing_cert));
    is(ra_encryption_certificate, NULL, "no separate ra encryption cert");
    CFReleaseNull(ca_certificate);
    CFReleaseNull(ra_signing_certificate);
    CFReleaseNull(ra_encryption_certificate);

    // SHA384
    const uint8_t _sha384_ca_fingerprint[] = { // SHA384 fingerprint of subCA
        0x42, 0x72, 0x18, 0x2b, 0x7f, 0x8e, 0xa6, 0x97, 0xf0, 0xc8, 0x5a, 0x7e, 0x35, 0x16, 0x5d, 0xfc,
        0x7b, 0x38, 0x17, 0x82, 0xc3, 0x2f, 0x16, 0xd7, 0x32, 0x97, 0x09, 0x0b, 0xc7, 0x93, 0xf8, 0x08,
        0x44, 0xc5, 0x7d, 0x03, 0xdd, 0x1f, 0x27, 0x34, 0xfd, 0x71, 0xe7, 0xba, 0x93, 0x21, 0x37, 0xe6
    };
    fingerprint = [NSData dataWithBytes:_sha384_ca_fingerprint length:sizeof(_sha384_ca_fingerprint)];
    ok_status(SecSCEPValidateCACertMessage(certificates, (__bridge CFDataRef)fingerprint,
        &ca_certificate, &ra_signing_certificate, &ra_encryption_certificate),
              "parse CA/RA getcacert message");
    ok(CFEqualSafe(ra_signing_certificate, expected_signing_cert));
    ok(CFEqualSafe(ca_certificate, expected_subCA_cert));
    is(ra_encryption_certificate, NULL, "no separate ra encryption cert");
    CFReleaseNull(ca_certificate);
    CFReleaseNull(ra_signing_certificate);
    CFReleaseNull(ra_encryption_certificate);

    // SHA512
    const uint8_t _sha512_ca_fingerprint[] = { // SHA512 fingerprint of root
        0x5c, 0xf9, 0x71, 0x47, 0x7d, 0x38, 0xc2, 0xf2, 0xcc, 0x4e, 0x62, 0xfe, 0x78, 0x5f, 0x23, 0xd1,
        0xec, 0x28, 0x76, 0x64, 0x46, 0xcc, 0x8f, 0x4d, 0xbf, 0xd0, 0xf1, 0x43, 0x55, 0x45, 0x92, 0x9b,
        0x03, 0xeb, 0x4d, 0x7e, 0xd8, 0xc3, 0x3e, 0x82, 0x52, 0x1b, 0x7c, 0x58, 0x1f, 0xef, 0x1e, 0xf6,
        0x2b, 0x77, 0x9d, 0x44, 0xfd, 0x09, 0x5f, 0xc1, 0x69, 0xa6, 0x76, 0xe2, 0xac, 0x11, 0x8a, 0xd4
    };
    fingerprint = [NSData dataWithBytes:_sha512_ca_fingerprint length:sizeof(_sha512_ca_fingerprint)];
    ok_status(SecSCEPValidateCACertMessage(certificates, (__bridge CFDataRef)fingerprint,
        &ca_certificate, &ra_signing_certificate, &ra_encryption_certificate),
              "parse CA/RA getcacert message");
    ok(CFEqualSafe(ra_signing_certificate, expected_signing_cert));
    ok(CFEqualSafe(ca_certificate, expected_root_cert));
    is(ra_encryption_certificate, NULL, "no separate ra encryption cert");
    CFReleaseNull(ca_certificate);
    CFReleaseNull(ra_signing_certificate);
    CFReleaseNull(ra_encryption_certificate);

    CFReleaseNull(certificates);
}

int si_63_scep(int argc, char *const *argv)
{
	plan_tests(93);

	tests();
    test_SCEP_algs();
    test_multiple_recipient_certs();
    test_GetCACert();

	return 0;
}
