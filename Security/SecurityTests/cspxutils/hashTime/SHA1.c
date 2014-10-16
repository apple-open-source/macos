/* Copyright (c) 1998,2004 Apple Computer, Inc.  All Rights Reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE COMPUTER, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE COMPUTER,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * SHA1.c - generic, portable SHA-1 hash object
 *
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 * 07 Jan 1998	Doug Mitchell at Apple
 *	Created.
 */

#include "SHA1.h"
#include "SHA1_priv.h"
#include <strings.h>

/* for now map falloc to malloc, FIXME */
#include <stdlib.h>
#define fmalloc(s)	malloc(s)
#define ffree(p)	free(p)

/*
 * Private data for this object. A sha1Obj handle is cast to a pointer
 * to one of these.
 */
typedef struct {
	SHS_INFO 	context;
	int 		isDone;

	/*
	 * For storing partial blocks
	 */
	BYTE		dataBuf[SHS_BLOCKSIZE];
	unsigned	bufBytes;		// valid bytes in dataBuf[p]
} sha1Inst;

/*
 * Alloc and init an empty sha1 object.
 */
sha1Obj sha1Alloc(void)
{
	sha1Inst *sinst;

	sinst = (sha1Inst *)fmalloc(sizeof(sha1Inst));
	if(sinst == NULL) {
		return NULL;
	}
	shsInit(&sinst->context);
	sha1Reinit((sha1Obj)sinst);
	return (sha1Obj)sinst;
}

/*
 * Reusable init function.
 */
void sha1Reinit(sha1Obj sha1)
{
	sha1Inst *sinst = (sha1Inst *) sha1;

	shsInit(&sinst->context);
	sinst->isDone = 0;
	sinst->bufBytes = 0;
}

/*
 * Free an sha1 object.
 */
void sha1Free(sha1Obj sha1)
{
	sha1Inst *sinst = (sha1Inst *) sha1;

	memset(sha1, 0, sizeof(sha1Inst));
	ffree(sinst);
}

/*
 * Add some data to the sha1 object.
 */
void sha1AddData(sha1Obj sha1,
	const unsigned char *data,
	unsigned dataLen)
{
	sha1Inst *sinst = (sha1Inst *) sha1;
	unsigned toMove;
	unsigned blocks;

	if(sinst->isDone) {
		/*
		 * Log some kind of error here...
		 */
		return;
	}

	/*
	 * First deal with partial buffered block
	 */
	if(sinst->bufBytes != 0) {
		toMove = SHS_BLOCKSIZE - sinst->bufBytes;
		if(toMove > dataLen) {
			toMove = dataLen;
		}
		memmove(sinst->dataBuf+sinst->bufBytes, data, toMove);
		data += toMove;
		dataLen -= toMove;
		sinst->bufBytes += toMove;
		if(sinst->bufBytes == SHS_BLOCKSIZE) {
		    shsUpdate(&sinst->context, sinst->dataBuf, SHS_BLOCKSIZE);
		    sinst->bufBytes = 0;
		}
	}

	/*
	 * Now the bulk of the data, in a multiple of full blocks
	 */
	blocks = dataLen / SHS_BLOCKSIZE;
	toMove = blocks * SHS_BLOCKSIZE;
	if(toMove != 0) {
	    shsUpdate(&sinst->context, data, toMove);
	    data += toMove;
	    dataLen -= toMove;
	}

	/*
	 * Store any remainder in dataBuf
	 */
	if(dataLen != 0) {
		memmove(sinst->dataBuf, data, dataLen);
		sinst->bufBytes = dataLen;
	}
}

/*
 * Obtain a pointer to completed message digest, and the length of the digest.
 */
unsigned char *sha1Digest(sha1Obj sha1)
{
	sha1Inst *sinst = (sha1Inst *) sha1;

	if(!sinst->isDone) {
		/*
		 * Deal with partial resid block
		 */
		if(sinst->bufBytes != 0) {
			shsUpdate(&sinst->context, sinst->dataBuf,
				sinst->bufBytes);
			sinst->bufBytes = 0;
		}
		shsFinal(&sinst->context);
		sinst->isDone = 1;
	}
	/*
	 * FIXME - should do explicit conversion to char array....?
	 */
	return (unsigned char *)sinst->context.digest;
}

/* As above, with copy. */
void sha1GetDigest(sha1Obj sha1,
	unsigned char *digest)
{
	unsigned char *dig = sha1Digest(sha1);
	memmove(digest, dig, SHS_DIGESTSIZE);
}

unsigned sha1DigestLen(void)
{
	return SHS_DIGESTSIZE;
}
