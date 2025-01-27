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
 * ckSHA1.h - generic, portable SHA-1 hash object
 *
 * Revision History
 * ----------------
 * 05 Jan 1998 at Apple
 *	Created.
 */

#ifndef	_CK_SHA1_H_
#define _CK_SHA1_H_

#if	!defined(__MACH__)
#include <feeTypes.h>
#else
#include <security_cryptkit/feeTypes.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

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

/*
 * Obtain the length of the message digest.
 */
unsigned sha1DigestLen(void);

#ifdef __cplusplus
}
#endif

#endif	/*_CK_SHA1_H_*/
