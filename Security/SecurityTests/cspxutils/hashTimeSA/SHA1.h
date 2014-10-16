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
 * SHA1.h - generic, portable SHA-1 hash object
 *
 * Revision History
 * ----------------
 * 05 Jan 1998	Doug Mitchell at Apple
 *	Created.
 */

#ifndef	_CK_SHA1_H_
#define _CK_SHA1_H_

#ifdef __cplusplus
extern "C" {
#endif

#define SHA1_DIGEST_SIZE		20	/* in bytes */
#define SHA1_BLOCK_SIZE			64	/* in bytes */

/*
 * Opaque sha1 object handle.
 */
typedef void *sha1Obj;

/*
 * Alloc and init an empty sha1 object.
 */
sha1Obj sha1Alloc(void);

/*
 * reinitialize an sha1 object for reuse.
 */
void sha1Reinit(sha1Obj sha1);

/*
 * Free an sha1 object.
 */
void sha1Free(sha1Obj sha1);

/*
 * Add some data to the sha1 object.
 */
void sha1AddData(sha1Obj sha1,
	const unsigned char *data,
	unsigned dataLen);

/*
 * Obtain a pointer to completed message digest. This disables further calls
 * to sha1AddData(). This pointer is NOT malloc'd; the associated data
 * persists only as long as this object does.
 */
unsigned char *sha1Digest(sha1Obj sha1);

/* As above, with copy. */
void sha1GetDigest(sha1Obj sha1,
	unsigned char *digest);
	
/*
 * Obtain the length of the message digest.
 */
unsigned sha1DigestLen(void);

#ifdef __cplusplus
}
#endif

#endif	/*_CK_SHA1_H_*/
