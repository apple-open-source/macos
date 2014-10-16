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
 * FeeHash.h - generic, portable MD5 hash object
 *
 * Revision History
 * ----------------
 * 22 Aug 96 at NeXT
 *	Created.
 */

#ifndef	_CK_FEEHASH_H_
#define _CK_FEEHASH_H_

#if	!defined(__MACH__)
#include <ckconfig.h>
#include <feeTypes.h>
#else
#include <security_cryptkit/ckconfig.h>
#include <security_cryptkit/feeTypes.h>
#endif

#if	CRYPTKIT_MD5_ENABLE

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Opaque hash object handle.
 */
typedef void *feeHash;

/*
 * Alloc and init an empty hash object.
 */
feeHash feeHashAlloc(void);

/*
 * reinitialize a hash object for reuse.
 */
void feeHashReinit(feeHash hash);

/*
 * Free a hash object.
 */
void feeHashFree(feeHash hash);

/*
 * Add some data to the hash object.
 */
void feeHashAddData(feeHash hash,
	const unsigned char *data,
	unsigned dataLen);

/*
 * Obtain a pointer to completed message digest. This disables further calls
 * to feeHashAddData(). This pointer is NOT malloc'd; the associated data
 * persists only as long as this object does.
 */
unsigned char *feeHashDigest(feeHash hash);

/*
 * Obtain the length of the message digest.
 */
unsigned feeHashDigestLen(void);

#ifdef __cplusplus
}
#endif

#endif	/* CRYPTKIT_MD5_ENABLE */

#endif	/*_CK_FEEHASH_H_*/
