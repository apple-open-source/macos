/*
 * Copyright (c) 2008-2010,2012-2014 Apple Inc. All Rights Reserved.
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
    CFDictionarySetValue(subject_alt_names, CFSTR("dnsname"), CFSTR("xey.nl"));

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
	CFStringRef nt_princ_name_key = CFSTR("ntPrincipalName");
	CFDictionaryRef nt_princ = CFDictionaryCreate(NULL, (const void **)&nt_princ_name_key, (const void **)&nt_princ_name_val, 1, NULL, NULL);
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

#if TARGET_OS_IPHONE
    CFDataRef scep_request = SecSCEPGenerateCertificateRequest(rdns,
        csr_parameters, phone_publicKey, phone_privateKey, NULL, ca_cert);
    isnt(scep_request, NULL, "got scep blob");
    //write_data("/tmp/scep_request.der", scep_request);
#endif

    CFReleaseNull(email_dn);
    CFReleaseNull(cn_dn);
    CFReleaseNull(dn_array[0]);
    CFReleaseNull(dn_array[1]);
    CFReleaseNull(rdns);

#if TARGET_OS_IPHONE
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
#endif

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
    CFDictionarySetValue(subject_alt_names, CFSTR("dnsname"), CFSTR("xey.nl"));

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

int si_62_csr(int argc, char *const *argv)
{
#if TARGET_OS_IPHONE
	plan_tests(27);
#else
    plan_tests(20);
#endif

	tests();
    test_ec_csr();

	return 0;
}
