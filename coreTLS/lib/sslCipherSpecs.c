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
#include "sslCipherSpecs.h"
#include "sslDebug.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include "sslUtils.h"
#include <tls_handshake_priv.h>

#include <string.h>
#include <assert.h>

#include <TargetConditionals.h>

#define ENABLE_ECDH      		1
#define ENABLE_AES_GCM          1
#define ENABLE_PSK              1

/*
    List of default CipherSuites we implement.

    Order by preference, PFS first, more security first

    Ordered by:
    Key Exchange first: ECDHE_ECDSA, ECDHE_RSA, ECDH_ECDSA, ECDH_RSA, DHE_RSA, RSA
    then by hash algorithm: SHA384, SHA256, SHA
    then by symmetric cipher: AES_256_GCM, AES_128_GCM, AES_256_CBC, AES_128_CBC, 3DES

    All RC4 ciphersuites are relegated at the end. They are likely to be soon deprecated by the IETF TLS WG.
    NULL ciphers, AnonDH ciphers, and PSK ciphers are not in this list and need to be enabled explicetely.
    The list is filtered based on server and dtls support if necessary.
*/

const uint16_t KnownCipherSuites[] = {
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


};

const unsigned CipherSuiteCount = sizeof(KnownCipherSuites)/sizeof(KnownCipherSuites[0]);


/* EC Curves we support: */
const uint16_t KnownCurves[] = {
    tls_curve_secp256r1,
    tls_curve_secp384r1,
    tls_curve_secp521r1
};

const unsigned CurvesCount = sizeof(KnownCurves)/sizeof(KnownCurves[0]);

/*
 * Given a valid ctx->validCipherSpecs array, calculate how many of those
 * cipherSpecs are *not* SSLv2 only, storing result in
 * ctx->numValidNonSSLv2Specs. ClientHello routines need this to set
 * up outgoing cipherSpecs arrays correctly.
 *
 * Also determines if any ECDSA/ECDH ciphers are enabled; we need to know
 * that when creating a hello message.
 */
void sslAnalyzeCipherSpecs(tls_handshake_t ctx)
{
	unsigned 		dex;
	const uint16_t  *cipherSuite;

	cipherSuite = &ctx->enabledCipherSuites[0];
	ctx->ecdsaEnable = false;
	for(dex=0; dex<ctx->numEnabledCipherSuites; dex++, cipherSuite++) {
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

/***
 *** End of publically exported functions declared in SecureTransport.h
 ***/

void InitCipherSpecParams(tls_handshake_t ctx)
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


bool cipherSuiteInSet(uint16_t cs, uint16_t *ciphersuites, size_t numCiphersuites)
{
    size_t i;

    for(i=0; i<numCiphersuites; i++) {
        if(ciphersuites[i]==cs)
            return true;
    }

    return false;
}


/* verify that a ciphersuite is valid for this server config */
/* Currently, only check that the server identity is proper */
static bool
verifyCipherSuite(tls_handshake_t ctx, uint16_t cs)
{
    KeyExchangeMethod kem = sslCipherSuiteGetKeyExchangeMethod(cs);

    switch (kem) {
        case SSL_RSA:
        case SSL_DHE_RSA:
        case SSL_ECDHE_RSA:
            return (ctx->signingPrivKeyRef->type == kSSLPrivKeyType_RSA);
            break;
        case SSL_ECDHE_ECDSA:
            return (ctx->signingPrivKeyRef->type == kSSLPrivKeyType_ECDSA);
            break;
        /* Other key exchange don't care about certificate key */
        default:
            return true;
    }
}


/* Server routine to select ciphersuite during anew handshake */
int
SelectNewCiphersuite(tls_handshake_t ctx)
{
    int i;

    assert(ctx->isServer);

    for (i=0; i<ctx->numEnabledCipherSuites; i++) {
        if(cipherSuiteInSet(ctx->enabledCipherSuites[i], ctx->requestedCipherSuites, ctx->numRequestedCipherSuites) &&
           verifyCipherSuite(ctx, ctx->enabledCipherSuites[i]))
        {
            ctx->selectedCipher = ctx->enabledCipherSuites[i];
            sslLogNegotiateDebug("TLS server: selected ciphersuite 0x%04x", (unsigned)ctx->selectedCipher);
            InitCipherSpecParams(ctx);
            return 0;
        }
    }

    return errSSLNegotiation;
}


/* Client Routine to validate the selected ciphersuite */
int
ValidateSelectedCiphersuite(tls_handshake_t ctx)
{
	unsigned i;

    assert(!ctx->isServer);
    assert(ctx->selectedCipher != 0);

    for (i=0; i<ctx->numEnabledCipherSuites; i++)
    {
        if (ctx->enabledCipherSuites[i] == ctx->selectedCipher) {
            InitCipherSpecParams(ctx);
            /* Make sure we're configured to handle this cipherSuite. */
            return errSSLSuccess; //sslVerifySelectedCipher(ctx);
        }
    }
    /* Not found */
    return errSSLNegotiation;
}

//
// MARK : Supported ciphersuites helpers
//

static
bool tls_handshake_kem_is_supported(bool server, KeyExchangeMethod kem)
{
    switch(kem) {
        case SSL_RSA:
        case SSL_DH_RSA:
        case SSL_DHE_RSA:
        case SSL_DH_anon:
        case TLS_PSK:
        case SSL_ECDHE_ECDSA:
        case SSL_ECDHE_RSA:
            return true;
        case SSL_ECDH_ECDSA:
        case SSL_ECDH_RSA:
        case SSL_ECDH_anon:
            return (!server); // Only supported on the client side
        default:
            return false;
    }
}

static
bool tls_handshake_sym_is_supported(bool dtls, SSL_CipherAlgorithm sym)
{
    switch (sym) {
        case SSL_CipherAlgorithmNull:
        case SSL_CipherAlgorithmAES_128_CBC:
        case SSL_CipherAlgorithm3DES_CBC:
        case SSL_CipherAlgorithmAES_256_CBC:
        case SSL_CipherAlgorithmAES_256_GCM:
        case SSL_CipherAlgorithmAES_128_GCM:
            return true;
        case SSL_CipherAlgorithmRC4_128:
            return !dtls;
        default:
            return false;
    }
}


static
bool tls_handshake_mac_is_supported(HMAC_Algs mac)
{
    switch (mac){
        case HA_Null:
        case HA_MD5:
        case HA_SHA1:
        case HA_SHA256:
        case HA_SHA384:
            return true;
        default:
            return false;
    }
}

bool tls_handshake_ciphersuite_is_supported(bool server, bool dtls, uint16_t ciphersuite)
{
    uint16_t cs = ciphersuite;

    KeyExchangeMethod kem = sslCipherSuiteGetKeyExchangeMethod(cs);
    SSL_CipherAlgorithm sym = sslCipherSuiteGetSymmetricCipherAlgorithm(cs);
    HMAC_Algs mac = sslCipherSuiteGetMacAlgorithm(cs);

    return tls_handshake_kem_is_supported(server, kem)
    && tls_handshake_sym_is_supported(dtls, sym)
    && tls_handshake_mac_is_supported(mac);
}

bool tls_handshake_curve_is_supported(uint16_t curve)
{
    switch(curve) {
        case tls_curve_secp256r1:
        case tls_curve_secp384r1:
        case tls_curve_secp521r1:
            return true;
        default:
            return false;
    }
}