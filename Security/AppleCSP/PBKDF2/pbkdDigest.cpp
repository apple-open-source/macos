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
 * pbkdDigest.cpp - SHA1/MD5 digest object for HMAC and PBE routines
 */

#include "pbkdDigest.h"
#include <Security/cssmerr.h>
#include <string.h>

/* Ops on a DigestCtx */
/* Note caller has to memset(0) the DigestCtx before using */
CSSM_RETURN DigestCtxInit(
	DigestCtx 	*ctx,
	CSSM_BOOL	isSha1)
{
	if(isSha1) {
		if(ctx->dig.sha1Context == NULL) {
			ctx->dig.sha1Context = sha1Alloc();
			if(ctx->dig.sha1Context == NULL) {
				return CSSMERR_CSP_MEMORY_ERROR;
			}
		}
		else {
			sha1Reinit(ctx->dig.sha1Context);
		}
	}
	else {
		MD5Init(&ctx->dig.md5Context);
	}
	ctx->isSha1 = isSha1;
	return CSSM_OK;
}

void DigestCtxFree(
	DigestCtx 	*ctx)
{
	if(ctx->isSha1) {
		sha1Free(ctx->dig.sha1Context);
	}
	memset(ctx, 0, sizeof(DigestCtx));
}

void DigestCtxUpdate(
	DigestCtx 	*ctx,
	const void *textPtr,
	UInt32 textLen)
{
	if(ctx->isSha1) {
		sha1AddData(ctx->dig.sha1Context, (unsigned char *)textPtr, textLen);
	}
	else {
		MD5Update(&ctx->dig.md5Context, (unsigned char *)textPtr, textLen);
	}
}

void DigestCtxFinal(
	DigestCtx 	*ctx,
	void 		*digest)
{
	if(ctx->isSha1) {
		sha1GetDigest(ctx->dig.sha1Context, (unsigned char *)digest);
	}
	else {
		MD5Final(&ctx->dig.md5Context, (unsigned char *)digest);
	}
}
