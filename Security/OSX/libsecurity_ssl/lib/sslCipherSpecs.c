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

#include "sslBuildFlags.h"
#include "sslContext.h"
#include "sslCipherSpecs.h"
#include "sslDebug.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include "sslPriv.h"

#include <tls_handshake.h>

#include <string.h>
#include <assert.h>
#include <Security/SecBase.h>

#include <TargetConditionals.h>


/* SecureTransport needs it's own copy of KnownCipherSuites for now, there is a copy in coreTLS,
   that is exported, but it actually should only included the "default" not the supported */

#define ENABLE_ECDH      		1
#define ENABLE_AES_GCM          1
#define ENABLE_PSK              1

static const uint16_t STKnownCipherSuites[] = {
#if ENABLE_AES_GCM
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
#endif
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
#if ENABLE_AES_GCM
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
#endif
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
#if ENABLE_ECDH
#if ENABLE_AES_GCM
    TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256,
#endif
    TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
#if ENABLE_AES_GCM
    TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256,
#endif
    TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA,
#endif

#if ENABLE_AES_GCM
    TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
#endif // ENABLE_AES_GCM
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,

#if ENABLE_AES_GCM
    TLS_RSA_WITH_AES_256_GCM_SHA384,
    TLS_RSA_WITH_AES_128_GCM_SHA256,
#endif
    TLS_RSA_WITH_AES_256_CBC_SHA256,
    TLS_RSA_WITH_AES_128_CBC_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA,
    TLS_RSA_WITH_AES_128_CBC_SHA,
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,

#if ENABLE_RC4
    TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
    TLS_ECDHE_RSA_WITH_RC4_128_SHA,
    TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
    TLS_ECDH_RSA_WITH_RC4_128_SHA,
    SSL_RSA_WITH_RC4_128_SHA,
    SSL_RSA_WITH_RC4_128_MD5,
#endif


    /* Unsafe ciphersuites */

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

};

static const unsigned STCipherSuiteCount = sizeof(STKnownCipherSuites)/sizeof(STKnownCipherSuites[0]);


/*
 * Convert an array of uint16_t
 * to an array of SSLCipherSuites.
 */
static OSStatus
cipherSuitesToCipherSuites(
                          size_t				numCipherSuites,
                          const uint16_t        *cipherSuites,
                          SSLCipherSuite		*ciphers,		/* RETURNED */
                          size_t				*numCiphers)	/* IN/OUT */
{
    size_t i;
	if(*numCiphers < numCipherSuites) {
		return errSSLBufferOverflow;
	}

    /* NOTE: this is required to go from uint16_t to SSLCipherSuite 
       which is either 32 or 16 bits, depending on the platform */
    for(i=0;i<numCipherSuites; i++) {
        ciphers[i]=cipherSuites[i];
    }
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
	*numCiphers = STCipherSuiteCount;
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
	return cipherSuitesToCipherSuites(STCipherSuiteCount,
		STKnownCipherSuites,
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
    uint16_t *cs;

	if((ctx == NULL) || (ciphers == NULL) || (numCiphers == 0)) {
		return errSecParam;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return errSecBadReq;
	}

    cs = (uint16_t *)sslMalloc(numCiphers * sizeof(uint16_t));
    if(cs == NULL) {
		return errSecAllocate;
	}

    for(int i=0; i<numCiphers; i++)
    {
        cs[i] = ciphers[i];
	}

    tls_handshake_set_ciphersuites(ctx->hdsk, cs, (unsigned) numCiphers);

    sslFree(cs);

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

    unsigned n;
    const uint16_t *ciphersuites;
    int err;

    err = tls_handshake_get_ciphersuites(ctx->hdsk, &ciphersuites, &n);

    if(err) {
        return err;
    } else {
        *numCiphers = n;
        return errSecSuccess;
    }
}

OSStatus
SSLGetEnabledCiphers		(SSLContextRef			ctx,
							 SSLCipherSuite			*ciphers,		/* RETURNED */
							 size_t					*numCiphers)	/* IN/OUT */
{
	if((ctx == NULL) || (ciphers == NULL) || (numCiphers == NULL)) {
		return errSecParam;
	}

    unsigned n;
    const uint16_t *ciphersuites;
    int err;

    err = tls_handshake_get_ciphersuites(ctx->hdsk, &ciphersuites, &n);

    if(err) {
        return err;
    } else {
        return cipherSuitesToCipherSuites(n,
                                          ciphersuites,
                                          ciphers,
                                          numCiphers);
    }
}
