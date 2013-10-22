/*
 * Copyright (c) 1999-2001,2005-2011 Apple Inc. All Rights Reserved.
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

#include "sslBuildFlags.h"
#include "CipherSuite.h"
#include "sslContext.h"
#include "sslCipherSpecs.h"
#include "sslDebug.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include "sslUtils.h"
#include "sslPriv.h"
#include "sslCrypto.h"

#include <string.h>
#include <assert.h>
#include <Security/SecBase.h>
#include <utilities/array_size.h>

#include <TargetConditionals.h>


#define ENABLE_RSA_DES_SHA_NONEXPORT		ENABLE_DES
#define ENABLE_RSA_DES_MD5_NONEXPORT		ENABLE_DES
#define ENABLE_RSA_DES_SHA_EXPORT		ENABLE_DES
#define ENABLE_RSA_RC4_MD5_EXPORT		ENABLE_RC4	/* the most common one */
#define ENABLE_RSA_RC4_MD5_NONEXPORT		ENABLE_RC4
#define ENABLE_RSA_RC4_SHA_NONEXPORT		ENABLE_RC4
#define ENABLE_RSA_RC2_MD5_EXPORT		ENABLE_RC2
#define ENABLE_RSA_RC2_MD5_NONEXPORT		ENABLE_RC2
#define ENABLE_RSA_3DES_SHA			ENABLE_3DES
#define ENABLE_RSA_3DES_MD5			ENABLE_3DES

#define ENABLE_ECDH      		1
#define ENABLE_AES_GCM                  0

#define ENABLE_PSK                      1

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

/*
 * List of all CipherSpecs we implement. Depending on a context's
 * exportable flag, not all of these might be available for use.
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
    TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
    TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
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
#if ENABLE_RC2
    SSL_RSA_WITH_RC2_CBC_MD5,
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
#endif
#if ENABLE_DH_EPHEM_DSA
    SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA,
#if ENABLE_DES
    SSL_DHE_DSS_WITH_DES_CBC_SHA,
#endif
#endif
#if ENABLE_AES_GCM
    TLS_DH_anon_WITH_AES_256_GCM_SHA384,
    TLS_DH_anon_WITH_AES_128_GCM_SHA256,
#endif
    TLS_DH_anon_WITH_AES_128_CBC_SHA256,
    TLS_DH_anon_WITH_AES_256_CBC_SHA256,
    TLS_DH_anon_WITH_AES_128_CBC_SHA,
    TLS_DH_anon_WITH_AES_256_CBC_SHA,
    SSL_DH_anon_WITH_RC4_128_MD5,
    SSL_DH_anon_WITH_3DES_EDE_CBC_SHA,
#if ENABLE_DES
    SSL_DH_anon_WITH_DES_CBC_SHA,
#endif
    TLS_ECDHE_ECDSA_WITH_NULL_SHA,
    TLS_ECDHE_RSA_WITH_NULL_SHA,
#if ENABLE_ECDH
    TLS_ECDH_ECDSA_WITH_NULL_SHA,
    TLS_ECDH_RSA_WITH_NULL_SHA,
#endif

#if ENABLE_PSK
    TLS_PSK_WITH_AES_256_CBC_SHA384,
    TLS_PSK_WITH_AES_128_CBC_SHA256,
    TLS_PSK_WITH_AES_256_CBC_SHA,
    TLS_PSK_WITH_AES_128_CBC_SHA,
    TLS_PSK_WITH_RC4_128_SHA,
    TLS_PSK_WITH_3DES_EDE_CBC_SHA,
    TLS_PSK_WITH_NULL_SHA384,
    TLS_PSK_WITH_NULL_SHA256,
    TLS_PSK_WITH_NULL_SHA,
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

static const unsigned CipherSuiteCount = array_size(KnownCipherSuites);


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
	size = CipherSuiteCount * sizeof(SSLCipherSuite);
	ctx->validCipherSuites = (SSLCipherSuite *)sslMalloc(size);
	if(ctx->validCipherSuites == NULL) {
		ctx->numValidCipherSuites = 0;
		return errSecAllocate;
	}

	/*
	 * Trim out inappropriate ciphers:
	 *  -- trim anonymous ciphers if !ctx->anonCipherEnable
	 *  -- trim ECDSA ciphers for server side if appropriate
	 *  -- trim ECDSA ciphers if TLSv1 disable or SSLv2 enabled (since
	 *     we MUST do the Client Hello extensions to make these ciphers
	 *     work reliably)
         *  -- trim Stream ciphers if DTLSv1 enable
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

    /* trim Stream Ciphers for DTLS */
    bool trimRC4 = ctx->isDTLS;

    bool trimDHE = (ctx->protocolSide == kSSLServerSide) &&
        !ctx->dhParamsEncoded.length;

	for(dex=0; dex<CipherSuiteCount; dex++) {
        KeyExchangeMethod kem = sslCipherSuiteGetKeyExchangeMethod(*src);
        uint8_t keySize = sslCipherSuiteGetSymmetricCipherKeySize(*src);
        HMAC_Algs mac = sslCipherSuiteGetMacAlgorithm(*src);
        SSL_CipherAlgorithm cipher = sslCipherSuiteGetSymmetricCipherAlgorithm(*src);
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
			/* trim out the anonymous (and null-auth-cipher) ciphers */
			if(mac == HA_Null) {
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
        if(ctx->falseStartEnabled) {
            switch(kem){
                case SSL_ECDHE_ECDSA:
                case SSL_ECDHE_RSA:
                case SSL_DHE_RSA:
                case SSL_DHE_DSS:
                    /* Ok for false start */
                    break;
                default:
					/* Not ok, skip */
					ctx->numValidCipherSuites--;
					src++;
					continue;
            }
            switch(cipher) {
                case SSL_CipherAlgorithmAES_128_CBC:
                case SSL_CipherAlgorithmAES_128_GCM:
                case SSL_CipherAlgorithmAES_256_CBC:
                case SSL_CipherAlgorithmAES_256_GCM:
                case SSL_CipherAlgorithmRC4_128:
                    /* Ok for false start */
                    break;
                default:
					/* Not ok, skip*/
					ctx->numValidCipherSuites--;
					src++;
					continue;
            }
        }

        /* This will skip the simple DES cipher suites, but not the NULL cipher ones */
        if (keySize == 8)
        {
            /* skip this one */
            ctx->numValidCipherSuites--;
            src++;
            continue;
        }

        /* Trim PSK ciphersuites, they need to be enabled explicitely */
        if (kem==TLS_PSK) {
            ctx->numValidCipherSuites--;
            src++;
            continue;
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

        if (trimRC4 && (cipher==SSL_CipherAlgorithmRC4_128)) {
            ctx->numValidCipherSuites--;
            src++;
            continue;
        }

        if(cipher==SSL_CipherAlgorithmNull) {
            ctx->numValidCipherSuites--;
            src++;
            continue;
        }

        /* This one is good to go */
        *dst++ = *src++;
	}
	sslAnalyzeCipherSpecs(ctx);
	return errSecSuccess;
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
    memcpy(ciphers, cipherSuites, numCipherSuites * sizeof(SSLCipherSuite));
	*numCiphers = numCipherSuites;
	return errSecSuccess;
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
		return errSecParam;
	}
	*numCiphers = CipherSuiteCount;
	return errSecSuccess;
}

OSStatus
SSLGetSupportedCiphers		 (SSLContextRef		ctx,
							  SSLCipherSuite	*ciphers,		/* RETURNED */
							  size_t			*numCiphers)	/* IN/OUT */
{
	if((ctx == NULL) || (ciphers == NULL) || (numCiphers == NULL)) {
		return errSecParam;
	}
	return cipherSuitesToCipherSuites(CipherSuiteCount,
		KnownCipherSuites,
		ciphers,
		numCiphers);
}

/*
 * Specify a (typically) restricted set of SSLCipherSuites to be enabled by
 * the current SSLContext. Can only be called when no session is active. Default
 * set of enabled SSLCipherSuites is NOT the same as the complete set of supported
 * SSLCipherSuites as obtained by SSLGetSupportedCiphers().
 */
OSStatus
SSLSetEnabledCiphers		(SSLContextRef			ctx,
							 const SSLCipherSuite	*ciphers,
							 size_t					numCiphers)
{
	size_t size;
    size_t foundCiphers=0;
	unsigned callerDex;
	unsigned tableDex;

	if((ctx == NULL) || (ciphers == NULL) || (numCiphers == 0)) {
		return errSecParam;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return errSecBadReq;
	}
	size = numCiphers * sizeof(SSLCipherSuite);
	ctx->validCipherSuites = (SSLCipherSuite *)sslMalloc(size);
	if(ctx->validCipherSuites == NULL) {
		ctx->numValidCipherSuites = 0;
		return errSecAllocate;
	}

	/*
	 * Run thru caller's specs, keep only the supported ones.
	 */
    for(callerDex=0; callerDex<numCiphers; callerDex++) {
        /* find matching CipherSpec in our known table */
        for(tableDex=0; tableDex<CipherSuiteCount; tableDex++) {
            if(ciphers[callerDex] == KnownCipherSuites[tableDex]) {
                ctx->validCipherSuites[foundCiphers] = KnownCipherSuites[tableDex];
                foundCiphers++;
                break;
            }
        }
	}

    if(foundCiphers==0) {
        /* caller specified only unsupported ciphersuites */
        sslFree(ctx->validCipherSuites);
        ctx->validCipherSuites = NULL;
        return errSSLBadCipherSuite;
    }
    
	/* success */
	ctx->numValidCipherSuites = foundCiphers;
	sslAnalyzeCipherSpecs(ctx);
	return errSecSuccess;
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
		return errSecParam;
	}
	if(ctx->validCipherSuites == NULL) {
		/* hasn't been set; use default */
		*numCiphers = CipherSuiteCount;
	}
	else {
		/* caller set via SSLSetEnabledCiphers */
		*numCiphers = ctx->numValidCipherSuites;
	}
	return errSecSuccess;
}

OSStatus
SSLGetEnabledCiphers		(SSLContextRef			ctx,
							 SSLCipherSuite			*ciphers,		/* RETURNED */
							 size_t					*numCiphers)	/* IN/OUT */
{
	if((ctx == NULL) || (ciphers == NULL) || (numCiphers == NULL)) {
		return errSecParam;
	}
	if(ctx->validCipherSuites == NULL) {
		/* hasn't been set; use default */
		return cipherSuitesToCipherSuites(CipherSuiteCount,
			KnownCipherSuites,
			ciphers,
			numCiphers);
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

void InitCipherSpecParams(SSLContext *ctx)
{
    SSLCipherSpecParams *dst = &ctx->selectedCipherSpecParams;
    dst->cipherSpec = ctx->selectedCipher;
    dst->macSize = sslCipherSuiteGetMacSize(ctx->selectedCipher);
    dst->macAlg = sslCipherSuiteGetMacAlgorithm(ctx->selectedCipher);
    dst->keySize = sslCipherSuiteGetSymmetricCipherKeySize(ctx->selectedCipher);
    dst->blockSize = sslCipherSuiteGetSymmetricCipherBlockIvSize(ctx->selectedCipher);
    dst->ivSize = dst->blockSize;
    dst->keyExchangeMethod = sslCipherSuiteGetKeyExchangeMethod(ctx->selectedCipher);
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
            InitCipherSpecParams(ctx);
            /* Make sure we're configured to handle this cipherSuite. */
            return sslVerifySelectedCipher(ctx);
        }
    }
    /* Not found */
    return errSSLNegotiation;
}
