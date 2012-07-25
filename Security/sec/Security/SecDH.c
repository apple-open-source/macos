/*
 * Copyright (c) 2007-2008,2010 Apple Inc. All Rights Reserved.
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
 * SecDH.c - Implement the crypto required for a Diffie-Hellman key exchange.
 */

#include "SecDH.h"
#include <libDER/DER_Keys.h>
#include <corecrypto/ccdh.h>
#include <libDER/DER_Keys.h>
#include <libDER/DER_Encode.h>
#include <libDER/asn1Types.h>
#include <libkern/OSByteOrder.h>
#include <security_utilities/debugging.h>
#include <Security/SecInternal.h>
#include <Security/SecRandom.h>
#include <stdlib.h>
#include <MacErrors.h>
#include <Security/SecBasePriv.h>

#ifdef DEBUG
#define DH_DEBUG  1
#endif

static inline ccdh_gp_t SecDH_gp(SecDHContext dh)
{
    void *p = dh;
    ccdh_gp_t gp = { .gp = p };
    return gp;
}

static inline ccdh_full_ctx_t SecDH_priv(SecDHContext dh)
{
    void *p = dh;
    cczp_t zp = { .u = p };
    cc_size s = ccn_sizeof_n(cczp_n(zp));
    ccdh_full_ctx_t priv = { .hdr = (struct ccdh_ctx_header *)(p+ccdh_gp_size(s)) };
    return priv;
}

static inline size_t SecDH_context_size(size_t p_len)
{
    cc_size n = ccn_nof_size(p_len);
    cc_size real_p_len = ccn_sizeof_n(n);
    size_t context_size = ccdh_gp_size(real_p_len)+ccdh_full_ctx_size(real_p_len);
    return context_size;
}

/* Shared static functions. */

static OSStatus
der2OSStatus(DERReturn derReturn)
{
	switch(derReturn)
	{
	case DR_Success:				return noErr;
	case DR_EndOfSequence:			return errSecDecode; 
	case DR_UnexpectedTag:			return errSecDecode;
	case DR_DecodeError:			return errSecDecode;
	case DR_Unimplemented:			return unimpErr;
	case DR_IncompleteSeq:			return errSecDecode;
	case DR_ParamErr:				return paramErr;
	case DR_BufOverflow:			return errSecBufferTooSmall;
	default:						return errSecInternal;
	}
}

static int dhRngCallback(struct ccrng_state *rng, unsigned long outlen, void *out)
{
    return SecRandomCopyBytes(kSecRandomDefault, outlen, out);
}

static struct ccrng_state dhrng = {
    .generate = dhRngCallback
};

OSStatus SecDHCreate(uint32_t g, const uint8_t *p, size_t p_len,
	uint32_t l, const uint8_t *recip, size_t recip_len, SecDHContext *pdh)
{
    cc_size n = ccn_nof_size(p_len);
    size_t context_size = SecDH_context_size(p_len);
    void *context = malloc(context_size);
    bzero(context, context_size);

    ccdh_gp_t gp;
    gp.gp = context;

    CCDH_GP_N(gp) = n;
    CCDH_GP_L(gp) = l;

    if(ccn_read_uint(n, CCDH_GP_PRIME(gp), p_len, p))
        goto errOut;
    if(recip) {
        if(ccn_read_uint(n+1, CCDH_GP_RECIP(gp), recip_len, recip))
            goto errOut;
        gp.zp.zp->mod_prime = cczp_mod;
    } else {
        cczp_init(gp.zp);
    };
    ccn_seti(n, CCDH_GP_G(gp), g);

    *pdh = (SecDHContext) context;

    return noErr;

errOut:
    SecDHDestroy(context);
    *pdh = NULL;
    return errSecInternal;

}

/* this used to be in libgDH */
/*
 * Support for encoding and decoding DH parameter blocks.
 * Apple form encodes the reciprocal of the prime p.
 */
/* PKCS3, Openssl compatible */
typedef struct {
	DERItem				p;
	DERItem				g;
	DERItem				l;
	DERItem				recip; /* Only used in Apple Custom blocks. */
} DER_DHParams;

static const DERItemSpec DER_DHParamsItemSpecs[] =
{
	{ DER_OFFSET(DER_DHParams, p),
        ASN1_INTEGER,
        DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
	{ DER_OFFSET(DER_DHParams, g),
        ASN1_INTEGER,
        DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
	{ DER_OFFSET(DER_DHParams, l),
        ASN1_INTEGER,
        DER_DEC_OPTIONAL | DER_ENC_SIGNED_INT },
    /* Not part of PKCS3 per-se, but we add it on just for kicks.  Since
     it's optional we will automatically decode any apple specific
     params, but we won't add this section unless the caller asks
     us to.  */
	{ DER_OFFSET(DER_DHParams, recip),
        ASN1_PRIVATE | ASN1_PRIMITIVE | 0,
        DER_DEC_OPTIONAL | DER_ENC_SIGNED_INT },
};
static const DERSize DER_NumDHParamsItemSpecs =
sizeof(DER_DHParamsItemSpecs) / sizeof(DERItemSpec);


OSStatus SecDHCreateFromParameters(const uint8_t *params,
	size_t params_len, SecDHContext *pdh)
{
    DERReturn drtn;
	DERItem paramItem = {(DERByte *)params, params_len};
	DER_DHParams decodedParams;
    uint32_t l;

    drtn = DERParseSequence(&paramItem,
                            DER_NumDHParamsItemSpecs, DER_DHParamsItemSpecs,
                            &decodedParams, sizeof(decodedParams));
    if(drtn)
        return drtn;

    drtn = DERParseInteger(&decodedParams.l, &l);
    if(drtn)
        return drtn;
    cc_size n = ccn_nof_size(decodedParams.p.length);
    cc_size p_len = ccn_sizeof_n(n);
    size_t context_size = ccdh_gp_size(p_len)+ccdh_full_ctx_size(p_len);
    void *context = malloc(context_size);
    if(context==NULL)
        return memFullErr;

    bzero(context, context_size);

    ccdh_gp_t gp;
    gp.gp = context;

    CCDH_GP_N(gp) = n;
    CCDH_GP_L(gp) = l;

    if(ccn_read_uint(n, CCDH_GP_PRIME(gp), decodedParams.p.length, decodedParams.p.data))
        goto errOut;
    if(decodedParams.recip.length) {
        if(ccn_read_uint(n+1, CCDH_GP_RECIP(gp), decodedParams.recip.length, decodedParams.recip.data))
            goto errOut;
        gp.zp.zp->mod_prime = cczp_mod;
    } else {
        cczp_init(gp.zp);
    };

    if(ccn_read_uint(n, CCDH_GP_G(gp), decodedParams.g.length, decodedParams.g.data))
        goto errOut;

    *pdh = (SecDHContext) context;
    return noErr;

errOut:
    SecDHDestroy(context);
    *pdh = NULL;
    return errSecInvalidKey;
}

OSStatus SecDHCreateFromAlgorithmId(const uint8_t *alg, size_t alg_len,
	SecDHContext *pdh) {
	DERAlgorithmId algorithmId;
	DERItem algId;

	algId.data = (uint8_t *)alg;
	algId.length = alg_len;

	DERReturn drtn = DERParseSequence(&algId,
		DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
		&algorithmId, sizeof(algorithmId));
	if (drtn != DR_Success)
		return der2OSStatus(drtn);

    return SecDHCreateFromParameters(algorithmId.params.data,
		algorithmId.params.length, pdh);
}

size_t SecDHGetMaxKeyLength(SecDHContext dh) {
    cczp_const_t zp;
    zp.u = (cc_unit *)dh;

    return ccn_sizeof_n(cczp_n(zp));
}


OSStatus SecDHGenerateKeypair(SecDHContext dh, uint8_t *pub_key,
	size_t *pub_key_len)
{
    int result;
    ccdh_gp_t gp = SecDH_gp(dh);
    ccdh_full_ctx_t priv = SecDH_priv(dh);

    if((result = ccdh_generate_key(gp, &dhrng, priv)))
        return result;

    /* output y as a big endian byte buffer */
    size_t ylen = ccn_write_uint_size(ccdh_gp_n(gp), ccdh_ctx_y(priv));
    if(*pub_key_len < ylen)
       return errSecBufferTooSmall;
    ccn_write_uint(ccdh_gp_n(gp),ccdh_ctx_y(priv), ylen, pub_key);
    *pub_key_len = ylen;

    return noErr;
}

OSStatus SecDHComputeKey(SecDHContext dh,
	const uint8_t *pub_key, size_t pub_key_len,
    uint8_t *computed_key, size_t *computed_key_len)
{
    ccdh_gp_t gp = SecDH_gp(dh);
    ccdh_full_ctx_t priv = SecDH_priv(dh);
    ccdh_pub_ctx_decl_gp(gp, pub);
    cc_size n = ccdh_gp_n(gp);
    cc_unit r[n];

    if(ccdh_import_pub(gp, pub_key_len, pub_key, pub))
        return errSecInvalidKey;

    if(ccdh_compute_key(priv, pub, r))
        return errSecInvalidKey;

    ccn_write_uint(n, r, *computed_key_len, computed_key);
    size_t out_size = ccn_write_uint_size(n, r);
    if(out_size < *computed_key_len)
        *computed_key_len=out_size;

    return noErr;
}

void SecDHDestroy(SecDHContext dh) {
	/* Zero out key material. */
    ccdh_gp_t gp = SecDH_gp(dh);
    cc_size p_len = ccn_sizeof_n(ccdh_gp_n(gp));
    size_t context_size = SecDH_context_size(p_len);

    bzero(dh, context_size);
    free(dh);
}
