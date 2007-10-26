/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */
/*
 * pbkdDigest.h - SHA1/MD5 digest object for HMAC and PBE routines
 */
 
#ifndef	_PBKD_DIGEST_H_
#define _PBKD_DIGEST_H_

#include <Security/cssmtype.h>
#include <CommonCrypto/CommonDigest.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kSHA1DigestSize  		CC_SHA1_DIGEST_LENGTH
#define kSHA1BlockSize  		CC_SHA1_BLOCK_BYTES

#define kMD5DigestSize  		CC_MD5_DIGEST_LENGTH
#define kMD5BlockSize  			CC_MD5_BLOCK_BYTES

#define kMD2DigestSize  		CC_MD2_DIGEST_LENGTH
#define kMD2BlockSize  			CC_MD2_BLOCK_BYTES

#define kMaxDigestSize			kSHA1DigestSize

typedef int (*DigestInitFcn)(void *ctx);
typedef int (*DigestUpdateFcn)(void *ctx, const void *data, unsigned long len);
typedef int (*DigestFinalFcn)(void *md, void *c);

/* callouts to eay/libmd implementations */
typedef struct {
	DigestInitFcn		init;
	DigestUpdateFcn		update;
	DigestFinalFcn		final;
} DigestOps;

typedef	struct {
	union {
		CC_SHA1_CTX 	sha1Context;
		CC_MD5_CTX		md5Context;
		CC_MD2_CTX		md2Context;
	} dig;
	DigestOps 		*ops;
	CSSM_ALGORITHMS hashAlg;
} DigestCtx;

/* Ops on a DigestCtx - all return zero on error, like the underlying digests do */
int DigestCtxInit(
	DigestCtx		*ctx,
	CSSM_ALGORITHMS hashAlg);
void DigestCtxFree(
	DigestCtx 	*ctx);
int DigestCtxUpdate(
	DigestCtx 	*ctx,
	const void 	*textPtr,
	uint32 textLen);
int DigestCtxFinal(
	DigestCtx 	*ctx,
	void 		*digest);

#ifdef __cplusplus
}
#endif

#endif	/* _PBKD_DIGEST_H_ */

