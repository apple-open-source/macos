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
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "cms-01-basic.h"
#include "smime_regressions.h"

#include <AssertMacros.h>

#include <utilities/SecCFRelease.h>

#include <Security/SecBase.h>
#include <Security/SecImportExport.h>
#include <Security/SecKeychain.h>
#include <Security/SecIdentity.h>
#include <Security/SecPolicy.h>

#include <Security/SecCmsMessage.h>
#include <Security/SecCmsSignedData.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsSignerInfo.h>
#include <Security/SecCmsEncoder.h>
#include <Security/SecCmsDecoder.h>
#include <Security/SecCmsEnvelopedData.h>
#include <Security/SecCmsRecipientInfo.h>

#include <security_asn1/secerr.h>
#include <security_asn1/seccomon.h>

#define TMP_KEYCHAIN_PATH "/tmp/cms_01_test.keychain"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"

#define kNumberSetupTests 10
static SecKeychainRef setup_keychain(const uint8_t *p12, size_t p12_len, SecIdentityRef *identity, SecCertificateRef *cert) {
    CFDataRef p12Data = NULL;
    CFArrayRef imported_items = NULL, oldSearchList = NULL;
    CFMutableArrayRef newSearchList = NULL;
    SecKeychainRef keychain = NULL;
    SecExternalFormat sef = kSecFormatPKCS12;
    SecItemImportExportKeyParameters keyParams = {
        .passphrase = CFSTR("password")
    };

    /* Create keychain and add to search list (for decryption) */
    unlink(TMP_KEYCHAIN_PATH);
    ok_status(SecKeychainCopySearchList(&oldSearchList),
              "Copy keychain search list");
    require(oldSearchList, out);
    ok(newSearchList = CFArrayCreateMutableCopy(NULL, CFArrayGetCount(oldSearchList)+1, oldSearchList),
       "Create new search list");
    ok_status(SecKeychainCreate(TMP_KEYCHAIN_PATH, 8, "password", false, NULL, &keychain),
              "Create keychain for identity");
    require(keychain, out);
    CFArrayAppendValue(newSearchList, keychain);
    ok_status(SecKeychainSetSearchList(newSearchList),
              "Set keychain search list");

    /* Load identity and set as signer */
    ok(p12Data = CFDataCreate(NULL, p12, p12_len),
       "Create p12 data");
    ok_status(SecItemImport(p12Data, NULL, &sef, NULL, 0, &keyParams, keychain, &imported_items),
              "Import identity");
    is(CFArrayGetCount(imported_items),1,"Imported 1 items");
    is(CFGetTypeID(CFArrayGetValueAtIndex(imported_items, 0)), SecIdentityGetTypeID(),
       "Got back an identity");
    ok(*identity = (SecIdentityRef) CFRetainSafe(CFArrayGetValueAtIndex(imported_items, 0)),
       "Retrieve identity");
    ok_status(SecIdentityCopyCertificate(*identity, cert),
              "Copy certificate");

    CFReleaseNull(p12Data);
    CFReleaseNull(imported_items);

out:
    CFReleaseNull(oldSearchList);
    CFReleaseNull(newSearchList);
    return keychain;
}

#define kNumberCleanupTests 1
static void cleanup_keychain(SecKeychainRef keychain, SecIdentityRef identity, SecCertificateRef cert) {
    /* Delete keychain - from the search list and from disk */
    ok_status(SecKeychainDelete(keychain), "Delete temporary keychain");
    CFReleaseNull(keychain);
    CFReleaseNull(cert);
    CFReleaseNull(identity);
}

static OSStatus sign_please(SecIdentityRef identity, SECOidTag digestAlgTag, bool withAttrs, uint8_t *expected_output, size_t expected_len) {

    OSStatus status = SECFailure;

    SecCmsMessageRef cmsg = NULL;
    SecCmsSignedDataRef sigd = NULL;
    SecCmsContentInfoRef cinfo = NULL;
    SecCmsSignerInfoRef signerInfo = NULL;
    SecCmsEncoderRef encoder = NULL;
    SecArenaPoolRef arena = NULL;
    CSSM_DATA cms_data = {
        .Data = NULL,
        .Length = 0
    };
    uint8_t string_to_sign[] = "This message is signed. Ain't it pretty?";

    /* setup the message */
    require_action_string(cmsg = SecCmsMessageCreate(NULL), out,
                          status = errSecAllocate, "Failed to create message");
    require_action_string(sigd = SecCmsSignedDataCreate(cmsg), out,
                          status = errSecAllocate, "Failed to create signed data");
    require_action_string(cinfo = SecCmsMessageGetContentInfo(cmsg), out,
                          status = errSecParam, "Failed to get cms content info");
    require_noerr_string(status = SecCmsContentInfoSetContentSignedData(cmsg, cinfo, sigd), out,
                         "Failed to set signed data into content info");
    require_action_string(cinfo = SecCmsSignedDataGetContentInfo(sigd), out,
                          status = errSecParam, "Failed to get content info from signed data");
    require_noerr_string(status = SecCmsContentInfoSetContentData(cmsg, cinfo, NULL, false), out,
                         "Failed to set signed data content info");
    require_action_string(signerInfo = SecCmsSignerInfoCreate(cmsg, identity, digestAlgTag), out,
                          status = errSecAllocate, "Failed to create signer info");
    require_noerr_string(status = SecCmsSignerInfoIncludeCerts(signerInfo, SecCmsCMCertOnly,
                                                               certUsageEmailSigner), out,
                         "Failed to put certs in signer info");

    if(withAttrs) {
        require_noerr_string(status = SecCmsSignerInfoAddSigningTime(signerInfo, 480000000.0), out,
                             "Couldn't add an attribute");
    }
    require_noerr_string(status = SecCmsSignedDataAddSignerInfo(sigd, signerInfo), out,
                         "Couldn't add signer info to signed data");

    /* encode now */
    require_noerr_string(status = SecArenaPoolCreate(1024, &arena), out,
                         "Failed to create arena");
    require_noerr_string(status = SecCmsEncoderCreate(cmsg, NULL, NULL, &cms_data, arena, NULL, NULL,
                                                      NULL, NULL, NULL, NULL, &encoder), out,
                         "Failed to create encoder");
    require_noerr_string(status = SecCmsEncoderUpdate(encoder, string_to_sign, sizeof(string_to_sign)), out,
                         "Failed to add data ");
    status = SecCmsEncoderFinish(encoder);
    encoder = NULL; // SecCmsEncoderFinish always frees the encoder but doesn't NULL it.
    require_noerr_quiet(status, out);

    /* verify the output matches expected results */
    if (expected_output) {
        require_action_string(expected_len == cms_data.Length, out,
                              status = -1, "Output size differs from expected");
        require_noerr_action_string(memcmp(expected_output, cms_data.Data, expected_len), out,
                                    status = -1, "Output differs from expected");
    }

out:
    if (encoder) {
        SecCmsEncoderDestroy(encoder);
    }
    if (arena) {
        SecArenaPoolFree(arena, false);
    }
    if (cmsg) {
        SecCmsMessageDestroy(cmsg);
    }
    return status;

}

static OSStatus verify_please(SecKeychainRef keychain, uint8_t *data_to_verify, size_t length) {
    OSStatus status = SECFailure;
    SecCmsDecoderRef decoder = NULL;
    SecCmsMessageRef cmsg = NULL;
    SecCmsContentInfoRef cinfo = NULL;
    SecCmsSignedDataRef sigd = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;

    if (!data_to_verify) {
        return errSecSuccess; // reasons...
    }

    require_noerr_string(status = SecCmsDecoderCreate(NULL, NULL, NULL, NULL, NULL,
                                                      NULL, NULL, &decoder), out,
                         "Failed to create decoder");
    require_noerr_string(status = SecCmsDecoderUpdate(decoder, data_to_verify, length), out,
                         "Failed to add data ");
    status = SecCmsDecoderFinish(decoder, &cmsg);
    decoder = NULL; // SecCmsDecoderFinish always frees the decoder
    require_noerr_quiet(status, out);

    require_action_string(cinfo = SecCmsMessageContentLevel(cmsg, 0), out,
                          status = errSecDecode, "Failed to get content info");
    require_action_string(SEC_OID_PKCS7_SIGNED_DATA == SecCmsContentInfoGetContentTypeTag(cinfo), out,
                          status = errSecDecode, "Content type was pkcs7 signed data");
    require_action_string(sigd = (SecCmsSignedDataRef)SecCmsContentInfoGetContent(cinfo), out,
                          status = errSecDecode, "Failed to get signed data");
    require_action_string(policy = SecPolicyCreateBasicX509(), out,
                          status = errSecAllocate, "Failed to create basic policy");
    status = SecCmsSignedDataVerifySignerInfo(sigd, 0, keychain, policy, &trust);

out:
    if (decoder) {
        SecCmsDecoderDestroy(decoder);
    }
    if (cmsg) {
        SecCmsMessageDestroy(cmsg);
    }
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    return status;
}

static uint8_t *invalidate_signature(uint8_t *cms_data, size_t length) {
    if (!cms_data || !length || (length < 10)) {
        return NULL;
    }
    uint8_t *invalid_cms = NULL;

    invalid_cms = malloc(length);
    if (invalid_cms) {
        memcpy(invalid_cms, cms_data, length);
        /* This modifies the signature part of the test cms binaries */
        invalid_cms[length - 10] = 0x00;
    }

    return invalid_cms;
}

static OSStatus invalidate_and_verify(SecKeychainRef kc, uint8_t *cms_data, size_t length) {
    OSStatus status = SECFailure;
    uint8_t *invalid_cms_data = NULL;

    if (!cms_data) {
        return SECFailure; // reasons...
    }

    require_action_string(invalid_cms_data = invalidate_signature(cms_data, length), out,
                          status = errSecAllocate, "Unable to allocate buffer for invalid cms data");
    status = verify_please(kc, invalid_cms_data, length);

out:
    if (invalid_cms_data) {
        free(invalid_cms_data);
    }
    return status;
}

/* forward declaration */
static OSStatus decrypt_please(uint8_t *data_to_decrypt, size_t length);

static OSStatus encrypt_please(SecCertificateRef recipient, SECOidTag encAlg, int keysize) {
    OSStatus status = SECFailure;
    SecCmsMessageRef cmsg = NULL;
    SecCmsEnvelopedDataRef envd = NULL;
    SecCmsContentInfoRef cinfo = NULL;
    SecCmsRecipientInfoRef rinfo = NULL;
    SecArenaPoolRef arena = NULL;
    SecCmsEncoderRef encoder = NULL;
    CSSM_DATA cms_data = {
        .Data = NULL,
        .Length = 0
    };
    const uint8_t data_to_encrypt[] = "This data is encrypted. Is cool, no?";

    /* set up the message */
    require_action_string(cmsg = SecCmsMessageCreate(NULL), out,
                          status = errSecAllocate, "Failed to create message");
    require_action_string(envd = SecCmsEnvelopedDataCreate(cmsg, encAlg, keysize), out,
                          status = errSecAllocate, "Failed to create enveloped data");
    require_action_string(cinfo = SecCmsMessageGetContentInfo(cmsg), out,
                          status = errSecParam, "Failed to get content info from cms message");
    require_noerr_string(status = SecCmsContentInfoSetContentEnvelopedData(cmsg, cinfo, envd), out,
                         "Failed to set enveloped data in cms message");
    require_action_string(cinfo = SecCmsEnvelopedDataGetContentInfo(envd), out,
                          status = errSecParam, "Failed to get content info from enveloped data");
    require_noerr_string(status = SecCmsContentInfoSetContentData(cmsg, cinfo, NULL, false), out,
                         "Failed to set data type in envelope");
    require_action_string(rinfo = SecCmsRecipientInfoCreate(cmsg, recipient), out,
                          status = errSecAllocate, "Failed to create recipient info");
    require_noerr_string(status = SecCmsEnvelopedDataAddRecipient(envd, rinfo), out,
                         "Failed to add recipient info to envelope");

    /* encode the message */
    require_noerr_string(status = SecArenaPoolCreate(1024, &arena), out,
                         "Failed to create arena");
    require_noerr_string(status = SecCmsEncoderCreate(cmsg, NULL, NULL, &cms_data, arena, NULL, NULL,
                                                      NULL, NULL, NULL, NULL, &encoder), out,
                         "Failed to create encoder");
    require_noerr_string(status = SecCmsEncoderUpdate(encoder, data_to_encrypt, sizeof(data_to_encrypt)), out,
                         "Failed to update encoder with data");
    status = SecCmsEncoderFinish(encoder);
    encoder = NULL; // SecCmsEncoderFinish always frees the encoder but doesn't NULL it.
    require_noerr_quiet(status, out);

    require_noerr_string(status = decrypt_please(cms_data.Data, cms_data.Length), out,
                         "Failed to decrypt the data we just encrypted");

out:
    if (encoder) {
        SecCmsEncoderDestroy(encoder);
    }
    if (arena) {
        SecArenaPoolFree(arena, false);
    }
    if (cmsg) {
        SecCmsMessageDestroy(cmsg);
    }
    return status;
}

static OSStatus decrypt_please(uint8_t *data_to_decrypt, size_t length) {
    OSStatus status = SECFailure;
    SecCmsDecoderRef decoder = NULL;
    SecCmsMessageRef cmsg = NULL;
    CSSM_DATA_PTR content = NULL;
    const uint8_t encrypted_string[] = "This data is encrypted. Is cool, no?";

    require_noerr_string(status = SecCmsDecoderCreate(NULL, NULL, NULL, NULL, NULL,
                                                      NULL, NULL, &decoder), out,
                         "Failed to create decoder");
    require_noerr_string(status = SecCmsDecoderUpdate(decoder, data_to_decrypt, length), out,
                         "Failed to add data ");
    status = SecCmsDecoderFinish(decoder, &cmsg);
    decoder = NULL; // SecCmsDecoderFinish always frees the decoder
    require_noerr_quiet(status, out);
    require_action_string(content = SecCmsMessageGetContent(cmsg), out,
                          status = errSecDecode, "Unable to get message contents");

    /* verify the output matches expected results */
    require_action_string(sizeof(encrypted_string) == content->Length, out,
                          status = -1, "Output size differs from expected");
    require_noerr_action_string(memcmp(encrypted_string, content->Data, content->Length), out,
                                status = -1, "Output differs from expected");

out:
    if (cmsg) {
        SecCmsMessageDestroy(cmsg);
    }
    return status;
}

/* Signing with attributes goes through a different code path than signing without,
 * so we need to test both. */
#define kNumberSignTests 10
static void sign_tests(SecIdentityRef identity, bool isRSA) {

    /* no attributes */
    is(sign_please(identity, SEC_OID_MD5, false, NULL, 0),
       SEC_ERROR_INVALID_ALGORITHM, "Signed with MD5. Not cool.");
    is(sign_please(identity, SEC_OID_SHA1, false, (isRSA) ? rsa_sha1 : NULL,
                   (isRSA) ? sizeof(rsa_sha1) : 0),
       errSecSuccess, "Signed with SHA-1");
    is(sign_please(identity, SEC_OID_SHA256, false, (isRSA) ? rsa_sha256 : NULL,
                   (isRSA) ? sizeof(rsa_sha256) : 0),
       errSecSuccess, "Signed with SHA-256");
    is(sign_please(identity, SEC_OID_SHA384, false, NULL, 0), errSecSuccess, "Signed with SHA-384");
    is(sign_please(identity, SEC_OID_SHA512, false, NULL, 0), errSecSuccess, "Signed with SHA-512");

    /* with attributes */
    is(sign_please(identity, SEC_OID_MD5, true, NULL, 0),
       SEC_ERROR_INVALID_ALGORITHM, "Signed with MD5 and attributes. Not cool.");
    is(sign_please(identity, SEC_OID_SHA1, true, (isRSA) ? rsa_sha1_attr : NULL,
                   (isRSA) ? sizeof(rsa_sha1_attr) : 0),
       errSecSuccess, "Signed with SHA-1 and attributes");
    is(sign_please(identity, SEC_OID_SHA256, true, (isRSA) ? rsa_sha256_attr : NULL,
                   (isRSA) ? sizeof(rsa_sha256_attr) : 0),
       errSecSuccess, "Signed with SHA-256 and attributes");
    is(sign_please(identity, SEC_OID_SHA384, true, NULL, 0),
       errSecSuccess, "Signed with SHA-384 and attributes");
    is(sign_please(identity, SEC_OID_SHA512, true, NULL, 0),
       errSecSuccess, "Signed with SHA-512 and attributes");
}

/* Verifying with attributes goes through a different code path than verifying without,
 * so we need to test both. */
#define kNumberVerifyTests 12
static void verify_tests(SecKeychainRef kc, bool isRsa) {
    /* no attributes */
    is(verify_please(kc, (isRsa) ? rsa_md5 : ec_md5,
                     (isRsa) ? sizeof(rsa_md5) : sizeof(ec_md5)),
       (isRsa) ? errSecSuccess : SECFailure,
       "Verify MD5, no attributes");
    is(verify_please(kc, (isRsa) ? rsa_sha1 : ec_sha1,
                     (isRsa) ? sizeof(rsa_sha1) : sizeof(ec_sha1)),
       errSecSuccess, "Verify SHA1, no attributes");
    is(verify_please(kc, (isRsa) ? rsa_sha256 : ec_sha256,
                     (isRsa) ? sizeof(rsa_sha256) : sizeof(ec_sha256)),
       errSecSuccess, "Verify SHA256, no attributes");

    /* with attributes */
    is(verify_please(kc, (isRsa) ? rsa_md5_attr : NULL,
                     (isRsa) ? sizeof(rsa_md5_attr) : 0),
       errSecSuccess, "Verify MD5, with attributes");
    is(verify_please(kc, (isRsa) ? rsa_sha1_attr : ec_sha1_attr,
                     (isRsa) ? sizeof(rsa_sha1_attr) : sizeof(ec_sha1_attr)),
       errSecSuccess, "Verify SHA1, with attributes");
    is(verify_please(kc, (isRsa) ? rsa_sha256_attr : ec_sha256_attr,
                     (isRsa) ? sizeof(rsa_sha256_attr) : sizeof(ec_sha256_attr)),
       errSecSuccess, "Verify SHA256, with attributes");

    /***** Once more, with validation errors *****/

    /* no attributes */
    is(invalidate_and_verify(kc, (isRsa) ? rsa_md5 : ec_md5,
                     (isRsa) ? sizeof(rsa_md5) : sizeof(ec_md5)),
       SECFailure, "Verify invalid MD5, no attributes");
    is(invalidate_and_verify(kc, (isRsa) ? rsa_sha1 : ec_sha1,
                     (isRsa) ? sizeof(rsa_sha1) : sizeof(ec_sha1)),
       SECFailure, "Verify invalid SHA1, no attributes");
    is(invalidate_and_verify(kc, (isRsa) ? rsa_sha256 : ec_sha256,
                     (isRsa) ? sizeof(rsa_sha256) : sizeof(ec_sha256)),
       SECFailure, "Verify invalid SHA256, no attributes");

    /* with attributes */
    is(invalidate_and_verify(kc, (isRsa) ? rsa_md5_attr : NULL,
                     (isRsa) ? sizeof(rsa_md5_attr) : 0),
       SECFailure, "Verify invalid MD5, with attributes");
    is(invalidate_and_verify(kc, (isRsa) ? rsa_sha1_attr : ec_sha1_attr,
                     (isRsa) ? sizeof(rsa_sha1_attr) : sizeof(ec_sha1_attr)),
       SECFailure, "Verify invalid SHA1, with attributes");
    is(invalidate_and_verify(kc, (isRsa) ? rsa_sha256_attr : ec_sha256_attr,
                     (isRsa) ? sizeof(rsa_sha256_attr) : sizeof(ec_sha256_attr)),
       SECFailure, "Verify invalid SHA256, with attributes");
}

#define kNumberEncryptTests 5
static void encrypt_tests(SecCertificateRef certificate) {
    is(encrypt_please(certificate, SEC_OID_DES_EDE3_CBC, 192),
       errSecSuccess, "Encrypt with 3DES");
    is(encrypt_please(certificate, SEC_OID_RC2_CBC, 128),
       errSecSuccess, "Encrypt with 128-bit RC2");
    is(encrypt_please(certificate, SEC_OID_AES_128_CBC, 128),
       errSecSuccess, "Encrypt with 128-bit AES");
    is(encrypt_please(certificate, SEC_OID_AES_192_CBC, 192),
       errSecSuccess, "Encrypt with 192-bit AES");
    is(encrypt_please(certificate, SEC_OID_AES_256_CBC, 256),
       errSecSuccess, "Encrypt with 256-bit AES");
}

#define kNumberDecryptTests 5
static void decrypt_tests(bool isRsa) {
    is(decrypt_please((isRsa) ? rsa_3DES : ec_3DES,
                      (isRsa) ? sizeof(rsa_3DES) : sizeof(ec_3DES)),
       errSecSuccess, "Decrypt 3DES");
    is(decrypt_please((isRsa) ? rsa_RC2 : ec_RC2,
                      (isRsa) ? sizeof(rsa_RC2) : sizeof(ec_RC2)),
       errSecSuccess, "Decrypt 128-bit RC2");
    is(decrypt_please((isRsa) ? rsa_AES_128 : ec_AES_128,
                      (isRsa) ? sizeof(rsa_AES_128) : sizeof(ec_AES_128)),
       errSecSuccess, "Decrypt 128-bit AES");
    is(decrypt_please((isRsa) ? rsa_AES_192 : ec_AES_192,
                      (isRsa) ? sizeof(rsa_AES_192) : sizeof(ec_AES_192)),
       errSecSuccess, "Decrypt 192-bit AES");
    is(decrypt_please((isRsa) ? rsa_AES_256 : ec_AES_256,
                      (isRsa) ? sizeof(rsa_AES_256) : sizeof(ec_AES_256)),
       errSecSuccess, "Decrypt 256-bit AES");
}

int cms_01_basic(int argc, char *const *argv)
{
    plan_tests(2*(kNumberSetupTests + kNumberSignTests + kNumberVerifyTests +
                  kNumberEncryptTests + kNumberDecryptTests + kNumberCleanupTests));

    SecKeychainRef kc = NULL;
    SecIdentityRef identity = NULL;
    SecCertificateRef certificate = NULL;

    /* RSA tests */
    kc = setup_keychain(_rsa_identity, sizeof(_rsa_identity), &identity, &certificate);
    sign_tests(identity, true);
    verify_tests(kc, true);
    encrypt_tests(certificate);
    decrypt_tests(true);
    cleanup_keychain(kc, identity, certificate);

    /* EC tests */
    kc = setup_keychain(_ec_identity, sizeof(_ec_identity), &identity, &certificate);
    sign_tests(identity, false);
    verify_tests(kc, false);
    encrypt_tests(certificate);
    decrypt_tests(false);
    cleanup_keychain(kc, identity, certificate);

    return 0;
}
