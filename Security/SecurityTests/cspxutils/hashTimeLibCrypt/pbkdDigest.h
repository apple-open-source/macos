/*
 * Copyright (c) 2003-2005 Apple Computer, Inc. All Rights Reserved.
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
 * pbkdDigest.h - SHA1/MD5 digest object
 */
 
#ifndef	_PBKD_DIGEST_H_
#define _PBKD_DIGEST_H_

#include <Security/cssmtype.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kSHA1DigestSize  		SHA_DIGEST_LENGTH
#define kSHA1BlockSize  		SHA_CBLOCK

#define kMD5DigestSize  		MD5_DIGEST_LENGTH
#define kMD5BlockSize  			MD5_CBLOCK

typedef int (*DigestInitFcn)(void *ctx);
typedef int (*DigestUpdateFcn)(void *ctx, const void *data, unsigned long len);
typedef int (*DigestFinalFcn)(void *md, void *c);

/* callouts to libcrypt */
typedef struct {
	DigestInitFcn		init;
	DigestUpdateFcn		update;
	DigestFinalFcn		final;
} DigestOps;

typedef	struct {
	union {
		SHA_CTX 	sha1Context;
		MD5_CTX		md5Context;
	} dig;
	DigestOps 		*ops;
	CSSM_BOOL 		isSha1;
} DigestCtx;

/* Ops on a DigestCtx */
int DigestCtxInit(
	DigestCtx 	*ctx,
	CSSM_BOOL	isSha1);
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

