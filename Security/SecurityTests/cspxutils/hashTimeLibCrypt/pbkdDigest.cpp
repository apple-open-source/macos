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
 * pbkdDigest.cpp - SHA1/MD5 digest object 
 */

#include "pbkdDigest.h"
#include <Security/cssmerr.h>
#include <string.h>

/* the casts are necessary to cover the polymorphous context types */
DigestOps Md5Ops = { 
	(DigestInitFcn)MD5_Init, 
	(DigestUpdateFcn)MD5_Update, 
	(DigestFinalFcn)MD5_Final
};
DigestOps Sha1Ops = { 
	(DigestInitFcn)SHA1_Init, 
	(DigestUpdateFcn)SHA1_Update, 
	(DigestFinalFcn)SHA1_Final
};

/* Ops on a DigestCtx */
int DigestCtxInit(
	DigestCtx 	*ctx,
	CSSM_BOOL	isSha1)
{
	if(isSha1) {
		ctx->ops = &Sha1Ops;
	}
	else {
		ctx->ops = &Md5Ops;
	}
	ctx->isSha1 = isSha1;
	return ctx->ops->init(&ctx->dig);
}

void DigestCtxFree(
	DigestCtx 	*ctx)
{
	memset(ctx, 0, sizeof(DigestCtx));
}

int DigestCtxUpdate(
	DigestCtx 	*ctx,
	const void *textPtr,
	uint32 textLen)
{
	return ctx->ops->update(&ctx->dig, textPtr, textLen);
}

int DigestCtxFinal(
	DigestCtx 	*ctx,
	void 		*digest)
{
	return ctx->ops->final(digest, &ctx->dig);
}
