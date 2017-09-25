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
#include "tls_handshake_priv.h"

#include <string.h>
#include <assert.h>

#include <TargetConditionals.h>

#define ENABLE_AES_GCM          1
#define ENABLE_PSK              1

/*
    This list is exported and used by a couple project, don't change it.
    Internally, use AllCipherSuites instead.
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

/* SigAlgs we support: */
const tls_signature_and_hash_algorithm KnownSigAlgs[] = {
    {tls_hash_algorithm_SHA256, tls_signature_algorithm_RSA},
    {tls_hash_algorithm_SHA1,   tls_signature_algorithm_RSA},
    {tls_hash_algorithm_SHA384, tls_signature_algorithm_RSA},
    {tls_hash_algorithm_SHA512, tls_signature_algorithm_RSA},
    {tls_hash_algorithm_SHA256, tls_signature_algorithm_ECDSA},
    {tls_hash_algorithm_SHA1,   tls_signature_algorithm_ECDSA},
    {tls_hash_algorithm_SHA384, tls_signature_algorithm_ECDSA},
    {tls_hash_algorithm_SHA512, tls_signature_algorithm_ECDSA},
};
const unsigned SigAlgsCount = sizeof(KnownSigAlgs)/sizeof(KnownSigAlgs[0]);

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
			case SSL_ECDHE_ECDSA:
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
            return ((ctx->signingPrivKeyRef) && (ctx->signingPrivKeyRef->desc.type == tls_private_key_type_rsa));
            break;
        case SSL_ECDHE_ECDSA:
            return ((ctx->signingPrivKeyRef) && (ctx->signingPrivKeyRef->desc.type == tls_private_key_type_ecdsa));
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
        case SSL_DHE_RSA:
        case SSL_DH_anon:
        case TLS_PSK:
        case SSL_ECDHE_ECDSA:
        case SSL_ECDHE_RSA:
        case SSL_ECDH_anon:
            return true;
        default:
            return false;
    }
}


static
bool tls_handshake_kem_is_allowed(tls_handshake_config_t config, KeyExchangeMethod kem)
{
    switch(config) {
        case tls_handshake_config_none:
        case tls_handshake_config_ATSv2:
            return true;
        case tls_handshake_config_ATSv1:
            return (kem == SSL_ECDHE_ECDSA || kem == SSL_ECDHE_RSA);
        case tls_handshake_config_legacy_DHE:
            return (kem==SSL_RSA || kem == SSL_DHE_RSA || kem == SSL_ECDHE_ECDSA || kem == SSL_ECDHE_RSA);
        case tls_handshake_config_ATSv1_noPFS:
        case tls_handshake_config_default:
        case tls_handshake_config_standard:
        case tls_handshake_config_RC4_fallback:
        case tls_handshake_config_TLSv1_fallback:
        case tls_handshake_config_TLSv1_RC4_fallback:
        case tls_handshake_config_legacy:
        case tls_handshake_config_3DES_fallback:
        case tls_handshake_config_TLSv1_3DES_fallback:
            return (kem==SSL_RSA || kem == SSL_ECDHE_ECDSA || kem == SSL_ECDHE_RSA);
        case tls_handshake_config_standard_TLSv3:
            return (kem == SSL_ECDHE_ECDSA);
        case tls_handshake_config_anonymous:
            return (kem==SSL_ECDH_anon || kem == SSL_DH_anon);
    }

    /* Note: we do this here instead of a 'default:' case, so that the compiler will warn us when
     adding new config in the enum */
    return (kem==SSL_RSA || kem == SSL_ECDHE_ECDSA || kem == SSL_ECDHE_RSA);
}

static
bool tls_handshake_kem_is_valid(tls_handshake_t ctx, KeyExchangeMethod kem)
{
    switch(kem) {
        case SSL_RSA:
        case SSL_DH_RSA:
        case SSL_DHE_RSA:
        case SSL_DH_anon:
        case TLS_PSK:
            return true;
        case SSL_ECDHE_ECDSA:
        case SSL_ECDHE_RSA:
        case SSL_ECDH_anon:
            return ctx->maxProtocolVersion!=tls_protocol_version_SSL_3; // EC ciphersuites not valid for SSLv3
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
bool tls_handshake_sym_is_allowed(tls_handshake_config_t config, SSL_CipherAlgorithm sym)
{
    switch(config) {
        case tls_handshake_config_none:
            return true;
        case tls_handshake_config_ATSv2:
            return (sym>=SSL_CipherAlgorithmAES_128_GCM);
        case tls_handshake_config_ATSv1:
        case tls_handshake_config_ATSv1_noPFS:
        case tls_handshake_config_anonymous:
        case tls_handshake_config_standard:
            return (sym>=SSL_CipherAlgorithmAES_128_CBC && sym != SSL_CipherAlgorithmChaCha20_Poly1305);
        case tls_handshake_config_standard_TLSv3:
            return (sym>=SSL_CipherAlgorithmAES_128_CBC);
        case tls_handshake_config_default:
        case tls_handshake_config_TLSv1_fallback:
        case tls_handshake_config_3DES_fallback:
        case tls_handshake_config_TLSv1_3DES_fallback:
            return (sym>=SSL_CipherAlgorithm3DES_CBC && sym != SSL_CipherAlgorithmChaCha20_Poly1305);
        case tls_handshake_config_legacy:
        case tls_handshake_config_RC4_fallback:
        case tls_handshake_config_TLSv1_RC4_fallback:
        case tls_handshake_config_legacy_DHE:
            return ((sym==SSL_CipherAlgorithmRC4_128) || (sym>=SSL_CipherAlgorithm3DES_CBC))
                && sym != SSL_CipherAlgorithmChaCha20_Poly1305;
    }

    /* Note: we do this here instead of a 'default:' case, so that the compiler will warn us when
     adding new config in the enum */
    return (sym>=SSL_CipherAlgorithm3DES_CBC);
}

static
bool tls_handshake_sym_is_valid(tls_handshake_t ctx, SSL_CipherAlgorithm sym)
{
    switch (sym) {
        case SSL_CipherAlgorithmNull:
        case SSL_CipherAlgorithmRC4_128:
            return true;
        case SSL_CipherAlgorithmAES_128_CBC:
        case SSL_CipherAlgorithm3DES_CBC:
        case SSL_CipherAlgorithmAES_256_CBC:
            return !(ctx->maxProtocolVersion==tls_protocol_version_SSL_3 && ctx->fallback);
        case SSL_CipherAlgorithmAES_256_GCM:
        case SSL_CipherAlgorithmAES_128_GCM:
            return ctx->maxProtocolVersion==tls_protocol_version_TLS_1_2;
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

static
bool tls_handshake_mac_is_allowed(tls_handshake_config_t config, HMAC_Algs mac)
{
    switch(config) {
        case tls_handshake_config_none:
            return true;
        case tls_handshake_config_standard_TLSv3:
        case tls_handshake_config_ATSv1:
        case tls_handshake_config_ATSv1_noPFS:
        case tls_handshake_config_ATSv2:
        case tls_handshake_config_anonymous:
            return (mac>=HA_SHA1);
        case tls_handshake_config_default:
        case tls_handshake_config_legacy:
        case tls_handshake_config_RC4_fallback:
        case tls_handshake_config_TLSv1_RC4_fallback:
        case tls_handshake_config_standard:
        case tls_handshake_config_TLSv1_fallback:
        case tls_handshake_config_legacy_DHE:
        case tls_handshake_config_3DES_fallback:
        case tls_handshake_config_TLSv1_3DES_fallback:
            return (mac>=HA_MD5);
    }

    /* Note: we do this here instead of a 'default:' case, so that the compiler will warn us when
     adding new config in the enum */
    return (mac>=HA_MD5);
}

static
bool tls_handshake_mac_is_valid(tls_handshake_t ctx, HMAC_Algs mac)
{
    /* All MACs are always valid */
    return true;
}

/* Do we support this ciphersuites ? */
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

/* Is this ciphersuite allowed in this configuration */
bool tls_handshake_ciphersuite_is_allowed(tls_handshake_config_t config, uint16_t ciphersuite)
{
    uint16_t cs = ciphersuite;

    KeyExchangeMethod kem = sslCipherSuiteGetKeyExchangeMethod(cs);
    SSL_CipherAlgorithm sym = sslCipherSuiteGetSymmetricCipherAlgorithm(cs);
    HMAC_Algs mac = sslCipherSuiteGetMacAlgorithm(cs);

    return tls_handshake_kem_is_allowed(config,kem)
            && tls_handshake_sym_is_allowed(config, sym)
            && tls_handshake_mac_is_allowed(config, mac);
}

/* Is this ciphersuite valid for the current context */
bool tls_handshake_ciphersuite_is_valid(tls_handshake_t ctx, uint16_t ciphersuite)
{
    uint16_t cs = ciphersuite;

    KeyExchangeMethod kem = sslCipherSuiteGetKeyExchangeMethod(cs);
    SSL_CipherAlgorithm sym = sslCipherSuiteGetSymmetricCipherAlgorithm(cs);
    HMAC_Algs mac = sslCipherSuiteGetMacAlgorithm(cs);

    return tls_handshake_kem_is_valid(ctx,kem)
            && tls_handshake_sym_is_valid(ctx, sym)
            && tls_handshake_mac_is_valid(ctx, mac);
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

bool tls_handshake_sigalg_is_supported(tls_signature_and_hash_algorithm sigalg)
{

    bool hash_supported;
    bool sig_supported;

    switch(sigalg.hash) {
        case tls_hash_algorithm_SHA1:
        case tls_hash_algorithm_SHA256:
        case tls_hash_algorithm_SHA384:
        case tls_hash_algorithm_SHA512:
            hash_supported = true;
            break;
        default:
            hash_supported = false;
    }

    switch(sigalg.signature) {
        case tls_signature_algorithm_RSA:
        case tls_signature_algorithm_ECDSA:
            sig_supported = true;
            break;
        default:
            sig_supported = false;
    }

    return sig_supported && hash_supported;
}
