/*
 * Copyright (c) 2006-2012 Apple Inc. All Rights Reserved.
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

/*
 * secCrypto.c - interface between SSL and SecKey/SecDH interfaces.
 */

#include "secCrypto.h"

#include <Security/Security.h>
#include <Security/SecKeyPriv.h>
#include <Security/oidsalg.h>
#include <AssertMacros.h>

// Include Keys and certs in this file.
#include "test-certs/Server1_Cert_rsa_rsa.h"
#include "test-certs/Server1_Key_rsa.h"
#include "test-certs/Server2_Cert_rsa_rsa.h"
#include "test-certs/Server2_Key_rsa.h"
#include "test-certs/Server1_Cert_ecc_ecc.h"
#include "test-certs/Server1_Key_ecc.h"
#include "test-certs/ecclientkey.h"
#include "test-certs/ecclientcert.h"
#include "test-certs/eccert.h"
#include "test-certs/eckey.h"

#if TARGET_OS_IPHONE

#include <Security/SecRSAKey.h>
#include <Security/SecECKey.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentityPriv.h>

#endif


#define test_printf(x...)
//#define test_printf printf

SSLCertificate server_cert;
tls_private_key_t server_key;

#include <Security/oidsalg.h>

/* Private Key operations */
static
SecAsn1Oid oidForSSLHash(tls_hash_algorithm hash)
{
    switch (hash) {
        case tls_hash_algorithm_SHA1:
            return CSSMOID_SHA1WithRSA;
        case tls_hash_algorithm_SHA256:
            return CSSMOID_SHA256WithRSA;
        case tls_hash_algorithm_SHA384:
            return CSSMOID_SHA384WithRSA;
        default:
            break;
    }
    // Internal error
    assert(0);
    // This guarantee failure down the line
    return CSSMOID_MD5WithRSA;
}

static
int mySSLPrivKeyRSA_sign(void *key, tls_hash_algorithm hash, const uint8_t *plaintext, size_t plaintextLen, uint8_t *sig, size_t *sigLen)
{
    SecKeyRef keyRef = key;

    if(hash == tls_hash_algorithm_None) {
        return SecKeyRawSign(keyRef, kSecPaddingPKCS1, plaintext, plaintextLen, sig, sigLen);
    } else {
        SecAsn1AlgId  algId;
        algId.algorithm = oidForSSLHash(hash);
        return SecKeySignDigest(keyRef, &algId, plaintext, plaintextLen, sig, sigLen);
    }
}

static
int mySSLPrivKeyECDSA_sign(void *key, const uint8_t *plaintext, size_t plaintextLen, uint8_t *sig, size_t *sigLen)
{
    SecKeyRef keyRef = key;

    return SecKeyRawSign(keyRef, kSecPaddingPKCS1, plaintext, plaintextLen, sig, sigLen);
}

static
int mySSLPrivKeyRSA_decrypt(void *key, const uint8_t *ciphertext, size_t ciphertextLen, uint8_t *plaintext, size_t *plaintextLen)
{
    SecKeyRef keyRef = key;

    return SecKeyDecrypt(keyRef, kSecPaddingPKCS1, ciphertext, ciphertextLen, plaintext, plaintextLen);
}


int init_server_keys(bool ecdsa,
                     unsigned char *cert_der, size_t cert_der_len,
                     unsigned char *key_der, size_t key_der_len,
                     SSLCertificate *cert, tls_private_key_t *key)
{
    int err = 0;

    cert->next = NULL;
    cert->derCert.data = cert_der;
    cert->derCert.length = cert_der_len;

    SecKeyRef privKey = NULL;

#if TARGET_OS_IPHONE
    if(ecdsa)
    {
        privKey = SecKeyCreateECPrivateKey(kCFAllocatorDefault, key_der, key_der_len,
                                           kSecKeyEncodingPkcs1);
    } else {
        privKey = SecKeyCreateRSAPrivateKey(kCFAllocatorDefault, key_der, key_der_len,
                                            kSecKeyEncodingPkcs1);
    }

    require_action(privKey, fail, err=-1);
#else
    // Create the SecKeyRef
    CFErrorRef error = NULL;
    CFDataRef keyData = CFDataCreate(kCFAllocatorDefault, key_der, key_der_len);
    CFMutableDictionaryRef parameters = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL);
    CFDictionarySetValue(parameters, kSecAttrKeyType, ecdsa?kSecAttrKeyTypeECDSA:kSecAttrKeyTypeRSA);
    CFDictionarySetValue(parameters, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
    privKey = SecKeyCreateFromData(parameters, keyData, &error);

    require_action(privKey, fail, err=(int)CFErrorGetCode(error));
#endif

    size_t keySize = SecKeyGetBlockSize(privKey);

    if(ecdsa) {
#if TARGET_OS_IPHONE
        /* Compute signature size from key size */
        size_t sigSize = 8+2*keySize;
#else
        size_t sigSize = keySize;
#endif
        require((*key = tls_private_key_ecdsa_create(privKey, sigSize,
                                                     SecECKeyGetNamedCurve(privKey), mySSLPrivKeyECDSA_sign)), fail);
    } else {
        require((*key = tls_private_key_rsa_create(privKey, keySize,
                                                   mySSLPrivKeyRSA_sign, mySSLPrivKeyRSA_decrypt)), fail);
    }

    err = 0;

fail:
#if !TARGET_OS_IPHONE
    CFReleaseSafe(parameters);
    CFReleaseSafe(keyData);
    CFReleaseSafe(error);
#endif

    return err;
}

void clean_server_keys(tls_private_key_t key)
{
    if(key == NULL)
        return;
    SecKeyRef privKey = tls_private_key_get_context(key);
    CFReleaseSafe(privKey);
    tls_private_key_destroy(key);
}

static const char base64_chars[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


static void base64_dump(tls_buffer data, const char *header)
{
    char line[65];
    uint32_t c;
    size_t n, m;

    test_printf("-----BEGIN %s-----\n", header);
    m = n = 0;
    while (n < data.length) {
        c = data.data[n];
        n++;

        c = c << 8;
        if (n < data.length)
            c |= data.data[n];
        n++;

        c = c << 8;
        if (n < data.length)
            c |= data.data[n];
        n++;

        line[m++] = base64_chars[(c & 0x00fc0000) >> 18];
        line[m++] = base64_chars[(c & 0x0003f000) >> 12];
        if (n > data.length + 1)
            line[m++] = '=';
        else
            line[m++] = base64_chars[(c & 0x00000fc0) >> 6];
        if (n > data.length)
            line[m++] = '=';
        else
            line[m++] = base64_chars[(c & 0x0000003f) >> 0];
        if (m == sizeof(line) - 1) {
            line[sizeof(line) - 1] = '\0';
            test_printf("%s\n", line);
            m = 0;
        }
        assert(m < sizeof(line) - 1);
    }
    if (m) {
        line[m] = '\0';
        test_printf("%s\n", line);
    }

    test_printf("-----END %s-----\n", header);

}

static int tls_create_trust_from_certs(const SSLCertificate *cert, SecTrustRef *trustRef)
{
    int err;
    CFMutableArrayRef certArray = NULL;
    CFDataRef certData = NULL;
    SecCertificateRef cfCert = NULL;

    if(cert==NULL) {
        test_printf("No certs, do not create SecTrustRef\n");
        *trustRef = NULL;
        return 0;
    }

    certArray = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    while(cert) {
        base64_dump(cert->derCert, "CERTIFICATE");
        require_action((certData = CFDataCreate(kCFAllocatorDefault, cert->derCert.data, cert->derCert.length)), out, err = errSecAllocate);
        require_action((cfCert = SecCertificateCreateWithData(kCFAllocatorDefault, certData)), out, err = errSecAllocate);
        CFArrayAppendValue(certArray, cfCert);
        CFReleaseNull(cfCert);
        CFReleaseNull(certData);
        cert=cert->next;
    }

    require_noerr((err=SecTrustCreateWithCertificates(certArray, NULL, trustRef)), out);

out:
    CFReleaseSafe(certData);
    CFReleaseSafe(cfCert);
    CFReleaseSafe(certArray);

    return err;
}

/* Create SecTrustRef */
int tls_create_peer_trust(tls_handshake_t hdsk, SecTrustRef *trustRef)
{
    int err;
    const SSLCertificate *cert;

    require_action(trustRef, out, err=errSecParam);

    cert = tls_handshake_get_peer_certificates(hdsk);

    return tls_create_trust_from_certs(cert, trustRef);
out:

    return err;
}


/* Extract the pubkey from a cert chain, and send it to the tls_handshake context */
int tls_set_peer_pubkey(tls_handshake_t hdsk, SecTrustRef trustRef)
{
    int err;
    CFIndex algId;
    SecKeyRef pubkey = NULL;
    CFDataRef modulus = NULL;
    CFDataRef exponent = NULL;
    CFDataRef ecpubdata = NULL;

    require_action((pubkey = SecTrustCopyPublicKey(trustRef)), errOut, err=-9808); // errSSLBadCert

#if TARGET_OS_IPHONE
	algId = SecKeyGetAlgorithmID(pubkey);
#else
	algId = SecKeyGetAlgorithmId(pubkey);
#endif

    err = -9809; //errSSLCrypto;

    switch(algId) {
        case kSecRSAAlgorithmID:
        {
            require((modulus = SecKeyCopyModulus(pubkey)), errOut);
            require((exponent = SecKeyCopyExponent(pubkey)), errOut);

            tls_buffer mod;
            tls_buffer exp;

            mod.data = (uint8_t *)CFDataGetBytePtr(modulus);
            mod.length = CFDataGetLength(modulus);

            exp.data = (uint8_t *)CFDataGetBytePtr(exponent);
            exp.length = CFDataGetLength(exponent);

            err = tls_handshake_set_peer_rsa_public_key(hdsk, &mod, &exp);
            break;
        }
        case kSecECDSAAlgorithmID:
        {
            tls_named_curve curve = (tls_named_curve)SecECKeyGetNamedCurve(pubkey);
            require((ecpubdata = SecECKeyCopyPublicBits(pubkey)), errOut);

            tls_buffer pubdata;
            pubdata.data = (uint8_t *)CFDataGetBytePtr(ecpubdata);
            pubdata.length = CFDataGetLength(ecpubdata);

            err = tls_handshake_set_peer_ec_public_key(hdsk, curve, &pubdata);

            break;
        }
        default:
            break;
    }

errOut:
    CFReleaseSafe(pubkey);
    CFReleaseSafe(modulus);
    CFReleaseSafe(exponent);
    CFReleaseSafe(ecpubdata);

    return err;
}

int tls_evaluate_trust(tls_handshake_t hdsk, SecTrustRef trustRef)
{
    int err;
    const tls_buffer *ocsp_response;
    CFDataRef ocsp_data = NULL;
    CFDictionaryRef trust_results = NULL;
    CFArrayRef trust_properties = NULL;
    SecTrustResultType trust_result = kSecTrustResultInvalid;

    ocsp_response = tls_handshake_get_peer_ocsp_response(hdsk);

    if(ocsp_response) {
        base64_dump(*ocsp_response, "OCSP RESPONSE");
        ocsp_data = CFDataCreate(kCFAllocatorDefault, ocsp_response->data, ocsp_response->length);
        require_noerr((err=SecTrustSetOCSPResponse(trustRef, ocsp_data)), errOut);
    }

    // SecTrustSetCTData(trsutRef, sct_data);

    require_noerr((err=SecTrustEvaluate(trustRef, &trust_result)), errOut);

    test_printf("SecTrustEvaluate result: %d\n", trust_result);

    trust_results = SecTrustCopyResult(trustRef);
    trust_properties = SecTrustCopyProperties(trustRef);

    //CFShow(trust_results);
    //CFShow(trust_properties);

    /* Pretend it's all OK so we can continue*/
    tls_handshake_set_peer_trust(hdsk, tls_handshake_trust_ok);

    err = noErr;

errOut:
    CFReleaseSafe(ocsp_data);
    CFReleaseSafe(trust_properties);
    CFReleaseSafe(trust_results);

    return err;

}

/* Extract the pubkey from a cert chain, and send it to the tls_handshake context */
int tls_set_encrypt_pubkey(tls_handshake_t hdsk, const SSLCertificate *certchain)
{
    int err;
    CFIndex algId;
    SecTrustRef trustRef = NULL;
    SecKeyRef pubkey = NULL;
    CFDataRef modulus = NULL;
    CFDataRef exponent = NULL;

#if 0
    { /* dump certs */
        int i=0;
        int j;
        const SSLCertificate *tmp = certchain;
        while(tmp) {
            printf("cert%d[] = {", i);
            for(j=0; j<tmp->derCert.length; j++) {
                if((j&0xf)==0)
                    printf("\n");
                printf("0x%02x, ", tmp->derCert.data[j]);
            }
            printf("}\n");
            tmp=tmp->next;
            i++;
        }
    }
#endif

    require_noerr((err=tls_create_trust_from_certs(certchain, &trustRef)), errOut);
    require_action((pubkey = SecTrustCopyPublicKey(trustRef)), errOut, err=-9808); // errSSLBadCert

#if TARGET_OS_IPHONE
    algId = SecKeyGetAlgorithmID(pubkey);
#else
    algId = SecKeyGetAlgorithmId(pubkey);
#endif

    err = -9809; //errSSLCrypto;

    switch(algId) {
        case kSecRSAAlgorithmID:
        {
            require((modulus = SecKeyCopyModulus(pubkey)), errOut);
            require((exponent = SecKeyCopyExponent(pubkey)), errOut);

            tls_buffer mod;
            tls_buffer exp;

            mod.data = (uint8_t *)CFDataGetBytePtr(modulus);
            mod.length = CFDataGetLength(modulus);

            exp.data = (uint8_t *)CFDataGetBytePtr(exponent);
            exp.length = CFDataGetLength(exponent);

            err = tls_handshake_set_encrypt_rsa_public_key(hdsk, &mod, &exp);
            break;
        }
        default:
            break;
    }

errOut:
    CFReleaseSafe(trustRef);
    CFReleaseSafe(pubkey);
    CFReleaseSafe(modulus);
    CFReleaseSafe(exponent);

    return err;
}

