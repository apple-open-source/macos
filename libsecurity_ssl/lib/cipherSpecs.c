/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
	File:		cipherSpecs.c

	Contains:	SSLCipherSpec declarations

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

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
#include "appleCdsa.h"
#include <string.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#define ENABLE_3DES			1		/* normally enabled */
#define ENABLE_RC4			1		/* normally enabled */
#define ENABLE_DES			1		/* normally enabled */
#define ENABLE_RC2			1		/* normally enabled */
#define ENABLE_AES			1		/* normally enabled, our first preference */
#define ENABLE_ECDHE		1
#define ENABLE_ECDHE_RSA	1
#define ENABLE_ECDH			1
#define ENABLE_ECDH_RSA		1

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

#if 	APPLE_DH
#define ENABLE_DH_ANON			1
#define ENABLE_DH_EPHEM_RSA		1
#define ENABLE_DH_EPHEM_DSA		1
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
    8,      /* Key size in bytes */
    8,      /* Secret key size = 64 bits */
    8,      /* IV size */
    8,      /* Block size */
    CSSM_ALGID_DES,
    CSSM_ALGID_DES,
    /* Note we don't want CSSM_ALGMODE_CBCPadIV8; our clients do that
     * for us */
    CSSM_ALGMODE_CBC_IV8,
	CSSM_PADDING_NONE,
    CCSymmInit,
    CCSymmEncryptDecrypt,
    CCSymmEncryptDecrypt,
    CCSymmFinish
};

static const SSLSymmetricCipher SSLCipherDES40_CBC = {
    8,      /* Key size in bytes */
    5,      /* Secret key size = 40 bits */
    8,      /* IV size */
    8,      /* Block size */
    CSSM_ALGID_DES,
    CSSM_ALGID_DES,
    CSSM_ALGMODE_CBC_IV8,
	CSSM_PADDING_NONE,
    CCSymmInit,
    CCSymmEncryptDecrypt,
    CCSymmEncryptDecrypt,
    CCSymmFinish
};
#endif	/* ENABLE_DES */

#if	ENABLE_3DES
static const SSLSymmetricCipher SSLCipher3DES_CBC = {
    24,     /* Key size in bytes */
    24,     /* Secret key size = 192 bits */
    8,      /* IV size */
    8,      /* Block size */
    CSSM_ALGID_3DES_3KEY,			// key gen 
    CSSM_ALGID_3DES_3KEY_EDE,		// encryption
    /* Note we don't want CSSM_ALGMODE_CBCPadIV8; our clients do that
     * for us */
    CSSM_ALGMODE_CBC_IV8,
	CSSM_PADDING_NONE,
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
    CSSM_ALGID_RC4,
    CSSM_ALGID_RC4,
    CSSM_ALGMODE_NONE,
	CSSM_PADDING_NONE,
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
    CSSM_ALGID_RC4,
    CSSM_ALGID_RC4,
    CSSM_ALGMODE_NONE,
	CSSM_PADDING_NONE,
    CCSymmInit,
    CCSymmEncryptDecrypt,
    CCSymmEncryptDecrypt,
    CCSymmFinish
};
#endif	/* ENABLE_RC4 */

#if		ENABLE_RC2
static const SSLSymmetricCipher SSLCipherRC2_40 = {
    16,         /* Key size in bytes */
    5,          /* Secret key size = 40 bits */
    8,          /* IV size */
    8,          /* Block size */
    CSSM_ALGID_RC2,
    CSSM_ALGID_RC2,
    CSSM_ALGMODE_CBC_IV8,
	CSSM_PADDING_NONE,
    CDSASymmInit,
    CDSASymmEncrypt,
    CDSASymmDecrypt,
    CDSASymmFinish
};

static const SSLSymmetricCipher SSLCipherRC2_128 = {
    16,         /* Key size in bytes */
    16,          /* Secret key size = 40 bits */
    8,          /* IV size */
    8,          /* Block size */
    CSSM_ALGID_RC2,
    CSSM_ALGID_RC2,
    CSSM_ALGMODE_CBC_IV8,
	CSSM_PADDING_NONE,
    CDSASymmInit,
    CDSASymmEncrypt,
    CDSASymmDecrypt,
    CDSASymmFinish
};

#endif	/* ENABLE_RC2*/

#if		ENABLE_AES

static const SSLSymmetricCipher SSLCipherAES_128 = {
    16,         /* Key size in bytes */
    16,			/* Secret key size */
    16,			/* IV size */
    16,			/* Block size */
    CSSM_ALGID_AES,
    CSSM_ALGID_AES,
    CSSM_ALGMODE_CBC_IV8,
	CSSM_PADDING_NONE,
    AESSymmInit,
    AESSymmEncrypt,
    AESSymmDecrypt,
    AESSymmFinish
};

static const SSLSymmetricCipher SSLCipherAES_256 = {
    32,         /* Key size in bytes */
    32,			/* Secret key size */
    16,			/* IV size - still 128 bits */
    16,			/* Block size - still 128 bits */
    CSSM_ALGID_AES,
    CSSM_ALGID_AES,
    CSSM_ALGMODE_CBC_IV8,
	CSSM_PADDING_NONE,
    CDSASymmInit,
    CDSASymmEncrypt,
    CDSASymmDecrypt,
    CDSASymmFinish
};

#endif	/* ENABLE_AES */

/* Even if we don't support NULL_WITH_NULL_NULL for transport, 
 * we need a reference for startup */
const SSLCipherSpec SSL_NULL_WITH_NULL_NULL_CipherSpec =
{   SSL_NULL_WITH_NULL_NULL,
    Exportable,
    SSL_NULL_auth,
    &HashHmacNull,
    &SSLCipherNull
};

/*
 * List of all CipherSpecs we implement. Depending on a context's 
 * exportable flag, not all of these might be available for use. 
 *
 * FIXME - I'm not sure the distinction between e.g. SSL_RSA and SSL_RSA_EXPORT
 * makes any sense here. See comments for the definition of 
 * KeyExchangeMethod in cryptType.h.
 */
/* Order by preference, domestic first */
static const SSLCipherSpec KnownCipherSpecs[] =
{
	/*** domestic only ***/
	#if ENABLE_ECDHE
	    {   
	    	TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, 
	    	NotExportable, 
	    	SSL_ECDHE_ECDSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherAES_256 
	    },
	    {   
	    	TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, 
	    	NotExportable, 
	    	SSL_ECDHE_ECDSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherAES_128 
	    },
	    {   
	    	TLS_ECDHE_ECDSA_WITH_RC4_128_SHA, 
	    	NotExportable, 
	    	SSL_ECDHE_ECDSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherRC4_128 
	    },
	    {   
	    	TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA, 
	    	NotExportable, 
	    	SSL_ECDHE_ECDSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipher3DES_CBC 
	    },
	#endif	/* ENABLE_ECDHE */

	#if ENABLE_ECDHE_RSA
	    {   
	    	TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA, 
	    	NotExportable, 
	    	SSL_ECDHE_RSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherAES_128 
	    },
	    {   
	    	TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA, 
	    	NotExportable, 
	    	SSL_ECDHE_RSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherAES_256 
	    },
	    {   
	    	TLS_ECDHE_RSA_WITH_RC4_128_SHA, 
	    	NotExportable, 
	    	SSL_ECDHE_RSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherRC4_128 
	    },
	    {   
	    	TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA, 
	    	NotExportable, 
	    	SSL_ECDHE_RSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipher3DES_CBC 
	    },

	#endif	/* ENABLE_ECDHE_RSA */
	
	#if ENABLE_ECDH
	    {   
	    	TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA, 
	    	NotExportable, 
	    	SSL_ECDH_ECDSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherAES_128 
	    },
	    {   
	    	TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA, 
	    	NotExportable, 
	    	SSL_ECDH_ECDSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherAES_256 
	    },
	    {   
	    	TLS_ECDH_ECDSA_WITH_RC4_128_SHA, 
	    	NotExportable, 
	    	SSL_ECDH_ECDSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherRC4_128 
	    },
	    {   
	    	TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA, 
	    	NotExportable, 
	    	SSL_ECDH_ECDSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipher3DES_CBC 
	    },
	#endif	/* ENABLE_ECDH */
	
	#if ENABLE_ECDH_RSA
	    {   
	    	TLS_ECDH_RSA_WITH_AES_128_CBC_SHA, 
	    	NotExportable, 
	    	SSL_ECDH_RSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherAES_128 
	    },
	    {   
	    	TLS_ECDH_RSA_WITH_AES_256_CBC_SHA, 
	    	NotExportable, 
	    	SSL_ECDH_RSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherAES_256 
	    },
	    {   
	    	TLS_ECDH_RSA_WITH_RC4_128_SHA, 
	    	NotExportable, 
	    	SSL_ECDH_RSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherRC4_128 
	    },
	    {   
	    	TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA, 
	    	NotExportable, 
	    	SSL_ECDH_RSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipher3DES_CBC 
	    },

	#endif	/* ENABLE_ECDH_RSA */
	
	#if	ENABLE_AES
	    {   
	    	TLS_RSA_WITH_AES_128_CBC_SHA, 
	    	NotExportable, 
	    	SSL_RSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherAES_128 
	    },	
	#endif	/* ENABLE_AES */
    #if	ENABLE_RSA_RC4_SHA_NONEXPORT
	    {   
	    	SSL_RSA_WITH_RC4_128_SHA, 
	    	NotExportable, 
	    	SSL_RSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherRC4_128 
	    },
    #endif
    #if	ENABLE_RSA_RC4_MD5_NONEXPORT
	    {   
	    	SSL_RSA_WITH_RC4_128_MD5, 
	    	NotExportable, 
	    	SSL_RSA, 
	    	&HashHmacMD5, 
	    	&SSLCipherRC4_128 
	    },
    #endif
	#if	ENABLE_AES
	    {   
	    	TLS_RSA_WITH_AES_256_CBC_SHA, 
	    	NotExportable, 
	    	SSL_RSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherAES_256 
	    },	
	#endif	/* ENABLE_AES */
	#if	ENABLE_RSA_3DES_SHA
	    {   
	    	SSL_RSA_WITH_3DES_EDE_CBC_SHA, 
	    	NotExportable, 
	    	SSL_RSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipher3DES_CBC 
	    },
	#endif
	#if	ENABLE_RSA_3DES_MD5
	    {   
	    	SSL_RSA_WITH_3DES_EDE_CBC_MD5, 
	    	NotExportable, 
	    	SSL_RSA, 
	    	&HashHmacMD5, 
	    	&SSLCipher3DES_CBC 
	    },
	#endif
	#if	ENABLE_RSA_DES_SHA_NONEXPORT
	    {   
	    	SSL_RSA_WITH_DES_CBC_SHA, 
	    	NotExportable, 
	    	SSL_RSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherDES_CBC 
	    },
    #endif
	#if	ENABLE_RSA_DES_MD5_NONEXPORT
	    {   
	    	SSL_RSA_WITH_DES_CBC_MD5, 
	    	NotExportable, 
	    	SSL_RSA, 
	    	&HashHmacMD5, 
	    	&SSLCipherDES_CBC 
	    },
    #endif
	/*** exportable ***/
	#if	ENABLE_RSA_RC4_MD5_EXPORT
		{   
			SSL_RSA_EXPORT_WITH_RC4_40_MD5, 
			Exportable, 
			SSL_RSA_EXPORT, 
			&HashHmacMD5, 
			&SSLCipherRC4_40 
		},
	#endif
	#if ENABLE_RSA_DES_SHA_EXPORT
	    {   
	    	SSL_RSA_EXPORT_WITH_DES40_CBC_SHA, 
	    	Exportable, 
	    	SSL_RSA_EXPORT, 
	    	&HashHmacSHA1, 
	    	&SSLCipherDES40_CBC 
	    },
	#endif 
	
    #if	ENABLE_RSA_RC2_MD5_EXPORT
	    {   
	    	SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5, 
	    	Exportable, 
	    	SSL_RSA_EXPORT, 
	    	&HashHmacMD5, 
	    	&SSLCipherRC2_40 
	    },
    #endif
    #if	ENABLE_RSA_RC2_MD5_NONEXPORT
	    {   
	    	SSL_RSA_WITH_RC2_CBC_MD5, 
	    	NotExportable, 
	    	SSL_RSA, 
	    	&HashHmacMD5, 
	    	&SSLCipherRC2_128 
	    },
    #endif
	#if ENABLE_AES
		{   
	    	TLS_DHE_DSS_WITH_AES_128_CBC_SHA, 
	    	NotExportable, 
	    	SSL_DHE_DSS, 
	    	&HashHmacSHA1, 
	    	&SSLCipherAES_128 
	    },
	    {   
	    	TLS_DHE_RSA_WITH_AES_128_CBC_SHA, 
	    	NotExportable, 
	    	SSL_DHE_RSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherAES_128 
	    },
		{   
	    	TLS_DHE_DSS_WITH_AES_256_CBC_SHA, 
	    	NotExportable, 
	    	SSL_DHE_DSS, 
	    	&HashHmacSHA1, 
	    	&SSLCipherAES_256
	    },
	    {   
	    	TLS_DHE_RSA_WITH_AES_256_CBC_SHA, 
	    	NotExportable, 
	    	SSL_DHE_RSA, 
	    	&HashHmacSHA1, 
	    	&SSLCipherAES_256 
	    },
	#endif
	#if ENABLE_DH_EPHEM_RSA
		{
			SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
			NotExportable,
			SSL_DHE_RSA,
	    	&HashHmacSHA1, 
	    	&SSLCipher3DES_CBC
		},
		{
			SSL_DHE_RSA_WITH_DES_CBC_SHA,
			NotExportable,
			SSL_DHE_RSA,
	    	&HashHmacSHA1, 
	    	&SSLCipherDES_CBC
		},
		{
			SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA,
			Exportable,
			SSL_DHE_RSA,
	    	&HashHmacSHA1, 
	    	&SSLCipherDES40_CBC
		},
	
	#endif	/* ENABLE_DH_EPHEM_RSA */
	#if ENABLE_DH_EPHEM_DSA
		{
			SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA,
			NotExportable,
			SSL_DHE_DSS,
	    	&HashHmacSHA1, 
	    	&SSLCipher3DES_CBC
		},
		{
			SSL_DHE_DSS_WITH_DES_CBC_SHA,
			NotExportable,
			SSL_DHE_DSS,
	    	&HashHmacSHA1, 
	    	&SSLCipherDES_CBC
		},
		{
			SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA,
			Exportable,
			SSL_DHE_DSS,
	    	&HashHmacSHA1, 
	    	&SSLCipherDES40_CBC
		},
	
	#endif	/* ENABLE_DH_EPHEM_DSA */
    #if ENABLE_DH_ANON
	    {   
	    	TLS_DH_anon_WITH_AES_128_CBC_SHA, 
	    	NotExportable, 
	    	SSL_DH_anon, 
	    	&HashHmacSHA1, 
	    	&SSLCipherAES_128 
	    },
	    {   
	    	TLS_DH_anon_WITH_AES_256_CBC_SHA, 
	    	NotExportable, 
	    	SSL_DH_anon, 
	    	&HashHmacSHA1, 
	    	&SSLCipherAES_256 
	    },
		{
			SSL_DH_anon_WITH_RC4_128_MD5,
			NotExportable,
			SSL_DH_anon,
	    	&HashHmacMD5, 
	    	&SSLCipherRC4_128
		},
		{
			SSL_DH_anon_WITH_3DES_EDE_CBC_SHA,
			NotExportable,
			SSL_DH_anon,
	    	&HashHmacSHA1, 
	    	&SSLCipher3DES_CBC 
		},
		{
			SSL_DH_anon_WITH_DES_CBC_SHA,
			NotExportable,
			SSL_DH_anon,
	    	&HashHmacSHA1, 
	    	&SSLCipherDES_CBC 
		},
		{
			SSL_DH_anon_EXPORT_WITH_RC4_40_MD5,
			Exportable,
			SSL_DH_anon,
	    	&HashHmacMD5, 
	    	&SSLCipherRC4_40 
		},
		{
			SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA,
			Exportable,
			SSL_DH_anon,
	    	&HashHmacSHA1, 
	    	&SSLCipherDES40_CBC 
		},
	#endif	/* APPLE_DH */
		/* this one definitely goes last */
	    {   
	    	SSL_RSA_WITH_NULL_MD5, 
	    	Exportable, 
	    	SSL_RSA, 
	    	&HashHmacMD5, 
	    	&SSLCipherNull 
	    },
};

static const unsigned CipherSpecCount = sizeof(KnownCipherSpecs) / sizeof(SSLCipherSpec);

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
	SSLCipherSpec	*cipherSpec;
	
	ctx->numValidNonSSLv2Specs = 0;
	cipherSpec = &ctx->validCipherSpecs[0];
	for(dex=0; dex<ctx->numValidCipherSpecs; dex++, cipherSpec++) {
		if(!CIPHER_SUITE_IS_SSLv2(cipherSpec->cipherSpec)) {
			ctx->numValidNonSSLv2Specs++;
		}
		switch(cipherSpec->keyExchangeMethod) {
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
OSStatus sslBuildCipherSpecArray(SSLContext *ctx)
{
	unsigned 		size;
	unsigned		dex;
	
	assert(ctx != NULL);
	assert(ctx->validCipherSpecs == NULL);
	
	ctx->numValidCipherSpecs = CipherSpecCount;
	size = CipherSpecCount * sizeof(SSLCipherSpec);
	ctx->validCipherSpecs = (SSLCipherSpec *)sslMalloc(size);
	if(ctx->validCipherSpecs == NULL) {
		ctx->numValidCipherSpecs = 0;
		return memFullErr;
	}
	
	/* 
	 * Trim out inappropriate ciphers:
	 *  -- trim anonymous ciphers if !ctx->anonCipherEnable
	 *  -- trim ECDSA ciphers for server side if appropriate
	 *  -- trim ECDSA ciphers if TLSv1 disable or SSLv2 enabled (since
	 *     we MUST do the Client Hello extensions to make these ciphers
	 *     work reliably)
	 */
	SSLCipherSpec *dst = ctx->validCipherSpecs;
	const SSLCipherSpec *src = KnownCipherSpecs;
	
	bool trimECDSA = false;
	if((ctx->protocolSide == SSL_ServerSide) && !SSL_ECDSA_SERVER) {
		trimECDSA = true;
	}
	if(ctx->versionSsl2Enable || !ctx->versionTls1Enable) {
		trimECDSA = true;
	}
	
	for(dex=0; dex<CipherSpecCount; dex++) {
		/* First skip ECDSA ciphers as appropriate */
		switch(src->keyExchangeMethod) {
			case SSL_ECDH_ECDSA:
			case SSL_ECDHE_ECDSA:
			case SSL_ECDH_RSA:
			case SSL_ECDHE_RSA:
			case SSL_ECDH_anon:
				if(trimECDSA) {
					/* Skip this one */
					ctx->numValidCipherSpecs--;
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
			if(src->cipher == &SSLCipherNull) {
				/* skip this one */
				ctx->numValidCipherSpecs--;
				src++;
				continue;
			}
			switch(src->keyExchangeMethod) {
				case SSL_DH_anon:
				case SSL_DH_anon_EXPORT:
				case SSL_ECDH_anon:
					/* skip this one */
					ctx->numValidCipherSpecs--;
					src++;
					continue;
				default:
					break;
			}
		}
		
		/* This one is good to go */
		*dst++ = *src++;
	}
	sslAnalyzeCipherSpecs(ctx);
	return noErr;
}

/*
 * Convert an array of SSLCipherSpecs (which is either KnownCipherSpecs or
 * ctx->validCipherSpecs) to an array of SSLCipherSuites.
 */
static OSStatus
cipherSpecsToCipherSuites(
	UInt32				numCipherSpecs,	/* size of cipherSpecs */
	const SSLCipherSpec	*cipherSpecs,
	SSLCipherSuite		*ciphers,		/* RETURNED */
	size_t				*numCiphers)	/* IN/OUT */
{
	unsigned dex;
	
	if(*numCiphers < numCipherSpecs) {
		return errSSLBufferOverflow;
	}
	for(dex=0; dex<numCipherSpecs; dex++) {
		ciphers[dex] = cipherSpecs[dex].cipherSpec;
	}
	*numCiphers = numCipherSpecs;
	return noErr;
}

/***
 *** Publically exported functions declared in SecureTransport.h
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
	*numCiphers = CipherSpecCount;
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
	return cipherSpecsToCipherSuites(CipherSpecCount,
		KnownCipherSpecs,
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
	unsigned 		size;
	unsigned 		callerDex;
	unsigned		tableDex;
	
	if((ctx == NULL) || (ciphers == NULL) || (numCiphers == 0)) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	size = numCiphers * sizeof(SSLCipherSpec);
	ctx->validCipherSpecs = (SSLCipherSpec *)sslMalloc(size);
	if(ctx->validCipherSpecs == NULL) {
		ctx->numValidCipherSpecs = 0;
		return memFullErr;
	}

	/* 
	 * Run thru caller's specs, finding a matching SSLCipherSpec for each one.
	 * If caller specifies one we don't know about, abort. 
	 */
	for(callerDex=0; callerDex<numCiphers; callerDex++) {
		/* find matching CipherSpec in our known table */
		int foundOne = 0;
		for(tableDex=0; tableDex<CipherSpecCount; tableDex++) {
			if(ciphers[callerDex] == KnownCipherSpecs[tableDex].cipherSpec) {
				ctx->validCipherSpecs[callerDex] = KnownCipherSpecs[tableDex];
				foundOne = 1;
				break;
			}
		}
		if(!foundOne) {
			/* caller specified one we don't implement */
			sslFree(ctx->validCipherSpecs);
			ctx->validCipherSpecs = NULL;
			return errSSLBadCipherSuite;
		}
	}
	
	/* success */
	ctx->numValidCipherSpecs = numCiphers;
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
	if(ctx->validCipherSpecs == NULL) {
		/* hasn't been set; use default */
		*numCiphers = CipherSpecCount;
	}
	else {
		/* caller set via SSLSetEnabledCiphers */
		*numCiphers = ctx->numValidCipherSpecs;
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
	if(ctx->validCipherSpecs == NULL) {
		/* hasn't been set; use default */
		return cipherSpecsToCipherSuites(CipherSpecCount,
			KnownCipherSpecs,
			ciphers,
			numCiphers);
	}
	else {
		/* use the ones specified in SSLSetEnabledCiphers() */
		return cipherSpecsToCipherSuites(ctx->numValidCipherSpecs,
			ctx->validCipherSpecs,
			ciphers,
			numCiphers);
	}
}

/***
 *** End of publically exported functions declared in SecureTransport.h
 ***/

/*
 * Given a valid ctx->selectedCipher and ctx->validCipherSpecs, set
 * ctx->selectedCipherSpec as appropriate. 
 */
OSStatus
FindCipherSpec(SSLContext *ctx)
{   

	unsigned i;
    
    assert(ctx != NULL);
    assert(ctx->validCipherSpecs != NULL);
    
    ctx->selectedCipherSpec = NULL;
    for (i=0; i<ctx->numValidCipherSpecs; i++)
    {   if (ctx->validCipherSpecs[i].cipherSpec == ctx->selectedCipher) {
        	ctx->selectedCipherSpec = &ctx->validCipherSpecs[i];
            break;
        }
    }    
    if (ctx->selectedCipherSpec == NULL)         /* Not found */
        return errSSLNegotiation;
		
	/* make sure we're configured to handle this one */
	return sslVerifyNegotiatedCipher(ctx);
}

