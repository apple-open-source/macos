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
#include <MiscCSPAlgs/MD5.h>
#include <MiscCSPAlgs/SHA1.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kSHA1DigestSize  		SHA1_DIGEST_SIZE
#define kSHA1BlockSize  		SHA1_BLOCK_SIZE

#define kMD5DigestSize  		MD5_DIGEST_SIZE
#define kMD5BlockSize  			MD5_BLOCK_SIZE


typedef	struct {
	union {
		sha1Obj 			sha1Context;	// must be allocd via sha1Alloc
		struct MD5Context	md5Context;
	} dig;
	CSSM_BOOL isSha1;
} DigestCtx;

/* Ops on a DigestCtx */
CSSM_RETURN DigestCtxInit(
	DigestCtx 	*ctx,
	CSSM_BOOL	isSha1);
void DigestCtxFree(
	DigestCtx 	*ctx);
void DigestCtxUpdate(
	DigestCtx 	*ctx,
	const void 	*textPtr,
	UInt32 textLen);
void DigestCtxFinal(
	DigestCtx 	*ctx,
	void 		*digest);

#ifdef __cplusplus
}
#endif

#endif	/* _PBKD_DIGEST_H_ */

