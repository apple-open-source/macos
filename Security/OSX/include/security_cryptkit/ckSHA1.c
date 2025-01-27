/* Copyright (c) 1998,2011,2014 Apple Inc.  All Rights Reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * ckSHA1.c - generic, portable SHA-1 hash object
 *
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 * 07 Jan 1998 at Apple
 *	Created.
 */
 
#include "ckconfig.h"
#include "feeTypes.h"
#include "ckSHA1.h"

/*
 * For linking with AppleCSP: use libSystem SHA1 implementation.
 */
#include <CommonCrypto/CommonDigest.h>
#include "falloc.h"
#include "platform.h"

/*
 * Trivial wrapper for SHA_CTX; a sha1Obj is a pointer to this.
 */
typedef struct {
	CC_SHA1_CTX		ctx;
	unsigned char   digest[CC_SHA1_DIGEST_LENGTH];
} Sha1Obj;

sha1Obj sha1Alloc(void)
{
	void *rtn = fmalloc(sizeof(Sha1Obj));
	memset(rtn, 0, sizeof(Sha1Obj));
	CC_SHA1_Init(&(((Sha1Obj *)rtn)->ctx));
	return (sha1Obj)rtn;
}

void sha1Reinit(sha1Obj sha1)
{
	Sha1Obj *ctx = (Sha1Obj *)sha1;
	CC_SHA1_Init(&ctx->ctx);
}

void sha1Free(sha1Obj sha1)
{
	memset(sha1, 0, sizeof(Sha1Obj));
	ffree(sha1);
}

void sha1AddData(sha1Obj sha1,
	const unsigned char *data,
	unsigned dataLen)
{
	Sha1Obj *ctx = (Sha1Obj *)sha1;
	CC_SHA1_Update(&ctx->ctx, data, dataLen);
}

unsigned char *sha1Digest(sha1Obj sha1)
{
	Sha1Obj *ctx = (Sha1Obj *)sha1;
	CC_SHA1_Final(ctx->digest, &ctx->ctx);
	return ctx->digest;
}

unsigned sha1DigestLen(void)
{
	return CC_SHA1_DIGEST_LENGTH;
}

