/*
 * Copyright (c) 1999-2001,2005-2013 Apple Inc. All Rights Reserved.
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
 * cipherSpecs.c - SSLCipherSpec declarations
 */

/* THIS FILE CONTAINS KERNEL CODE */

#include <tls_ciphersuites.h>
#include "CipherSuite.h"
#include "tls_types.h"

KeyExchangeMethod sslCipherSuiteGetKeyExchangeMethod(uint16_t cipherSuite)
{
    switch (cipherSuite) {
        case TLS_NULL_WITH_NULL_NULL:
            return SSL_NULL_auth;

        case SSL_RSA_WITH_RC2_CBC_MD5:
        case SSL_RSA_WITH_DES_CBC_MD5:
        case SSL_RSA_WITH_3DES_EDE_CBC_MD5:
        case TLS_RSA_WITH_NULL_MD5:
        case TLS_RSA_WITH_NULL_SHA:
        case TLS_RSA_WITH_RC4_128_MD5:
        case TLS_RSA_WITH_RC4_128_SHA:
        case SSL_RSA_WITH_IDEA_CBC_SHA:
        case SSL_RSA_WITH_DES_CBC_SHA:
        case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_RSA_WITH_AES_128_CBC_SHA:
        case TLS_RSA_WITH_AES_256_CBC_SHA:
        case TLS_RSA_WITH_NULL_SHA256:
        case TLS_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_RSA_WITH_AES_256_GCM_SHA384:
            return SSL_RSA;

        case SSL_RSA_EXPORT_WITH_RC4_40_MD5:
        case SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5:
        case SSL_RSA_EXPORT_WITH_DES40_CBC_SHA:
            return SSL_RSA_EXPORT;

        case SSL_DH_DSS_WITH_DES_CBC_SHA:
        case TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_DSS_WITH_AES_128_CBC_SHA:
        case TLS_DH_DSS_WITH_AES_256_CBC_SHA:
        case TLS_DH_DSS_WITH_AES_128_CBC_SHA256:
        case TLS_DH_DSS_WITH_AES_256_CBC_SHA256:
        case TLS_DH_DSS_WITH_AES_128_GCM_SHA256:
        case TLS_DH_DSS_WITH_AES_256_GCM_SHA384:
            return SSL_DH_DSS;

        case SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA:
            return SSL_DH_DSS_EXPORT;

        case SSL_DH_RSA_WITH_DES_CBC_SHA:
        case TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DH_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DH_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DH_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DH_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DH_RSA_WITH_AES_256_GCM_SHA384:
            return SSL_DH_RSA;

        case SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA:
            return SSL_DH_RSA_EXPORT;

        case SSL_DHE_DSS_WITH_DES_CBC_SHA:
        case TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_DSS_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_DSS_WITH_AES_256_GCM_SHA384:
            return SSL_DHE_DSS;

        case SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA:
            return SSL_DHE_DSS_EXPORT;

        case SSL_DHE_RSA_WITH_DES_CBC_SHA:
        case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
            return SSL_DHE_RSA;

        case SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA:
            return SSL_DHE_RSA_EXPORT;

        case SSL_DH_anon_WITH_DES_CBC_SHA:
        case TLS_DH_anon_WITH_RC4_128_MD5:
        case TLS_DH_anon_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_anon_WITH_AES_128_CBC_SHA:
        case TLS_DH_anon_WITH_AES_256_CBC_SHA:
        case TLS_DH_anon_WITH_AES_128_CBC_SHA256:
        case TLS_DH_anon_WITH_AES_256_CBC_SHA256:
        case TLS_DH_anon_WITH_AES_128_GCM_SHA256:
        case TLS_DH_anon_WITH_AES_256_GCM_SHA384:
            return SSL_DH_anon;

        case SSL_DH_anon_EXPORT_WITH_RC4_40_MD5:
        case SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA:
            return SSL_DH_anon_EXPORT;

        case SSL_FORTEZZA_DMS_WITH_NULL_SHA:
        case SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA:
            return SSL_Fortezza;

        case TLS_ECDHE_ECDSA_WITH_NULL_SHA:
        case TLS_ECDHE_ECDSA_WITH_RC4_128_SHA:
        case TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
            return SSL_ECDHE_ECDSA;

        case TLS_ECDH_ECDSA_WITH_NULL_SHA:
        case TLS_ECDH_ECDSA_WITH_RC4_128_SHA:
        case TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384:
            return SSL_ECDH_ECDSA;

        case TLS_ECDHE_RSA_WITH_NULL_SHA:
        case TLS_ECDHE_RSA_WITH_RC4_128_SHA:
        case TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
            return SSL_ECDHE_RSA;

        case TLS_ECDH_RSA_WITH_NULL_SHA:
        case TLS_ECDH_RSA_WITH_RC4_128_SHA:
        case TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384:
            return SSL_ECDH_RSA;

        case TLS_ECDH_anon_WITH_NULL_SHA:
        case TLS_ECDH_anon_WITH_RC4_128_SHA:
        case TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_anon_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_anon_WITH_AES_256_CBC_SHA:
            return SSL_ECDH_anon;

        case TLS_PSK_WITH_NULL_SHA:
        case TLS_PSK_WITH_RC4_128_SHA:
        case TLS_PSK_WITH_3DES_EDE_CBC_SHA:
        case TLS_PSK_WITH_AES_128_CBC_SHA:
        case TLS_PSK_WITH_AES_256_CBC_SHA:
        case TLS_PSK_WITH_AES_128_GCM_SHA256:
        case TLS_PSK_WITH_AES_256_GCM_SHA384:
        case TLS_PSK_WITH_AES_128_CBC_SHA256:
        case TLS_PSK_WITH_AES_256_CBC_SHA384:
        case TLS_PSK_WITH_NULL_SHA256:
        case TLS_PSK_WITH_NULL_SHA384:
            return TLS_PSK;

        case TLS_DHE_PSK_WITH_NULL_SHA:
        case TLS_DHE_PSK_WITH_RC4_128_SHA:
        case TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_PSK_WITH_AES_128_CBC_SHA:
        case TLS_DHE_PSK_WITH_AES_256_CBC_SHA:
        case TLS_DHE_PSK_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_PSK_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_PSK_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_PSK_WITH_AES_256_CBC_SHA384:
        case TLS_DHE_PSK_WITH_NULL_SHA256:
        case TLS_DHE_PSK_WITH_NULL_SHA384:
            return TLS_DHE_PSK;

        case TLS_RSA_PSK_WITH_NULL_SHA:
        case TLS_RSA_PSK_WITH_RC4_128_SHA:
        case TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA:
        case TLS_RSA_PSK_WITH_AES_128_CBC_SHA:
        case TLS_RSA_PSK_WITH_AES_256_CBC_SHA:
        case TLS_RSA_PSK_WITH_AES_128_GCM_SHA256:
        case TLS_RSA_PSK_WITH_AES_256_GCM_SHA384:
        case TLS_RSA_PSK_WITH_AES_128_CBC_SHA256:
        case TLS_RSA_PSK_WITH_AES_256_CBC_SHA384:
        case TLS_RSA_PSK_WITH_NULL_SHA256:
        case TLS_RSA_PSK_WITH_NULL_SHA384:
            return TLS_RSA_PSK;

        default:
            return SSL_NULL_auth;
    }
}

tls_protocol_version sslCipherSuiteGetMinSupportedTLSVersion(uint16_t cipherSuite) {
    switch (cipherSuite) {
        case SSL_RSA_EXPORT_WITH_RC4_40_MD5:
        case SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5:
        case SSL_RSA_WITH_IDEA_CBC_SHA:
        case SSL_RSA_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_RSA_WITH_DES_CBC_SHA:
        case SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DH_DSS_WITH_DES_CBC_SHA:
        case SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DH_RSA_WITH_DES_CBC_SHA:
        case SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DHE_DSS_WITH_DES_CBC_SHA:
        case SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DHE_RSA_WITH_DES_CBC_SHA:
        case SSL_DH_anon_EXPORT_WITH_RC4_40_MD5:
        case SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DH_anon_WITH_DES_CBC_SHA:
        case SSL_FORTEZZA_DMS_WITH_NULL_SHA:
        case SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA:
        case TLS_NULL_WITH_NULL_NULL:
        case TLS_RSA_WITH_NULL_MD5:
        case TLS_RSA_WITH_NULL_SHA:
        case TLS_RSA_WITH_RC4_128_MD5:
        case TLS_RSA_WITH_RC4_128_SHA:
        case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_RSA_WITH_AES_128_CBC_SHA:
        case TLS_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_DSS_WITH_AES_128_CBC_SHA:
        case TLS_DH_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DH_DSS_WITH_AES_256_CBC_SHA:
        case TLS_DH_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DH_anon_WITH_RC4_128_MD5:
        case TLS_DH_anon_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_anon_WITH_AES_128_CBC_SHA:
        case TLS_DH_anon_WITH_AES_256_CBC_SHA:
            return tls_protocol_version_SSL_3;

        case TLS_ECDH_ECDSA_WITH_NULL_SHA:
        case TLS_ECDH_ECDSA_WITH_RC4_128_SHA:
        case TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_NULL_SHA:
        case TLS_ECDHE_ECDSA_WITH_RC4_128_SHA:
        case TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDH_RSA_WITH_NULL_SHA:
        case TLS_ECDH_RSA_WITH_RC4_128_SHA:
        case TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_NULL_SHA:
        case TLS_ECDHE_RSA_WITH_RC4_128_SHA:
        case TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDH_anon_WITH_NULL_SHA:
        case TLS_ECDH_anon_WITH_RC4_128_SHA:
        case TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_anon_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_anon_WITH_AES_256_CBC_SHA:
            return tls_protocol_version_TLS_1_0;

        case TLS_RSA_WITH_NULL_SHA256:
        case TLS_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DH_DSS_WITH_AES_128_CBC_SHA256:
        case TLS_DH_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DH_DSS_WITH_AES_256_CBC_SHA256:
        case TLS_DH_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DH_anon_WITH_AES_128_CBC_SHA256:
        case TLS_DH_anon_WITH_AES_256_CBC_SHA256:
        case TLS_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DH_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DH_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_DSS_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_DSS_WITH_AES_256_GCM_SHA384:
        case TLS_DH_DSS_WITH_AES_128_GCM_SHA256:
        case TLS_DH_DSS_WITH_AES_256_GCM_SHA384:
        case TLS_DH_anon_WITH_AES_128_GCM_SHA256:
        case TLS_DH_anon_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384:
            return tls_protocol_version_TLS_1_2;
        default:
            return tls_protocol_version_TLS_1_2;
    }
}

HMAC_Algs sslCipherSuiteGetMacAlgorithm(uint16_t cipherSuite) {
    switch (cipherSuite) {
        case TLS_NULL_WITH_NULL_NULL:
            return HA_Null;
        case SSL_RSA_WITH_RC2_CBC_MD5:
        case SSL_RSA_WITH_DES_CBC_MD5:
        case SSL_RSA_WITH_3DES_EDE_CBC_MD5:
        case TLS_RSA_WITH_NULL_MD5:
        case SSL_RSA_EXPORT_WITH_RC4_40_MD5:
        case SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5:
        case TLS_RSA_WITH_RC4_128_MD5:
        case SSL_DH_anon_EXPORT_WITH_RC4_40_MD5:
        case TLS_DH_anon_WITH_RC4_128_MD5:
            return HA_MD5;
        case TLS_RSA_WITH_NULL_SHA:
        case SSL_RSA_WITH_IDEA_CBC_SHA:
        case SSL_RSA_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_RSA_WITH_DES_CBC_SHA:
        case SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DH_DSS_WITH_DES_CBC_SHA:
        case SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DH_RSA_WITH_DES_CBC_SHA:
        case SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DHE_DSS_WITH_DES_CBC_SHA:
        case SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DHE_RSA_WITH_DES_CBC_SHA:
        case SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DH_anon_WITH_DES_CBC_SHA:
        case SSL_FORTEZZA_DMS_WITH_NULL_SHA:
        case SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA:
        case TLS_RSA_WITH_RC4_128_SHA:
        case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_RSA_WITH_AES_128_CBC_SHA:
        case TLS_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_DSS_WITH_AES_128_CBC_SHA:
        case TLS_DH_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DH_DSS_WITH_AES_256_CBC_SHA:
        case TLS_DH_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DH_anon_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_anon_WITH_AES_128_CBC_SHA:
        case TLS_DH_anon_WITH_AES_256_CBC_SHA:
        case TLS_ECDH_ECDSA_WITH_NULL_SHA:
        case TLS_ECDH_ECDSA_WITH_RC4_128_SHA:
        case TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_NULL_SHA:
        case TLS_ECDHE_ECDSA_WITH_RC4_128_SHA:
        case TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDH_RSA_WITH_NULL_SHA:
        case TLS_ECDH_RSA_WITH_RC4_128_SHA:
        case TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_NULL_SHA:
        case TLS_ECDHE_RSA_WITH_RC4_128_SHA:
        case TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDH_anon_WITH_NULL_SHA:
        case TLS_ECDH_anon_WITH_RC4_128_SHA:
        case TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_anon_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_anon_WITH_AES_256_CBC_SHA:
        case TLS_PSK_WITH_NULL_SHA:
        case TLS_PSK_WITH_RC4_128_SHA:
        case TLS_PSK_WITH_3DES_EDE_CBC_SHA:
        case TLS_PSK_WITH_AES_128_CBC_SHA:
        case TLS_PSK_WITH_AES_256_CBC_SHA:
        case TLS_DHE_PSK_WITH_NULL_SHA:
        case TLS_DHE_PSK_WITH_RC4_128_SHA:
        case TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_PSK_WITH_AES_128_CBC_SHA:
        case TLS_DHE_PSK_WITH_AES_256_CBC_SHA:
        case TLS_RSA_PSK_WITH_NULL_SHA:
        case TLS_RSA_PSK_WITH_RC4_128_SHA:
        case TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA:
        case TLS_RSA_PSK_WITH_AES_128_CBC_SHA:
        case TLS_RSA_PSK_WITH_AES_256_CBC_SHA:
            return HA_SHA1;
        case TLS_RSA_WITH_NULL_SHA256:
        case TLS_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DH_DSS_WITH_AES_128_CBC_SHA256:
        case TLS_DH_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DH_DSS_WITH_AES_256_CBC_SHA256:
        case TLS_DH_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DH_anon_WITH_AES_128_CBC_SHA256:
        case TLS_DH_anon_WITH_AES_256_CBC_SHA256:
        case TLS_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DH_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_DSS_WITH_AES_128_GCM_SHA256:
        case TLS_DH_DSS_WITH_AES_128_GCM_SHA256:
        case TLS_DH_anon_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_PSK_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_PSK_WITH_AES_128_GCM_SHA256:
        case TLS_RSA_PSK_WITH_AES_128_GCM_SHA256:
        case TLS_PSK_WITH_AES_128_CBC_SHA256:
        case TLS_PSK_WITH_NULL_SHA256:
        case TLS_DHE_PSK_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_PSK_WITH_NULL_SHA256:
        case TLS_RSA_PSK_WITH_AES_128_CBC_SHA256:
        case TLS_RSA_PSK_WITH_NULL_SHA256:
            return HA_SHA256;
        case TLS_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DH_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_DSS_WITH_AES_256_GCM_SHA384:
        case TLS_DH_DSS_WITH_AES_256_GCM_SHA384:
        case TLS_DH_anon_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_PSK_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_PSK_WITH_AES_256_GCM_SHA384:
        case TLS_RSA_PSK_WITH_AES_256_GCM_SHA384:
        case TLS_PSK_WITH_AES_256_CBC_SHA384:
        case TLS_PSK_WITH_NULL_SHA384:
        case TLS_DHE_PSK_WITH_AES_256_CBC_SHA384:
        case TLS_DHE_PSK_WITH_NULL_SHA384:
        case TLS_RSA_PSK_WITH_AES_256_CBC_SHA384:
        case TLS_RSA_PSK_WITH_NULL_SHA384:
            return HA_SHA384;
        default:
            return HA_Null;
    }
}

uint8_t sslCipherSuiteGetMacSize(uint16_t cipherSuite) {
    if ((sslCipherSuiteGetSymmetricCipherAlgorithm(cipherSuite) == SSL_CipherAlgorithmAES_128_GCM) ||
        (sslCipherSuiteGetSymmetricCipherAlgorithm(cipherSuite) == SSL_CipherAlgorithmAES_256_GCM)) {
        return 0;
    }
    switch (sslCipherSuiteGetMacAlgorithm(cipherSuite)) {
        case HA_Null:
            return 0;
        case HA_MD5:
            return 16;
        case HA_SHA1:
            return 20;
        case HA_SHA256:
            return 32;
        case HA_SHA384:
            return 48;
        default:
            return 0;
    }
}

SSL_CipherAlgorithm sslCipherSuiteGetSymmetricCipherAlgorithm(uint16_t cipherSuite) {
    switch (cipherSuite) {
        case TLS_NULL_WITH_NULL_NULL:
        case TLS_RSA_WITH_NULL_MD5:
        case TLS_RSA_WITH_NULL_SHA:
        case TLS_RSA_WITH_NULL_SHA256:
        case SSL_FORTEZZA_DMS_WITH_NULL_SHA:
        case TLS_ECDH_ECDSA_WITH_NULL_SHA:
        case TLS_ECDHE_ECDSA_WITH_NULL_SHA:
        case TLS_ECDH_RSA_WITH_NULL_SHA:
        case TLS_ECDHE_RSA_WITH_NULL_SHA:
        case TLS_ECDH_anon_WITH_NULL_SHA:
        case TLS_PSK_WITH_NULL_SHA:
        case TLS_DHE_PSK_WITH_NULL_SHA:
        case TLS_RSA_PSK_WITH_NULL_SHA:
        case TLS_PSK_WITH_NULL_SHA256:
        case TLS_PSK_WITH_NULL_SHA384:
        case TLS_DHE_PSK_WITH_NULL_SHA256:
        case TLS_DHE_PSK_WITH_NULL_SHA384:
        case TLS_RSA_PSK_WITH_NULL_SHA256:
        case TLS_RSA_PSK_WITH_NULL_SHA384:
            return SSL_CipherAlgorithmNull;
        case SSL_RSA_WITH_RC2_CBC_MD5:
            return SSL_CipherAlgorithmRC2_128;
        case SSL_RSA_WITH_DES_CBC_MD5:
        case SSL_RSA_WITH_DES_CBC_SHA:
        case SSL_DH_DSS_WITH_DES_CBC_SHA:
        case SSL_DH_RSA_WITH_DES_CBC_SHA:
        case SSL_DHE_DSS_WITH_DES_CBC_SHA:
        case SSL_DHE_RSA_WITH_DES_CBC_SHA:
        case SSL_DH_anon_WITH_DES_CBC_SHA:
            return SSL_CipherAlgorithmDES_CBC;
        case TLS_RSA_WITH_RC4_128_MD5:
        case TLS_RSA_WITH_RC4_128_SHA:
        case TLS_DH_anon_WITH_RC4_128_MD5:
        case TLS_ECDH_ECDSA_WITH_RC4_128_SHA:
        case TLS_ECDHE_ECDSA_WITH_RC4_128_SHA:
        case TLS_ECDH_RSA_WITH_RC4_128_SHA:
        case TLS_ECDHE_RSA_WITH_RC4_128_SHA:
        case TLS_ECDH_anon_WITH_RC4_128_SHA:
        case TLS_PSK_WITH_RC4_128_SHA:
        case TLS_DHE_PSK_WITH_RC4_128_SHA:
        case TLS_RSA_PSK_WITH_RC4_128_SHA:
            return SSL_CipherAlgorithmRC4_128;
        case SSL_RSA_WITH_3DES_EDE_CBC_MD5:
        case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_DH_anon_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA:
        case TLS_PSK_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA:
        case TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA:
            return SSL_CipherAlgorithm3DES_CBC;
        case TLS_RSA_WITH_AES_128_CBC_SHA:
        case TLS_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DH_DSS_WITH_AES_128_CBC_SHA:
        case TLS_DH_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DH_DSS_WITH_AES_128_CBC_SHA256:
        case TLS_DH_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DH_anon_WITH_AES_128_CBC_SHA:
        case TLS_DH_anon_WITH_AES_128_CBC_SHA256:
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_anon_WITH_AES_128_CBC_SHA:
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_PSK_WITH_AES_128_CBC_SHA:
        case TLS_DHE_PSK_WITH_AES_128_CBC_SHA:
        case TLS_RSA_PSK_WITH_AES_128_CBC_SHA:
        case TLS_PSK_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_PSK_WITH_AES_128_CBC_SHA256:
        case TLS_RSA_PSK_WITH_AES_128_CBC_SHA256:
            return SSL_CipherAlgorithmAES_128_CBC;
        case TLS_RSA_WITH_AES_256_CBC_SHA:
        case TLS_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DH_DSS_WITH_AES_256_CBC_SHA:
        case TLS_DH_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DH_DSS_WITH_AES_256_CBC_SHA256:
        case TLS_DH_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DH_anon_WITH_AES_256_CBC_SHA:
        case TLS_DH_anon_WITH_AES_256_CBC_SHA256:
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDH_anon_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384:
        case TLS_PSK_WITH_AES_256_CBC_SHA:
        case TLS_DHE_PSK_WITH_AES_256_CBC_SHA:
        case TLS_RSA_PSK_WITH_AES_256_CBC_SHA:
        case TLS_PSK_WITH_AES_256_CBC_SHA384:
        case TLS_DHE_PSK_WITH_AES_256_CBC_SHA384:
        case TLS_RSA_PSK_WITH_AES_256_CBC_SHA384:
            return SSL_CipherAlgorithmAES_256_CBC;
        case TLS_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DH_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_DSS_WITH_AES_128_GCM_SHA256:
        case TLS_DH_DSS_WITH_AES_128_GCM_SHA256:
        case TLS_DH_anon_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_PSK_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_PSK_WITH_AES_128_GCM_SHA256:
        case TLS_RSA_PSK_WITH_AES_128_GCM_SHA256:
            return SSL_CipherAlgorithmAES_128_GCM;
        case TLS_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DH_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_DSS_WITH_AES_256_GCM_SHA384:
        case TLS_DH_DSS_WITH_AES_256_GCM_SHA384:
        case TLS_DH_anon_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_PSK_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_PSK_WITH_AES_256_GCM_SHA384:
        case TLS_RSA_PSK_WITH_AES_256_GCM_SHA384:
            return SSL_CipherAlgorithmAES_256_GCM;
        case TLS_AES_128_GCM_SHA256:
            return SSL_CipherAlgorithmAES_128_GCM;
        case TLS_AES_256_GCM_SHA384:
            return SSL_CipherAlgorithmAES_256_GCM;
        case TLS_CHACHA20_POLY1305_SHA256:
            return SSL_CipherAlgorithmChaCha20_Poly1305;
        case TLS_AES_128_CCM_SHA256:
        case TLS_AES_128_CCM_8_SHA256:
        default:
            return SSL_CipherAlgorithmNull;
    }
}

uint8_t sslCipherSuiteGetSymmetricCipherKeySize(uint16_t cipherSuite) {
    SSL_CipherAlgorithm alg = sslCipherSuiteGetSymmetricCipherAlgorithm(cipherSuite);

    switch (alg) {
        case SSL_CipherAlgorithmNull:
            return 0;
        case SSL_CipherAlgorithmDES_CBC:
            return 8;
        case SSL_CipherAlgorithmRC2_128:
        case SSL_CipherAlgorithmRC4_128:
        case SSL_CipherAlgorithmAES_128_CBC:
        case SSL_CipherAlgorithmAES_128_GCM:
            return 16;
        case SSL_CipherAlgorithm3DES_CBC:
            return 24;
        case SSL_CipherAlgorithmAES_256_CBC:
        case SSL_CipherAlgorithmAES_256_GCM:
        case SSL_CipherAlgorithmChaCha20_Poly1305:
            return 32;
        default:
            return 0;
    }
}


/* Same function for block and iv size */
uint8_t sslCipherSuiteGetSymmetricCipherBlockIvSize(uint16_t cipherSuite) {
    SSL_CipherAlgorithm alg = sslCipherSuiteGetSymmetricCipherAlgorithm(cipherSuite);

    switch (alg) {
        case SSL_CipherAlgorithmNull:
        case SSL_CipherAlgorithmRC4_128:
            return 0;
        case SSL_CipherAlgorithmDES_CBC:
        case SSL_CipherAlgorithm3DES_CBC:
        case SSL_CipherAlgorithmRC2_128:
            return 8;
        case SSL_CipherAlgorithmAES_128_CBC:
        case SSL_CipherAlgorithmAES_256_CBC:
            return 16;
        case SSL_CipherAlgorithmAES_128_GCM:
        case SSL_CipherAlgorithmAES_256_GCM:
        case SSL_CipherAlgorithmChaCha20_Poly1305:
            return 12;
        default:
            return 0;
    }
}

size_t
sslCipherSuiteGetKeydataSize(uint16_t cipherSuite)
{
    uint8_t macSize = sslCipherSuiteGetMacSize(cipherSuite);
    uint8_t keySize = sslCipherSuiteGetSymmetricCipherKeySize(cipherSuite);
    uint8_t ivSize = sslCipherSuiteGetSymmetricCipherBlockIvSize(cipherSuite);

    return macSize*2 + keySize*2 + ivSize*2;
}


