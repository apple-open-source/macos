/*
 * Copyright (c) 2006-2013 Apple Inc. All Rights Reserved.
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
 * sslCrypto.c - internally implemented handshake layer crypto.
 */

#include "tls_handshake_priv.h"

#include "sslCrypto.h"
#include "CipherSuite.h"
#include "sslMemory.h"
#include "sslUtils.h"
#include "sslDebug.h"
#include <AssertMacros.h>

#include <string.h>
#include <assert.h>

#include <TargetConditionals.h>

#include <corecrypto/ccec.h>
#include <corecrypto/ccdh.h>
#include <corecrypto/ccrsa.h>

#include <stdlib.h>

/* extern struct ccrng_state *ccDRBGGetRngState(); */
#include <CommonCrypto/CommonRandomSPI.h>
#define CCRNGSTATE() ccDRBGGetRngState()
//#define CCRNGSTATE() ccDevRandomGetRngState()

#if APPLE_DH

int sslDecodeDhParams(
                      ccdh_const_gp_t params,			/* Input - corecrypto object */
                      tls_buffer		*prime,			/* Output - wire format */
                      tls_buffer		*generator)     /* Output - wire format */
{
    int ortn = errSSLSuccess;
    cc_size n = ccdh_gp_n(params);

    require_noerr(ortn=SSLAllocBuffer(prime, ccn_write_uint_size(n, ccdh_gp_prime(params))), errOut);
    require_noerr(ortn=SSLAllocBuffer(generator, ccn_write_uint_size(n, ccdh_gp_g(params))), errOut);

    ccn_write_uint(n, ccdh_gp_prime(params), prime->length, prime->data);
    ccn_write_uint(n, ccdh_gp_g(params), generator->length, generator->data);

    // TODO: might leaks on allocation failure.
errOut:
    return ortn;
}

int sslEncodeDhParams(ccdh_gp_t         *params,		/* data mallocd and RETURNED - corecrypto object */
                      const tls_buffer	*prime,			/* Wire format */
                      const tls_buffer	*generator)     /* Wire format */
{
    int ortn = errSSLAllocate;

    cc_size n = ccn_nof_size(prime->length);
    ccdh_gp_t gp;

    gp = sslMalloc(ccdh_gp_size(ccn_sizeof_n(n)));
    if(gp == NULL)
        return errSSLAllocate;
    bzero(gp, ccdh_gp_size(ccn_sizeof_n(n)));

    CCDH_GP_N(gp) = n;
    require_noerr(ortn=ccn_read_uint(n, CCDH_GP_PRIME(gp), prime->length, prime->data), errOut);
    cczp_init(CCDH_GP_ZP(gp));
    CCDH_GP_L(gp) = 0;
    require_noerr(ortn=ccn_read_uint(n, CCDH_GP_G(gp),generator->length, generator->data), errOut);

    *params = gp;
    return errSSLSuccess;

errOut:
    sslFree(gp);
    return ortn;

}

int sslDhCreateKey(ccdh_const_gp_t gp, ccdh_full_ctx_t *dhKey)
{
    ccdh_full_ctx_t dhContext = NULL;
    int ortn;

    dhContext = sslMalloc(ccdh_full_ctx_size(ccdh_ccn_size(gp)));
    if(!dhContext)
        return errSSLAllocate;

    struct ccrng_state *rng = CCRNGSTATE();
    if (!rng) {
        abort();
    }

    require_noerr((ortn=ccdh_generate_key(gp, rng, dhContext)), errOut);

    *dhKey = dhContext;
    return errSSLSuccess;

errOut:
    sslFree(dhContext);
    return ortn;
}

int sslDhExportPub(ccdh_full_ctx_t dhKey, tls_buffer *pubKey)
{
    int ortn = errSSLSuccess;

    size_t pub_size = ccdh_export_pub_size(ccdh_ctx_public(dhKey));

    require_noerr(ortn = SSLAllocBuffer(pubKey, pub_size), errOut);

    ccdh_export_pub(ccdh_ctx_public(dhKey), pubKey->data);

errOut:
    return ortn;
}

int sslDhKeyExchange(ccdh_full_ctx_t dhKey, const tls_buffer *dhPeerPublic, tls_buffer *preMasterSecret)
{
    int result = errSSLProtocol, rtn;
    cc_unit *r = NULL;
    size_t len;
    cc_size n;

    ccdh_const_gp_t gp = ccdh_ctx_gp(dhKey);
    ccdh_pub_ctx_decl_gp(gp, pubKey);
    require_noerr(ccdh_import_pub(gp, dhPeerPublic->length, dhPeerPublic->data, pubKey), errOut);

    n = ccdh_gp_n(gp);
    len = ccn_sizeof_n(n);
    r = sslMalloc(len);

    require_noerr_action(rtn = SSLAllocBuffer(preMasterSecret, len), errOut, result = rtn);
    require_noerr(ccdh_compute_key(dhKey, pubKey, r), errOut);

    ccn_write_uint(n, r, &preMasterSecret->length, preMasterSecret->data);
    size_t out_size = ccn_write_uint_size(n, r);
    if(out_size < preMasterSecret->length)
        preMasterSecret->length=out_size;

    sslDebugLog("sslEcdhKeyExchange: exchanged key length=%ld, data=%p\n",
                preMasterSecret->length, preMasterSecret->data);

    result = errSSLSuccess;

errOut:
    if (r) {
        memset(r, 0, len);
        sslFree(r);
    }
    if (result)
        sslErrorLog("sslDhKeyExchange: failed to compute key (error %d)\n", (int)result);
    return result;
}

#endif /* APPLE_DH */


/*
 * Generate ECDH key pair with the given tls_named_curve.
 */
int sslEcdhCreateKey(ccec_const_cp_t cp, ccec_full_ctx_t *ecdhKey)
{
    ccec_full_ctx_t ecdhContext = NULL;
    int ortn;

    ecdhContext = sslMalloc(ccec_full_ctx_size(ccec_ccn_size(cp)));
    if(!ecdhContext)
        return errSSLAllocate;

    struct ccrng_state *rng = CCRNGSTATE();
    if (!rng) {
        abort();
    }

    require_noerr((ortn=ccec_generate_key(cp, rng, ecdhContext)), errOut);

    *ecdhKey = ecdhContext;
    return errSSLSuccess;

errOut:
    return ortn;
}


int sslEcdhExportPub(ccec_full_ctx_t ecdhKey, tls_buffer *pubKey)
{
    int ortn = errSSLSuccess;

    size_t pub_size = ccec_export_pub_size(ccec_ctx_pub(ecdhKey));

    require_noerr(ortn = SSLAllocBuffer(pubKey, pub_size), errOut);
    ccec_export_pub(ccec_ctx_pub(ecdhKey), pubKey->data);

errOut:
    return ortn;
}

/*
 * Perform ECDH key exchange. Obtained key material is the same
 * size as our private key.
 *
 * On entry, ecdhPrivate is our private key. The peer's public key
 * is either ctx->ecdhPeerPublic for ECDHE exchange, or
 * ctx->peerPubKey for ECDH exchange.
 */

int sslEcdhKeyExchange(const ccec_full_ctx_t ecdhKey, const ccec_pub_ctx_t ecdhPeerPublic, tls_buffer *preMasterSecret)
{
    int ortn = errSSLSuccess;

    size_t len = 1 + 2 * ccec_ccn_size(ccec_ctx_cp(ecdhKey));

    ortn = SSLAllocBuffer(preMasterSecret, len);
    require_noerr(ortn, errOut);
    ortn = ccecdh_compute_shared_secret(ecdhKey, ecdhPeerPublic, &preMasterSecret->length, preMasterSecret->data, ccrng(NULL));
    require_noerr(ortn, errOut);

    sslDebugLog("sslEcdhKeyExchange: exchanged key length=%ld, data=%p\n",
                preMasterSecret->length, preMasterSecret->data);

errOut:
    return ortn;
}


int sslRand(tls_buffer *buf)
{
    if(buf->length == 0) {
        sslErrorLog("sslRand: zero buf->length\n");
        return 0;
    }

    struct ccrng_state *rng = CCRNGSTATE();
    if (!rng) {
        abort();
    }

    return ccrng_generate(rng, buf->length, buf->data);
}

/*
 * Free a pubKey object.
 */
int sslFreePubKey(SSLPubKey *pubKey)
{
    if (pubKey) {
        sslFree(pubKey->rsa);
        pubKey->rsa=NULL;
        sslFree(pubKey->ecc);
        pubKey->ecc = NULL;
    }
    return errSSLSuccess;
}


#if 0
/*
 * Get algorithm id for a SSLPubKey object.
 */
int sslPubKeyGetAlgorithmID(SSLPubKey *pubKey)
{
    pubKey->type;
}
#endif

/*
 * Raw RSA/DSA sign/verify.
 */
int sslRawSign(
               tls_private_key_t   privKey,
               const uint8_t       *plainText,
               size_t              plainTextLen,
               uint8_t				*sig,			// mallocd by caller; RETURNED
               size_t              sigLen,         // available
               size_t              *actualBytes)   // RETURNED
{
    size_t inOutSigLen = sigLen;

    assert(actualBytes != NULL);
    int status = errSSLParam;
    if (privKey->desc.type == tls_private_key_type_rsa) {
        status = privKey->desc.rsa.sign(privKey->ctx, tls_hash_algorithm_None, plainText, plainTextLen, sig, &inOutSigLen);
    } else if (privKey->desc.type == tls_private_key_type_ecdsa) {
        status = privKey->desc.ecdsa.sign(privKey->ctx, plainText, plainTextLen, sig, &inOutSigLen);
    } else {
        status = errSSLParam;
    }

    if (status) {
        sslErrorLog("privKey->desc.rsa.sign: failed (error %d)\n", (int)status);
    }

    /* Since the KeyExchange already allocated modulus size bytes we'll
     use all of them.  SecureTransport has always sent that many bytes,
     so we're not going to deviate, to avoid interoperability issues. */
    if (!status && (privKey->desc.type == tls_private_key_type_rsa) && (inOutSigLen < sigLen)) {
        size_t offset = sigLen - inOutSigLen;
        memmove(sig + offset, sig, inOutSigLen);
        memset(sig, 0, offset);
        inOutSigLen = sigLen;
    }


    *actualBytes = inOutSigLen;
    return status;
}


/* TLS 1.2 ECDSA signature */
int sslEcdsaSign(
                 tls_private_key_t   privKey,
                 const uint8_t       *plainText,
                 size_t              plainTextLen,
                 uint8_t				*sig,			// mallocd by caller; RETURNED
                 size_t              sigLen,         // available
                 size_t              *actualBytes)   // RETURNED
{
    size_t inOutSigLen = sigLen;

    assert(actualBytes != NULL);
    assert(privKey->desc.type == tls_private_key_type_ecdsa);

    int status = privKey->desc.ecdsa.sign(privKey->ctx, plainText, plainTextLen, sig,
                                          &inOutSigLen);

    if (status) {
        sslErrorLog("privKey->desc.ecdsa.sign: failed (error %d)\n", (int)status);
    }

    *actualBytes = inOutSigLen;
    return status;
}


/* TLS 1.2 RSA signature */
int sslRsaSign(
               tls_private_key_t   privKey,
               tls_hash_algorithm   hash,
               const uint8_t       *plainText,
               size_t              plainTextLen,
               uint8_t				*sig,			// mallocd by caller; RETURNED
               size_t              sigLen,         // available
               size_t              *actualBytes)   // RETURNED
{
    size_t inOutSigLen = sigLen;

    assert(actualBytes != NULL);
    assert(privKey->desc.type == tls_private_key_type_rsa);

    int status = privKey->desc.rsa.sign(privKey->ctx, hash, plainText, plainTextLen, sig, &inOutSigLen);

    if (status) {
        sslErrorLog("privKey->desc.rsa.sign: failed (error %d)\n", (int)status);
    }

    /* Since the KeyExchange already allocated modulus size bytes we'll
     use all of them.  SecureTransport has always sent that many bytes,
     so we're not going to deviate, to avoid interoperability issues. */
    if (!status && (inOutSigLen < sigLen)) {
        size_t offset = sigLen - inOutSigLen;
        memmove(sig + offset, sig, inOutSigLen);
        memset(sig, 0, offset);
        inOutSigLen = sigLen;
    }

    *actualBytes = inOutSigLen;
    return status;
}

/* FIXME: This is a "raw" verify, without OID, this should really be implemented in corecrypto. */
#define RSA_PKCS1_PAD_SIGN		0x01

static
int sslRawRsaVerify(
                    SSLPubKey           *pubKey,
                    const uint8_t       *plainText,
                    size_t              plainTextLen,
                    const uint8_t       *sig,
                    size_t              sigLen)         // available
{
    const uint8_t *oid = NULL;
    bool valid;

    if(!pubKey->isRSA || pubKey->rsa==NULL) {
        sslErrorLog("Internal Error: Invalid RSA public key\n");
        return errSSLInternal;
    }

    int status = ccrsa_verify_pkcs1v15(pubKey->rsa, oid, plainTextLen, plainText, sigLen, sig, &valid);

    if (status) {
        sslErrorLog("sslRawRsaVerify: ccrsa_verify_pkcs1v15 failed (error %d)\n", (int) status);
    } else {
        if(!valid) {
            sslErrorLog("sslRawRsaVerify: ccrsa_verify_pkcs1v15 signature verify error\n", (int) status);
            status = errSSLCrypto;
        }
    }
    return status;
}

static
int sslRawEccVerify(
                    SSLPubKey           *pubKey,
                    const uint8_t       *plainText,
                    size_t              plainTextLen,
                    const uint8_t       *sig,
                    size_t              sigLen)         // available
{
    int status = errSSLCrypto;
    bool valid;

    if(pubKey->isRSA || pubKey->ecc==NULL) {
        sslErrorLog("Internal Error: Invalid EC public key\n");
        return errSSLInternal;
    }

    status = ccec_verify(pubKey->ecc, plainTextLen, plainText, sigLen, sig, &valid);
    if (status) {
        sslErrorLog("sslRawEccVerify: ccec_verify failed (error %d)\n", (int) status);
    } else {
        if(!valid) {
            sslErrorLog("sslRawEccVerify: ccec_verify signature verify error\n", (int) status);
            status = errSSLCrypto;
        }
    }
    return status;
}

int sslRawVerify(
                 SSLPubKey           *pubKey,
                 const uint8_t       *plainText,
                 size_t              plainTextLen,
                 const uint8_t       *sig,
                 size_t              sigLen)         // available
{
    if(pubKey->isRSA) {
        return sslRawRsaVerify(pubKey, plainText, plainTextLen, sig, sigLen);
    } else {
        return sslRawEccVerify(pubKey, plainText, plainTextLen, sig, sigLen);
    }
}

#include <corecrypto/ccsha1.h>
#include <corecrypto/ccsha2.h>

static
const uint8_t *ccoidForSSLHash(tls_hash_algorithm hash)
{
    switch (hash) {
        case tls_hash_algorithm_SHA1:
            return ccoid_sha1;
        case tls_hash_algorithm_SHA256:
            return ccoid_sha256;
        case tls_hash_algorithm_SHA384:
            return ccoid_sha384;
        case tls_hash_algorithm_SHA512:
            return ccoid_sha512;
        default:
            break;
    }
    // Internal error
    assert(0);
    // This guarantee failure down the line
    return NULL;
}


/* TLS 1.2 RSA verify */
int sslRsaVerify(
                 SSLPubKey           *pubKey,
                 tls_hash_algorithm   hash,
                 const uint8_t       *plainText,
                 size_t              plainTextLen,
                 const uint8_t       *sig,
                 size_t              sigLen)         // available
{
    const uint8_t *oid = ccoidForSSLHash(hash);
    bool valid;

    if(!pubKey->isRSA || pubKey->rsa==NULL) {
        sslErrorLog("Internal Error: Invalid RSA public key\n");
        return errSSLInternal;
    }

    int status = ccrsa_verify_pkcs1v15(pubKey->rsa, oid, plainTextLen, plainText, sigLen, sig, &valid);

    if (status) {
        sslErrorLog("sslRsaVerify: ccrsa_verify_pkcs1v15 failed (error %d)\n", (int) status);
    } else {
        if(!valid) {
            sslErrorLog("sslRsaVerify: ccrsa_verify_pkcs1v15 signature verify error\n", (int) status);
            status = errSSLCrypto;
        }
    }
    return status;
}


/* TODO: Parts of this should really be in corecrypto */
#define RSA_PKCS1_PAD_ENCRYPT	0x02

/*
 * Encrypt/Decrypt
 */
int sslRsaEncrypt(
                  SSLPubKey           *pubKey,
                  const uint8_t       *plainText,
                  size_t              plainTextLen,
                  uint8_t				*cipherText,		// mallocd by caller; RETURNED
                  size_t              cipherTextLen,      // available
                  size_t              *actualBytes)       // RETURNED
{
    size_t ctlen = cipherTextLen;

    assert(actualBytes != NULL);

    if(!pubKey->isRSA || pubKey->rsa==NULL) {
        sslErrorLog("Internal Error: Invalid RSA public key\n");
        return errSSLInternal;
    }

    ccrsa_pub_ctx_t pubkey = pubKey->rsa;

    cc_unit s[ccrsa_ctx_n(pubkey)];
    const size_t m_size = ccn_write_uint_size(ccrsa_ctx_n(pubkey), ccrsa_ctx_m(pubkey));

    uint8_t* sBytes = (uint8_t*) s;

    // Create PKCS1 padding:
    //
    // 0x00, 0x01 (RSA_PKCS1_PAD_ENCRYPT), 0xFF .. 0x00, signedData
    //
    const int kMinimumPadding = 1 + 1 + 8 + 1;

    require_quiet(plainTextLen <= m_size - kMinimumPadding, errOut);

    size_t prefix_zeros = ccn_sizeof_n(ccrsa_ctx_n(pubkey)) - m_size;

    while (prefix_zeros--)
        *sBytes++ = 0x00;

    size_t pad_size = m_size - plainTextLen;

    *sBytes++ = 0x00;
    *sBytes++ = RSA_PKCS1_PAD_ENCRYPT;

    struct ccrng_state *rng = CCRNGSTATE();
    if (!rng) {
        abort();
    }

    require(ccrng_generate(rng, pad_size - 3, sBytes) == 0, errOut);

    // Remove zeroes from the random pad

    const uint8_t* sEndOfPad = sBytes + (pad_size - 3);
    while (sBytes < sEndOfPad)
    {
        if (*sBytes == 0x00)
            *sBytes = 0xFF; // Michael said 0xFF was good enough.

        ++sBytes;
    }

    *sBytes++ = 0x00;

    memcpy(sBytes, plainText, plainTextLen);

    ccn_swap(ccrsa_ctx_n(pubkey), s);

    require(ccrsa_pub_crypt(pubkey, s, s) == 0, errOut);

    ccn_write_uint_padded(ccrsa_ctx_n(pubkey), s, m_size, cipherText);
    ctlen = m_size;

    /* Since the KeyExchange already allocated modulus size bytes we'll
     use all of them.  SecureTransport has always sent that many bytes,
     so we're not going to deviate, to avoid interoperability issues. */
    if (ctlen < cipherTextLen) {
        size_t offset = cipherTextLen - ctlen;
        memmove(cipherText + offset, cipherText, ctlen);
        memset(cipherText, 0, offset);
        ctlen = cipherTextLen;
    }

    if (actualBytes) {
        *actualBytes = ctlen;
    }

    return errSSLSuccess;

errOut:
    sslErrorLog("***sslRsaEncrypt error\n");
    return errSSLCrypto;
}


int sslRsaDecrypt(
                  tls_private_key_t   privKey,
                  const uint8_t       *cipherText,
                  size_t              cipherTextLen,
                  uint8_t				*plainText,			// mallocd by caller; RETURNED
                  size_t              plainTextLen,		// available
                  size_t              *actualBytes) 		// RETURNED
{
    assert(privKey->desc.type == tls_private_key_type_rsa);
    size_t ptlen = plainTextLen;

    assert(actualBytes != NULL);

    int status = privKey->desc.rsa.decrypt(privKey->ctx, cipherText, cipherTextLen, plainText, &ptlen);
    *actualBytes = ptlen;

    if (status) {
        sslErrorLog("sslRsaDecrypt: privKey->desc.rsa->decrypt failed (error %d)\n", (int)status);
    }

    return status;
}

/*
 * Obtain size of the modulus of privKey in bytes.
 */
size_t sslPrivKeyLengthInBytes(tls_private_key_t privKey)
{
    return privKey->desc.rsa.size;
}

/*
 * Obtain size of the modulus of pubKey in bytes.
 */
size_t sslPubKeyLengthInBytes(SSLPubKey *pubKey)
{
    assert(pubKey->isRSA);
    return ccn_write_uint_size(ccrsa_ctx_n(pubKey->rsa), ccrsa_ctx_m(pubKey->rsa));
}


/*
 * Obtain maximum size of signature in bytes.
 */
int sslGetMaxSigSize(
                     tls_private_key_t privKey,
                     size_t            *maxSigSize)
{
    assert(maxSigSize != NULL);

    if (privKey == NULL) {
        return errSSLInternal;
    }

    if (privKey->desc.type == tls_private_key_type_ecdsa) {
        *maxSigSize = privKey->desc.ecdsa.size;
    } else if (privKey->desc.type == tls_private_key_type_rsa) {
        *maxSigSize = privKey->desc.rsa.size;
    } else
        return errSSLParam;

    return errSSLSuccess;
}


/*
 * Given raw RSA key bits, cook up a SSLPubKey. Used in
 * Server-initiated key exchange.
 */
int sslGetPubKeyFromBits(
                         const tls_buffer		*modulus,
                         const tls_buffer		*exponent,
                         SSLPubKey           *pubKey)
{
    if (!pubKey)
        return errSSLParam;

    cc_size n = ccn_nof_size(modulus->length);

    cc_unit mod[n];
    cc_unit exp[n];
    ccrsa_pub_ctx_t pub;

    require_noerr(ccn_read_uint(n, mod, modulus->length, modulus->data), errOut);
    require_noerr(ccn_read_uint(n, exp, exponent->length, exponent->data), errOut);
    require((ccn_bitlen(n, exp) > 1), errOut);

    require((pub = sslMalloc(ccrsa_pub_ctx_size(ccn_sizeof_n(n)))), errOut);

    ccrsa_ctx_n(pub) = n;
    ccrsa_init_pub(pub, mod, exp);

    pubKey->isRSA = true;
    pubKey->rsa = pub;
    return errSSLSuccess;

errOut:
    sslErrorLog("sslGetPubKeyFromBits: SecKeyCreateRSAPublicKey failed\n");
    return errSSLCrypto;
}

/* Given a curve name and bits, create a public EC key */
int sslGetEcPubKeyFromBits(
                           tls_named_curve namedCurve,
                           const tls_buffer     *pubKeyBits,
                           SSLPubKey           *pubKey)
{
    ccec_pub_ctx_t ecpub;
    ccec_const_cp_t cp;

    switch (namedCurve) {
        case tls_curve_secp256r1:
            cp = ccec_cp_256();
            break;
        case tls_curve_secp384r1:
            cp = ccec_cp_384();
            break;
        case tls_curve_secp521r1:
            cp = ccec_cp_521();
            break;
        default:
            /* should not have gotten this far */
            sslErrorLog("sslEcdhGenerateKeyPair: bad namedCurve (%u)\n",
                        (unsigned)namedCurve);
            return errSSLParam;
    }

    ecpub = sslMalloc(ccec_pub_ctx_size(ccec_ccn_size(cp)));
    if(ecpub==NULL)
        return errSSLAllocate;
    
    require_noerr(ccec_import_pub(cp, pubKeyBits->length, pubKeyBits->data, ecpub), errOut);
    
    pubKey->isRSA = false;
    pubKey->ecc = ecpub;
    
    return errSSLSuccess;
errOut:
    sslFree(ecpub);
    return errSSLCrypto;
}
