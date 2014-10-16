/*
 * Copyright (c) 1999-2001,2005-2014 Apple Inc. All Rights Reserved.
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

#include "CipherSuite.h"
#include "cipherSpecs.h"
#include "sslTypes.h"

/*

cipher spec preferences from openssl.  first column includes the dh anon
cipher suites.  second column is more interesting: default.

seems to be:
Asymmetric: DHE-RSA > DHE-DSS > RSA
Symmetric : AES-256 > 3DES > AES-128 > RC4-128 > DES > DES40 > RC2-40 > RC4-40

DH_anon w/ AES are preferred over DHE_RSA when enabled, all others at the bottom.

    3a TLS_DH_anon_WITH_AES_256_CBC_SHA
    39 TLS_DHE_RSA_WITH_AES_256_CBC_SHA				1
    38 TLS_DHE_DSS_WITH_AES_256_CBC_SHA				2
    35 TLS_RSA_WITH_AES_256_CBC_SHA					3
    34 TLS_DH_anon_WITH_AES_128_CBC_SHA
    33 TLS_DHE_RSA_WITH_AES_128_CBC_SHA				7
    32 TLS_DHE_DSS_WITH_AES_128_CBC_SHA				8
    2f TLS_RSA_WITH_AES_128_CBC_SHA					9
    16 SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA			4
    15 SSL_DHE_RSA_WITH_DES_CBC_SHA					12
    14 SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA		15
    13 SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA			5
    12 SSL_DHE_DSS_WITH_DES_CBC_SHA					13
    11 SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA		16
    0a SSL_RSA_WITH_3DES_EDE_CBC_SHA				6
    09 SSL_RSA_WITH_DES_CBC_SHA						14
    08 SSL_RSA_EXPORT_WITH_DES40_CBC_SHA			17
    06 SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5			18
    05 SSL_RSA_WITH_RC4_128_SHA						10
    04 SSL_RSA_WITH_RC4_128_MD5						11
    03 SSL_RSA_EXPORT_WITH_RC4_40_MD5				19
    1b SSL_DH_anon_WITH_3DES_EDE_CBC_SHA
    1a SSL_DH_anon_WITH_DES_CBC_SHA
    19 SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA
    18 SSL_DH_anon_WITH_RC4_128_MD5
    17 SSL_DH_anon_EXPORT_WITH_RC4_40_MD5
 
 */

KeyExchangeMethod sslCipherSuiteGetKeyExchangeMethod(SSLCipherSuite cipherSuite)
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

#if 0
static SSL_SignatureAlgorithm sslCipherSuiteGetSignatureAlgorithm(SSLCipherSuite cipherSuite) {
    switch (sslCipherSuiteGetKeyExchangeMethod(cipherSuite)) {
        case SSL_NULL_auth:
            return SSL_SignatureAlgorithmAnonymous;
        case SSL_RSA:
        case SSL_RSA_EXPORT:
        case SSL_DH_RSA:
        case SSL_DH_RSA_EXPORT:
        case SSL_DHE_RSA:
        case SSL_DHE_RSA_EXPORT:
        case SSL_ECDHE_RSA:
        case SSL_ECDH_RSA:
            return SSL_SignatureAlgorithmRSA;
        case SSL_DH_DSS:
        case SSL_DH_DSS_EXPORT:
        case SSL_DHE_DSS:
        case SSL_DHE_DSS_EXPORT:
            return SSL_SignatureAlgorithmDSA;
        case SSL_DH_anon:
        case SSL_DH_anon_EXPORT:
            return SSL_SignatureAlgorithmAnonymous;
        case SSL_ECDHE_ECDSA:
        case SSL_ECDH_ECDSA:
            return SSL_SignatureAlgorithmECDSA;
        default:
            return SSL_SignatureAlgorithmAnonymous;
    }
}
#endif

#if 0
static SSLProtocolVersion sslCipherSuiteGetMinSupportedTLSVersion(SSLCipherSuite cipherSuite) {
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
            return SSL_Version_3_0;

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
            return TLS_Version_1_0;

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
            return TLS_Version_1_2;
        default:
            return TLS_Version_1_2;
    }
}
#endif

HMAC_Algs sslCipherSuiteGetMacAlgorithm(SSLCipherSuite cipherSuite) {
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

uint8_t sslCipherSuiteGetMacSize(SSLCipherSuite cipherSuite) {
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

SSL_CipherAlgorithm sslCipherSuiteGetSymmetricCipherAlgorithm(SSLCipherSuite cipherSuite) {
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
        default:
            return SSL_CipherAlgorithmNull;
    }
}

uint8_t sslCipherSuiteGetSymmetricCipherKeySize(SSLCipherSuite cipherSuite) {
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
            return 32;
        default:
            return 0;
    }
}


/* Same function for block and iv size */
uint8_t sslCipherSuiteGetSymmetricCipherBlockIvSize(SSLCipherSuite cipherSuite) {
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
        case SSL_CipherAlgorithmAES_128_GCM:
        case SSL_CipherAlgorithmAES_256_CBC:
        case SSL_CipherAlgorithmAES_256_GCM:
            return 16;
        default:
            return 0;
    }
}

