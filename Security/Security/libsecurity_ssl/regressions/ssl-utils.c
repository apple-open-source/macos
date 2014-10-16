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

#if TARGET_OS_IPHONE

#include <Security/SecRSAKey.h>
#include <Security/SecECKey.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentityPriv.h>

#include "privkey-1.h"
#include "cert-1.h"

static
CFArrayRef chain_from_der(const unsigned char *pkey_der, size_t pkey_der_len, const unsigned char *cert_der, size_t cert_der_len)
{
    SecKeyRef pkey = NULL;
    SecCertificateRef cert = NULL;
    SecIdentityRef ident = NULL;
    CFArrayRef items = NULL;

    require(pkey = SecKeyCreateRSAPrivateKey(kCFAllocatorDefault, pkey_der, pkey_der_len, kSecKeyEncodingPkcs1), errOut);
    require(cert = SecCertificateCreateWithBytes(kCFAllocatorDefault, cert_der, cert_der_len), errOut);
    require(ident = SecIdentityCreate(kCFAllocatorDefault, cert, pkey), errOut);
    require(items = CFArrayCreate(kCFAllocatorDefault, (const void **)&ident, 1, &kCFTypeArrayCallBacks), errOut);

errOut:
    CFReleaseSafe(pkey);
    CFReleaseSafe(cert);
    CFReleaseSafe(ident);
    return items;
}

#else

#include "identity-1.h"
#define P12_PASSWORD "password"

static
CFArrayRef chain_from_p12(const unsigned char *p12_data, size_t p12_len)
{
    char keychain_path[] = "/tmp/keychain.XXXXXX";

    SecKeychainRef keychain = NULL;
    CFArrayRef list = NULL;
    CFDataRef data = NULL;

    SecExternalFormat format=kSecFormatPKCS12;
    SecExternalItemType type=kSecItemTypeAggregate;
    SecItemImportExportFlags flags=0;
    SecKeyImportExportParameters params = {0,};
    CFArrayRef out = NULL;

    require_noerr(SecKeychainCopyDomainSearchList(kSecPreferencesDomainUser, &list), errOut);
    require(mktemp(keychain_path), errOut);
    require_noerr(SecKeychainCreate (keychain_path, strlen(P12_PASSWORD), P12_PASSWORD,
                                     FALSE, NULL, &keychain), errOut);
    require_noerr(SecKeychainSetDomainSearchList(kSecPreferencesDomainUser, list), errOut);	// restores the previous search list
    require(data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, p12_data, p12_len, kCFAllocatorNull), errOut);


    params.passphrase=CFSTR("password");
    params.keyAttributes = CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_SENSITIVE;

    require_noerr(SecKeychainItemImport(data, CFSTR(".p12"), &format, &type, flags,
                                        &params, keychain, &out), errOut);

errOut:
    CFReleaseSafe(data);
    CFReleaseSafe(keychain);
    CFReleaseSafe(list);

    return out;
}

#endif

CFArrayRef server_chain(void)
{
#if TARGET_OS_IPHONE
    return chain_from_der(privkey_1_der, privkey_1_der_len, cert_1_der, cert_1_der_len);
#else
    return chain_from_p12(identity_1_p12, identity_1_p12_len);
#endif
}

CFArrayRef client_chain(void)
{
#if TARGET_OS_IPHONE
    return chain_from_der(privkey_1_der, privkey_1_der_len, cert_1_der, cert_1_der_len);
#else
    return chain_from_p12(identity_1_p12, identity_1_p12_len);
#endif
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


        default:
            return "Unknown Ciphersuite";
    }

}
