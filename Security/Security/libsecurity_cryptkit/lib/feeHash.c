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
 * FeeHash.c - generic, portable MD5 hash object
 *
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 * 22 Aug 96 at NeXT
 *	Created.
 */

#include "ckconfig.h"

#if	CRYPTKIT_MD5_ENABLE

#include "feeTypes.h"
#include "feeHash.h"
#include "ckMD5.h"
#include "falloc.h"
#include "platform.h"

/*
 * Private data for this object. A feeHash handle is cast to aa pointer
 * to one of these.
 */
typedef struct {
	MD5Context		context;
	int 			isDone;
	unsigned char 	digest[MD5_DIGEST_SIZE];
} hashInst;

/*
 * Alloc and init an empty hash object.
 */
feeHash feeHashAlloc(void)
{
	hashInst *hinst;

	hinst = (hashInst *) fmalloc(sizeof(hashInst));
	MD5Init(&hinst->context);
	hinst->isDone = 0;
	return hinst;
}

void feeHashReinit(feeHash hash)
{
	hashInst *hinst = (hashInst *) hash;

	MD5Init(&hinst->context);
	hinst->isDone = 0;
}

/*
 * Free a hash object.
 */
void feeHashFree(feeHash hash)
{
	hashInst *hinst = (hashInst *) hash;

	memset(hinst, 0, sizeof(hashInst));
	ffree(hinst);
}

/*
 * Add some data to the hash object.
 */
void feeHashAddData(feeHash hash,
	const unsigned char *data,
	unsigned dataLen)
{
	hashInst *hinst = (hashInst *) hash;

	if(hinst->isDone) {
		/*
		 * Log some kind of error here...
		 */
		return;
	}
	MD5Update(&hinst->context, data, dataLen);
}

/*
 * Obtain a pointer to completed message digest, and the length of the digest.
 */
unsigned char *feeHashDigest(feeHash hash)
{
	hashInst *hinst = (hashInst *) hash;

	if(!hinst->isDone) {
		MD5Final(&hinst->context, hinst->digest);
		hinst->isDone = 1;
	}
	return hinst->digest;
}

unsigned feeHashDigestLen(void)
{
	return MD5_DIGEST_SIZE;
}

#endif	/* CRYPTKIT_MD5_ENABLE*/
