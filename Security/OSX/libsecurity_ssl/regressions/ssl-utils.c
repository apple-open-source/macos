/*
 * Copyright (c) 2012,2014 Apple Inc. All Rights Reserved.
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


#include <Security/Security.h>
#include <AssertMacros.h>

#include "ssl-utils.h"

#include <Security/SecCertificatePriv.h>
#include "test-certs/CA-RSA_Cert.h"
#include "test-certs/ServerRSA_Key.h"
#include "test-certs/ServerRSA_Cert_CA-RSA.h"
#include "test-certs/ClientRSA_Key.h"
#include "test-certs/ClientRSA_Cert_CA-RSA.h"
#include "test-certs/UntrustedClientRSA_Key.h"
#include "test-certs/UntrustedClientRSA_Cert_Untrusted-CA-RSA.h"

#include <Security/SecIdentityPriv.h>
#include <Security/SecCertificatePriv.h>

#include "test-certs/eckey.h"
#include "test-certs/eccert.h"
#include "test-certs/ecclientcert.h"
#include "test-certs/ecclientkey.h"
#include "privkey-1.h"
#include "cert-1.h"

#if TARGET_OS_IPHONE
#include <Security/SecRSAKey.h>
#include <Security/SecECKey.h>
#endif


static
SecKeyRef create_private_key_from_der(bool ecdsa, const unsigned char *pkey_der, size_t pkey_der_len)
{
    SecKeyRef privKey;
#if TARGET_OS_IPHONE
    if(ecdsa) {
        privKey = SecKeyCreateECPrivateKey(kCFAllocatorDefault, pkey_der, pkey_der_len, kSecKeyEncodingPkcs1);
    } else {
        privKey = SecKeyCreateRSAPrivateKey(kCFAllocatorDefault, pkey_der, pkey_der_len, kSecKeyEncodingPkcs1);
    }
#else
    CFErrorRef error = NULL;
    CFDataRef keyData = CFDataCreate(kCFAllocatorDefault, pkey_der, pkey_der_len);
    CFMutableDictionaryRef parameters = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL);
    CFDictionarySetValue(parameters, kSecAttrKeyType, ecdsa?kSecAttrKeyTypeECDSA:kSecAttrKeyTypeRSA);
    CFDictionarySetValue(parameters, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
    privKey = SecKeyCreateFromData(parameters, keyData, &error);
    CFReleaseNull(keyData);
    CFReleaseNull(parameters);
    CFReleaseNull(error);
#endif
    return privKey;
}

static
CF_RETURNS_RETAINED
CFArrayRef chain_from_der(bool ecdsa, const unsigned char *pkey_der, size_t pkey_der_len, const unsigned char *cert_der, size_t cert_der_len)
{
    SecKeyRef pkey = NULL;
    SecCertificateRef cert = NULL;
    SecIdentityRef ident = NULL;
    CFArrayRef items = NULL;

    require(pkey = create_private_key_from_der(ecdsa, pkey_der, pkey_der_len), errOut);
    require(cert = SecCertificateCreateWithBytes(kCFAllocatorDefault, cert_der, cert_der_len), errOut);
    require(ident = SecIdentityCreate(kCFAllocatorDefault, cert, pkey), errOut);
    require(items = CFArrayCreate(kCFAllocatorDefault, (const void **)&ident, 1, &kCFTypeArrayCallBacks), errOut);

errOut:
    CFReleaseSafe(pkey);
    CFReleaseSafe(cert);
    CFReleaseSafe(ident);
    return items;
}

CFArrayRef server_ec_chain(void)
{
    return chain_from_der(true, eckey_der, eckey_der_len, eccert_der, eccert_der_len);
}

CFArrayRef trusted_roots(void)
{
    SecCertificateRef cert = NULL;
    CFArrayRef roots = NULL;

    require(cert = SecCertificateCreateWithBytes(kCFAllocatorDefault, CA_RSA_Cert_der, CA_RSA_Cert_der_len), errOut);
    require(roots = CFArrayCreate(kCFAllocatorDefault, (const void **)&cert, 1, &kCFTypeArrayCallBacks), errOut);

errOut:
    CFReleaseSafe(cert);
    return roots;
}

CFArrayRef server_chain(void)
{
    return chain_from_der(false, ServerRSA_Key_der, ServerRSA_Key_der_len,
                          ServerRSA_Cert_CA_RSA_der, ServerRSA_Cert_CA_RSA_der_len);
}

CFArrayRef trusted_client_chain(void)
{
    return chain_from_der(false, ClientRSA_Key_der, ClientRSA_Key_der_len,
                          ClientRSA_Cert_CA_RSA_der, ClientRSA_Cert_CA_RSA_der_len);
}

CFArrayRef trusted_ec_client_chain(void)
{
    return chain_from_der(true, ecclientkey_der, ecclientkey_der_len, ecclientcert_der, ecclientcert_der_len);
}

CFArrayRef untrusted_client_chain(void)
{
    return chain_from_der(false, UntrustedClientRSA_Key_der, UntrustedClientRSA_Key_der_len,
                          UntrustedClientRSA_Cert_Untrusted_CA_RSA_der, UntrustedClientRSA_Cert_Untrusted_CA_RSA_der_len);
}

const char *ciphersuite_name(SSLCipherSuite cs)
{

#define C(x) case x: return #x;
    switch (cs) {

            /* TLS 1.2 addenda, RFC 5246 */

            /* Initial state. */
            C(TLS_NULL_WITH_NULL_NULL)

            /* Server provided RSA certificate for key exchange. */
            C(TLS_RSA_WITH_NULL_MD5)
            C(TLS_RSA_WITH_NULL_SHA)
            C(TLS_RSA_WITH_RC4_128_MD5)
            C(TLS_RSA_WITH_RC4_128_SHA)
            C(TLS_RSA_WITH_3DES_EDE_CBC_SHA)
            C(TLS_RSA_WITH_AES_128_CBC_SHA)
            C(TLS_RSA_WITH_AES_256_CBC_SHA)
            C(TLS_RSA_WITH_NULL_SHA256)
            C(TLS_RSA_WITH_AES_128_CBC_SHA256)
            C(TLS_RSA_WITH_AES_256_CBC_SHA256)

            /* Server-authenticated (and optionally client-authenticated) Diffie-Hellman. */
            C(TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA)
            C(TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA)
            C(TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA)
            C(TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA)
            C(TLS_DH_DSS_WITH_AES_128_CBC_SHA)
            C(TLS_DH_RSA_WITH_AES_128_CBC_SHA)
            C(TLS_DHE_DSS_WITH_AES_128_CBC_SHA)
            C(TLS_DHE_RSA_WITH_AES_128_CBC_SHA)
            C(TLS_DH_DSS_WITH_AES_256_CBC_SHA)
            C(TLS_DH_RSA_WITH_AES_256_CBC_SHA)
            C(TLS_DHE_DSS_WITH_AES_256_CBC_SHA)
            C(TLS_DHE_RSA_WITH_AES_256_CBC_SHA)
            C(TLS_DH_DSS_WITH_AES_128_CBC_SHA256)
            C(TLS_DH_RSA_WITH_AES_128_CBC_SHA256)
            C(TLS_DHE_DSS_WITH_AES_128_CBC_SHA256)
            C(TLS_DHE_RSA_WITH_AES_128_CBC_SHA256)
            C(TLS_DH_DSS_WITH_AES_256_CBC_SHA256)
            C(TLS_DH_RSA_WITH_AES_256_CBC_SHA256)
            C(TLS_DHE_DSS_WITH_AES_256_CBC_SHA256)
            C(TLS_DHE_RSA_WITH_AES_256_CBC_SHA256)

            /* Completely anonymous Diffie-Hellman */
            C(TLS_DH_anon_WITH_RC4_128_MD5)
            C(TLS_DH_anon_WITH_3DES_EDE_CBC_SHA)
            C(TLS_DH_anon_WITH_AES_128_CBC_SHA)
            C(TLS_DH_anon_WITH_AES_256_CBC_SHA)
            C(TLS_DH_anon_WITH_AES_128_CBC_SHA256)
            C(TLS_DH_anon_WITH_AES_256_CBC_SHA256)

            /* Addenda from rfc 5288 AES Galois Counter Mode (GCM) Cipher Suites
             for TLS. */
            C(TLS_RSA_WITH_AES_128_GCM_SHA256)
            C(TLS_RSA_WITH_AES_256_GCM_SHA384)
            C(TLS_DHE_RSA_WITH_AES_128_GCM_SHA256)
            C(TLS_DHE_RSA_WITH_AES_256_GCM_SHA384)
            C(TLS_DH_RSA_WITH_AES_128_GCM_SHA256)
            C(TLS_DH_RSA_WITH_AES_256_GCM_SHA384)
            C(TLS_DHE_DSS_WITH_AES_128_GCM_SHA256)
            C(TLS_DHE_DSS_WITH_AES_256_GCM_SHA384)
            C(TLS_DH_DSS_WITH_AES_128_GCM_SHA256)
            C(TLS_DH_DSS_WITH_AES_256_GCM_SHA384)
            C(TLS_DH_anon_WITH_AES_128_GCM_SHA256)
            C(TLS_DH_anon_WITH_AES_256_GCM_SHA384)

            /* ECDSA addenda, RFC 4492 */
            C(TLS_ECDH_ECDSA_WITH_NULL_SHA)
            C(TLS_ECDH_ECDSA_WITH_RC4_128_SHA)
            C(TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA)
            C(TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA)
            C(TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA)
            C(TLS_ECDHE_ECDSA_WITH_NULL_SHA)
            C(TLS_ECDHE_ECDSA_WITH_RC4_128_SHA)
            C(TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA)
            C(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA)
            C(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA)
            C(TLS_ECDH_RSA_WITH_NULL_SHA)
            C(TLS_ECDH_RSA_WITH_RC4_128_SHA)
            C(TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA)
            C(TLS_ECDH_RSA_WITH_AES_128_CBC_SHA)
            C(TLS_ECDH_RSA_WITH_AES_256_CBC_SHA)
            C(TLS_ECDHE_RSA_WITH_NULL_SHA)
            C(TLS_ECDHE_RSA_WITH_RC4_128_SHA)
            C(TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA)
            C(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA)
            C(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA)
            C(TLS_ECDH_anon_WITH_NULL_SHA)
            C(TLS_ECDH_anon_WITH_RC4_128_SHA)
            C(TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA)
            C(TLS_ECDH_anon_WITH_AES_128_CBC_SHA)
            C(TLS_ECDH_anon_WITH_AES_256_CBC_SHA)

            /* Addenda from rfc 5289  Elliptic Curve Cipher Suites with
             HMAC SHA-256/384. */
            C(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256)
            C(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384)
            C(TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256)
            C(TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384)
            C(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256)
            C(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384)
            C(TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256)
            C(TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384)

            /* Addenda from rfc 5289  Elliptic Curve Cipher Suites with
             SHA-256/384 and AES Galois Counter Mode (GCM) */
            C(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256)
            C(TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384)
            C(TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256)
            C(TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384)
            C(TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256)
            C(TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384)
            C(TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256)
            C(TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384)

            /* RFC 5746 - Secure Renegotiation */
            C(TLS_EMPTY_RENEGOTIATION_INFO_SCSV)

            /*
             * Tags for SSL 2 cipher kinds which are not specified
             * for SSL 3.
             */
            C(SSL_RSA_WITH_RC2_CBC_MD5)
            C(SSL_RSA_WITH_IDEA_CBC_MD5)
            C(SSL_RSA_WITH_DES_CBC_MD5)
            C(SSL_RSA_WITH_3DES_EDE_CBC_MD5)
            C(SSL_NO_SUCH_CIPHERSUITE)

            C(SSL_RSA_EXPORT_WITH_RC4_40_MD5)
            C(SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5)
            C(SSL_RSA_WITH_IDEA_CBC_SHA)
            C(SSL_RSA_EXPORT_WITH_DES40_CBC_SHA)
            C(SSL_RSA_WITH_DES_CBC_SHA)
            C(SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA)
            C(SSL_DH_DSS_WITH_DES_CBC_SHA)
            C(SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA)
            C(SSL_DH_RSA_WITH_DES_CBC_SHA)
            C(SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA)
            C(SSL_DHE_DSS_WITH_DES_CBC_SHA)
            C(SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA)
            C(SSL_DHE_RSA_WITH_DES_CBC_SHA)
            C(SSL_DH_anon_EXPORT_WITH_RC4_40_MD5)
            C(SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA)
            C(SSL_DH_anon_WITH_DES_CBC_SHA)
            C(SSL_FORTEZZA_DMS_WITH_NULL_SHA)
            C(SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA)

            /* PSK */
            C(TLS_PSK_WITH_AES_256_CBC_SHA384)
            C(TLS_PSK_WITH_AES_128_CBC_SHA256)
            C(TLS_PSK_WITH_AES_256_CBC_SHA)
            C(TLS_PSK_WITH_AES_128_CBC_SHA)
            C(TLS_PSK_WITH_RC4_128_SHA)
            C(TLS_PSK_WITH_3DES_EDE_CBC_SHA)
            C(TLS_PSK_WITH_NULL_SHA384)
            C(TLS_PSK_WITH_NULL_SHA256)
            C(TLS_PSK_WITH_NULL_SHA)


        default:
            return "Unknown Ciphersuite";
    }

}
