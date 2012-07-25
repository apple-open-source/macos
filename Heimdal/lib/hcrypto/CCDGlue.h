/*
 * Copyright (c) 1995 - 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Compat glue with CommonCrypto Digest functions
 */
 
#ifndef	_HCRYPTO_COMMON_DIGEST_H
#define _HCRYPTO_COMMON_DIGEST_H

#ifdef __cplusplus
#define HCCC_CPP_BEGIN extern "C" {
#define HCCC_CPP_END }
#else
#define HCCC_CPP_BEGIN
#define HCCC_CPP_END
#endif

#include <krb5-types.h>

#define CCDigest hc_CCDigest
#define CCDigestCreate hc_CCDigestCreate
#define CCDigestUpdate hc_CCDigestUpdate
#define CCDigestFinal hc_CCDigestFinal
#define CCDigestDestroy hc_CCDigestDestroy
#define CCDigestReset hc_CCDigestReset
#define CCDigestBlockSize hc_CCDigestBlockSize
#define CCDigestOutputSize hc_CCDigestOutputSize

HCCC_CPP_BEGIN

typedef struct CCDigestCtx_s *CCDigestRef;
typedef const struct CCDigest_s *CCDigestAlg;


#define kCCDigestMD2 (&hc_kCCDigestMD2_s)
extern const struct CCDigest_s hc_kCCDigestMD2_s;
#define kCCDigestMD4 (&hc_kCCDigestMD4_s)
extern const struct CCDigest_s hc_kCCDigestMD4_s;
#define kCCDigestMD5 (&hc_kCCDigestMD5_s)
extern const struct CCDigest_s hc_kCCDigestMD5_s;
#define kCCDigestSHA1 (&hc_kCCDigestSHA1_s)
extern const struct CCDigest_s hc_kCCDigestSHA1_s;
#define kCCDigestSHA256 (&hc_kCCDigestSHA256_s)
extern const struct CCDigest_s hc_kCCDigestSHA256_s;
#define kCCDigestSHA384 (&hc_kCCDigestSHA384_s)
extern const struct CCDigest_s hc_kCCDigestSHA384_s;
#define kCCDigestSHA512 (&hc_kCCDigestSHA512_s)
extern const struct CCDigest_s hc_kCCDigestSHA512_s;


int		CCDigest(CCDigestAlg, const void *, size_t, void *);
CCDigestRef	CCDigestCreate(CCDigestAlg);
int		CCDigestUpdate(CCDigestRef, const void *, size_t);
int		CCDigestFinal(CCDigestRef, void *);
void		CCDigestDestroy(CCDigestRef);
void		CCDigestReset(CCDigestRef);
size_t		CCDigestBlockSize(CCDigestRef) ;
size_t		CCDigestOutputSize(CCDigestRef);
    

#define CC_MD4_DIGEST_LENGTH 16
#define CC_MD5_DIGEST_LENGTH 16
#define CC_SHA1_DIGEST_LENGTH 20
#define CC_SHA256_DIGEST_LENGTH 32
#define CC_SHA384_DIGEST_LENGTH 48
#define CC_SHA512_DIGEST_LENGTH 64

/*
 *
 */
#if 0
typedef struct CCRandom_s *CCRandomRef;

#define kCCRandomDefault ((CCRandomRef)NULL)

#define CCRandomCopyBytes hc_CCRandomCopyBytes

int
CCRandomCopyBytes(CCRandomRef, void *, size_t);
#endif
/*
 *
 */

#ifndef HAVE_CCDESISWEAKKEY

#define CCDesIsWeakKey hc_CCDesIsWeakKey
#define CCDesSetOddParity hc_CCDesSetOddParity
#define CCDesCBCCksum hc_CCDesCBCCksum

CCCryptorStatus
CCDesIsWeakKey(const void *key, size_t length);

void
CCDesSetOddParity(void *key, size_t Length);

uint32_t
CCDesCBCCksum(void *input, void *output,
	      size_t length, void *key, size_t keylen,
	      void *ivec);

#endif

HCCC_CPP_END

#endif	/* _HCRYPTO_COMMON_DIGEST_H */
