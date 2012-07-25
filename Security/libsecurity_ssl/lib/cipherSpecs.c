/*
 * Copyright (c) 1999-2001,2005-2012 Apple Inc. All Rights Reserved.
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

#include "ssl.h"
#include "CipherSuite.h"
#include "sslContext.h"
#include "cryptType.h"
#include "symCipher.h"
#include "cipherSpecs.h"
#include "sslDebug.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include "sslUtils.h"
#include "sslPriv.h"
#include "sslCrypto.h"

#include <string.h>
#include <TargetConditionals.h>

#define ENABLE_RSA_DES_SHA_NONEXPORT		ENABLE_DES
#define ENABLE_RSA_DES_MD5_NONEXPORT		ENABLE_DES
#define ENABLE_RSA_DES_SHA_EXPORT			ENABLE_DES
#define ENABLE_RSA_RC4_MD5_EXPORT			ENABLE_RC4	/* the most common one */
#define ENABLE_RSA_RC4_MD5_NONEXPORT		ENABLE_RC4
#define ENABLE_RSA_RC4_SHA_NONEXPORT		ENABLE_RC4
#define ENABLE_RSA_RC2_MD5_EXPORT			ENABLE_RC2
#define ENABLE_RSA_RC2_MD5_NONEXPORT		ENABLE_RC2
#define ENABLE_RSA_3DES_SHA					ENABLE_3DES
#define ENABLE_RSA_3DES_MD5					ENABLE_3DES

#define ENABLE_ECDH       					1

#define ENABLE_AES_GCM          0

#if 	APPLE_DH
#define ENABLE_DH_ANON			1
#define ENABLE_DH_EPHEM_RSA		1
#if USE_CDSA_CRYPTO
#define ENABLE_DH_EPHEM_DSA		1
#else
#define ENABLE_DH_EPHEM_DSA		0
#endif
#else
#define ENABLE_DH_ANON			0
#define ENABLE_DH_EPHEM_RSA		0
#define ENABLE_DH_EPHEM_DSA		0
#endif	/* APPLE_DH */

extern const SSLSymmetricCipher SSLCipherNull;		/* in sslNullCipher.c */

/*
 * The symmetric ciphers currently supported (in addition to the
 * NULL cipher in nullciph.c).
 */
#if	ENABLE_DES
static const SSLSymmetricCipher SSLCipherDES_CBC = {
    kCCKeySizeDES,      /* Key size in bytes */
    kCCKeySizeDES,      /* Secret key size = 64 bits */
    kCCBlockSizeDES,      /* IV size */
    kCCBlockSizeDES,      /* Block size */
    kCCAlgorithmDES,
    CCSymmInit,
    CCSymmEncryptDecrypt,
    CCSymmEncryptDecrypt,
    CCSymmFinish
};

static const SSLSymmetricCipher SSLCipherDES40_CBC = {
    kCCKeySizeDES,      /* Key size in bytes */
    5,                  /* Secret key size = 40 bits */
    kCCBlockSizeDES,      /* IV size */
    kCCBlockSizeDES,      /* Block size */
    kCCAlgorithmDES,
    CCSymmInit,
    CCSymmEncryptDecrypt,
    CCSymmEncryptDecrypt,
    CCSymmFinish
};
#endif	/* ENABLE_DES */

#if	ENABLE_3DES
static const SSLSymmetricCipher SSLCipher3DES_CBC = {
    kCCKeySize3DES,     /* Key size in bytes */
    kCCKeySize3DES,     /* Secret key size = 192 bits */
    kCCBlockSize3DES,      /* IV size */
    kCCBlockSize3DES,      /* Block size */
    kCCAlgorithm3DES,
    CCSymmInit,
    CCSymmEncryptDecrypt,
    CCSymmEncryptDecrypt,
    CCSymmFinish
};
#endif	/* ENABLE_3DES */

#if		ENABLE_RC4
static const SSLSymmetricCipher SSLCipherRC4_40 = {
    16,         /* Key size in bytes */
    5,          /* Secret key size = 40 bits */
    0,          /* IV size */
    0,          /* Block size */
    kCCAlgorithmRC4,
    CCSymmInit,
    CCSymmEncryptDecrypt,
    CCSymmEncryptDecrypt,
    CCSymmFinish
};

static const SSLSymmetricCipher SSLCipherRC4_128 = {
    16,         /* Key size in bytes */
    16,         /* Secret key size = 128 bits */
    0,          /* IV size */
    0,          /* Block size */
    kCCAlgorithmRC4,
    CCSymmInit,
    CCSymmEncryptDecrypt,
    CCSymmEncryptDecrypt,
    CCSymmFinish
};
#endif	/* ENABLE_RC4 */

#if		ENABLE_RC2
static const SSLSymmetricCipher SSLCipherRC2_40 = {
    kCCKeySizeMaxRC2,         /* Key size in bytes */
    5,                        /* Secret key size = 40 bits */
    kCCBlockSizeRC2,          /* IV size */
    kCCBlockSizeRC2,          /* Block size */
    kCCAlgorithmRC2,
    CCSymmInit,
    CCSymmEncryptDecrypt,
    CCSymmEncryptDecrypt,
    CCSymmFinish
};

static const SSLSymmetricCipher SSLCipherRC2_128 = {
    kCCKeySizeMaxRC2,         /* Key size in bytes */
    kCCKeySizeMaxRC2,          /* Secret key size = 128 bits */
    kCCBlockSizeRC2,          /* IV size */
    kCCBlockSizeRC2,          /* Block size */
    kCCAlgorithmRC2,
    CCSymmInit,
    CCSymmEncryptDecrypt,
    CCSymmEncryptDecrypt,
    CCSymmFinish
};
#endif	/* ENABLE_RC2*/

#if		ENABLE_AES
static const SSLSymmetricCipher SSLCipherAES_128_CBC = {
    kCCKeySizeAES128,         /* Key size in bytes */
    kCCKeySizeAES128,			/* Secret key size */
    kCCBlockSizeAES128,			/* IV size */
    kCCBlockSizeAES128,			/* Block size */
    kCCAlgorithmAES128,
    CCSymmInit,
    CCSymmEncryptDecrypt,
    CCSymmEncryptDecrypt,
    CCSymmFinish
};
#endif	/* ENABLE_AES */

#if ENABLE_AES256
static const SSLSymmetricCipher SSLCipherAES_256_CBC = {
    kCCKeySizeAES256,         /* Key size in bytes */
    kCCKeySizeAES256,			/* Secret key size */
    kCCBlockSizeAES128,			/* IV size - still 128 bits */
    kCCBlockSizeAES128,			/* Block size - still 128 bits */
    kCCAlgorithmAES128,
    CCSymmInit,
    CCSymmEncryptDecrypt,
    CCSymmEncryptDecrypt,
    CCSymmFinish
};
#endif /* ENABLE_AES256 */

#if ENABLE_AES
static const SSLSymmetricCipher SSLCipherAES_128_GCM = {
    kCCKeySizeAES128,         /* Key size in bytes */
    kCCKeySizeAES128,			/* Secret key size */
    kCCBlockSizeAES128,			/* IV size */
    kCCBlockSizeAES128,			/* Block size */
    kCCAlgorithmAES128,
    CCSymmInit,
    CCSymmEncryptDecrypt,
    CCSymmEncryptDecrypt,
    CCSymmFinish
};
#endif /* ENABLE_AES_GCM */

#if ENABLE_AES256
static const SSLSymmetricCipher SSLCipherAES_256_GCM = {
    kCCKeySizeAES256,         /* Key size in bytes */
    kCCKeySizeAES256,			/* Secret key size */
    kCCBlockSizeAES128,			/* IV size - still 128 bits */
    kCCBlockSizeAES128,			/* Block size - still 128 bits */
    kCCAlgorithmAES128,
    CCSymmInit,
    CCSymmEncryptDecrypt,
    CCSymmEncryptDecrypt,
    CCSymmFinish
};
#endif /* ENABLE_AES256_GCM */

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

/*
 * List of all CipherSpecs we implement. Depending on a context's
 * exportable flag, not all of these might be available for use.
 *
 * FIXME - I'm not sure the distinction between e.g. SSL_RSA and SSL_RSA_EXPORT
 * makes any sense here. See comments for the definition of
 * KeyExchangeMethod in cryptType.h.
 */
/* Order by preference, domestic first */
static const SSLCipherSuite KnownCipherSuites[] = {
#if ENABLE_AES_GCM
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
#endif
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
#if ENABLE_AES_GCM
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
#endif
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_RC4_128_SHA,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
#if ENABLE_ECDH
#if ENABLE_AES_GCM
    TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256,
#endif
    TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256,
#if ENABLE_AES_GCM
    TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256,
#endif
    TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
    TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDH_RSA_WITH_RC4_128_SHA,
    TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA,
#endif
#if ENABLE_AES_GCM
    TLS_RSA_WITH_AES_256_GCM_SHA384,
    TLS_RSA_WITH_AES_128_GCM_SHA256,
#endif
    TLS_RSA_WITH_AES_256_CBC_SHA256,
    TLS_RSA_WITH_AES_128_CBC_SHA256,
    TLS_RSA_WITH_AES_128_CBC_SHA,
    SSL_RSA_WITH_RC4_128_SHA,
    SSL_RSA_WITH_RC4_128_MD5,
    TLS_RSA_WITH_AES_256_CBC_SHA,
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,
#if ENABLE_SSLV2
    SSL_RSA_WITH_3DES_EDE_CBC_MD5,
#endif
#if ENABLE_DES
    SSL_RSA_WITH_DES_CBC_SHA,
#endif
#if ENABLE_SSLV2
    SSL_RSA_WITH_DES_CBC_MD5,
#endif
    SSL_RSA_EXPORT_WITH_RC4_40_MD5,
#if ENABLE_DES
    SSL_RSA_EXPORT_WITH_DES40_CBC_SHA,
#endif
#if ENABLE_RC2
    SSL_RSA_WITH_RC2_CBC_MD5,
    SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,
#endif
#if ENABLE_AES_GCM
#  if ENABLE_DH_EPHEM_DSA
    TLS_DHE_DSS_WITH_AES_256_GCM_SHA384,
#  endif // ENABLE_DH_EPHEM_DSA
    TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,
#  if ENABLE_DH_EPHEM_DSA
    TLS_DHE_DSS_WITH_AES_128_GCM_SHA256,
#  endif // ENABLE_DH_EPHEM_DSA
    TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
#endif // ENABLE_AES_GCM
#if ENABLE_DH_EPHEM_DSA
    TLS_DHE_DSS_WITH_AES_128_CBC_SHA256,
#endif
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
#if ENABLE_DH_EPHEM_DSA
    TLS_DHE_DSS_WITH_AES_256_CBC_SHA256,
#endif
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,
#if ENABLE_DH_EPHEM_DSA
    TLS_DHE_DSS_WITH_AES_128_CBC_SHA,
#endif
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
#if ENABLE_DH_EPHEM_DSA
    TLS_DHE_DSS_WITH_AES_256_CBC_SHA,
#endif
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
#if ENABLE_DES
    SSL_DHE_RSA_WITH_DES_CBC_SHA,
    SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA,
#endif
#if ENABLE_DH_EPHEM_DSA
    SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA,
#if ENABLE_DES
    SSL_DHE_DSS_WITH_DES_CBC_SHA,
#endif
    SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA,
#endif
    TLS_DH_anon_WITH_AES_256_GCM_SHA384,
    TLS_DH_anon_WITH_AES_128_GCM_SHA256,
    TLS_DH_anon_WITH_AES_128_CBC_SHA256,
    TLS_DH_anon_WITH_AES_256_CBC_SHA256,
    TLS_DH_anon_WITH_AES_128_CBC_SHA,
    TLS_DH_anon_WITH_AES_256_CBC_SHA,
    SSL_DH_anon_WITH_RC4_128_MD5,
    SSL_DH_anon_WITH_3DES_EDE_CBC_SHA,
#if ENABLE_DES
    SSL_DH_anon_WITH_DES_CBC_SHA,
#endif
    SSL_DH_anon_EXPORT_WITH_RC4_40_MD5,
#if ENABLE_DES
    SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA,
#endif
	TLS_ECDHE_ECDSA_WITH_NULL_SHA,
	TLS_ECDHE_RSA_WITH_NULL_SHA,
#if ENABLE_ECDH
    TLS_ECDH_ECDSA_WITH_NULL_SHA,
	TLS_ECDH_RSA_WITH_NULL_SHA,
#endif
    TLS_RSA_WITH_NULL_SHA256,
    SSL_RSA_WITH_NULL_SHA,
    SSL_RSA_WITH_NULL_MD5

#if 0
    /* We don't support these yet. */
    TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA,
    TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_RSA_WITH_RC4_128_SHA,
    TLS_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_RSA_WITH_RC4_128_MD5,
    TLS_DH_DSS_WITH_AES_256_GCM_SHA384,
    TLS_DH_DSS_WITH_AES_128_GCM_SHA256,
    TLS_DH_RSA_WITH_AES_256_GCM_SHA384,
    TLS_DH_RSA_WITH_AES_128_GCM_SHA256,
    TLS_DH_DSS_WITH_AES_256_CBC_SHA256,
    TLS_DH_RSA_WITH_AES_256_CBC_SHA256,
    TLS_DH_DSS_WITH_AES_128_CBC_SHA256,
    TLS_DH_RSA_WITH_AES_128_CBC_SHA256,
    TLS_DH_DSS_WITH_AES_256_CBC_SHA,
    TLS_DH_RSA_WITH_AES_256_CBC_SHA,
	TLS_DH_DSS_WITH_AES_128_CBC_SHA,
    TLS_DH_RSA_WITH_AES_128_CBC_SHA,
    TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA,
    TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDH_anon_WITH_AES_256_CBC_SHA,
	TLS_ECDH_anon_WITH_AES_128_CBC_SHA,
    TLS_ECDH_anon_WITH_RC4_128_SHA,
    TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDH_anon_WITH_NULL_SHA,
#endif
};

static const unsigned CipherSuiteCount = sizeof(KnownCipherSuites) / sizeof(*KnownCipherSuites);

static KeyExchangeMethod sslCipherSuiteGetKeyExchangeMethod(SSLCipherSuite cipherSuite) {
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

        default:
            sslErrorLog("Invalid cipherSuite %02hX", cipherSuite);
            assert(0);
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
            sslErrorLog("Invalid cipherSuite %02hX", cipherSuite);
            assert(0);
            return SSL_SignatureAlgorithmAnonymous;
    }
}
#endif

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
            sslErrorLog("Invalid cipherSuite %02hX", cipherSuite);
            assert(0);
            return TLS_Version_1_2;
    }
}

static SSL_HashAlgorithm sslCipherSuiteGetHashAlgorithm(SSLCipherSuite cipherSuite) {
    switch (cipherSuite) {
        case TLS_NULL_WITH_NULL_NULL:
            return SSL_HashAlgorithmNone;
        case SSL_RSA_WITH_RC2_CBC_MD5:
        case SSL_RSA_WITH_DES_CBC_MD5:
        case SSL_RSA_WITH_3DES_EDE_CBC_MD5:
        case TLS_RSA_WITH_NULL_MD5:
        case SSL_RSA_EXPORT_WITH_RC4_40_MD5:
        case SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5:
        case TLS_RSA_WITH_RC4_128_MD5:
        case SSL_DH_anon_EXPORT_WITH_RC4_40_MD5:
        case TLS_DH_anon_WITH_RC4_128_MD5:
            return SSL_HashAlgorithmMD5;
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
            return SSL_HashAlgorithmSHA1;
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
            return SSL_HashAlgorithmSHA256;
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
            return SSL_HashAlgorithmSHA384;
        default:
            sslErrorLog("Invalid cipherSuite %02hX", cipherSuite);
            assert(0);
            return SSL_HashAlgorithmNone;
    }
}

static const HashHmacReference* sslCipherSuiteGetHashHmacReference(SSLCipherSuite cipherSuite) {
    switch (sslCipherSuiteGetHashAlgorithm(cipherSuite)) {
        case SSL_HashAlgorithmNone:
            return &HashHmacNull;
        case SSL_HashAlgorithmMD5:
            return &HashHmacMD5;
        case SSL_HashAlgorithmSHA1:
            return &HashHmacSHA1;
        case SSL_HashAlgorithmSHA256:
            return &HashHmacSHA256;
        case SSL_HashAlgorithmSHA384:
            return &HashHmacSHA384;
        default:
            sslErrorLog("Invalid hashAlgorithm %02hX", cipherSuite);
            assert(0);
            return &HashHmacNull;
    }
}

static const SSLSymmetricCipher *sslCipherSuiteGetSymmetricCipher(SSLCipherSuite cipherSuite) {
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
            return &SSLCipherNull;
#if ENABLE_RC4
        case SSL_RSA_EXPORT_WITH_RC4_40_MD5:
        case SSL_DH_anon_EXPORT_WITH_RC4_40_MD5:
            return &SSLCipherRC4_40;
#endif
#if ENABLE_RC2
        case SSL_RSA_WITH_RC2_CBC_MD5:
        case SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5:
            return &SSLCipherRC2_40;
#endif
#if ENABLE_IDEA
        case SSL_RSA_WITH_IDEA_CBC_SHA:
            return &SSLCipherIDEA_CBC;
#endif
#if ENABLE_DES
        case SSL_RSA_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA:
        case SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA:
            return &SSLCipherDES40_CBC;
        case SSL_RSA_WITH_DES_CBC_MD5:
        case SSL_RSA_WITH_DES_CBC_SHA:
        case SSL_DH_DSS_WITH_DES_CBC_SHA:
        case SSL_DH_RSA_WITH_DES_CBC_SHA:
        case SSL_DHE_DSS_WITH_DES_CBC_SHA:
        case SSL_DHE_RSA_WITH_DES_CBC_SHA:
        case SSL_DH_anon_WITH_DES_CBC_SHA:
            return &SSLCipherDES_CBC;
#endif
#if ENABLE_FORTEZZA
        case SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA:
            return &SSLCipherFORTEZZA_CBC;
#endif
#if ENABLE_RC4
        case TLS_RSA_WITH_RC4_128_MD5:
        case TLS_RSA_WITH_RC4_128_SHA:
        case TLS_DH_anon_WITH_RC4_128_MD5:
        case TLS_ECDH_ECDSA_WITH_RC4_128_SHA:
        case TLS_ECDHE_ECDSA_WITH_RC4_128_SHA:
        case TLS_ECDH_RSA_WITH_RC4_128_SHA:
        case TLS_ECDHE_RSA_WITH_RC4_128_SHA:
        case TLS_ECDH_anon_WITH_RC4_128_SHA:
            return &SSLCipherRC4_128;
#endif
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
            return &SSLCipher3DES_CBC;
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
            return &SSLCipherAES_128_CBC;
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
            return &SSLCipherAES_256_CBC;
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
            return &SSLCipherAES_128_GCM;
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
            return &SSLCipherAES_256_GCM;
        default:
            sslErrorLog("Invalid cipherSuite %02hX", cipherSuite);
            assert(0);
            return &SSLCipherNull;
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
            return SSL_CipherAlgorithmAES_256_GCM;
        default:
            return SSL_CipherAlgorithmNull;
    }
}

/*
 * Given a valid ctx->validCipherSpecs array, calculate how many of those
 * cipherSpecs are *not* SSLv2 only, storing result in
 * ctx->numValidNonSSLv2Specs. ClientHello routines need this to set
 * up outgoing cipherSpecs arrays correctly.
 *
 * Also determines if any ECDSA/ECDH ciphers are enabled; we need to know
 * that when creating a hello message.
 */
static void sslAnalyzeCipherSpecs(SSLContext *ctx)
{
	unsigned 		dex;
	const SSLCipherSuite *cipherSuite;

#if ENABLE_SSLV2
	ctx->numValidNonSSLv2Suites = 0;
#endif
	cipherSuite = &ctx->validCipherSuites[0];
	ctx->ecdsaEnable = false;
	for(dex=0; dex<ctx->numValidCipherSuites; dex++, cipherSuite++) {
#if ENABLE_SSLV2
		if(!CIPHER_SPEC_IS_SSLv2(*cipherSuite)) {
			ctx->numValidNonSSLv2Suites++;
		}
#endif
		switch(sslCipherSuiteGetKeyExchangeMethod(*cipherSuite)) {
			case SSL_ECDH_ECDSA:
			case SSL_ECDHE_ECDSA:
			case SSL_ECDH_RSA:
			case SSL_ECDHE_RSA:
			case SSL_ECDH_anon:
				ctx->ecdsaEnable = true;
				break;
			default:
				break;
		}
	}
}

/*
 * Build ctx->validCipherSpecs as a copy of KnownCipherSpecs, assuming that
 * validCipherSpecs is currently not valid (i.e., SSLSetEnabledCiphers() has
 * not been called).
 */
OSStatus sslBuildCipherSuiteArray(SSLContext *ctx)
{
	size_t          size;
	unsigned        dex;

	assert(ctx != NULL);
	assert(ctx->validCipherSuites == NULL);

	ctx->numValidCipherSuites = CipherSuiteCount;
	size = CipherSuiteCount * sizeof(SSLCipherSpec);
	ctx->validCipherSuites = (SSLCipherSuite *)sslMalloc(size);
	if(ctx->validCipherSuites == NULL) {
		ctx->numValidCipherSuites = 0;
		return memFullErr;
	}

	/*
	 * Trim out inappropriate ciphers:
	 *  -- trim anonymous ciphers if !ctx->anonCipherEnable (default)
	 *  -- trim ECDSA ciphers for server side if appropriate
	 *  -- trim ECDSA ciphers if TLSv1 disable or SSLv2 enabled (since
	 *     we MUST do the Client Hello extensions to make these ciphers
	 *     work reliably)
	 *  -- trim 40 and 56-bit ciphers if !ctx->weakCipherEnable (default)
	 *  -- trim ciphers incompatible with our private key in server mode
	 *  -- trim RC4 ciphers if DTLSv1 enable
	 */
	SSLCipherSuite *dst = ctx->validCipherSuites;
	const SSLCipherSuite *src = KnownCipherSuites;

	bool trimECDSA = false;
	if((ctx->protocolSide == kSSLServerSide) && !SSL_ECDSA_SERVER) {
		trimECDSA = true;
	}
	if(ctx->minProtocolVersion == SSL_Version_2_0
       || ctx->maxProtocolVersion == SSL_Version_3_0) {
        /* We trim ECDSA cipher suites if SSL2 is enabled or
           The maximum allowed protocol is SSL3.  Note that this
           won't trim ECDSA cipherspecs for DTLS which should be
           the right thing to do here. */
		trimECDSA = true;
	}

    bool trimRC4 = ctx->isDTLS;

    bool trimDHE = (ctx->protocolSide == kSSLServerSide) &&
        !ctx->dhParamsEncoded.length;

	for(dex=0; dex<CipherSuiteCount; dex++) {
        KeyExchangeMethod kem = sslCipherSuiteGetKeyExchangeMethod(*src);
        const SSLSymmetricCipher *cipher = sslCipherSuiteGetSymmetricCipher(*src);
        SSLProtocolVersion minVersion = sslCipherSuiteGetMinSupportedTLSVersion(*src);

        /* Trim according to supported versions */
        if(((ctx->isDTLS) && (minVersion>TLS_Version_1_1)) ||  /* DTLS is like TLS.1.1 */
            (minVersion > ctx->maxProtocolVersion))
        {
            ctx->numValidCipherSuites--;
            src++;
            continue;
        }

        /* First skip ECDSA ciphers as appropriate */
		switch(kem) {
			case SSL_ECDH_ECDSA:
			case SSL_ECDHE_ECDSA:
			case SSL_ECDH_RSA:
			case SSL_ECDHE_RSA:
			case SSL_ECDH_anon:
				if(trimECDSA) {
					/* Skip this one */
					ctx->numValidCipherSuites--;
					src++;
					continue;
				}
				else {
					break;
				}
			default:
				break;
		}

		if(!ctx->anonCipherEnable) {
			/* trim out the anonymous (and null-cipher) ciphers */
			if(cipher == &SSLCipherNull) {
				/* skip this one */
				ctx->numValidCipherSuites--;
				src++;
				continue;
			}
			switch(kem) {
				case SSL_DH_anon:
				case SSL_DH_anon_EXPORT:
				case SSL_ECDH_anon:
					/* skip this one */
					ctx->numValidCipherSuites--;
					src++;
					continue;
				default:
					break;
			}
		}

		if (false
			/* trim out 40 and 56 bit ciphers (considered unsafe to use) */
#if ENABLE_RC4
			|| (cipher == &SSLCipherRC4_40)
#endif
#if ENABLE_RC2
			|| (cipher == &SSLCipherRC2_40)
#endif
#if ENABLE_DES
			|| (cipher == &SSLCipherDES_CBC)
			|| (cipher == &SSLCipherDES40_CBC)
#endif
			) {
				/* skip this one */
				ctx->numValidCipherSuites--;
				src++;
				continue;
		}

		if(ctx->protocolSide == kSSLServerSide && ctx->signingPrivKeyRef != NULL) {
			/* in server mode, trim out ciphers incompatible with our private key */
			SSLCipherSpec testCipherSpec = {
				.cipherSpec = *src,
				.keyExchangeMethod = kem,
				.cipher = cipher
			};
			if(sslVerifySelectedCipher(ctx, &testCipherSpec) != noErr) {
				/* skip this one */
				ctx->numValidCipherSuites--;
				src++;
				continue;
			}
		}

        if (trimDHE) {
			switch(kem) {
				case SSL_DHE_DSS:
				case SSL_DHE_DSS_EXPORT:
				case SSL_DHE_RSA:
				case SSL_DHE_RSA_EXPORT:
					/* skip this one */
					ctx->numValidCipherSuites--;
					src++;
					continue;
				default:
					break;
			}
		}

        if (trimRC4 && cipher && (cipher->keyAlg == kCCAlgorithmRC4)) {
            ctx->numValidCipherSuites--;
            src++;
            continue;
        }

		/* This one is good to go */
        *dst++ = *src++;
	}
	sslAnalyzeCipherSpecs(ctx);
	return noErr;
}

/*
 * Convert an array of SSLCipherSuites (which is always KnownCipherSpecs)
 * to an array of SSLCipherSuites.
 */
static OSStatus
cipherSuitesToCipherSuites(
                          size_t				numCipherSuites,
                          const SSLCipherSuite	*cipherSuites,
                          SSLCipherSuite		*ciphers,		/* RETURNED */
                          size_t				*numCiphers)	/* IN/OUT */
{
	if(*numCiphers < numCipherSuites) {
		return errSSLBufferOverflow;
	}
    memcpy(ciphers, cipherSuites, numCipherSuites * 2);
	*numCiphers = numCipherSuites;
	return noErr;
}

/***
 *** Publicly exported functions declared in SecureTransport.h
 ***/

/*
 * Determine number and values of all of the SSLCipherSuites we support.
 * Caller allocates output buffer for SSLGetSupportedCiphers() and passes in
 * its size in *numCiphers. If supplied buffer is too small, errSSLBufferOverflow
 * will be returned.
 */
OSStatus
SSLGetNumberSupportedCiphers (SSLContextRef	ctx,
							  size_t		*numCiphers)
{
	if((ctx == NULL) || (numCiphers == NULL)) {
		return paramErr;
	}
	*numCiphers = CipherSuiteCount;
	return noErr;
}

OSStatus
SSLGetSupportedCiphers		 (SSLContextRef		ctx,
							  SSLCipherSuite	*ciphers,		/* RETURNED */
							  size_t			*numCiphers)	/* IN/OUT */
{
	if((ctx == NULL) || (ciphers == NULL) || (numCiphers == NULL)) {
		return paramErr;
	}
	return cipherSuitesToCipherSuites(CipherSuiteCount,
		KnownCipherSuites,
		ciphers,
		numCiphers);
}

/*
 * Specify a (typically) restricted set of SSLCipherSuites to be enabled by
 * the current SSLContext. Can only be called when no session is active. Default
 * set of enabled SSLCipherSuites is the same as the complete set of supported
 * SSLCipherSuites as obtained by SSLGetSupportedCiphers().
 */
OSStatus
SSLSetEnabledCiphers		(SSLContextRef			ctx,
							 const SSLCipherSuite	*ciphers,
							 size_t					numCiphers)
{
	size_t          size;
	unsigned 		callerDex;
	unsigned		validDex;
	unsigned		tableDex;

	if((ctx == NULL) || (ciphers == NULL) || (numCiphers == 0)) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	ctx->numValidCipherSuites = 0;
	size = numCiphers * sizeof(SSLCipherSuite);
	ctx->validCipherSuites = (SSLCipherSuite *)sslMalloc(size);
	if(ctx->validCipherSuites == NULL) {
		return memFullErr;
	}

	/*
	 * Run thru caller's specs, finding a matching SSLCipherSpec for each one.
	 * If caller specifies one we don't know about, skip it.
	 */
	for(callerDex=0, validDex=0; callerDex<numCiphers; callerDex++) {
		/* find matching CipherSpec in our known table */
		int foundOne = 0;
		for(tableDex=0; tableDex<CipherSuiteCount; tableDex++) {
			if(ciphers[callerDex] == KnownCipherSuites[tableDex]) {
                ctx->validCipherSuites[validDex++] = KnownCipherSuites[tableDex];
				ctx->numValidCipherSuites++;
				foundOne = 1;
				break;
			}
		}
		if(!foundOne) {
			/* caller specified one we don't implement */
            sslErrorLog("SSLSetEnabledCiphers: invalid cipher suite %04hX",
				ciphers[callerDex]);
			#if 0
			sslFree(ctx->validCipherSuites);
			ctx->validCipherSuites = NULL;
			ctx->numValidCipherSuites = 0;
			return errSSLBadCipherSuite;
			#endif
		}
	}

	/* success */
	sslAnalyzeCipherSpecs(ctx);
	return noErr;
}

/*
 * Determine number and values of all of the SSLCipherSuites currently enabled.
 * Caller allocates output buffer for SSLGetEnabledCiphers() and passes in
 * its size in *numCiphers. If supplied buffer is too small, errSSLBufferOverflow
 * will be returned.
 */
OSStatus
SSLGetNumberEnabledCiphers 	(SSLContextRef			ctx,
							 size_t					*numCiphers)
{
	if((ctx == NULL) || (numCiphers == NULL)) {
		return paramErr;
	}
	if(ctx->validCipherSuites == NULL) {
		/* hasn't been set; build default array temporarily */
		OSStatus status = sslBuildCipherSuiteArray(ctx);
		if(!status) {
			*numCiphers = ctx->numValidCipherSuites;
			/* put things back as we found them */
			sslFree(ctx->validCipherSuites);
			ctx->validCipherSuites = NULL;
			ctx->numValidCipherSuites = 0;
		} else {
			/* unable to build default array; use known cipher count */
			*numCiphers = CipherSuiteCount;
		}
	}
	else {
		/* caller set via SSLSetEnabledCiphers */
		*numCiphers = ctx->numValidCipherSuites;
	}
	return noErr;
}

OSStatus
SSLGetEnabledCiphers		(SSLContextRef			ctx,
							 SSLCipherSuite			*ciphers,		/* RETURNED */
							 size_t					*numCiphers)	/* IN/OUT */
{
	if((ctx == NULL) || (ciphers == NULL) || (numCiphers == NULL)) {
		return paramErr;
	}
	if(ctx->validCipherSuites == NULL) {
		/* hasn't been set; build default array temporarily */
		OSStatus status = sslBuildCipherSuiteArray(ctx);
		if(!status) {
			status = cipherSuitesToCipherSuites(ctx->numValidCipherSuites,
				ctx->validCipherSuites,
				ciphers,
				numCiphers);
			/* put things back as we found them */
			sslFree(ctx->validCipherSuites);
			ctx->validCipherSuites = NULL;
			ctx->numValidCipherSuites = 0;
		} else {
			/* unable to build default array; use known cipher suite array */
			status = cipherSuitesToCipherSuites(CipherSuiteCount,
				KnownCipherSuites,
				ciphers,
				numCiphers);
		}
		return status;
	}
	else {
		/* use the ones specified in SSLSetEnabledCiphers() */
		return cipherSuitesToCipherSuites(ctx->numValidCipherSuites,
			ctx->validCipherSuites,
			ciphers,
			numCiphers);
	}
}

/***
 *** End of publically exported functions declared in SecureTransport.h
 ***/

void InitCipherSpec(SSLContext *ctx)
{
    SSLCipherSpec *dst = &ctx->selectedCipherSpec;
    dst->cipherSpec = ctx->selectedCipher;
    dst->cipher = sslCipherSuiteGetSymmetricCipher(ctx->selectedCipher);
    dst->isExportable = dst->cipher->secretKeySize < 6 ? Exportable : NotExportable;
    dst->keyExchangeMethod = sslCipherSuiteGetKeyExchangeMethod(ctx->selectedCipher);
    dst->macAlgorithm = sslCipherSuiteGetHashHmacReference(ctx->selectedCipher);
};

OSStatus
FindCipherSpec(SSLContext *ctx)
{
	unsigned i;

    assert(ctx != NULL);
    assert(ctx->validCipherSuites != NULL);

    for (i=0; i<ctx->numValidCipherSuites; i++)
    {
        if (ctx->validCipherSuites[i] == ctx->selectedCipher) {
            InitCipherSpec(ctx);
            /* make sure we're configured to handle this one */
            return sslVerifySelectedCipher(ctx, &ctx->selectedCipherSpec);
        }
    }
    /* Not found */
    return errSSLNegotiation;
}
