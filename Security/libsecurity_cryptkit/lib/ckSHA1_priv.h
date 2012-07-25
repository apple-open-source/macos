/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE COMPUTER, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE COMPUTER,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * ckSHA1_priv.h - private low-level API for SHA-1 hash algorithm
 *
 * Revision History
 * ----------------
 * 22 Aug 96	Doug Mitchell at NeXT
 *	Created.
 */

/* Useful defines/typedefs */

#ifndef	_CK_SHA1_PRIV_H_
#define	_CK_SHA1_PRIV_H_

#include "ckconfig.h"

#if	!CRYPTKIT_LIBMD_DIGEST

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char   BYTE;
typedef unsigned long   LONG;

/* The SHS block size and message digest sizes, in bytes */

#define SHS_BLOCKSIZE   64
#define SHS_DIGESTSIZE  20

/* The structure for storing SHS info */

typedef struct {
	       LONG digest[ 5 ];            /* Message digest */
	       LONG countLo, countHi;       /* 64-bit bit count */
	       LONG data[ 80 ];             /* SHS data buffer */
	       } SHS_INFO;

extern void shsInit(SHS_INFO *shsInfo);
extern void shsUpdate(SHS_INFO *shsInfo,
	const BYTE *buffer,
	int count);
extern void shsFinal(SHS_INFO *shsInfo);

#ifdef __cplusplus
}
#endif

#endif	/* !CRYPTKIT_LIBMD_DIGEST */

#endif	/* _CK_SHA1_PRIV_H_ */
